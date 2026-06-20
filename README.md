# EVERFRESH — Greenhouse Controller

Automated environmental control for **_Cojoba angustifolia_** (Everfresh / Monkey
Earring Tree), a tropical legume, on a **Particle Photon**.

| Variable    | Target band  | Notes                              |
|-------------|--------------|------------------------------------|
| Temperature | **75–85 °F** | Warm tropical understory species   |
| Humidity    | **50–80 % RH** | High ambient humidity              |
| VPD         | logged       | Vapor Pressure Deficit, for future VPD-based control |

> **Setting up?** Follow [WIRING_AND_SETUP.md](WIRING_AND_SETUP.md) (staged,
> safest-first bring-up). For data logging, see [LOGGING.md](LOGGING.md).

---

## How it works

**Two SHT31 sensors** over one I²C bus:
- **canopy** (`0x44`) — the **control point**; every decision uses this.
- **ambient** (`0x45`) — room air (what the vent pulls in); logged, **and gates vent
  cooling** — the vent only cools when the room is meaningfully cooler than the canopy.

**Four actuators:**
- **HEAT** — 120 VAC heater via SSR (on/off).
- **FOG** — 24 VDC ultrasonic fogger transducer via MOSFET (on/off). Raises humidity;
  also provides evaporative cooling when hot.
- **CIRC** — 4-wire PWM circulation fan: **internal mixing, no fresh air**. Full speed
  during any active cooling (fog mist-dispersion or venting) + **continuous gentle
  mixing** between cycles.
