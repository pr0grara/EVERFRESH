# EVERFRESH — Observations Log

Running field journal for the **_Cojoba angustifolia_** chamber. **Newest entries at
top**; add new notes under the current date. Companion to
[GROW_PHILOSOPHY.md](GROW_PHILOSOPHY.md) (the *why*) and [README.md](README.md) (the
control logic). Plant-behavior cues here are as valuable as the sensor data.

---

## 2026-07-23

- **Drying pinnae are back, and the pattern points at the ceramic element again.** ~10 drying
  pinnae on a single leaf sitting near the ceramic heater, plus ~5 more scattered plant-wide.
  Ara's read: it started after the heater. That is the **same signature as the 7/09 episode**
  ("pinnae not unfurling fully since the ceramic element went in — dry-air stress during frond
  expansion"), which is what prompted the original de-stress ladder pull-down. The 7/15a and
  7/15b ladder raises walked that de-stress back, and the symptom returned. Treat the ceramic
  heater as a standing desiccation source that the fog loop has to be tuned *against*, not as a
  one-time event we already solved.
- **Two distinct populations, probably two causes.** The ~10 on the heater-adjacent leaf are
  concentrated and local → radiant/convective load on that specific leaf, a *placement* problem
  more than a setpoint problem. The ~5 scattered elsewhere are chamber-wide → that's the part
  the VPD ladder is responsible for. Fixing one won't fix the other; the down-tick below only
  addresses the scattered five.
- **⚠️ Suspected sensor contact — highest-priority thing to check physically.** Ara noted the
  affected leaf was *possibly touching the canopy SHT31*. If true this is not a cosmetic detail,
  it corrupts the control point: a leaf against the sensor puts its own humid transpiration
  boundary layer over the element, so the sensor reads RH **too high** → computed canopy VPD too
  **low** → the controller believes the chamber is wetter than it is and under-fogs. That single
  fault would explain the plant-wide drying without any setpoint being wrong at all. **Check and
  clear the sensor before drawing conclusions from this cycle's telemetry** — otherwise the
  down-tick is being evaluated against a lying sensor.
- **Otherwise the plant is doing well.** Trunk thickened, height added, leaves maturing (pinnae
  swelling), roots beginning to circle. The hormetic program is broadly working; this is a trim,
  not a retreat.
- **Roots circling → transplant due.** Logging it here as the next physical intervention. Worth
  sequencing deliberately: transplanting is itself a real stressor, so don't stack it on top of
  an unresolved desiccation problem. Sensor check and the VPD down-tick first, confirm recovery,
  then pot up.
- **Action taken: v1.2.10 gentle VPD down-tick** (flashed OTA same day). Morning HI 1.20→1.10,
  afternoon 1.30→1.20, evening HI 1.30→1.20, spike cap 1.50→1.45. Floors untouched. Roughly
  half-way back toward 7/15a rather than a full revert — fog now comes on sooner without
  abandoning the ladder. **Monitor-and-adjust:** the pinnae are the dosimeter. Watch whether the
  scattered drying arrests and whether the youngest leaf (the early-warning gauge, per 6/21)
  opens cleanly tips-first in the morning. No further ladder moves until there's a plant-read to
  justify one.

## 2026-06-21

- **Overcast day = muted danger window (a natural "rest" day).** Direct A/B vs the
  cloudless 6/20 (peak 93 °F, VPD 2.5): 6/21 peaked only **~84 °F**, VPD capped **~1.3 kPa**.
  The **vent never fired** (temp never reached the 94 °F trigger) and the fogger handled
  the whole warm-but-controlled afternoon solo — dense fog sawtooth ~3:30–7 PM, **no RH
  crash**. Two takeaways: (1) validates the peak-shaver vent design on a *normal* day —
  it correctly stayed asleep; (2) natural cloud cover gives a built-in stress/rest rhythm
  (hammer on sunny days, recover on cloudy ones), and the overcast afternoon sat right in
  the hardening sweet spot (~84 °F, VPD ~1.0–1.3). Confirms the west-window sun is *the*
  load driver — remove it (clouds) and the spike vanishes.
- **The youngest leaf is the most sensitive gauge.** Its pinnae open *first* in the
  morning (tips first, ~8:30) and are also *first to close* under stress. Young tissue
  has the most responsive pulvini + the plant protects its newest growth first → it
  leads in both directions. **Use it as the early-warning indicator** for stress onset
  and recovery. "Tips-first" opening is a graded signal: partial open = light just
  crossing the threshold.
- **Grow light is photosynthetically near-negligible.** Pinnae don't respond to it even
  after 2+ hrs; they open mid-morning only when ambient *natural* daylight rises past a
  threshold → the bulb is dimmer than even *indirect* room daylight (corroborated by it
  adding only ~1°F of heat at light-on). Real light budget = natural (indirect all day +
  the intense 2–4 hr afternoon window sun). Bulb's only real value would be **winter**
  supplementation, and only if upgraded far brighter / full-spectrum. *Not yet PPFD-measured.*
- **6 AM grow-light-on is visible in telemetry** — a clean inflection ~6:15 AM: temp ↑,
  RH ↓, VPD ↑. Pairs with the 8 PM light-off seen in the prior evening's glide.
- **Calm recovery night.** Temp floored ~70 °F (held up by the aquarium-heater water
  bottle, not room-cold), RH flat ~73–74 %, VPD ~0.65 kPa, fogger silent all night.
  The 22-hr hormetic "rest" phase after the afternoon stress pulse.

## 2026-06-20

- **Sealing the chamber bottom (string cinch) = the single biggest RH-stability lever.**
  The 65–85 % RH sawtooth collapsed to a near-flat ~77 %. Dwarfed every firmware tweak.
- **Continuous circ mixing ≫ the old on/off schedule** for holding RH — slows the decay
  and stops the fogger thrashing. The fan evaporates the damp floor back into the air.
- **The vent is a peak-shaver, not a cooler.** A 3-min vent-off test during the live
  spike: RH 45 → 75 %, VPD 2.5 → 1.25 kPa, **temp barely moved**. Continuous venting was
  crashing humidity for almost no cooling. Now it pulses 1-min-on / 3-min-off above 94 °F.
  We optimize **VPD**, with temperature as a safety ceiling.
- **Solar spike** (~6:00–7:15 PM): temp → 93 °F, RH → 42 %, VPD → 2.5 kPa. West-window
  direct sun, ~2–4 hr/day — the only real heat danger.
- **Incomplete pinnae closure during the spike** on a cloudless, above-average-load day →
  *less* stress than on prior milder days under the old vent-heavy regime. Biological
  confirmation of the VPD-first strategy.
- **Evening self-stabilization.** Fogger silent from ~7:15 PM, yet RH held dead flat ~77 %
  via an internal water cycle (floor evaporation ≈ condensation on the cooling walls) plus
  the warm water-bottle's slow baseline. The tiny RH "teeth" are integer-rounding
  artifacts (telemetry logs RH as whole %), **not** fog events.
