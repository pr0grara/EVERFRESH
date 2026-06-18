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
false, and the safety path fires — heater and fogger forced off, fan on its
hourly-minimum ventilation only.
Seeing this confirms the control loop, cloud variables, and fail-safe logic all
work *before any hardware exists*.

✅ **Pass:** status shows the `ALARM` line.
❌ **Fail:** device not online → resolve connectivity first (it should breathe
cyan; re-run `particle setup` / Wi-Fi credentials if not).

---

## Step 1 — Sensor (3.3 V, I²C — safe on the breadboard)

The single canopy SHT31 is the only subsystem that belongs on a breadboard
long-term.

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
particle get <device-name> status         # ALARM gone; shows T=..F RH=..% fan=0% (off below 77F)
```

Breathe on the sensor — RH should jump within a few seconds.

✅ **Pass:** `canopyTempF` reads plausible room temperature and `status` no longer
shows `ALARM`.
❌ **`canopyTempF` reads `-1`:** the sensor is failing to ack or failing CRC. Usual
causes: wiring, the ADDR pin not strapped low (must be `0x44`), or missing pull-ups.

---

## Step 2 — Airflow fan (12 V, ON/OFF via MOSFET)

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

The fan is ON/OFF for now (runs at full 12 V). The simplest test is to force it:

```bash
particle call <device-name> setFan "100"    # fan ON
particle get  <device-name> fanDuty          # reads 100
particle call <device-name> setFan "0"       # fan OFF
particle call <device-name> clearOverrides "" # back to auto (temp-driven)
```

In automatic mode it turns on when the canopy is above 77 °F, and is forced on for
≥ 5 minutes each hour regardless of temp — so even in a cool room you'll see it run
a few minutes at the top of the hour.

✅ **Pass:** fan runs full-speed on `setFan "100"` and stops on `"0"`.
❌ **Fan won't spin at all:** confirm the MOSFET is logic-level, grounds are common,
and the PWM frequency is under the board's ceiling (`FAN_PWM_FREQ`).

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
hard cutoff (`TEMP_SAFETY_F = 92 °F`) kills it if the sensor reads too hot.

---

## Bench-testing with manual override functions

You don't need to fool a sensor or edit setpoints to test an actuator. The
firmware exposes cloud functions that force an actuator for a bounded window, then
auto-revert to automatic control (default 60 s, capped at 600 s):

```bash
particle call <device-name> runFogger "30"     # fogger ON for 30 s
particle call <device-name> runHeater "20"     # heater ON for 20 s
particle call <device-name> setFan "80,120"    # fan to 80% for 120 s
particle call <device-name> clearOverrides ""  # cancel everything, back to auto
```

These are perfect for each bring-up step: `setFan "100"` to confirm the fan
circuit, `runFogger "10"` to confirm the fogger MOSFET, `runHeater "10"` to watch
the SSR's DC side toggle in Step 4a. Watch the result on `status` or `mode`:

```bash
particle get <device-name> mode      # reads "manual" while an override is active
particle get <device-name> status
```

**Safety still applies during overrides:** the overheat cutoff and the
no-valid-sensor lockout will still force the heater off. The one exception —
`runHeater` is allowed to fire even with no sensor wired, specifically so you can
bench-test the SSR in Step 4a — but if a sensor *is* reading hot, overheat wins.

---

## Quick reference

### Cloud variables (`particle get <device-name> <variable>`)

| Variable      | Meaning                                   |
|---------------|-------------------------------------------|
| `status`      | One-line summary string                   |
| `canopyTempF` | Canopy temperature °F (`-1` = invalid)    |
| `canopyRH`    | Canopy humidity % (`-1` = invalid)        |
| `fanDuty`     | Current airflow fan speed %               |
| `heat` / `fog`| Actuator state (0/1)                      |
| `mode`        | `auto` or `manual` (override active)      |

### Cloud functions (`particle call <device-name> <fn> "<arg>"`)

| Function          | Arg               | Effect                                  |
|-------------------|-------------------|-----------------------------------------|
| `runFogger`       | seconds           | Force fogger ON for N s                  |
| `runHeater`       | seconds           | Force heater ON for N s                  |
| `setFan`          | `duty` / `duty,s` | Hold fan at duty% for N s               |
| `clearOverrides`  | (ignored)         | Cancel all overrides, return to auto    |

### Events (subscribe / webhook on these)

| Event                 | When                            |
|-----------------------|---------------------------------|
| `everfresh/telemetry` | every 60 s — JSON snapshot      |
| `everfresh/alert`     | on alert state change           |
| `everfresh/cmd`       | on manual override              |

---

## Bring-up checklist

- [ ] Step 0 — `status` shows `ALARM: no valid sensor` (firmware alive)
- [ ] Step 1 — canopy sensor (0x44) reads room temp
- [ ] Step 2 — fan runs on `setFan "100"`, stops on `"0"`, auto-on above 77 °F
- [ ] Step 3 — fogger runs on low-RH demand (water in reservoir!)
- [ ] Step 4a — SSR DC side toggles on heat call
- [ ] Step 4b — heater AC side switched (hot leg, enclosed)
- [ ] Verified hard thermal cutoff behavior
- [ ] (optional) manual override function added for maintenance
