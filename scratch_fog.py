#!/usr/bin/env python3
"""EVERFRESH — vapor-load observer experiment (offline, read-only).

Reconstructs the HIDDEN pool state from data already in the sheet:

    pool_charge(t) ~=  integral(fog_on dt)        # charge: water injected
                       - k * integral(max(dpGap,0) dt)   # discharge: floor evaporation

Charge comes two independent ways (cross-checked here):
  1. EVENT rows  — change=="fog" on/off transitions -> exact runtime.
  2. TELEM rows  — fog 0/1 sampled ~60s -> Riemann sum (sanity check vs #1).

Discharge uses dpGap (canopyDP-ambientDP), the "floor-water evaporation
tachometer" already logged. k is unknown — tune --k and see if the resulting
pool curve tracks the RH inertia you observe. This script DECIDES NOTHING in the
controller; it's the validation step before any firmware change.

Usage:  python3 scratch_fog.py [--k 6.0] [--plot]
"""

import argparse, csv, io, urllib.request
from datetime import datetime, timedelta, date
from zoneinfo import ZoneInfo

SHEET_ID = "11bLXCx34tZWXnYe-mk9Jl52WSTpmxNaIX0cW2fmCqa8"
TZ  = ZoneInfo("America/Los_Angeles")
URL = f"https://docs.google.com/spreadsheets/d/{SHEET_ID}/gviz/tq?tqx=out:csv&sheet=log"

MAX_SAMPLE_GAP_S = 180   # cap dt between telem samples so an offline gap can't blow up the integral


def g(r, *names):
    for n in names:
        v = r.get(n)
        if v and v.strip():
            return v.strip()
    return ""


def fnum(r, *names):
    try:
        return float(g(r, *names))
    except ValueError:
        return float("nan")


def parse_dt(s):
    try:
        return datetime.fromisoformat(s.replace("Z", "+00:00")).astimezone(TZ)
    except ValueError:
        return None


def fetch():
    req = urllib.request.Request(URL, headers={"User-Agent": "everfresh-snap"})
    raw = urllib.request.urlopen(req, timeout=60).read().decode("utf-8", "replace")
    if raw.lstrip().startswith("<"):
        raise SystemExit("ERROR: got HTML not CSV — sheet isn't publicly readable.")
    return list(csv.DictReader(io.StringIO(raw)))


def clip_seconds_per_day(t0, t1):
    """Split an on-interval [t0,t1] into {local_date: seconds}, clipping at midnight."""
    out = {}
    cur = t0
    while cur < t1:
        midnight_next = datetime(cur.year, cur.month, cur.day, tzinfo=TZ) + timedelta(days=1)
        seg_end = min(t1, midnight_next)
        out[cur.date()] = out.get(cur.date(), 0.0) + (seg_end - cur).total_seconds()
        cur = seg_end
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--k", type=float, default=6.0,
                    help="discharge weight: fog-minutes drained per (degF*hour) of positive dpGap")
    ap.add_argument("--plot", action="store_true", help="render PNG to the scratchpad")
    args = ap.parse_args()

    rows = fetch()

    # --- split the unified log into event toggles and telemetry samples ---
    fog_events = []   # (dt, "on"/"off")  from change=="fog" rows
    telem = []        # (dt, fog01, dpgap) from telemetry rows
    for r in rows:
        dt = parse_dt(g(r, "published_at"))
        if dt is None:
            continue
        change = g(r, "change", "ev")          # actuator name on event rows
        state  = g(r, "state")
        if change == "fog" and state in ("on", "off"):
            fog_events.append((dt, state))
        fog01 = g(r, "fog")                     # telemetry rows carry 0/1
        if fog01 in ("0", "1"):
            telem.append((dt, int(fog01), fnum(r, "dpGap"), fnum(r, "canopyRH", "rh")))
    fog_events.sort(key=lambda x: x[0])
    telem.sort(key=lambda x: x[0])

    if not telem:
        raise SystemExit("ERROR: no telemetry rows parsed.")
    data_end = telem[-1][0]

    # --- charge #1: exact fog-on intervals from the event stream ---
    intervals = []
    on_start = None
    for dt, st in fog_events:
        if st == "on" and on_start is None:
            on_start = dt
        elif st == "off" and on_start is not None:
            intervals.append((on_start, dt)); on_start = None
    dangling = on_start is not None
    if dangling:                                # still ON at end of data — close at last sample
        intervals.append((on_start, data_end))

    charge_evt = {}   # date -> fog minutes (exact)
    for t0, t1 in intervals:
        for d, sec in clip_seconds_per_day(t0, t1).items():
            charge_evt[d] = charge_evt.get(d, 0.0) + sec / 60.0

    # --- charge #2 + discharge: Riemann sums over 60s telemetry ---
    charge_tel = {}   # date -> fog minutes (sampled)
    disch = {}        # date -> degF*hours of positive dpGap
    for i in range(len(telem) - 1):
        dt, fog01, dpgap, _rh = telem[i]
        step = min((telem[i + 1][0] - dt).total_seconds(), MAX_SAMPLE_GAP_S)
        if step <= 0:
            continue
        d = dt.date()
        if fog01:
            charge_tel[d] = charge_tel.get(d, 0.0) + step / 60.0
        if dpgap == dpgap and dpgap > 0:        # NaN-safe
            disch[d] = disch.get(d, 0.0) + dpgap * step / 3600.0

    # Per-day canopy RH floor (10th pct) and mean — the RH-inertia signal to test
    # the pool against. A fuller pool should hold a higher floor between fog cycles.
    rh_by_day = {}
    for dt, _f, _dp, r in telem:
        if r == r:
            rh_by_day.setdefault(dt.date(), []).append(r)

    def pct(xs, q):
        s = sorted(xs); i = max(0, min(len(s) - 1, int(q * len(s))))
        return s[i]

    days = sorted(set(charge_evt) | set(charge_tel) | set(disch))

    print(f"rows={len(rows)}  telem={len(telem)}  fog_events={len(fog_events)}  "
          f"intervals={len(intervals)}{'  (last still OPEN)' if dangling else ''}")
    print(f"data through {data_end.strftime('%-m/%d %-I:%M%p')}\n")
    print(f"{'day':>10} {'fog_min(evt)':>13} {'fog_min(tel)':>13} "
          f"{'dpGap_degF·h':>13} {'pool(cum)':>10} {'RH_floor':>9} {'RH_mean':>8}")
    cum = 0.0
    series = []   # (pool_level, rh_floor) on days with a real discharge signal
    for d in days:
        ce = charge_evt.get(d, 0.0)
        ct = charge_tel.get(d, 0.0)
        di = disch.get(d, 0.0)
        cum += ce - args.k * di
        rhs = rh_by_day.get(d, [])
        floor = pct(rhs, 0.10) if rhs else float("nan")
        mean  = sum(rhs) / len(rhs) if rhs else float("nan")
        print(f"{d.isoformat():>10} {ce:>13.1f} {ct:>13.1f} {di:>13.2f} "
              f"{cum:>+10.1f} {floor:>9.1f} {mean:>8.1f}")
        if di > 0 and rhs:                  # only days the observer is actually defined
            series.append((cum, floor))

    # GO/NO-GO: does pool level predict the RH floor? Pearson r over valid days.
    if len(series) >= 3:
        xs = [a for a, _ in series]; ys = [b for _, b in series]
        mx, my = sum(xs) / len(xs), sum(ys) / len(ys)
        cov = sum((a - mx) * (b - my) for a, b in series)
        vx = sum((a - mx) ** 2 for a in xs); vy = sum((b - my) ** 2 for b in ys)
        r = cov / (vx * vy) ** 0.5 if vx and vy else float("nan")
        print(f"\nGO/NO-GO  pool-vs-RH_floor  r = {r:+.2f}  over {len(series)} days "
              f"(dpGap-valid, k={args.k})")
        print("  r strongly positive -> fuller pool holds a higher RH floor -> observer is real.")
        print("  r near 0 / negative -> pool level doesn't explain RH inertia at this k.")
    else:
        print(f"\nNot enough dpGap-valid days yet for a correlation (have {len(series)}).")
    print("evt vs tel fog-minutes should track closely — big gaps = dropped events / offline.")

    if args.plot:
        make_plot(telem, intervals, args.k)


