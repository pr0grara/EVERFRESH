#!/usr/bin/env python3
import base64, pathlib

HERE = pathlib.Path(__file__).parent

def datauri(fn, mime="image/jpeg"):
    b = (HERE / fn).read_bytes()
    return f"data:{mime};base64," + base64.b64encode(b).decode()

HERO1 = datauri("hero1.jpg")
HERO2 = datauri("hero2.jpg")
ROOM  = datauri("room.jpg")

# ---------- SVG cutaway diagram ----------
CUTAWAY = r'''
<svg viewBox="0 0 560 840" xmlns="http://www.w3.org/2000/svg" class="cut">
  <defs>
    <linearGradient id="wood" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="#c07a41"/><stop offset="1" stop-color="#9c5c2c"/>
    </linearGradient>
    <linearGradient id="woodDark" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#8a4f24"/><stop offset="1" stop-color="#a5622f"/>
    </linearGradient>
    <linearGradient id="glass" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#eaf3ef" stop-opacity=".7"/>
      <stop offset="1" stop-color="#cfe3dc" stop-opacity=".45"/>
    </linearGradient>
    <radialGradient id="ledglow" cx="0.5" cy="0" r="0.9">
      <stop offset="0" stop-color="#fff4d0" stop-opacity=".95"/>
      <stop offset="1" stop-color="#fff4d0" stop-opacity="0"/>
    </radialGradient>
    <radialGradient id="heatglow" cx="0.5" cy="0.4" r="0.7">
      <stop offset="0" stop-color="#ffb257" stop-opacity=".9"/>
      <stop offset="1" stop-color="#ffb257" stop-opacity="0"/>
    </radialGradient>
  </defs>

  <!-- outer frame -->
  <rect x="150" y="52" width="260" height="720" rx="10" fill="url(#wood)" stroke="#6f4522" stroke-width="3"/>
  <!-- attic compartment -->
  <rect x="158" y="60" width="244" height="86" rx="5" fill="url(#woodDark)" stroke="#6f4522" stroke-width="2"/>
  <text x="280" y="108" text-anchor="middle" font-size="13" fill="#f4e7cf" font-family="Georgia" font-style="italic">sealed attic</text>
  <!-- chamber glass -->
  <rect x="168" y="152" width="224" height="512" rx="4" fill="url(#glass)" stroke="#7aa398" stroke-width="1.5"/>
  <!-- door seam (front) -->
  <line x1="280" y1="152" x2="280" y2="664" stroke="#7aa398" stroke-width="1" stroke-dasharray="4 4" opacity=".6"/>
  <!-- LED glow + bar -->
  <rect x="176" y="158" width="208" height="120" fill="url(#ledglow)"/>
  <rect x="196" y="160" width="168" height="12" rx="6" fill="#fdf3d4" stroke="#d9b968" stroke-width="1"/>
  <!-- circ fan -->
  <circle cx="280" cy="196" r="15" fill="none" stroke="#4a4a4a" stroke-width="2"/>
  <path d="M280 196 L288 188 M280 196 L272 188 M280 196 L288 204 M280 196 L272 204" stroke="#4a4a4a" stroke-width="2"/>
  <!-- down airflow arrows -->
  <g stroke="#3f7d8c" stroke-width="2" fill="none" opacity=".8">
    <path d="M232 220 L232 250"/><path d="M228 244 L232 252 L236 244"/>
    <path d="M328 220 L328 250"/><path d="M324 244 L328 252 L332 244"/>
  </g>
  <!-- heat bulb + glow, mid level -->
  <circle cx="214" cy="470" r="42" fill="url(#heatglow)"/>
  <path d="M204 452 h20 a4 4 0 0 1 4 4 v6 a14 14 0 0 1 -28 0 v-6 a4 4 0 0 1 4 -4 z" fill="#c9c9c9" stroke="#7a7a7a" stroke-width="1.5"/>
  <rect x="211" y="444" width="6" height="10" fill="#7a7a7a"/>
  <!-- tree -->
  <g stroke="#5b7d3a" stroke-width="2.5" fill="none">
    <path d="M300 612 C300 540 300 470 300 360"/>
    <path d="M300 400 C270 388 250 392 232 372"/>
    <path d="M300 430 C334 418 356 420 376 398"/>
    <path d="M300 470 C272 460 252 462 236 444"/>
    <path d="M300 500 C332 490 352 492 372 474"/>
    <path d="M300 360 C288 344 280 340 268 330"/>
    <path d="M300 360 C314 346 324 342 336 332"/>
  </g>
  <!-- fine leaflets hint -->
  <g stroke="#6f9a44" stroke-width="1" opacity=".7">
    <path d="M240 376 l-6 -5 M250 380 l-6 -5 M260 384 l-6 -5"/>
    <path d="M368 402 l6 -5 M358 406 l6 -5 M348 410 l6 -5"/>
  </g>
  <!-- pot -->
  <path d="M276 612 h48 l-6 44 h-36 z" fill="#b5623a" stroke="#8a4527" stroke-width="1.5"/>
  <ellipse cx="300" cy="612" rx="24" ry="5" fill="#8a4527"/>
  <!-- base pan -->
  <rect x="172" y="666" width="216" height="52" rx="3" fill="#7d5230" stroke="#5c3a1f" stroke-width="1.5"/>
  <!-- reservoir + water + fogger -->
  <rect x="326" y="682" width="50" height="28" rx="2" fill="#3f6f7a" opacity=".85"/>
  <path d="M326 690 q12 -5 25 0 t25 0" stroke="#bfe3ea" stroke-width="1.5" fill="none"/>
  <circle cx="351" cy="702" r="4" fill="#cfeaf0"/>
  <!-- mist -->
  <g fill="#dfeeea" opacity=".5">
    <circle cx="345" cy="672" r="5"/><circle cx="356" cy="666" r="4"/><circle cx="335" cy="666" r="3.5"/>
  </g>
  <!-- canopy sensor -->
  <rect x="336" y="452" width="16" height="11" rx="2" fill="#2f5233" stroke="#1f381f"/>
  <!-- passive intake port (left wall low) + outdoor sensor -->
  <rect x="150" y="628" width="18" height="26" rx="2" fill="#5c3a1f"/>
  <g stroke="#3f7d8c" stroke-width="2" fill="none" opacity=".8">
    <path d="M120 641 L166 641"/><path d="M158 636 L168 641 L158 646"/>
  </g>
  <rect x="96" y="632" width="18" height="13" rx="2" fill="#2f5233" stroke="#1f381f"/>
  <!-- exhaust out top -->
  <g stroke="#c0673c" stroke-width="2.5" fill="none">
    <path d="M280 60 L280 30"/><path d="M273 40 L280 28 L287 40"/>
  </g>
  <circle cx="280" cy="74" r="11" fill="none" stroke="#4a4a4a" stroke-width="2"/>
  <path d="M280 74 L286 68 M280 74 L274 68 M280 74 L286 80 M280 74 L274 80" stroke="#4a4a4a" stroke-width="1.6"/>
  <!-- casters -->
  <circle cx="192" cy="784" r="11" fill="#3a2f26"/><circle cx="192" cy="784" r="4" fill="#a9884e"/>
  <circle cx="368" cy="784" r="11" fill="#3a2f26"/><circle cx="368" cy="784" r="4" fill="#a9884e"/>
  <!-- door handle (brass) -->
  <rect x="384" y="392" width="6" height="34" rx="3" fill="#a9884e"/>

  <!-- ================= callout numbers (leaders point to real features) ================= -->
  <g font-family="Georgia" font-weight="bold">
    <!-- RIGHT column -->
    <g><line x1="300" y1="40"  x2="456" y2="60"  stroke="#c0673c" stroke-width="1"/><circle cx="470" cy="60"  r="14" fill="#c0673c"/><text x="470" y="65"  text-anchor="middle" fill="#fff" font-size="13">1</text></g>
    <g><line x1="364" y1="166" x2="456" y2="150" stroke="#2f5233" stroke-width="1"/><circle cx="470" cy="150" r="14" fill="#2f5233"/><text x="470" y="155" text-anchor="middle" fill="#fff" font-size="13">2</text></g>
    <g><line x1="296" y1="196" x2="456" y2="212" stroke="#2f5233" stroke-width="1"/><circle cx="470" cy="212" r="14" fill="#2f5233"/><text x="470" y="217" text-anchor="middle" fill="#fff" font-size="13">3</text></g>
    <g><line x1="330" y1="362" x2="456" y2="342" stroke="#c0673c" stroke-width="1"/><circle cx="470" cy="342" r="14" fill="#c0673c"/><text x="470" y="347" text-anchor="middle" fill="#fff" font-size="13">6</text></g>
    <g><line x1="352" y1="457" x2="456" y2="440" stroke="#2f5233" stroke-width="1"/><circle cx="470" cy="440" r="14" fill="#2f5233"/><text x="470" y="445" text-anchor="middle" fill="#fff" font-size="13">4</text></g>
    <g><line x1="368" y1="692" x2="456" y2="620" stroke="#2f5233" stroke-width="1"/><circle cx="470" cy="620" r="14" fill="#2f5233"/><text x="470" y="625" text-anchor="middle" fill="#fff" font-size="13">7</text></g>
    <g><line x1="388" y1="700" x2="456" y2="708" stroke="#2f5233" stroke-width="1"/><circle cx="470" cy="708" r="14" fill="#2f5233"/><text x="470" y="713" text-anchor="middle" fill="#fff" font-size="13">8</text></g>
    <!-- LEFT column -->
    <g><line x1="158" y1="100" x2="66"  y2="90"  stroke="#c0673c" stroke-width="1"/><circle cx="50" cy="90"  r="14" fill="#c0673c"/><text x="50" y="95"  text-anchor="middle" fill="#fff" font-size="12">10</text></g>
    <g><line x1="176" y1="470" x2="66"  y2="470" stroke="#2f5233" stroke-width="1"/><circle cx="50" cy="470" r="14" fill="#2f5233"/><text x="50" y="475" text-anchor="middle" fill="#fff" font-size="13">5</text></g>
    <g><line x1="96"  y1="638" x2="66"  y2="612" stroke="#3f7d8c" stroke-width="1"/><circle cx="50" cy="612" r="14" fill="#3f7d8c"/><text x="50" y="617" text-anchor="middle" fill="#fff" font-size="13">9</text></g>
  </g>
</svg>
'''

