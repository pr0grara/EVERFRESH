#!/usr/bin/env python3
"""EVERFRESH — 48-hour snapshot. Pulls the 'log' sheet as public CSV, prints a
text summary (stats + actuator duty + per-day solar peaks) and renders one 48h
chart. Standalone; reuses the fetch/column conventions of sheet_snapshots.py."""

import csv, io, os, sys, urllib.request
from datetime import datetime, timedelta
from zoneinfo import ZoneInfo
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.dates as mdates

SHEET_ID = "11bLXCx34tZWXnYe-mk9Jl52WSTpmxNaIX0cW2fmCqa8"
TZ = ZoneInfo("America/Los_Angeles")
CSV_URL = (f"https://docs.google.com/spreadsheets/d/{SHEET_ID}"
           f"/gviz/tq?tqx=out:csv&sheet=log")
HOURS = 48

def _g(row, *names):
    for n in names:
        v = row.get(n)
        if v is not None and v.strip():
            return v.strip()
    return ""
def _n(row, *names):
    try: return float(_g(row, *names))
    except ValueError: return float("nan")

req = urllib.request.Request(CSV_URL, headers={"User-Agent": "everfresh-snap"})
with urllib.request.urlopen(req, timeout=30) as r:
    raw = r.read().decode("utf-8", "replace")
if raw.lstrip().startswith("<"):
    sys.exit("ERROR: got HTML not CSV — sheet not publicly readable.")
rows = list(csv.DictReader(io.StringIO(raw)))

pts = []
for row in rows:
    t, h = _g(row, "canopyTempF", "tempF"), _g(row, "canopyRH", "rh")
    if not t or not h: continue
    try: tempF, rh = float(t), float(h)
    except ValueError: continue
    try:
        dt = datetime.fromisoformat(_g(row, "published_at").replace("Z", "+00:00")).astimezone(TZ)
    except ValueError: continue
    pts.append(dict(
        dt=dt, ct=tempF, crh=rh, cvpd=_n(row,"canopyVPD","VPD","vpd"),
        at=_n(row,"ambientTempF","at"), arh=_n(row,"ambientRH"), avpd=_n(row,"ambientVPD","avpd"),
        dpgap=_n(row,"dpGap"),
        heat=_n(row,"heat"), fog=_n(row,"fog"), circ=_n(row,"circ"), vent=_n(row,"vent"),
        phase=_g(row,"phase"), regime=_g(row,"regime"), fw=_g(row,"fw"),
    ))
pts.sort(key=lambda p: p["dt"])
if not pts: sys.exit("no data")

anchor = pts[-1]["dt"]
cutoff = anchor - timedelta(hours=HOURS)
w = [p for p in pts if cutoff <= p["dt"] <= anchor]

def stat(key):
    vals = [p[key] for p in w if p[key] == p[key]]
    if not vals: return None
    return min(vals), sum(vals)/len(vals), max(vals)
def duty(key):
    vals = [p[key] for p in w if p[key] == p[key]]
    if not vals: return None
    return 100.0*sum(1 for v in vals if v >= 0.5)/len(vals)

last = w[-1]
span_h = (w[-1]["dt"] - w[0]["dt"]).total_seconds()/3600
# gap detection: consecutive points > 5 min apart
gaps = []
for a,b in zip(w, w[1:]):
    d = (b["dt"]-a["dt"]).total_seconds()/60
    if d > 5: gaps.append((a["dt"], b["dt"], d))

print(f"=== EVERFRESH 48h snapshot ===")
print(f"window: {w[0]['dt']:%-m/%d %-I:%M%p} -> {anchor:%-m/%d %-I:%M%p}  ({span_h:.1f}h span, {len(w)} pts)")
print(f"firmware: {last['fw']}")
print(f"\nLATEST ({last['dt']:%-m/%d %-I:%M%p}): canopy {last['ct']:.1f}F / {last['crh']:.0f}% / VPD {last['cvpd']:.2f}"
      f" | ambient {last['at']:.1f}F / {last['arh']:.0f}% / VPD {last['avpd']:.2f}"
      f" | dpGap {last['dpgap']:.1f} | phase {last['phase']} | regime {last['regime']}")