def make_plot(telem, intervals, k):
    """Continuous pool estimate over the whole record."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import matplotlib.dates as mdates

    # Build cumulative charge (minutes) and discharge (degF*h) at each telem timestamp.
    def fog_on_at(t):
        return any(a <= t <= b for a, b in intervals)

    times, cum_charge, cum_disch, pool, rh = [], [], [], [], []
    cc = cd = 0.0
    for i in range(len(telem) - 1):
        t, _f, dp, r = telem[i]
        step = min((telem[i + 1][0] - t).total_seconds(), MAX_SAMPLE_GAP_S)
        if step <= 0:
            continue
        if fog_on_at(t):
            cc += step / 60.0
        if dp == dp and dp > 0:
            cd += dp * step / 3600.0
        times.append(t); cum_charge.append(cc); cum_disch.append(cd)
        pool.append(cc - k * cd); rh.append(r)

    fig, ax = plt.subplots(figsize=(13, 5))
    ax.plot(times, cum_charge, color="#4285F4", lw=1.4, label="cum fog-min (charge)")
    ax.plot(times, [k * x for x in cum_disch], color="#EA4335", lw=1.4,
            label=f"k·cum dpGap (discharge, k={k})")
    ax.plot(times, pool, color="#34A853", lw=2.0, label="pool estimate = charge − discharge")
    ax.axhline(0, color="k", lw=0.5, alpha=0.3)
    ax.set_ylabel("fog-minute units")
    ax.xaxis.set_major_formatter(mdates.DateFormatter("%-m/%d", tz=TZ))

    # GO/NO-GO overlay: canopy RH on its own axis. Hypothesis — a fuller pool (green
    # rising) should hold a HIGHER RH floor / decay slower between fog cycles. If the
    # green pool and the RH envelope move together, the observer is real.
    ax2 = ax.twinx()
    ax2.plot(times, rh, color="#EA4335", lw=0.8, alpha=0.45, label="canopy RH %")
    ax2.set_ylabel("canopy RH %", color="#EA4335")
    ax2.tick_params(axis="y", colors="#EA4335")
    ax2.set_ylim(40, 100)

    lines = ax.get_lines() + ax2.get_lines()
    ax.legend(lines, [l.get_label() for l in lines], loc="upper left", frameon=False, fontsize=9)
    ax.set_title("EVERFRESH vapor-load observer (offline) — does the green pool track RH inertia?")
    ax.grid(True, alpha=0.3)
    fig.autofmt_xdate()
    fig.tight_layout()
    out = ("/private/tmp/claude-501/-Users-arabaghdassarian-Desktop-Work-EVRFRSH/"
           "732b2d01-5472-40c0-81d3-00ac6eaba5aa/scratchpad/fog_observer.png")
    fig.savefig(out, dpi=130)
    print(f"\nplot -> {out}")


if __name__ == "__main__":
    main()