LEGEND = [
    ("1",  "#c0673c", "Active top exhaust", "PC fan in a port cut to the fan body (foam collar, no short-circuit) — the peak-shaver during the afternoon solar spike."),
    ("2",  "#2f5233", "Full-spectrum white LED", "Sleek dimmable panel under the top; driver hidden in the attic. Warm daylight glow, not blurple."),
    ("3",  "#2f5233", "Circulation fan", "Blows down to mix the gradient; gentle continuous trickle between cycles."),
    ("4",  "#2f5233", "Canopy sensor", "SHT31 at leaf height — the control point every decision uses."),
    ("5",  "#2f5233", "Reptile heat bulb", "Mid-level above the reservoir, radiating up. Your proven heat source."),
    ("6",  "#c0673c", "The tree", "Cojoba angustifolia — room to reach ~3 ft before the next rebuild."),
    ("7",  "#2f5233", "Fogger + reservoir", "Humidity source; refill through the front door."),
    ("8",  "#2f5233", "Waterproof base pan", "Sealed, sloped to a corner — catches fog-spit + condensation."),
    ("9",  "#3f7d8c", "Passive intake + outdoor sensor", "Low screened port; second SHT31 reads the air the exhaust pulls in."),
    ("10", "#c0673c", "Sealed attic", "All electronics — exhaust fan, LED driver, Photon — out of the humid zone."),
]

