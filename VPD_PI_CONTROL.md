# Moisture-regime-gated PI humidity control

This documents the humidity-control redesign in [everfresh.ino](everfresh.ino), how it
departs from the previous logic, what to watch, and **how to roll back fast** if it
misbehaves. Heat is unchanged in every mode — the temperature loop owns the heater.

## Why this exists

The chamber has a **water-inertia** problem the old control couldn't see. Standing floor
water is a hidden reservoir: vent/fog move the *air's* humidity, but the wet floor
re-evaporates into it, so gains are transitory and the system sawtooths. Some nights the
tent over-humidifies (condensation); usually it's too dry. **Instantaneous VPD can't tell
these apart**, so the controller never knew which aggressive action was *safe*.

The fix is a **sense organ**: the inside−outside **dew-point gap**
`dpGap = canopyDewPoint − ambientDewPoint` (°F). It reveals the water *load*, not just the
air. Example (a real condensing night): canopy 70°F/80% → DP 63.6, ambient 69°F/58% →
DP 53.2 → **gap +10°F** = water-loaded → venting will dump moisture to the much drier room
(and fogging would be a mistake). A negative gap means the room is as wet or wetter →
venting is futile and fogging is the safe lever.

## The three control modes (runtime-selectable)

`controlMode` (default **3**) selects the humidity controller. **Change it live with the
`setControlMode` cloud function — no reflash.** This is the primary safety valve.

| Mode | Name | Fog logic | Dry-vent logic | Notes |
|---|---|---|---|---|
| **0** | Original RH bang-bang | `RH_FOG_ON/OFF` (65/90%) + temp-cooling | none | Most conservative fallback |
| **1** | Diurnal VPD bang-bang | `vpdWantFog` (center-restoring per phase) | `vpdWantDryVent` (settled-disarm pulse) | Prior iteration |
| **2** | Regime-gated PI | PI effort → time-duty, regime-interlocked | PI effort → vent time-duty + dry-down pump | Tight servo (chatters) |
| **3** | Wide-hysteresis excursion | `excWantFog`: moisten hard past wet edge, then rest | `excWantVent`: dry hard past dry edge (circ idles), then rest | **Default / new** |

Temp-cooling fog (>`COOL_ON_F`), `RH_CEILING` (95%) fog block, overheat, manual overrides,
emergency vent, temp-cooling vent, and the dwell-gated RH-relief vent are **active in all
modes** — the mode only swaps the *humidity-servo* logic.

## How mode 3 works (default)

Mode 2's PI servo pins VPD to the band *midpoint* with a tight 0.04 kPa deadband, so it
micro-actuates the vent every few seconds and never lets the chamber settle. **Mode 3 is the
hormetic opposite** ([GROW_PHILOSOPHY.md](GROW_PHILOSOPHY.md)): it commits to one big swing,
then rests. It's a latched 3-state machine on canopy VPD, `excursionUpdate()`:

- **REST** — humidity servo OFF; VPD drifts. After the rest plateau (`EXC_REST_MS`, 12 min)
  it re-arms: **too wet** (VPD < bandLo) **and a real dew-point gap** (`dpGap ≥
  EXC_DRY_ARM_GAP_F`, 2.5°F) → **DRY**; **too dry** (VPD > bandHi) and fog feasible → **MOIST**.
  The dry-arm gate is stricter than `dpGap > 0`: without it, a near-equilibrium morning (room
  ≈ as moist as the tent) launches a vent that churns for the full max-drive with zero gain
  (observed 6/23). Too-wet-but-gap-too-small flags `BELOW-ROOM` and waits instead of venting.
  **Arm dwell:** the trigger must HOLD for `EXC_ARM_DWELL_MS` (2 min) before committing — a
  single boot/RH transient (6/23: a 1-sample RH spike to ~67% armed a vent 30 s after flash)
  resets the dwell instead of launching a swing. Same "sustained, not instantaneous" guard as
  the RH-relief vent and the mode-1 dry-vent.
