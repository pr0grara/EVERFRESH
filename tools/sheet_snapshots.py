#!/usr/bin/env python3
"""EVERFRESH — daily chart snapshots from the Google Sheet log.

Pulls the 'log' sheet as CSV and renders temp / RH / VPD charts for several time
windows into a dated folder (one folder per run day). Designed to run once a day
via launchd — see com.everfresh.snapshots.plist and tools/README.md.

Requires: matplotlib  (pip install matplotlib)

The sheet must be readable WITHOUT auth — set it to
"Anyone with the link -> Viewer" (or File -> Share -> Publish to web).
View-only access is enough; this script never writes to the sheet.
"""

import argparse
import csv
import io
import sys
import urllib.request
from datetime import datetime, timedelta
from pathlib import Path
from zoneinfo import ZoneInfo

import matplotlib
matplotlib.use("Agg")            # headless — no display needed
import matplotlib.pyplot as plt
import matplotlib.dates as mdates

# ============================ CONFIG ============================
SHEET_ID   = "11bLXCx34tZWXnYe-mk9Jl52WSTpmxNaIX0cW2fmCqa8"   # from sheets-logger.gs
SHEET_NAME = "log"
OUTPUT_DIR = Path.home() / "Desktop/Work/EVRFRSH/snapshots"
TZ         = ZoneInfo("America/Los_Angeles")                  # matches sheets-logger TZ
WINDOWS    = {"1h": 1, "3h": 3, "6h": 6, "12h": 12, "24h": 24}   # label -> hours back

# The snapshot represents this FIXED daily moment, not the time the script happens to
# run. So a launchd job delayed by sleep (e.g. the Mac woken at 9am) still renders the
# previous day's 9pm view correctly. Override per-run with --asof now / --asof <ISO>.
TARGET_HOUR   = 21    # 9 PM
TARGET_MINUTE = 0
MAX_BACKFILL_DAYS = 30   # safety cap on how far back a single run will fill

COL_TEMP = "#4285F4"   # blue   (left axis)
COL_RH   = "#EA4335"   # red    (left axis)
COL_VPD  = "#FBBC04"   # yellow (right axis)
COL_DP   = "#34A853"   # green  (left axis — dew points, °F; gap between the two = dpGap)
COL_GAP  = "#9334E6"   # purple (3rd axis — dpGap °F, the floor-evaporation tachometer)
# ===============================================================

CSV_URL = (f"https://docs.google.com/spreadsheets/d/{SHEET_ID}"
           f"/gviz/tq?tqx=out:csv&sheet={SHEET_NAME}")


def fetch_rows():
    """Download the sheet as CSV and return a list of dict rows."""
    req = urllib.request.Request(CSV_URL, headers={"User-Agent": "everfresh-snap"})
    with urllib.request.urlopen(req, timeout=30) as r:
        raw = r.read().decode("utf-8", "replace")
    if raw.lstrip().startswith("<"):
        sys.exit("ERROR: got HTML, not CSV — the sheet isn't publicly readable.\n"
                 "Fix: Share -> 'Anyone with the link -> Viewer' (or Publish to web).")
    return list(csv.DictReader(io.StringIO(raw)))


def _get(row, *names):
    """First non-empty value among candidate column names (schema-tolerant)."""
    for n in names:
        v = row.get(n)
        if v is not None and v.strip():
            return v.strip()
    return ""


def _num(row, *names):
    """Float of the first present candidate column, or NaN if none parse."""
    try:
        return float(_get(row, *names))
    except ValueError:
        return float("nan")