legend_html = "".join(
    f'<div class="leg"><span class="dot" style="background:{c}">{n}</span>'
    f'<div><b>{t}</b><p>{d}</p></div></div>'
    for n, c, t, d in LEGEND
)

HTML = f'''<!DOCTYPE html><html><head><meta charset="utf-8"><style>
@page {{ size: Letter; margin: 0; }}
* {{ box-sizing: border-box; -webkit-print-color-adjust: exact; print-color-adjust: exact; }}
html,body {{ margin:0; padding:0; font-family:"Avenir Next","Helvetica Neue",Arial,sans-serif; color:#3a332b; }}
.page {{ width:8.5in; height:11in; position:relative; overflow:hidden; page-break-after:always; background:#f2e9d8; }}
.page:last-child {{ page-break-after:auto; }}
h1,h2,h3 {{ font-family:Georgia,"Times New Roman",serif; margin:0; }}
.eyebrow {{ font-size:11px; letter-spacing:.32em; text-transform:uppercase; color:#c0673c; font-weight:700; }}
.green {{ color:#2f5233; }} .terra {{ color:#c0673c; }} .brass{{color:#a9884e;}}
p {{ line-height:1.5; }}

/* COVER */
.cover {{ padding:0; display:flex; flex-direction:column; }}
.cover .top {{ padding:.7in .8in .3in; text-align:center; }}
.cover h1 {{ font-size:78px; color:#2f5233; line-height:.95; margin:.06in 0; letter-spacing:-1px; }}
.cover .sub {{ font-size:13px; letter-spacing:.34em; text-transform:uppercase; color:#c0673c; }}
.cover .heroWrap {{ flex:1; display:flex; align-items:center; justify-content:center; background:radial-gradient(ellipse at 50% 42%, #fbf4e6 0%, #eadfca 70%); }}
.cover .heroWrap img {{ height:6.0in; border-radius:6px; box-shadow:0 18px 50px rgba(90,60,20,.28); }}
.cover .foot {{ padding:.28in .8in .5in; text-align:center; }}
.cover .foot .tag {{ font-family:Georgia,serif; font-style:italic; font-size:15px; color:#5a5040; max-width:5in; margin:0 auto .16in; line-height:1.5; }}
.cover .foot .spec {{ font-size:10.5px; letter-spacing:.22em; text-transform:uppercase; color:#a08a6a; }}
.rule {{ width:56px; height:3px; background:#c0673c; margin:.12in auto; border:0; }}

/* generic content page */
.pad {{ padding:.62in .72in; height:100%; }}
.ph {{ margin-bottom:.14in; }}
.ph h2 {{ font-size:34px; color:#2f5233; }}

/* concept */
.roomimg {{ width:100%; border-radius:6px; box-shadow:0 12px 30px rgba(90,60,20,.22); margin:.1in 0 .22in; }}
.lead {{ font-family:Georgia,serif; font-size:15.5px; color:#4a4236; line-height:1.6; }}
.stats {{ display:flex; gap:.18in; margin-top:.24in; }}
.stat {{ flex:1; background:#e8dcc4; border-radius:6px; padding:.16in .18in; }}
.stat .k {{ font-size:9px; letter-spacing:.16em; text-transform:uppercase; color:#a08a6a; }}
.stat .v {{ font-family:Georgia,serif; font-size:19px; color:#2f5233; font-weight:700; }}
.stat .v small {{ font-size:11px; color:#8a7d6b; font-weight:400; }}
.prio {{ margin-top:.22in; display:flex; gap:.1in; flex-wrap:wrap; }}
.chip {{ font-size:11px; padding:.06in .14in; border-radius:20px; background:#2f5233; color:#f2e9d8; }}
.chip.n2{{background:#c0673c;}} .chip.n3{{background:#a9884e;}} .chip.n4{{background:#b9ac91;color:#4a4236;}}

/* anatomy */
.anat {{ display:flex; gap:.24in; height:8.9in; }}
.anat .draw {{ width:56%; display:flex; align-items:center; }}
.anat .draw svg {{ width:100%; height:auto; }}
.anat .legendCol {{ width:44%; }}
.leg {{ display:flex; gap:.1in; margin-bottom:.115in; align-items:flex-start; }}
.leg .dot {{ flex:none; width:22px; height:22px; border-radius:50%; color:#fff; font-family:Georgia,serif; font-weight:700; font-size:12px; display:flex; align-items:center; justify-content:center; }}
.leg b {{ font-size:12.5px; color:#3a332b; }}
.leg p {{ margin:1px 0 0; font-size:10.3px; line-height:1.35; color:#6a5f4e; }}

/* systems / tables */
table {{ width:100%; border-collapse:collapse; font-size:11.5px; }}
th {{ text-align:left; font-family:Georgia,serif; color:#2f5233; border-bottom:2px solid #c0673c; padding:5px 6px; font-size:11px; letter-spacing:.04em;}}
td {{ padding:5px 6px; border-bottom:1px solid #ddd0b6; vertical-align:top; }}
.two {{ display:flex; gap:.3in; margin-top:.2in; }}
.two > div {{ flex:1; }}
.card {{ background:#e8dcc4; border-radius:6px; padding:.16in .2in; margin-top:.12in; }}
.card h3 {{ font-size:14px; color:#2f5233; margin-bottom:4px; }}
.card ul {{ margin:0; padding-left:16px; }} .card li {{ font-size:11px; line-height:1.5; margin:2px 0; }}
.small {{ font-size:10.5px; color:#7a6e5b; }}
.budget td.amt {{ text-align:right; font-family:Georgia,serif; color:#2f5233; }}
.total td {{ border-top:2px solid #c0673c; font-weight:700; }}
.confirm {{ margin-top:.16in; background:#2f5233; color:#f2e9d8; border-radius:6px; padding:.18in .22in; }}
.confirm h3 {{ color:#fdf3d4; font-size:14px; margin-bottom:6px; }}
.confirm ol {{ margin:0; padding-left:18px; }} .confirm li {{ font-size:11.5px; line-height:1.55; margin:3px 0; }}
.footline {{ position:absolute; bottom:.34in; left:.72in; right:.72in; display:flex; justify-content:space-between; font-size:9px; letter-spacing:.22em; text-transform:uppercase; color:#a99a7c; border-top:1px solid #d8cbb0; padding-top:6px; }}
.thumb {{ float:right; width:1.5in; border-radius:5px; box-shadow:0 6px 16px rgba(90,60,20,.2); margin:0 0 .1in .16in; }}
.airflow {{ margin-top:.24in; border:1.5px solid #d8c6a2; border-radius:6px; padding:.18in .2in; }}
.afrow {{ display:flex; align-items:stretch; gap:.06in; margin-top:.1in; }}
.afstep {{ flex:1; display:flex; gap:.08in; }}
.afstep p {{ margin:0; font-size:10.3px; line-height:1.35; color:#5a5040; }}
.afnum {{ flex:none; width:22px; height:22px; border-radius:50%; color:#fff; font-family:Georgia,serif; font-weight:700; font-size:12px; display:flex; align-items:center; justify-content:center; }}
.afnum.green{{background:#2f5233;}} .afnum.brass{{background:#a9884e;}} .afnum.terra2{{background:#c0673c;}}
.afarrow {{ align-self:center; color:#b9a67f; font-size:20px; }}
</style></head><body>

<!-- ===== COVER ===== -->
<section class="page cover">
  <div class="top">
    <div class="sub">Everfresh &middot; Grow Chamber v2</div>
    <h1>The Vitrine</h1>
    <div class="eyebrow">A Living Showpiece Cabinet</div>
  </div>
  <div class="heroWrap"><img src="{HERO1}"></div>
  <div class="foot">
    <hr class="rule">
    <div class="tag">A warm-wood-frame, all-glass grow cabinet on casters — built to grow one <i>Cojoba angustifolia</i> hard, and to be beautiful doing it.</div>
    <div class="spec">~24 &times; 18 &times; 52 in &nbsp;&bull;&nbsp; ~$200 + LED &nbsp;&bull;&nbsp; ~10-day build</div>
  </div>
</section>

<!-- ===== CONCEPT ===== -->
<section class="page">
  <div class="pad">
    <div class="ph"><div class="eyebrow">The Idea</div><h2>A cabinet the tree lives in</h2></div>
    <img class="roomimg" src="{ROOM}">
    <p class="lead">v1 was a wire rack wrapped in taped plastic — leaky, fogged, awkward to open, and no one's idea of a centerpiece. <b>v2 flips the goal:</b> a glass-on-all-four-sides conservatory cabinet that reads as furniture. More room for the leader that's already arching for the light, a full front door for easy daily access, and the glowing tree as the whole point of the room.</p>
    <div class="stats">
      <div class="stat"><div class="k">Interior</div><div class="v">24&times;18&times;52<small> in</small></div></div>
      <div class="stat"><div class="k">Volume</div><div class="v">~14.6<small> cu ft</small></div></div>
      <div class="stat"><div class="k">Cabinet budget</div><div class="v">~$200<small> +LED</small></div></div>
      <div class="stat"><div class="k">Timeline</div><div class="v">~10<small> days</small></div></div>
    </div>
    <div style="margin-top:.28in"><div class="eyebrow">Priorities, in order</div>
      <div class="prio">
        <span class="chip">1 &nbsp;More room</span>
        <span class="chip n2">2 &nbsp;Easy access + beautiful</span>
        <span class="chip n3">3 &nbsp;Leak-sealing</span>
        <span class="chip n4">4 &nbsp;Insulation (out by design)</span>
      </div>
    </div>
    <p class="small" style="margin-top:.22in">Glass on all sides means insulation can't come along — and it's your last priority anyway. The trade: heat &amp; light spill to the room (the glow is the feature). Keep the west window shaded for the 4&ndash;8 PM solar spike, still the highest-leverage thermal fix.</p>
  </div>
  <div class="footline"><span>Everfresh &middot; The Vitrine</span><span>Concept</span></div>
</section>

<!-- ===== ANATOMY ===== -->
<section class="page">
  <div class="pad">
    <div class="ph"><div class="eyebrow">Anatomy</div><h2>How it all fits</h2></div>
    <div class="anat">
      <div class="draw">{CUTAWAY}</div>
      <div class="legendCol">{legend_html}</div>
    </div>
  </div>
  <div class="footline"><span>Everfresh &middot; The Vitrine</span><span>Cutaway</span></div>
</section>

<!-- ===== SYSTEMS ===== -->
<section class="page">
  <div class="pad">
    <div class="ph"><div class="eyebrow">Systems</div><h2>What carries over &amp; what changes</h2></div>
    <table>
      <tr><th style="width:50%">Keep from v1</th><th>Change for v2</th></tr>
      <tr><td>Fogger + reservoir (humidity)</td><td>Drop heat mat &amp; intentional floor pool</td></tr>
      <tr><td>Reptile heat bulb, mid-level, radiating up</td><td>Grow light &rarr; sleek dimmable white LED</td></tr>
      <tr><td>Circ fan up top, blowing down</td><td>Enclosure &rarr; wood + glass vitrine</td></tr>
      <tr><td>Passive low intake + active top exhaust</td><td>Access: taped flap &rarr; full swing door</td></tr>
      <tr><td>2&times; SHT31 (canopy + intake) &middot; Photon</td><td>Hidden waterproof base pan for runoff</td></tr>
    </table>
    <p class="small" style="margin-top:.1in"><b>No firmware changes needed</b> — sensor roles, actuators, and airflow direction are all unchanged.</p>
    <div class="two">
      <div class="card"><h3>Lighting — white, not blurple</h3>
        <ul>
          <li><b>ViparSpectra P1000</b> ~$100 — recommended</li>
          <li>Mars Hydro TS600 ~$70 — value</li>
          <li>Barrina bars ~$35 — sleekest</li>
        </ul>
        <p class="small">Dimmable (hormesis tuning). Driver in the attic. Target ~200&ndash;400 PPFD.</p>
      </div>
      <div class="card"><h3>Seal the leaks</h3>
        <ul>
          <li>EPDM <b>bulb gasket</b> around the door</li>
          <li><b>Compression / roller latch</b> pulls it tight</li>
          <li>Panels bedded in 100% silicone</li>
          <li>All wires via <b>sealed glands</b> in the attic</li>
        </ul>
      </div>
    </div>
    <div class="card"><h3>Aesthetic — warm &amp; tropical</h3>
      <ul>
        <li><b>Frame:</b> Douglas fir (honey) or cedar (rot-resistant, reddish); sapele if you splurge</li>
        <li><b>Finish:</b> teak/Danish oil &rarr; spar urethane — warmth + waterproofs the wood in lieu of a liner</li>
        <li><b>Hardware:</b> brass hinge, handle &amp; latch — conservatory feel</li>
      </ul>
    </div>
    <div class="airflow">
      <div class="eyebrow">Airflow &mdash; the negative-pressure box</div>
      <div class="afrow">
        <div class="afstep"><span class="afnum brass">A</span><p><b>Passive intake, low</b> — a screened port draws room air in at the base.</p></div>
        <div class="afarrow">&rarr;</div>
        <div class="afstep"><span class="afnum green">B</span><p><b>Circ fan pushes down</b> — mixes the gradient; heat bulb warms the rising column.</p></div>
        <div class="afarrow">&rarr;</div>
        <div class="afstep"><span class="afnum terra2">C</span><p><b>Active exhaust, top</b> — pulls stale/hot air out; the afternoon peak-shaver.</p></div>
      </div>
      <p class="small" style="margin:.08in 0 0">Sealed tight, this finally lets the CO&#8322; vent schedule come back on &mdash; the airflow goal v1 never quite reached.</p>
    </div>
  </div>
  <div class="footline"><span>Everfresh &middot; The Vitrine</span><span>Systems</span></div>
</section>

<!-- ===== BUILD ===== -->
<section class="page">
  <div class="pad">
    <img class="thumb" src="{HERO2}">
    <div class="ph"><div class="eyebrow">The Build</div><h2>Budget &amp; plan</h2></div>
    <table class="budget">
      <tr><th>Item</th><th style="text-align:right">Est.</th></tr>
      <tr><td>Acrylic, 1/8&Prime; (3/16&Prime; door), ~30 sq ft cut to size &mdash; TAP Plastics</td><td class="amt">$90</td></tr>
      <tr><td>Frame wood &mdash; Douglas fir / cedar (2&times;2 posts, 1&times;2 rails)</td><td class="amt">$40</td></tr>
      <tr><td>Casters &times;4 (2 locking)</td><td class="amt">$18</td></tr>
      <tr><td>Piano hinge + brass handle + latch</td><td class="amt">$22</td></tr>
      <tr><td>EPDM bulb weatherstrip</td><td class="amt">$10</td></tr>
      <tr><td>Silicone &times;2 + glue + fasteners</td><td class="amt">$15</td></tr>
      <tr><td>Teak oil + spar urethane</td><td class="amt">$20</td></tr>
      <tr class="total"><td>Cabinet total <span class="small">(LED separate)</span></td><td class="amt">~$215</td></tr>
    </table>
    <div class="two">
      <div style="flex:1.15">
        <div class="eyebrow" style="margin-top:.06in">Build sequence</div>
        <table>
          <tr><td style="width:20px;color:#c0673c;font-weight:700">1</td><td>Order/cut acrylic at TAP (lead time &mdash; first)</td></tr>
          <tr><td style="color:#c0673c;font-weight:700">2</td><td>Mill + dry-fit frame; cut joinery + panel rabbets</td></tr>
          <tr><td style="color:#c0673c;font-weight:700">3</td><td>Finish wood (oil + spar) <i>before</i> glazing</td></tr>
          <tr><td style="color:#c0673c;font-weight:700">4</td><td>Build base pan + attic; cut exhaust/intake ports</td></tr>
          <tr><td style="color:#c0673c;font-weight:700">5</td><td>Glaze panels in silicone; hang door + gasket + latch</td></tr>
          <tr><td style="color:#c0673c;font-weight:700">6</td><td>Fogger leak-check by eye before wiring</td></tr>
          <tr><td style="color:#c0673c;font-weight:700">7</td><td>Mount LED, fans, heat bulb, fogger, sensors</td></tr>
          <tr><td style="color:#c0673c;font-weight:700">8</td><td>Power up, seal-test, tune</td></tr>
        </table>
      </div>
    </div>
    <div class="confirm">
      <h3>Three calls to lock it</h3>
      <ol>
        <li><b>Footprint</b> — 24&times;18 (my pick) or 20&times;20 square?</li>
        <li><b>LED</b> — ViparSpectra P1000 (recommended), TS600, or Barrina bars?</li>
        <li><b>Frame wood</b> — Douglas fir (cheap, honey), cedar (rot-proof), or sapele (splurge)?</li>
      </ol>
    </div>
  </div>
  <div class="footline"><span>Everfresh &middot; The Vitrine</span><span>Build &amp; Budget</span></div>
</section>

</body></html>'''

out = HERE / "pamphlet.html"
out.write_text(HTML)
print("wrote", out, len(HTML), "bytes")