- **DRY** — vent runs *continuously* until VPD **overshoots** `bandHi + EXC_OVERSHOOT_KPA`
  (overdried past the far edge), then → REST. Bails to REST (flagged `BELOW-ROOM`) if the gap
  closes, **the drive stalls** (VPD fails to rise `EXC_STALL_MIN_KPA` for `EXC_STALL_WINDOW_MS`,
  1 min — the room-floor stop: you can't vent the canopy drier than the incoming air; judged
  every 3 s control cycle, not the 60 s log cadence), or `EXC_MAX_DRIVE_MS` (20 min) trips. The
  stall window is *no-progress* time: any `EXC_STALL_MIN_KPA` gain re-anchors the clock, so a
  slowly-working swing keeps running. **Circ is held at the idle mix
  floor while drying** (NOT ramped) — forced convection over the standing floor water
  re-evaporates it into the air (RH↑/VPD↓, worst at night) and fights the excursion. Unlike
  mode 2's dry-down (which ramps circ to evaporate+exhaust the reservoir), mode 3 lets the vent
  do the drying and leaves the floor alone. Fog still wins circ for mist dispersion if
  temp-cooling fires mid-excursion.
- **MOIST** — fog runs continuously until VPD overshoots `bandLo − EXC_OVERSHOOT_KPA`, then →
  REST. Bails on `RH_CEILING`, a stall (fog can't lower VPD), or the drive timeout.

Same regime interlock + vent-feasibility (`dpGap > 0`) as mode 2 pick the lever; heat is never
touched. Net behavior: deliberate 20–45 min swings with real rest plateaus, instead of a
buzzing servo. Telemetry: `ex=DRYING|MOIST|REST` appended to `status`; `veff`/`feff` flip to
1.0 when the vent/fog lever is driving.

## How mode 2 works

1. **Regime** (`resolveRegime`, dual-hysteresis on `dpGap`): **WET** (gap ≥ 4°F),
   **DRY** (gap ≤ 1°F), **NEUTRAL** between. No ambient sensor → NEUTRAL (safe).
2. **Hard interlock:** WET → vent/dry-down enabled, **fog suppressed**. DRY → fog enabled,
   **vent-dry suppressed**. (Temp-cooling + RH-relief vents stay independent.)
3. **PI** (`vpdPIUpdate`) servos canopy VPD to the per-phase **band midpoint** (`vtgt`).
   Output splits by error sign into fog vs vent effort; each maps to a time-duty pulse over
   a 5-min window (`piPulseOn`).
4. **Vent-authority fade:** vent effort ×`clamp(dpGap/6, 0, 1)` → fades to 0 as the gap
   closes. You can't dry below room dew point — that's the physical stop, and it also
   prevents integral windup. (To go drier needs heat; out of scope → `BELOW-ROOM` status.)
5. **Dry-down pump** (`vpdWantDryDown`): WET + VPD well below setpoint → circ ramps to 35%
   *with* the vent to evaporate+exhaust the floor reservoir so drying sticks. **circ never
   drops below the 1% mold-safe floor.**
6. **Anti-windup:** integral clamped, conditional integration, bled when the active lever
   is suppressed/infeasible, held inside the deadband.

## What to watch (telemetry + `status` variable)

New JSON fields: `cm` (mode), `cdp`/`adp` (dew points), `dpgap`, `rg` (WET/NEUTRAL/DRY),
`veff`/`feff` (vent/fog effort 0–1). Cloud variables: `controlMode`, `dpGap`.

- **Healthy:** `vpd` converges toward `vtgt`; `veff`/`feff` taper as it arrives; regime
  matches reality (WET when `dpgap` large).
- **Windup smell:** effort pinned at 1.0 while `vpd` never reaches `vtgt` *and* `dpgap`
  isn't closing → likely the `BELOW-ROOM` ceiling (can't dry further) — expected, not a bug.