def parse_points(rows):
    """Keep telemetry rows (numeric canopy temp + RH); return one tuple per row:
    (dt, temp, rh, vpd, atemp, arh, avpd, cdp, adp, dpgap) — canopy first, then
    ambient (the room sensor added later; NaN on older rows that predate it), then
    the dew points and inside-outside dew-point gap (dpGap = canopyDP - ambientDP).
    Column names are matched leniently so this works against both the current
    sheets-logger.gs schema (vpd_kpa) and the older deployed sheet (VPD)."""
    pts = []
    for row in rows:
        # Canopy is the control point. Names listed newest-first so this works
        # across the rename (canopy* now, bare tempF/rh/VPD on older rows).
        t = _get(row, "canopyTempF", "tempF")
        h = _get(row, "canopyRH", "rh")
        if not t or not h:
            continue
        try:
            tempF, rh = float(t), float(h)
        except ValueError:
            continue
        vpd = _num(row, "canopyVPD", "VPD", "vpd_kpa", "vpd")
        try:
            dt = datetime.fromisoformat(_get(row, "published_at").replace("Z", "+00:00")).astimezone(TZ)
        except ValueError:
            continue
        # Ambient (room) sensor — NaN before it was installed; matplotlib gaps NaNs.
        atemp = _num(row, "ambientTempF", "atempF", "at")
        arh   = _num(row, "ambientRH", "arh")
        avpd  = _num(row, "ambientVPD", "avpd_kpa", "avpd")
        # Dew points (°F) and the gap that drives the regime / vent authority. Same
        # NaN-on-old-rows behavior; dpGap is the floor-water evaporation tachometer.
        cdp   = _num(row, "canopyDP")
        adp   = _num(row, "ambientDP")
        dpgap = _num(row, "dpGap")
        pts.append((dt, tempF, rh, vpd, atemp, arh, avpd, cdp, adp, dpgap))
    pts.sort(key=lambda p: p[0])
    return pts


def plot_window(pts, label, hours, out_dir, anchor):
    """Render one time window. Window is anchored to the latest data point so the
    chart always has data even if the device dropped offline before the run."""
    cutoff = anchor - timedelta(hours=hours)
    w = [p for p in pts if cutoff <= p[0] <= anchor]   # window ENDS at the anchor moment
    if not w:
        print(f"  {label}: no data in window, skipping")
        return
    times = [p[0] for p in w]

    fig, ax = plt.subplots(figsize=(12, 4.5))
    # Canopy = solid (the control point); ambient/room = dashed, same colors so each
    # metric pairs visually. Ambient lines gap automatically where the value is NaN
    # (rows logged before the room sensor was installed).
    ax.plot(times, [p[1] for p in w], color=COL_TEMP, linewidth=1.5, label="Canopy T °F")
    ax.plot(times, [p[2] for p in w], color=COL_RH,   linewidth=1.5, label="Canopy RH %")
    ax.plot(times, [p[4] for p in w], color=COL_TEMP, linewidth=1.2, linestyle="--",
            alpha=0.7, label="Ambient T °F")
    ax.plot(times, [p[5] for p in w], color=COL_RH,   linewidth=1.2, linestyle="--",
            alpha=0.7, label="Ambient RH %")
    # Dew points (°F) live on the same left axis as temp — the vertical distance
    # between the two green lines IS dpGap (canopyDP above ambientDP when canopy is
    # wetter / the floor reservoir is evaporating).
    ax.plot(times, [p[7] for p in w], color=COL_DP, linewidth=1.3, label="Canopy DP °F")
    ax.plot(times, [p[8] for p in w], color=COL_DP, linewidth=1.1, linestyle="--",
            alpha=0.7, label="Ambient DP °F")
    ax.set_ylim(40, 100)
    ax.set_ylabel("°F  /  %RH")

    ax2 = ax.twinx()
    ax2.plot(times, [p[3] for p in w], color=COL_VPD, linewidth=1.5, label="Canopy VPD kPa")
    ax2.plot(times, [p[6] for p in w], color=COL_VPD, linewidth=1.2, linestyle="--",
             alpha=0.7, label="Ambient VPD kPa")
    ax2.set_ylim(0, 2.5)
    ax2.set_ylabel("VPD kPa")

    # Third axis (offset to the right): dpGap itself, the floor-evaporation
    # tachometer. Fixed scale so the magnitude is comparable across snapshots; the
    # faint line at 0 is the regime flip (wet above / dry below).
    ax3 = ax.twinx()
    ax3.spines["right"].set_position(("axes", 1.07))
    ax3.plot(times, [p[9] for p in w], color=COL_GAP, linewidth=1.6, label="dpGap °F")
    ax3.axhline(0, color=COL_GAP, linewidth=0.7, alpha=0.25)
    ax3.set_ylim(-2, 14)
    ax3.set_ylabel("dpGap °F", color=COL_GAP)
    ax3.tick_params(axis="y", colors=COL_GAP)

    ax.xaxis.set_major_formatter(mdates.DateFormatter("%-I:%M %p", tz=TZ))
    fig.autofmt_xdate()
    ax.set_title(f"EVERFRESH — last {label}   (as of {anchor.strftime('%-m/%d %-I:%M %p')})")
    ax.grid(True, alpha=0.3)

    # Combined legend across all three axes.
    lines = ax.get_lines() + ax2.get_lines() + ax3.get_lines()
    ax.legend(lines, [l.get_label() for l in lines], loc="upper center", ncol=3,
              frameon=False, fontsize=8)

    # Explicit margins instead of tight_layout: the offset 3rd-axis spine isn't
    # tight_layout-compatible (warns + can clip). Leave room on the right for it.
    fig.subplots_adjust(left=0.06, right=0.89, top=0.88, bottom=0.18)
    path = out_dir / f"{label}.png"
    fig.savefig(path, dpi=130)
    plt.close(fig)
    print(f"  {label}: {len(w)} pts -> {path}")