for label,key in [("canopy T F","ct"),("canopy RH %","crh"),("canopy VPD","cvpd"),
                  ("ambient T F","at"),("ambient RH %","arh"),("ambient VPD","avpd"),
                  ("dpGap F","dpgap")]:
    s = stat(key)
    if s: print(f"  {label:14s} min {s[0]:6.2f}  mean {s[1]:6.2f}  max {s[2]:6.2f}")

print("\nActuator duty (% of samples ON):")
for label,key in [("heat","heat"),("fog","fog"),("circ","circ"),("vent","vent")]:
    d = duty(key)
    if d is not None: print(f"  {label:5s} {d:5.1f}%")

# per-day solar window peaks (4-8 PM)
print("\nSolar-window peaks (4-8 PM):")
days = sorted({p["dt"].date() for p in w})
for day in days:
    sw = [p for p in w if p["dt"].date()==day and 16 <= p["dt"].hour < 20]
    if not sw: continue
    tp = max(sw, key=lambda p:p["ct"])
    vp = max((p for p in sw if p["cvpd"]==p["cvpd"]), key=lambda p:p["cvpd"], default=None)
    line = f"  {day:%-m/%d}: peak canopy {tp['ct']:.1f}F @ {tp['dt']:%-I:%M%p}"
    if vp: line += f" | peak VPD {vp['cvpd']:.2f} @ {vp['dt']:%-I:%M%p}"
    print(line)

if gaps:
    print(f"\nData gaps (>5min): {len(gaps)}")
    for a,b,d in gaps[:6]:
        print(f"  {a:%-m/%d %-I:%M%p} -> {b:%-I:%M%p}  ({d:.0f}min)")

# ---- chart ----
times=[p["dt"] for p in w]
fig, ax = plt.subplots(figsize=(13,5))
ax.plot(times,[p["ct"] for p in w],color="#4285F4",lw=1.4,label="Canopy T F")
ax.plot(times,[p["crh"] for p in w],color="#EA4335",lw=1.4,label="Canopy RH %")
ax.plot(times,[p["at"] for p in w],color="#4285F4",lw=1.0,ls="--",alpha=.6,label="Ambient T F")
ax.plot(times,[p["arh"] for p in w],color="#EA4335",lw=1.0,ls="--",alpha=.6,label="Ambient RH %")
ax.set_ylim(40,100); ax.set_ylabel("F / %RH")
ax2=ax.twinx()
ax2.plot(times,[p["cvpd"] for p in w],color="#FBBC04",lw=1.6,label="Canopy VPD kPa")
ax2.set_ylim(0,2.5); ax2.set_ylabel("VPD kPa")
# shade solar windows
for day in days:
    s=datetime(day.year,day.month,day.day,16,tzinfo=TZ); e=datetime(day.year,day.month,day.day,20,tzinfo=TZ)
    ax.axvspan(max(s,times[0]),min(e,times[-1]),color="#FBBC04",alpha=.07)
ax.xaxis.set_major_formatter(mdates.DateFormatter("%-m/%d %-I%p",tz=TZ))
fig.autofmt_xdate()
ax.set_title(f"EVERFRESH — last 48h  (as of {anchor:%-m/%d %-I:%M%p})  fw {last['fw']}")
l1,lb1=ax.get_legend_handles_labels(); l2,lb2=ax2.get_legend_handles_labels()
ax.legend(l1+l2,lb1+lb2,loc="upper left",fontsize=8,ncol=3)
fig.tight_layout()
out=os.path.join(os.environ.get("CODE_ATTACH_OUTBOX","."),"everfresh_48h.png")
fig.savefig(out,dpi=120); print(f"\nchart -> {out}")
