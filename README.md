# EVERFRESH — Greenhouse Controller

Automated environmental control for **_Cojoba angustifolia_** (Everfresh / Monkey
Earring Tree) — a tropical legume — running on a **Particle Photon 2** (also builds
on the original Photon and Argon).

The controller holds the plant's micro-climate inside its preferred envelope:

| Variable    | Target band | Why                                            |
|-------------|-------------|------------------------------------------------|
| Temperature | **75–85 °F** | Warm tropical understory species               |
| Humidity    | **50–80 % RH** | High ambient humidity, but not waterlogged air |

---

## Table of contents

1. [How it works](#how-it-works)
2. [Hardware](#hardware)
3. [Wiring](#wiring)
4. [Control logic](#control-logic)
5. [Safety](#safety)
6. [Configuration reference](#configuration-reference)
7. [Build & flash](#build--flash)
8. [Remote monitoring](#remote-monitoring)
9. [Tuning guide](#tuning-guide)
10. [Open items / TODO](#open-items--todo)

---

## How it works

The plant lives in a sealed housing. Four physical effects are managed with three
controlled actuators plus passive airflow:

- **Heat** — a 120 VAC aquarium heater (in a water reservoir) *or* a heat mat,
  switched by a solid-state relay (SSR).
- **Humidity & cooling (shared)** — a 12 VDC ultrasonic fogger sitting in a
  reservoir beneath the housing floor, paired with a 12 VDC fan on the floor
  pointing down. This single combo both **raises humidity** and provides
  **evaporative cooling**.
- **Airflow** — a 12 VDC fan at the top of the housing creating positive
  pressure, **variable speed** (PWM). Runs 24/7 at a baseline and ramps up when
  the air needs to move — notably, it is the *only* active way to **lower**
  humidity (there is no dehumidifier).

Two **SHT31 temp/RH sensors** read the environment: one at the **canopy**, one at
the **trunk**. The canopy is the primary control point (where the leaves
transpire); the trunk is a fallback and a safety input.

Control is **hysteresis (deadband) bang-bang** — appropriate because every
actuator is on/off (the fan being the exception, with proportional speed).

> **Setting it up for the first time?** Follow
> [WIRING_AND_SETUP.md](WIRING_AND_SETUP.md) — a staged, safest-first bring-up
> procedure that brings each subsystem online one at a time.

---

## Hardware

### Bill of materials

| Qty | Part                                   | Notes                                                  |
|-----|----------------------------------------|--------------------------------------------------------|
| 1   | Particle Photon 2                      | Original Photon also works (it's EOL — fine to reuse)  |
| 2   | SHT31-D breakout                       | I²C; one strapped to 0x44, one to 0x45                 |
| 1   | Solid-state relay (SSR), 3–32 VDC in   | For the 120 VAC heater. **Use an SSR, not a mechanical relay.** |
| 2   | Logic-level N-channel MOSFET           | e.g. IRLZ44N / AOI518 — one for fogger combo, one for airflow fan |
| 2   | Flyback/Schottky diode (1N5819)        | Across each DC fan/motor load                          |
| 2   | Resistor ~220 Ω                        | Gate series resistor (one per MOSFET)                  |
| 2   | Resistor ~10 kΩ                        | Gate pull-down (one per MOSFET)                        |
| 1   | 12 VDC power supply                     | Powers fogger + both fans; size for combined current   |
| 1   | 5 VDC / USB supply                      | Powers the Photon                                      |
| —   | Ultrasonic fogger, heater/mat, 2× fans | Per the housing build                                  |

> **Why an SSR for the heater?** A mechanical relay cycling a resistive heater all
> day will pit its contacts and eventually weld or fail — the single most likely
> point of failure in this system. SSRs switch silently with no moving parts.

---

## Wiring

### Pin map (defaults in `everfresh.ino`)

| Signal      | Photon pin | Drives                          | Switch element        |
|-------------|------------|---------------------------------|-----------------------|
| `PIN_HEAT`  | `D2`       | 120 VAC heater                  | SSR (digital on/off)  |
| `PIN_FOG`   | `D3`       | 12 VDC fogger + mist fan combo  | MOSFET (digital on/off)|
| `PIN_CIRC`  | `A5`       | 12 VDC airflow fan              | MOSFET (**PWM**)       |
| I²C SDA/SCL | `D0`/`D1`  | Both SHT31 sensors              | —                     |

> `PIN_CIRC` is on `A5` because it must be **PWM-capable**. The original Photon's
> `D4` is *not* a PWM pin. Verify A5 supports PWM on whichever board you flash.

### Sensors (I²C, 3.3 V)

Both SHT31s share the same two-wire bus:

```
SHT31 (canopy)  VIN→3V3  GND→GND  SDA→D0  SCL→D1   ADDR→GND  (= 0x44)
SHT31 (trunk)   VIN→3V3  GND→GND  SDA→D0  SCL→D1   ADDR→3V3  (= 0x45)
```

The only difference between the two is the **ADDR pin**: tie it low for 0x44,
high for 0x45. Most breakouts have on-board pull-ups on SDA/SCL; if neither does,
add 4.7 kΩ pull-ups to 3V3.

### 120 VAC heater (SSR)

```
Photon D2 ──────────────▶ SSR  DC+ input
Photon GND ─────────────▶ SSR  DC− input
                          SSR  AC terminals  ──▶ in series with heater's hot line
```

⚠️ **120 VAC is dangerous.** Switch only the **hot** (live) conductor through the
SSR, keep mains wiring enclosed and strain-relieved, and never work on it live.
Heat-sink the SSR if it runs near its current rating.

### 2-wire DC fan / fogger (low-side MOSFET + PWM)

A 2-wire fan can't take a PWM control signal directly, so we chop its **power**
with a logic-level N-channel MOSFET on the low (ground) side. The airflow fan
uses `analogWrite` (PWM); the fogger combo uses `digitalWrite` (full on/off) —
**same circuit** either way.

```
                +12V ───────────────┬──────────────┐
                                     │              │
                                  [ FAN + ]      ▲ cathode
                                  [ FAN   ]   1N5819  (flyback diode,
                                  [ FAN - ]      ▼ anode   across the fan)
                                     │              │
                                     ├──────────────┘
                                     │  ◀── MOSFET DRAIN
        Photon A5 ──[ 220Ω ]──┬──── GATE
                              │
                          [ 10kΩ ]   (gate pull-down — keeps fan OFF at boot/reset)
                              │
                             GND ──── MOSFET SOURCE ──── 12V supply GND ──── Photon GND
```

Key points:

- **Logic-level** MOSFET required — its gate must fully turn on at 3.3 V (the
  Photon's logic level). A standard IRF540 will *not*; an IRLZ44N or AOI518 will.
- **Flyback diode** (1N5819) across the fan: cathode to +12 V, anode to the
  drain. Protects the MOSFET from the inductive kick when the motor switches off.
- **Gate series resistor** (~220 Ω) limits inrush into the gate; **pull-down**
  (~10 kΩ) guarantees the fan is OFF while the Photon boots or resets.
- **Common ground is mandatory** — the 12 V supply ground and the Photon ground
  must be tied together, or the gate has no reference and switching is erratic.

Replicate this exact circuit on `D3` for the fogger + mist fan combo.

---

## Control logic

All thresholds live in the `CONFIG` block of `everfresh.ino`. Each actuator has a
**deadband** so it can't rapidly toggle, plus **minimum on/off timers** to protect
the equipment.

### Heater

| Condition          | Action          |
|--------------------|-----------------|
| Temp `< 76 °F`     | Heat **ON**     |
| Temp `> 78.5 °F`   | Heat **OFF**    |
| In between         | Hold last state |

Heats toward the *middle* of the band so it isn't constantly chasing the 75 °F
floor.

### Fogger (dual-purpose: humidity **or** cooling)

The fogger turns on if **either** demand is true, subject to a hard ceiling:

- **Humidity demand:** RH `< 55 %` → on; RH `> 65 %` → off.
- **Cooling demand:** Temp `> 84 °F` → on; Temp `< 82 °F` → off.
- **Hard ceiling:** if RH `≥ 78 %`, the fogger is forced **OFF** regardless —
  cooling must never push humidity out of range.

> **The unavoidable conflict:** when it's both *too hot* and *too humid*, no
> actuator can fix both — fogging would cool but over-humidify. In that case the
> fogger stays off and the airflow fan takes over (below).

### Airflow fan (variable speed)

Baseline always-on, ramped up on demand (priority order):

| Trigger                                   | Speed                                  |
|-------------------------------------------|----------------------------------------|
| Default                                   | `FAN_MIN_DUTY` (50 %)                   |
| RH above `RH_FOG_OFF` (65 %)              | Ramps **linearly** 50 % → 100 % as RH approaches the 78 % ceiling |
| Too hot **and** RH at ceiling             | 100 % (only remaining cooling/venting tool) |
| Canopy/trunk temp gap `> 4 °F`            | At least 70 % (destratify / mix air)   |

The fan only ramps for venting when RH is *above* the fog-off point — below that
it stays at baseline so it doesn't blow away humidity the fogger is building.

---

## Safety

The firmware is written to fail safe — losing a sensor or wedging the bus must
never leave a 120 VAC heater running blind in a wet box.

- **No blind heating.** If *both* sensors fail to return a valid reading (NaN or
  out-of-range), the heater and fogger are forced OFF and an `ALARM` status is
  published. The fan stays at baseline.
- **Hard thermal cutoff.** If *either* sensor reads `≥ 92 °F` (`TEMP_SAFETY_F`),
  the heater is killed immediately, independent of the normal control band.
- **Application watchdog.** If `loop()` hangs for 60 s (e.g. a stuck I²C read),
  the board resets itself.
- **Known-safe boot state.** All relays/MOSFETs are driven OFF before anything
  else in `setup()`; the gate pull-downs hold the fans off during the reset
  window before firmware runs.
- **Minimum on/off timers.** Heater 60 s, fogger 20 s — prevents chatter and
  thermal/relay stress.

**Not yet handled (see TODO):** the ultrasonic fogger has *no dry-run
protection*. If the reservoir empties, the fogger transducer can overheat and
fail. Add a float switch before leaving this unattended.

---

## Configuration reference

Everything tunable is at the top of `everfresh.ino`:

| Constant            | Default   | Meaning                                              |
|---------------------|-----------|------------------------------------------------------|
| `RELAY_ACTIVE_LOW`  | `false`   | `false` = active-HIGH (SSR/MOSFET). `true` for cheap blue relay boards. |
| `PIN_HEAT/FOG/CIRC` | D2/D3/A5  | Output pins                                          |
| `HEAT_ON_F`         | `76.0`    | Heater on below this                                 |
| `HEAT_OFF_F`        | `78.5`    | Heater off above this                                |
| `COOL_ON_F`         | `84.0`    | Fog-for-cooling on above this                        |
| `COOL_OFF_F`        | `82.0`    | Fog-for-cooling off below this                       |
| `TEMP_SAFETY_F`     | `92.0`    | Either sensor above this → heater hard OFF           |
| `RH_FOG_ON`         | `55.0`    | Fog-for-humidity on below this                       |
| `RH_FOG_OFF`        | `65.0`    | Fog-for-humidity off above this                      |
| `RH_CEILING`        | `78.0`    | Never fog above this; fan venting target             |
| `FAN_MIN_DUTY`      | `50`      | Always-on baseline fan speed (% of supply voltage)   |
| `FAN_MAX_DUTY`      | `100`     | Max fan speed                                        |
| `FAN_PWM_FREQ`      | `25000`   | PWM frequency (Hz) — above hearing range             |
| `DESTRAT_GAP_F`     | `4.0`     | Canopy/trunk gap that triggers a mixing boost        |
| `HEAT_MIN_ON/OFF`   | `60000`   | Heater min on/off time (ms)                          |
| `FOG_MIN_ON/OFF`    | `20000`   | Fogger min on/off time (ms)                          |
| `CONTROL_INTERVAL`  | `3000`    | Control loop period (ms)                             |
| `PUBLISH_INTERVAL`  | `60000`   | Cloud publish period (ms)                            |

---

## Build & flash

Using the [Particle CLI](https://docs.particle.io/getting-started/developer-tools/cli/):

```bash
# compile + flash over USB (or over-the-air if your device is online)
particle flash <device-name> everfresh.ino
```

Or paste `everfresh.ino` into the [Particle Web IDE](https://build.particle.io)
and flash from there.

**No external libraries required** — the SHT31 sensor driver is inlined in the
firmware (it talks to the sensors directly over `Wire`/I²C), so there's nothing
to add in the Libraries panel.

---

## Remote monitoring

Because it's a Particle device, live state is available with no extra
infrastructure. Via the [Particle Console](https://console.particle.io), mobile
app, or CLI:

```bash
particle get <device-name> status      # e.g. "T=78.4F RH=61% | heat=off fog=off fan=50%"
particle get <device-name> canopyTempF
particle get <device-name> fanDuty
```

**Cloud variables:** `canopyTempF`, `canopyRH`, `trunkTempF`, `trunkRH`,
`fanDuty`, `heat` (0/1), `fog` (0/1), `mode` (`auto`/`manual`), `status`. A sensor
reading of `-1` means that sensor is currently invalid.

**Published events:**

| Event                   | When                              | Payload                          |
|-------------------------|-----------------------------------|----------------------------------|
| `everfresh/telemetry`   | every 60 s                        | JSON snapshot (see below)        |
| `everfresh/alert`       | only when alert state *changes*   | `no-sensor` / `overheat` / `cleared` |
| `everfresh/cmd`         | when a manual override is invoked | what was commanded               |

Telemetry JSON (compact, under the 255-byte event limit):

```json
{"ct":78.4,"crh":61,"tt":77.9,"trh":63,"heat":0,"fog":0,"fan":50,"mode":"auto"}
```

Keys: `ct`/`crh` = canopy temp/RH, `tt`/`trh` = trunk temp/RH, `heat`/`fog` =
actuator state (0/1), `fan` = duty %, `mode` = auto or manual. Hook
`everfresh/alert` to a webhook / IFTTT / email so you're notified the moment a
sensor drops out or it overheats — these fire on the *transition*, not on a timer.

### Manual override functions (testing & maintenance)

Force an actuator for a bounded window; it auto-reverts to automatic control when
the window expires (default 60 s, hard-capped at 600 s). Call from CLI or the app:

```bash
particle call <device-name> runFogger "30"     # fogger ON for 30 s
particle call <device-name> runHeater "20"     # heater ON for 20 s (overheat cutoff still applies)
particle call <device-name> setFan "80,120"    # fan to 80% for 120 s  (or just "80" for default window)
particle call <device-name> clearOverrides ""  # cancel all overrides now, back to auto
```

While any override is active, `mode` reads `manual`. Safety always wins: the
overheat cutoff and the no-valid-sensor lockout still force the heater off
regardless of an override (the one exception: a manual `runHeater` is allowed to
fire with no sensor present, for bench-testing the SSR — but overheat still kills
it if a sensor *is* reading hot).

---

## Tuning guide

Start here after first power-on:

1. **Fan baseline.** Watch the fan at `FAN_MIN_DUTY = 50`. If it stalls or won't
   start, raise it (e.g. 60–70). If it's louder/faster than you want at idle,
   lower it. Remember duty ≈ fraction of supply voltage (50 % of 12 V ≈ 6 V).
2. **Relay polarity.** If the heater/fogger turn on when they should be off, flip
   `RELAY_ACTIVE_LOW`.
3. **Verify the safety cutoff** by temporarily lowering `TEMP_SAFETY_F` below
   room temp and confirming the heater refuses to run. Then restore it.
4. **Watch a full cycle** in the console for a day. If the heater cycles too
   often, widen its deadband (lower `HEAT_ON_F` or raise `HEAT_OFF_F`). Same idea
   for the fogger via `RH_FOG_ON/OFF`.
5. **Control point.** Currently canopy-primary. To average both sensors or
   control off the trunk instead, change `readSensors()`.

---

## Open items / TODO

- [ ] **Fogger dry-run protection** — add a reservoir float switch on a GPIO and
      block fogging when water is low (transducer burnout risk).
- [ ] **Confirm A5 is PWM-capable** on the exact board being used.
- [ ] **Finalize switch hardware** — SSR for heater, 2× logic-level MOSFETs for
      the DC loads (2-wire fan confirmed).
- [ ] **Alerting** — wire the `everfresh` publish to a notification on `ALARM` /
      `OVERHEAT`.
- [ ] Consider a min/max RH and temp **data log** (Particle integration → sheet)
      to dial in setpoints over the plant's first season.
```
