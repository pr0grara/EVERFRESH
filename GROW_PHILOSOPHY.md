# EVERFRESH — Grow Philosophy

The guiding intent behind the control strategy for **_Cojoba angustifolia_** (Everfresh /
Monkey Earring Tree). The firmware setpoints exist to serve *this* — when in doubt about a
tuning decision, come back here.

> **Goal: raise a strong, resilient plant — not a coddled one.** A little stress builds
> character and should not be shirked.

---

## The core idea: hormesis (hardening)

Controlled, sub-damaging stress triggers adaptive responses that make a plant *tougher*:
heat-shock proteins, thicker cuticles, denser tissue, stronger stems, and better root and
vascular systems. A plant raised in flat, perfectly comfortable conditions grows soft,
leggy, and fragile. One cycled through mild stress grows hard. For a long-lived specimen
tree, **the "grow hard" path is the better one.**

The art is staying in **eustress** (beneficial stress) and out of **distress** (damage).
Hormesis is dose-dependent: the dose that strengthens sits just below the dose that harms.

---

## Flip the weakness into an advantage: the solar window

The chamber gets a **maximum of ~2 hours of direct west-window sun** per day, which spikes
the temperature. Rather than fight it, we **use** it:

- **It's light, not just heat.** That solar load is high-intensity PAR. *Cojoba* matures
  into a sun tree — direct light drives photosynthesis, compact internodes, thicker leaves,
  sturdier stems. The heat is the byproduct; the light is a genuine asset.
- **It mimics the native climate.** A hot, humid tropical afternoon at 90–95 °F is exactly
  this species' evolutionary comfort zone. What it is *not* adapted to is hot-and-*dry*.
- **The duty cycle is ideal.** ~2 hours of stress + ~22 hours of recovery. Hormesis works
  as stress-*then-recover* (like exercise + rest — adaptation happens during recovery). The
  heating-dominant baseline the rest of the day provides that recovery.

---

## Eustress vs. distress — the three rules

1. **The enemy is desiccation, not warmth.** 90–95 °F at *moderate* humidity is eustress.
   The same 92 °F at 42 % RH (VPD ~2.5 kPa) is distress — water ripped from the leaves
   faster than roots resupply. Same temperature, opposite outcome. Keep humidity up enough
   that the heat is "warm," not "drying."
2. **Don't over-correct into coddling.** Crushing RH to ~90 % (VPD ~0.5 kPa) during the
   window is *too soft* — a little transpirational demand is part of what builds the
   vascular system and cuticle. **Aim for moderate VPD (~1.0–1.5 kPa)** during the spike —
   native-afternoon-like. The current firmware already lands here (90–92 °F at RH 70–78 %).
3. **A plant can only adapt if it's resourced.** Hardening a well-watered, well-fed plant
   builds strength; stressing a thirsty or hungry one just damages it. Keep roots watered
   and nutrition solid so it can *mount* the adaptive response.

---

## Where the line is

| Condition | Zone |
|-----------|------|
| 90–95 °F, moderate–high RH, ~2 h/day | **Prime eustress** — lean in |
| 95–100 °F | Upper edge — fine short-term, watch duration |
| > 100 °F, or many hours | Tips toward **distress** regardless of RH |

**Duration matters as much as peak.** A short humid spike is a non-event; the same
temperature for many hours is not.

---

## Reading the plant (live feedback)

- **Pinnae closure is the dosimeter.** This species folds its pinnae under heat/drought
  stress. *Incomplete* closure during the spike = within the hardening zone (good). Hard,
  full closure that's slow to reopen the next morning = overshoot, back off.
- **Adaptation is working:** compact, dark, thick new growth; vigor.
- **Overshoot / distress:** leaf-edge scorch, persistent wilt, leaf drop, growth that
  arrests and doesn't rebound, fungal spotting (the one real downside of warm + humid —
  continuous circ mixing is the mitigation).

---

## How the control system serves this

- **Lets temperature ride during the window** — the vent only *pulses* above 94 °F (short
  bursts so humidity recovers), and the hard force-vent backstop sits high (99 °F). We don't
  fight the spike.
- **Holds humidity enough to avoid desiccation, not so much it coddles** — fog targets a
  moderate RH band, landing VPD in the strengthening range during the spike.
- **Forces full recovery** — the heating-dominant baseline returns the chamber to optimal
  the other ~22 hours.
- **We optimize VPD, with temperature as a safety ceiling** — not the other way around.

See [README.md](README.md) for the control logic and setpoints that implement this.