- **VENT** — 2-wire exchange fan via MOSFET: **fresh-air exchange** with ambient.
  On-demand — cooling (when the room can actually help) + high-RH safety. (Scheduled
  exchange is off while the chamber is leaky; re-enable once it's sealed.)

Control is **hysteresis bang-bang** for heat and fog (RH is the humidity loop), and
temperature-reactive threshold logic for the fans. VPD is computed on-device and logged.

---

## Hardware

### Bill of materials

| Qty | Part                              | Notes                                              |
|-----|-----------------------------------|----------------------------------------------------|
| 1   | Particle Photon                   | (Photon 2 / Argon also fine)                       |
| 2   | SHT31-D breakout                  | I²C; canopy ADDR→GND (0x44), ambient ADDR→3V3 (0x45) |
| 1   | Solid-state relay (SSR), 3–32 V in | For the 120 VAC heater — **use an SSR, not mechanical** |
| 3   | Logic-level N-ch MOSFET (or module) | Fogger (24 V), vent fan (12 V), circ-fan power (12 V) |
| 2   | Flyback diode (1N5819)            | Across each fan motor (vent, circ)                 |
| 1   | 4-wire PWM fan                    | Circulation (internal mixing)                      |
| 1   | 2-wire fan                        | Vent / air exchange                                |
| 1   | Ultrasonic fogger (24 V)          | + reservoir                                        |
| —   | 12 V + 24 V + 5 V/USB supplies    | 12 V fans, 24 V fogger, 5 V Photon                 |

> **Why an SSR for the heater?** A mechanical relay cycling a resistive heater all
> day pits and welds its contacts — the most likely failure point. SSRs are silent
> and have no moving parts.

### Pin map

| Pin   | Drives                          | Element            |
|-------|---------------------------------|--------------------|
| D0/D1 | both SHT31 (SDA/SCL)            | I²C                |
| D2    | heater                          | SSR (on/off)       |
| D3    | fogger transducer (24 V)        | MOSFET (on/off)    |
| A5    | circ fan **PWM wire** (speed)   | direct to 4-wire fan |
| D4    | circ fan **power** (true off)   | MOSFET (low-side)  |
| A4    | vent fan (2-wire)               | MOSFET (on/off)    |

---

## Wiring

### Sensors (I²C, 3.3 V)

```
canopy  SHT31  VIN→3V3 GND→GND SDA→D0 SCL→D1  ADDR→GND  (= 0x44)
ambient SHT31  VIN→3V3 GND→GND SDA→D0 SCL→D1  ADDR→3V3  (= 0x45)
```

⚠️ Power sensors from **3V3**, not VIN — their onboard pull-ups must reference 3.3 V
(the Photon isn't 5 V-tolerant). Most breakouts include pull-ups; if not, add 4.7 kΩ
SDA/SCL → 3V3.

### Low-side MOSFET (fogger, vent fan, circ-fan power)

Each switched load uses a logic-level N-channel MOSFET on the **ground** side:

```
        +V ──────────────┬───────────┐
                       [ LOAD ]    ▲ 1N5819 (flyback, across motor loads)
                          │         │
        Photon pin ─[220Ω]┴─ GATE   │
                          │         │
                      [10kΩ]   DRAIN┘
                          │
                         GND ── SOURCE ── supply GND ── Photon GND
```

- **Logic-level** MOSFET (fully on at 3.3 V gate — e.g. IRLZ44N).
- **Flyback diode** across the fan motors (vent, circ); fogger transducer is
  largely non-inductive.
- **Common ground is mandatory:** Photon GND ↔ 12 V(−) ↔ 24 V(−) all tied.

### Circulation fan (4-wire) — two pins

- **PWM wire → A5** (speed; 25 kHz, driven straight from the Photon).
- **Power via a low-side MOSFET on D4** — gives a **true off**. A 4-wire fan idles
  at 0 % PWM, so the MOSFET cuts power to stop it fully.
- **Cut and insulate the tach wire.** Two gotchas this avoids/handles:
  - With the power MOSFET off, the fan can back-feed to ground through a *signal*
    wire and idle at min RPM. The firmware **releases the PWM pin to Hi-Z when off**
    to kill that path; a connected tach wire would reopen it.

### 120 VAC heater (SSR)

`D2 → SSR DC+`, `GND → SSR DC−`. Switch only the **hot** conductor through the SSR's
AC terminals, fully enclosed. ⚠️ Mains never touches the breadboard.

---

## Control logic

All thresholds are in the `CONFIG` block of `everfresh.ino`.

### Heater (hysteresis)
| Condition | Action |
|-----------|--------|
| Temp < 76 °F (`HEAT_ON_F`) | ON |
| Temp > 78.5 °F (`HEAT_OFF_F`) | OFF |
| Temp ≥ 92 °F (`TEMP_SAFETY_F`), any cause | hard OFF |

### Fogger (humidity + evaporative cooling)
- **Humidity:** ON below `RH_FOG_ON` (65 %), OFF above `RH_FOG_OFF` (80 %).
- **Cooling:** ON above `COOL_ON_F` (84 °F), OFF below `COOL_OFF_F` (82 °F).
- **Hard ceiling:** never fog above `RH_CEILING` (90 %).
- ~30 s bursts. Min on/off timers prevent chatter.

### Circulation fan (internal mixing — no fresh air)
Three speed regimes:
- **100 % during any active cooling** — fogging (disperse mist) *or* venting (push hot
  air at the vent so heat doesn't stratify).
- **RH-assist ramp** — when idle but RH is sagging toward the fog-on point, circ speed
  ramps proportionally from `CIRC_MIX_DUTY` up to `CIRC_RH_ASSIST_MAX` (35 %) across the
  band `[RH_FOG_ON, RH_FOG_ON + RH_ASSIST_BAND]` (65→75 %). More airflow over the damp
  floor = more evaporation = RH propped up *before* the fogger has to fire. Goal: flatten
  the RH decay and stretch the inter-fog interval (fewer, shallower sawteeth). Capped at
  35 % so sustained airflow doesn't desiccate the foliage.
- **Idle: continuous gentle mixing** (`CIRC_MIX_DUTY`) when RH is comfortable.
- Recirculating over the damp floor homogenizes the air *and* evaporates standing
  water back into the chamber — sustains RH between fog cycles while drying the floor.
  (Proven 6/20: continuous mixing slowed RH decay dramatically and stopped the fogger
  thrashing vs. the old on/off mixing schedule.)

### Cooling ladder (temperature-reactive)
Cooling escalates with canopy temperature; everything is hysteretic (no chatter). The
chamber runs *cold* most of the day, so these only fire during the afternoon solar
spike — temperature is effectively the schedule.

| Canopy temp | Response |
|-------------|----------|
| ≥ `COOL_ON_F` (84 °F) | **Fog** — evaporative cooling + restores the RH the heat spike steals (cut at `RH_CEILING`) |
| ≥ `VENT_TEMP_ON_F` (86 °F) | **Vent** adds (primary heat-*energy* removal) **+ circ to full** |
| ≥ `TEMP_SAFETY_F` (92 °F) | **Panic**: vent + circ forced full (beats manual), heater hard-off |

### Vent fan (fresh-air exchange — on/off)
On-demand only, since the leaky chamber already exchanges air passively:
- **Cooling** (primary): vent if temp ≥ `VENT_TEMP_ON_F` (off below `VENT_TEMP_OFF_F`),
  **but only when venting actually cools** — with a valid ambient reading it requires
  `canopy − ambient ≥ VENT_AMBIENT_DELTA_F` (3 °F). Until the ambient sensor is
  installed the guard is skipped (the room is known cooler than the sunlit tent).
- **High-RH safety**: vent if RH ≥ `VENT_RH_ON` (off below `VENT_RH_OFF`) — an
  independent humidity-*down* relief valve near saturation, *not* ambient-gated.
- **Scheduled exchange** (`VENT_DURATION_MS` every `VENT_INTERVAL_MS`) is gated by
  `VENT_SCHEDULE_ENABLED` — **off** for now; re-enable once the chamber is sealed and
  leaks no longer supply fresh air.

---

## Safety

- **No blind heating** — if the canopy sensor is invalid, heater + fogger forced OFF,
  `ALARM` published.
- **Hard thermal cutoff** at `TEMP_SAFETY_F` — heater off *and* vent + circ forced to
  full to dump heat, overriding any manual hold.
- **Watchdog** resets the board if `loop()` hangs > 60 s.
- **Known-safe boot** — all loads off before anything else.
- **Min on/off timers** on heater (60 s) and fogger (20 s).
- **Manual overrides auto-expire** (default 60 s, max 600 s) so nothing is forced
  forever; the overheat cutoff still wins over a manual heater override.

**Not yet handled:** fogger dry-run protection (add a reservoir float switch).

---

## Configuration reference

| Constant | Default | Meaning |
|----------|---------|---------|
| `RELAY_ACTIVE_LOW` | `false` | `false` = active-HIGH (SSR/MOSFET) |
| `HEAT_ON_F` / `HEAT_OFF_F` | 76 / 78.5 | Heater band |
| `TEMP_SAFETY_F` | 92 | Heater hard-off |
| `COOL_ON_F` / `COOL_OFF_F` | 84 / 82 | Fog-for-cooling band |
| `RH_FOG_ON` / `RH_FOG_OFF` | 65 / 80 | Fog (humidity) band |
| `RH_CEILING` | 90 | Never fog above |
| `CIRC_FOG_DUTY` | 100 | Circ % during any active cooling (fog or vent) |
| `CIRC_MIX_DUTY` | (your fan's min) | Circ % for continuous between-cycle mixing; **0 disables mixing** |
| `CIRC_RH_ASSIST_MAX` | 35 | Circ % ceiling for the RH-assist ramp (gentle, foliage-safe) |
| `RH_ASSIST_BAND` | 10 | RH span above `RH_FOG_ON` over which circ ramps idle→assist |
| `VENT_SCHEDULE_ENABLED` | `false` | Periodic exchange on/off — off while chamber is leaky |
| `VENT_DUTY` | 100 | Vent on/off (on=full) |
| `VENT_INTERVAL_MS` / `_DURATION_MS` | 120 / 2 min | Periodic exchange (when enabled) |
| `VENT_TEMP_ON_F` / `_OFF_F` | 86 / 82 | Vent-to-cool band |
| `VENT_RH_ON` / `_OFF` | 88 / 80 | Vent-to-shed-humidity band |
| `VENT_AMBIENT_DELTA_F` | 3 | Min canopy − ambient gap to vent-cool (skipped w/o ambient sensor) |
| `HEAT_MIN_ON/OFF`, `FOG_MIN_ON/OFF` | 60s / 20s | Anti-chatter |

---

## Build & flash

```bash
particle flash <device-name> everfresh.ino
```
No external libraries (the SHT31 driver is inlined). Flashes over-the-air when the
device is online. The VS Code linter flags `D2`/`String`/`Wire` etc. as undefined —
that's cosmetic (it lacks the Particle headers); the real `particle` build is fine.

---

## Remote monitoring

**Cloud variables:** `canopyTempF`, `canopyRH`, `vpd`, `ambientTempF`, `ambientRH`,
`ambientVPD`, `circ`, `vent`, `heat`, `fog`, `mode`, `status`. A reading of `-1`
means that sensor is currently invalid.

**Manual override functions** — `0 = off, nonzero = on`; auto-revert after the window:

```bash
particle call <device> setFog  "1"      # fogger ON (or "1,30" = 30s);  "0" = OFF
particle call <device> setHeat "1"/"0"   # heater (overheat cutoff still applies)
particle call <device> setCirc "60"      # circ fan at 60% (0 = true off via power MOSFET)
particle call <device> setVent "100"/"0" # vent fan on/off
particle call <device> clearOverrides "" # cancel all, back to auto
```

**Published events:**

| Event | When | Payload |
|-------|------|---------|
| `everfresh/telemetry` | every 60 s | JSON snapshot (below) |
| `everfresh/event` | actuator toggles | `{"ev":"fog","state":"on","ct":..,"crh":..}` |
| `everfresh/alert` | alarm state change | `no-sensor` / `overheat` / `cleared` |
| `everfresh/cmd` | manual override invoked | what was commanded |

Telemetry JSON:
```json
{"ct":78.4,"crh":61,"vpd":0.95,"at":74.0,"arh":55,"avpd":1.20,"heat":0,"fog":0,"circ":0,"vent":0,"mode":"auto"}
```
`ct/crh/vpd` = canopy temp/RH/VPD, `at/arh/avpd` = ambient, `circ/vent` = fan duty %.
Forward these to Google Sheets per [LOGGING.md](LOGGING.md).

---

## Tuning guide

1. **Circ min speed.** Find the lowest PWM your fan reliably *starts* at
   (`setCirc "20"`, lower until it won't spin, back off a notch) and set
   `CIRC_MIX_DUTY` there. `0` keeps mixing off (circ then only runs during cooling).
2. **Verify true-off** — `setCirc "0"` should dead-stop the circ fan (power MOSFET
   cuts it). If it idles, check the tach wire is cut/insulated.
3. **Relay polarity** — flip `RELAY_ACTIVE_LOW` if loads invert.
4. **Safety cutoff** — temporarily lower `TEMP_SAFETY_F` below room temp and confirm
   the heater refuses to run; restore.
5. **Think in VPD** — log a few days; the comfortable band for this plant is roughly
   0.6–1.0 kPa. Eventually drive fogging off VPD instead of raw RH.

---

## Field notes & observations (2026-06)

- **Fogger:** ~30 s dose. Long continuous pulses (a past bug) **saturate the box** —
  liquid deposits on surfaces and RH then falls very slowly. Keep doses short.
  Asymmetry: humidity goes up fast (fogger) but down slowly (passive leak / vent).
- **Constant ventilation is a load:** an A/B test (unplugging an always-on fan)
  stretched the fog sawtooth period ~4 → ~7 min (~45 % less fogger runtime) for the
  same RH band. → vent should be *mostly off*.
- **Leaky chamber → scheduled venting is redundant:** the chamber is far from
  airtight, so leaks already supply fresh air / CO₂. Scheduled exchange is therefore
  **off** (`VENT_SCHEDULE_ENABLED = false`); the vent is purely on-demand (cooling +
  high-RH safety). Re-enable the schedule once the chamber is sealed.
- **4-wire fan + low-side power MOSFET:** with power cut, the fan back-feeds to
  ground through its **signal wires** and idles. Fixed by releasing the PWM pin to
  **Hi-Z when off** and cutting the tach wire. (2-wire fans don't have this — no
  signal wires.)
- **Recirculation ≠ ventilation:** the circ fan mixes internal air (and evaporates
  floor water back into the chamber); only the vent exchanges with ambient. Decoupling
  them was the key architecture split.
- **Continuous mixing sustains RH (6/20):** under the old 15-on/15-off mixing schedule,
  RH crashed and the fogger refired every ~5 min during the OFF halves; with mixing left
  **on continuously**, RH decayed far more slowly and fog cycling nearly stopped. The
  circ fan is effectively a slow humidifier (evaporates the damp floor back into the
  air). **Open question:** with continuous mixing RH appears to settle in the **low 60s
  %** — watch whether that floor is too dry for this tropical species; if so, raise
  `RH_FOG_ON` to fog sooner, or accept the higher fog duty.

---

## Open items / TODO

- [ ] **Vent fan true-off check** — 2-wire on MOSFET stops cleanly; confirm in logs.
- [ ] **Heater + SSR (Step 4)** — 120 VAC subsystem still to be wired.
- [ ] **Fogger dry-run protection** — reservoir float switch on a GPIO.
- [ ] **Flyback diodes** on the fan MOSFETs before permanent install.
- [ ] **Install ambient SHT31 (0x45)** — arriving 6/21; activates the vent cooling
      guard (`VENT_AMBIENT_DELTA_F`) automatically once it reads valid.
- [ ] **RH floor check** — confirm the low-60s %RH settling point under continuous
      mixing isn't too dry for the plant (raise `RH_FOG_ON` if so).
- [ ] **VPD-based control + day/night zones** — once enough logs are digested.
- [ ] **Optional solar-window awareness** — publish a "spike-season" flag / slightly
      pre-emptive cooling during the afternoon window (needs cloud-time + DST).
- [ ] **Alerting** — hook `everfresh/alert` to a phone/email notification.

**Done:** dual SHT31 (canopy control + ambient), fogger, decoupled circ (PWM + true-off
power MOSFET) and vent fans, **continuous circ mixing + RH-assist speed ramp**, **temperature-reactive cooling
ladder** (fog → ambient-gated vent + full circ → overheat panic), manual-override cloud
functions (0/1), on-device VPD, Google Sheets logging with local time + VPD.