def resolve_anchor(pts, asof):
    """The moment the snapshot represents (windows END here).
      default        -> the most recent TARGET_HOUR:TARGET_MINUTE at or before now,
                        so a late/asleep run still renders the intended daily moment.
      now / latest   -> the latest data point (ad-hoc "show me current").
      <ISO datetime> -> that exact moment (regenerate any past day's view)."""
    if asof and asof.lower() in ("now", "latest"):
        return pts[-1][0]
    if asof:
        dt = datetime.fromisoformat(asof)
        return dt.replace(tzinfo=TZ) if dt.tzinfo is None else dt.astimezone(TZ)
    now = datetime.now(TZ)
    target = now.replace(hour=TARGET_HOUR, minute=TARGET_MINUTE, second=0, microsecond=0)
    if target > now:                          # before today's target -> use yesterday's
        target -= timedelta(days=1)
    return target


def folder_complete(anchor):
    """True if this date's folder already has every window rendered."""
    d = OUTPUT_DIR / anchor.strftime("%Y-%m-%d")
    return all((d / f"{label}.png").exists() for label in WINDOWS)


def has_data(pts, anchor):
    """True if any data falls inside the largest window ending at this anchor."""
    cutoff = anchor - timedelta(hours=max(WINDOWS.values()))
    return any(cutoff <= p[0] <= anchor for p in pts)


def render_target(pts, anchor):
    out_dir = OUTPUT_DIR / anchor.strftime("%Y-%m-%d")   # folder = the moment's date
    out_dir.mkdir(parents=True, exist_ok=True)
    print(f"[{anchor.strftime('%Y-%m-%d %-I:%M %p')}] -> {out_dir}")
    for label, hours in WINDOWS.items():
        plot_window(pts, label, hours, out_dir, anchor)


def main():
    parser = argparse.ArgumentParser(description="Render EVERFRESH chart snapshots.")
    parser.add_argument("--asof", default=None,
                        help="'now'/'latest' for current data, or an ISO datetime "
                             "(e.g. 2026-06-20T21:00) to render one specific moment. "
                             "Default: backfill every missing daily 9pm up to now.")
    parser.add_argument("--force", action="store_true",
                        help="re-render days whose folder already exists.")
    args = parser.parse_args()

    pts = parse_points(fetch_rows())
    if not pts:
        sys.exit("ERROR: no valid telemetry rows found in the sheet.")

    # --asof: render exactly one moment, no backfill.
    if args.asof:
        render_target(pts, resolve_anchor(pts, args.asof))
        return

    # Default: fill every missing daily 9pm snapshot from the most recent one back
    # through any days the machine missed (asleep), then render oldest-first. Stops at
    # the first already-rendered day, when data runs out, or at MAX_BACKFILL_DAYS.
    t0 = resolve_anchor(pts, None)                 # most recent 9pm <= now
    earliest = pts[0][0]
    targets = []
    for n in range(MAX_BACKFILL_DAYS):
        d = (t0 - timedelta(days=n)).date()
        target = datetime(d.year, d.month, d.day, TARGET_HOUR, TARGET_MINUTE, tzinfo=TZ)
        if target < earliest:                      # walked past where data begins
            break
        if folder_complete(target) and not args.force:
            break                                  # reached already-rendered history
        if has_data(pts, target):
            targets.append(target)

    if not targets:
        print("Up to date — nothing to render.")
        return
    print(f"Rendering {len(targets)} day(s): "
          f"{targets[-1].strftime('%-m/%d')} .. {targets[0].strftime('%-m/%d')}")
    for anchor in reversed(targets):               # oldest first
        render_target(pts, anchor)


if __name__ == "__main__":
    main()
