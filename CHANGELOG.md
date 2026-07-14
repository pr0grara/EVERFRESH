# EVERFRESH — Changelog

Firmware (`everfresh.ino`) version history, newest first. Each entry: version — date — very brief summary. Versioning is loose semver: `1.MINOR.PATCH`, minor for a new control paradigm/behavior, patch for a tune/fix.

> Renumbered 2026-07-09 so each minor line tracks a control paradigm: `1.1.x` = the VPD-control era, `1.2.x` = the ceramic-heater era. Git commit subjects predating this use the older flat `1.0.x` numbers.

## v1.2.5 — 2026-07-14
Cooling-vent overhaul: looser duty cap, higher band, elastic ambient-tracking release.

- **Duty cap loosened.** Continuous-ON allowance 1 min→3 min and the forced break 3 min→1 min (25%→~75% ceiling). The pulse was never a fixed cycle — `tempVentOn` releases the vent the instant canopy hits the release point or the room stops being cooler, so a mid-burst temp drop still cuts it immediately; the ON time only caps a *sustained* excursion. The old 25% throttle protected RH between bursts, but v1.2.4's independent RH-hold fog now backfills humidity continuously, so the vent only needs a brief 1-min break for fog to work a closed chamber efficiently.
- **Band raised.** `COOL_ON_F` 84→90, `COOL_OFF_F` 82→88 — a hot tropical afternoon regularly rides 85–100°F, so stop fighting heat until 90. `VENT_EMERGENCY_F` stays 99; vent left VPD-uninhibited on purpose (low ambient RH means the fogger can re-crash VPD fast).
- **Elastic release.** The 3°F `VENT_AMBIENT_DELTA_F` gap did double duty (engage + release); split into `VENT_AMBIENT_DELTA_F`=3 (engage — a lead worth starting on) and `VENT_AMBIENT_DELTA_OFF`=1 (release — chase until the canopy is within 1°F of ambient). Net vent release = `max(COOL_OFF_F, ambient+1)`, so on a 92°F-ambient bake it vents 96→~93 and quits instead of pinning ON forever against an unreachable 88° (it can't pull below ambient). No-ambient fails closed → cooling hands to fog.

## v1.2.4 — 2026-07-10
Fog no longer starves during solar bakes. In mode 3 the fogger was slaved to the excursion's MOIST swing, whose stall detector quit within ~1 min once a cooling-vent pulse re-dried the air, then forced a 12-min rest — so during the 7/09–10 4–8 PM spikes canopy VPD parked at 3–4 kPa (RH ~32%) while fog rested. Added an independent, continuous RH/VPD-hold fog term (center-restoring, evening-cap aware) that bypasses the excursion rest/stall/re-arm entirely: vent owns temperature, fog holds RH, both run together. Purely additive — vent-first cooling, fog-for-cooling (`!ventCools()`), and the 95% RH ceiling are unchanged.

## v1.2.3 — 2026-07-09
Expose firmware version remotely: the `version` cloud variable (`particle get <device> version`) AND a `fw` field in the telemetry JSON → a `fw` column in the Sheet (requires the Apps Script redeploy). Closes the "was it actually flashed?" blind spot and stamps every logged row with the build that produced it.

## v1.2.2 — 2026-07-09
Vent-first cooling. Vent is now the preferred cooler whenever it actually cools (`ventCools()`: room ≥3°F cooler than canopy), engaging from 84°F up; fog is forced for cooling ONLY when venting can't (`!ventCools()`). Fog keeps its RH/VPD job. No-ambient fallback flipped to fog-cooling (was heat-importing venting).

## v1.2.1 — 2026-07-09
VPD de-stress. Whole VPD ladder pulled down ~0.25–0.35 kPa, evening cap 2.2→1.4 kPa, night-export thresholds lowered (0.55/0.85→0.35/0.55) — pinnae stopped unfurling under the ceramic element.

## v1.2.0 — 2026-07-08
Ceramic-heater era. Circ night-rule fix + heater circ interlock (holds min circ while the ceramic element is energized so heat can't stratify past the canopy sensor), plus prior fan/night work.

## v1.1.3 — 2026-06-23
Mode 3 (excursion): arm dwell + faster stall window.

## v1.1.2 — 2026-06-23
Mode 3: room-floor stall stop + stricter dry-arm gate.

## v1.1.1 — 2026-06-23
Add controlMode 3: wide-hysteresis excursion (hormetic) humidity control.

## v1.1.0 — 2026-06-23
VPD-based control (the regime change) — first VPD-driven control paradigm.

## v1.0.6 — 2026-06-22
Vent for high-RH relief only after RH is sustained-high, not on an instantaneous spike.

## v1.0.5 — 2026-06-22
Add ambient temp/RH sensor (2nd SHT31 on software I2C); update observations + logger.

## v1.0.4 — 2026-06-20
Update action thresholds; 25% duty cooling vent (1 min ON / 3 min OFF) to preserve RH/VPD.

## v1.0.3 — 2026-06-20
Variable-speed circ fan: ramp circ up as RH approaches the fog trigger (elongate the sawtooth).

## v1.0.2 — 2026-06-20
Circ fan always on; RH on/off triggers bumped to 65/80.

## v1.0.1 — 2026-06-20
Vent off except for cooling and shedding excess humidity.

## v1.0.0 — 2026-06-20
First versioned firmware (heat/fog/circ/vent hysteresis control + Google Sheets logging).
