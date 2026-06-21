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


def parse_points(rows):
    """Keep telemetry rows (numeric temp + RH); return [(dt, temp, rh, vpd), ...].
    Column names are matched leniently so this works against both the current
    sheets-logger.gs schema (vpd_kpa) and the older deployed sheet (VPD)."""
    pts = []
    for row in rows:
        t = _get(row, "tempF")
        h = _get(row, "rh")
        if not t or not h:
            continue
        try:
            tempF, rh = float(t), float(h)
        except ValueError:
            continue
        try:
            vpd = float(_get(row, "VPD", "vpd_kpa", "vpd"))
        except ValueError:
            vpd = float("nan")
        try:
            dt = datetime.fromisoformat(_get(row, "published_at").replace("Z", "+00:00")).astimezone(TZ)
        except ValueError:
            continue
        pts.append((dt, tempF, rh, vpd))
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
    ax.plot(times, [p[1] for p in w], color=COL_TEMP, linewidth=1.5, label="Temp °F")
    ax.plot(times, [p[2] for p in w], color=COL_RH,   linewidth=1.5, label="RH %")
    ax.set_ylim(40, 100)
    ax.set_ylabel("°F  /  %RH")

    ax2 = ax.twinx()
    ax2.plot(times, [p[3] for p in w], color=COL_VPD, linewidth=1.5, label="VPD kPa")
    ax2.set_ylim(0, 2.5)
    ax2.set_ylabel("VPD kPa")

    ax.xaxis.set_major_formatter(mdates.DateFormatter("%-I:%M %p", tz=TZ))
    fig.autofmt_xdate()
    ax.set_title(f"EVERFRESH — last {label}   (as of {anchor.strftime('%-m/%d %-I:%M %p')})")
    ax.grid(True, alpha=0.3)

    # Combined legend across both axes.
    lines = ax.get_lines() + ax2.get_lines()
    ax.legend(lines, [l.get_label() for l in lines], loc="upper center", ncol=3, frameon=False)

    fig.tight_layout()
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
