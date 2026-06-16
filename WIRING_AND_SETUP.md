# EVERFRESH — Wiring & Setup Guide

Step-by-step bring-up for the greenhouse controller. The guiding principle is
**incremental bring-up**: prove each subsystem in isolation, lowest-voltage and
safest first, with **120 VAC mains absolutely last**. Don't wire everything at
once and hope — bring up one stage, confirm it on the Particle cloud, then move on.

> Full circuit diagrams (SSR, the 2-wire fan MOSFET schematic, BOM) live in
> [README.md](README.md). This guide is the *order of operations*.

---

## Prerequisites

- Firmware flashed (`particle flash <device-name> everfresh.ino`) — already done.
- Device online (breathing cyan) and claimed to your Particle account.
- [Particle CLI](https://docs.particle.io/getting-started/developer-tools/cli/)
  installed, or the [Console](https://console.particle.io) open in a browser.

Throughout, `<device-name>` is your Photon's name in the Particle console.

---

## Step 0 — Confirm the firmware is alive (zero wiring)

The board is already running the control loop. With nothing wired yet, check it:

```bash
particle get <device-name> status
```

Expected output:

```
ALARM: no valid sensor — heat/fog OFF
```

This is **correct and expected**. Both SHT31 reads fail, `sensorsValid` goes
false, and the safety path fires — heater and fogger forced off, fan at baseline.
Seeing this confirms the control loop, cloud variables, and fail-safe logic all
work *before any hardware exists*.

✅ **Pass:** status shows the `ALARM` line.
❌ **Fail:** device not online → resolve connectivity first (it should breathe
cyan; re-run `particle setup` / Wi-Fi credentials if not).

---

## Step 1 — Sensors (3.3 V, I²C — safe on the breadboard)

The sensors are the only subsystem that belongs on a breadboard long-term.
Bring up **one sensor first.**

### Wire the canopy sensor (0x44)

| SHT31 pin | Photon pin |
|-----------|------------|
| VIN       | 3V3        |
| GND       | GND        |
| SDA       | D0         |
| SCL       | D1         |
| **ADDR**  | **GND**  → address `0x44` |

If your breakout has **no** onboard I²C pull-ups, add **4.7 kΩ** from SDA→3V3 and
SCL→3V3 (most breakouts already include them).

### Verify

```bash
particle get <device-name> canopyTempF    # should read room temp in °F
particle get <device-name> status         # ALARM gone; shows T=..F RH=..% fan=50%
```

Breathe on the sensor — RH should jump within a few seconds.

### Add the trunk sensor (0x45)

Identical wiring, but **`ADDR → 3V3`** (= `0x45`). Both share the same SDA/SCL bus.

```bash
particle get <device-name> trunkTempF      # should read room temp
```

✅ **Pass:** both `canopyTempF` and `trunkTempF` read plausible room temperature.
❌ **A sensor reads `-1`:** it's failing to ack or failing CRC. Usual causes:
wiring, wrong/duplicate ADDR strap (two sensors on the same address collide), or
missing pull-ups.

---

## Step 2 — Airflow fan (12 V, variable speed via MOSFET)

Build the **low-side MOSFET circuit** from [README.md](README.md#2-wire-dc-fan--fogger-low-side-mosfet--pwm),
driven from **`A5`**.

Critical wiring rules:

- **Separate 12 V supply** for the fan — not the Photon's 3V3/5V.
- **Common ground:** the 12 V supply ground **must** tie to the Photon ground, or
  the MOSFET gate has no reference and switching is erratic.
- **Logic-level** N-channel MOSFET (e.g. IRLZ44N) — fully on at 3.3 V gate.
- **Flyback diode** (1N5819) across the fan; **220 Ω** gate series resistor;
  **10 kΩ** gate pull-down.
- Keep the fan **current** off the breadboard (terminal blocks / soldered). Only
  the gate signal belongs on the breadboard.

### Verify

- On boot/reset you'll see the **800 ms full-speed kick**, then it settles to
  `FAN_MIN_DUTY` (50 %).
- To watch it ramp: lightly cover or breathe on the canopy sensor to push RH
  above 65 %, and confirm the fan speeds up:

```bash
particle get <device-name> fanDuty          # climbs toward 100 as RH rises
```

✅ **Pass:** fan spins at baseline, kicks on boot, and `fanDuty` tracks RH.
❌ **Fan won't start at baseline:** raise `FAN_MIN_DUTY` (stiction); confirm the
MOSFET is logic-level and grounds are common.

---

## Step 3 — Fogger + mist fan combo (12 V, on/off)

Same MOSFET circuit as Step 2, but driven from **`D3`** (full on/off, no PWM).

⚠️ **Put water in the reservoir before energizing.** A dry ultrasonic transducer
can overheat and fail in seconds. There is no dry-run protection in firmware yet.

### Verify

Let RH fall below 55 % (or temporarily raise `RH_FOG_ON` in the config and
reflash), and confirm the fogger switches on:

```bash
particle get <device-name> status            # should show fog=ON when RH is low
```

✅ **Pass:** fogger runs when humidity demand exists, stops above `RH_FOG_OFF`.

---

## Step 4 — Heater + SSR (120 VAC — LAST, deliberately)

⚠️ **120 VAC is dangerous.** Do not rush this stage. Mains never touches the
breadboard.

### 4a — Prove the DC side first (no mains connected)

Wire only the SSR's **DC input**:

- SSR DC `+` → `D2`
- SSR DC `−` → GND

Force a heat call (temporarily set `HEAT_ON_F` high, e.g. `120.0`, and reflash),
then verify with a multimeter that the SSR's DC input toggles, or watch the SSR's
indicator LED:

```bash
particle get <device-name> status            # should show heat=ON
```

Restore `HEAT_ON_F` to `76.0` afterward.

### 4b — Connect the AC side (only after 4a passes)

- Switch the **hot (live)** conductor only, in series through the SSR's AC terminals.
- Keep all mains wiring enclosed, strain-relieved, and never worked on live.
- Heat-sink the SSR if it runs near its current rating.

✅ **Pass:** heater energizes on a heat call and de-energizes when satisfied; the
hard cutoff (`TEMP_SAFETY_F = 92 °F`) kills it if either sensor reads too hot.

---

## Bench-testing tip: setpoint gaming vs. manual override

During wiring you'll want to force actuators on/off without fooling a sensor or
constantly editing setpoints. Two options:

1. **Temporarily edit setpoints** in the config and reflash (simple, but slow).
2. **Manual override cloud function** (if added to the firmware): toggle any
   actuator from CLI/phone with an auto-expiring timeout that reverts to automatic
   control. e.g. `particle call <device-name> set "heat=on"`.

If the override function isn't in your build yet, ask to have it added — it makes
this whole bring-up phase much faster and is worth keeping for future maintenance.

---

## Quick reference — cloud variables

| Variable      | Meaning                                   |
|---------------|-------------------------------------------|
| `status`      | One-line summary string                   |
| `canopyTempF` | Canopy temperature °F (`-1` = invalid)    |
| `canopyRH`    | Canopy humidity % (`-1` = invalid)        |
| `trunkTempF`  | Trunk temperature °F                      |
| `trunkRH`     | Trunk humidity %                          |
| `fanDuty`     | Current airflow fan speed %               |

```bash
particle get <device-name> <variable>
particle monitor <device-name> status        # poll continuously (CLI helper)
```

---

## Bring-up checklist

- [ ] Step 0 — `status` shows `ALARM: no valid sensor` (firmware alive)
- [ ] Step 1 — canopy sensor (0x44) reads room temp
- [ ] Step 1 — trunk sensor (0x45) reads room temp
- [ ] Step 2 — fan spins, boots with kick, `fanDuty` tracks RH
- [ ] Step 3 — fogger runs on low-RH demand (water in reservoir!)
- [ ] Step 4a — SSR DC side toggles on heat call
- [ ] Step 4b — heater AC side switched (hot leg, enclosed)
- [ ] Verified hard thermal cutoff behavior
- [ ] (optional) manual override function added for maintenance
