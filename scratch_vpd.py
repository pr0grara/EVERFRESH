import csv, io, urllib.request
from datetime import datetime, timedelta
from zoneinfo import ZoneInfo
SHEET_ID="11bLXCx34tZWXnYe-mk9Jl52WSTpmxNaIX0cW2fmCqa8"
TZ=ZoneInfo("America/Los_Angeles")
URL=f"https://docs.google.com/spreadsheets/d/{SHEET_ID}/gviz/tq?tqx=out:csv&sheet=log"
req=urllib.request.Request(URL,headers={"User-Agent":"everfresh-snap"})
raw=urllib.request.urlopen(req,timeout=60).read().decode("utf-8","replace")
rows=list(csv.DictReader(io.StringIO(raw)))
def g(r,*n):
    for x in n:
        v=r.get(x)
        if v and v.strip(): return v.strip()
    return ""
def num(r,*n):
    try: return float(g(r,*n))
    except: return float("nan")
pts=[]
for r in rows:
    t=g(r,"canopyTempF","tempF"); h=g(r,"canopyRH","rh")
    if not t or not h: continue
    try:
        dt=datetime.fromisoformat(g(r,"published_at").replace("Z","+00:00")).astimezone(TZ)
    except: continue
    vpd=num(r,"canopyVPD","VPD","vpd_kpa","vpd")
    dpgap=num(r,"dpGap")
    pts.append((dt,float(t),float(h),vpd,dpgap,g(r,"status")))
pts.sort(key=lambda p:p[0])
print("total rows:",len(pts),"range",pts[0][0],"->",pts[-1][0])
print()
# Night window: 10pm previous day -> 7am. Label by the morning date.
def night_stats(morning_date):
    start=datetime(morning_date.year,morning_date.month,morning_date.day,0,0,tzinfo=TZ)-timedelta(hours=2) # 10pm prev
    end=datetime(morning_date.year,morning_date.month,morning_date.day,7,0,tzinfo=TZ)
    w=[p for p in pts if start<=p[0]<=end and p[3]==p[3]]
    if not w: return None
    vs=[p[3] for p in w]
    n=len(vs)
    dt_min=(w[-1][0]-w[0][0]).total_seconds()/3600
    frac_lo=sum(1 for v in vs if v<0.4)/n
    return dict(n=n,span_h=round(dt_min,1),mean=round(sum(vs)/n,3),mn=round(min(vs),3),mx=round(max(vs),3),
                h_below_04=round(frac_lo*dt_min,1),h_below_055=round(sum(1 for v in vs if v<0.55)/n*dt_min,1))
from datetime import date
for d in [date(2026,6,24),date(2026,6,25),date(2026,6,26),date(2026,6,27),date(2026,6,28)]:
    s=night_stats(d)
    print(f"night->{d} (10pm-7am):", s)
