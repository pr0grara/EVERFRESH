# EVERFRESH — Observations Log

Running field journal for the **_Cojoba angustifolia_** chamber. **Newest entries at
top**; add new notes under the current date. Companion to
[GROW_PHILOSOPHY.md](GROW_PHILOSOPHY.md) (the *why*) and [README.md](README.md) (the
control logic). Plant-behavior cues here are as valuable as the sensor data.

---

## 2026-06-21

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
