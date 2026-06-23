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

`controlMode` (default **2**) selects the humidity controller. **Change it live with the
`setControlMode` cloud function — no reflash.** This is the primary safety valve.

| Mode | Name | Fog logic | Dry-vent logic | Notes |
|---|---|---|---|---|
| **0** | Original RH bang-bang | `RH_FOG_ON/OFF` (65/90%) + temp-cooling | none | Most conservative fallback |
| **1** | Diurnal VPD bang-bang | `vpdWantFog` (center-restoring per phase) | `vpdWantDryVent` (settled-disarm pulse) | The prior iteration |
| **2** | Regime-gated PI | PI effort → time-duty, regime-interlocked | PI effort → vent time-duty + dry-down pump | **Default / new** |

Temp-cooling fog (>`COOL_ON_F`), `RH_CEILING` (95%) fog block, overheat, manual overrides,
emergency vent, temp-cooling vent, and the dwell-gated RH-relief vent are **active in all
modes** — the mode only swaps the *humidity-servo* logic.

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
   `particle call <device> setControlMode 1` → diurnal VPD bang-bang.
   `particle call <device> setControlMode 0` → original RH bang-bang.
   (Switching resets the PI integrator for a clean handoff.) Confirm via the `controlMode`
   variable / `cm` in telemetry.
2. **Recompile:** change `int controlMode = 2;` near the top of the config block to `1` or
   `0` and flash, to make the fallback the boot default.
3. **Full removal (git):** `git revert <this commit>` or `git checkout <prev> -- everfresh.ino`.
   Restores the file to before the regime-PI layer entirely.

## Tuning knobs (symptom → knob)

- Reaches target too slowly / never → raise `VPD_KP`, then `VPD_KI`.
- Overshoots / oscillates around `vtgt` → lower `VPD_KP`/`VPD_KI`.
- Vents when it shouldn't (or won't when it should) → `DPGAP_WET_ON/OFF`, `DPGAP_DRY_ON/OFF`.
- Vent gives up too early/late as it dries → `DPGAP_VENT_FULL_F`.
- Dry-down too aggressive on foliage → lower `CIRC_DRYDOWN_DUTY` (never below `CIRC_MIX_DUTY`).
- Night too dry/humid → `VPD_NIGHT_LO/HI` (currently 0.75–0.9).

## Bench testing without weather

- `setPhase "-1|0..3|3,sun"` — force the diurnal phase (sets the setpoint band).
- `setRegime "-1|0|1|2"` — force DRY/NEUTRAL/WET without spoofing the ambient sensor.
- `setControlMode "0|1|2"` — switch controllers.
- `setFog/setHeat/setCirc/setVent` + `clearOverrides` — manual actuator holds (override PI).

## Known limits / follow-ups

- **Can't dry below room dew point** without the heater (flagged `BELOW-ROOM`). The
  permanent fix for the floor-water disturbance is physical (drain/wick the standing water).
- PI gains (`VPD_KP/KI`) are first guesses — tune against logged `vpd` vs `vtgt` and `veff`.
- Build in the Particle Web IDE / `particle compile` before flashing (no local toolchain).