- **Chatter:** regime flipping every cycle → widen the `DPGAP_*` hysteresis.

## GUIDE BACK (rollback), fastest first

1. **Runtime (seconds, no reflash) — primary:**
   `particle call <device> setControlMode 2` → regime-gated PI (tight servo).
   `particle call <device> setControlMode 1` → diurnal VPD bang-bang.
   `particle call <device> setControlMode 0` → original RH bang-bang.
   (Switching resets the PI integrator *and* excursion state for a clean handoff.) Confirm via
   the `controlMode` variable / `cm` in telemetry.
2. **Recompile:** change `int controlMode = 3;` near the top of the config block to `2`/`1`/
   `0` and flash, to make the fallback the boot default.
3. **Full removal (git):** `git revert <this commit>` or `git checkout <prev> -- everfresh.ino`.
   Restores the file to before the regime-PI layer entirely.

## Tuning knobs (symptom → knob)

**Mode 3 (excursion):**
- Swings too small / not enough stress → raise `EXC_OVERSHOOT_KPA` (further past the edge).
- Rest plateau too short (re-fires too soon) → raise `EXC_REST_MS`.
- Vent/fog runs too long chasing an unreachable overshoot → lower `EXC_MAX_DRIVE_MS`, or
  tighten the room-floor stop (lower `EXC_STALL_WINDOW_MS` / raise `EXC_STALL_MIN_KPA`).
- Dry swings fire when the room is barely drier (futile vent) → raise `EXC_DRY_ARM_GAP_F`.
- Dry swings never fire even on a genuinely dry room → lower `EXC_DRY_ARM_GAP_F`.
- Swings still arm on brief transients → raise `EXC_ARM_DWELL_MS`; too slow to react to a real
  shift → lower it.
- Arms on tiny edge noise → raise `EXC_TRIGGER_MARGIN`.
- Vent leaves humid pockets while drying (circ too low to feed it) → raise the dry-excursion
  circ floor (`CIRC_MIX_DUTY`, or split out a dedicated knob). Default: circ idles so it doesn't
  evaporate the floor back into the air.

**Mode 2 (PI):**
- Reaches target too slowly / never → raise `VPD_KP`, then `VPD_KI`.
- Overshoots / oscillates around `vtgt` → lower `VPD_KP`/`VPD_KI`.
- Vents when it shouldn't (or won't when it should) → `DPGAP_WET_ON/OFF`, `DPGAP_DRY_ON/OFF`.
- Vent gives up too early/late as it dries → `DPGAP_VENT_FULL_F`.
- Dry-down too aggressive on foliage → lower `CIRC_DRYDOWN_DUTY` (never below `CIRC_MIX_DUTY`).
- Night too dry/humid → `VPD_NIGHT_LO/HI` (currently 0.75–0.9).

## Bench testing without weather

- `setPhase "-1|0..3|3,sun"` — force the diurnal phase (sets the setpoint band).
- `setRegime "-1|0|1|2"` — force DRY/NEUTRAL/WET without spoofing the ambient sensor.
- `setControlMode "0|1|2|3"` — switch controllers (3 = excursion, default).
- Mode 3 swings are slow by design — `setRegime`/`setPhase` to force a band/regime, then watch
  `ex=` march REST→DRYING/MOIST→REST in `status` rather than waiting on real drift.
- `setFog/setHeat/setCirc/setVent` + `clearOverrides` — manual actuator holds (override PI).

## Known limits / follow-ups

- **Can't dry below room dew point** without the heater (flagged `BELOW-ROOM`). The
  permanent fix for the floor-water disturbance is physical (drain/wick the standing water).
- PI gains (`VPD_KP/KI`) are first guesses — tune against logged `vpd` vs `vtgt` and `veff`.
- Build in the Particle Web IDE / `particle compile` before flashing (no local toolchain).
