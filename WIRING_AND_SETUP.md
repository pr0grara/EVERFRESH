# EVERFRESH — Wiring & Setup Guide

Step-by-step bring-up. Guiding principle: **incremental bring-up** — prove each
subsystem in isolation, lowest-voltage and safest first, with **120 VAC mains
absolutely last**. Bring up one stage, confirm it on the Particle cloud, move on.

> Full circuit diagrams, BOM, and pin map live in [README.md](README.md). This
> guide is the *order of operations*. Data logging: [LOGGING.md](LOGGING.md).

---

## Prerequisites

- Firmware flashed (`particle flash <device-name> everfresh.ino`).
- Device online (breathing cyan), claimed to your Particle account.
- [Particle CLI](https://docs.particle.io/getting-started/developer-tools/cli/) or
  the [Console](https://console.particle.io). `<device-name>` is your Photon's name.

**Pin map** (see README for wiring detail):

| Pin | Drives |
|-----|--------|
| D0/D1 | I²C — canopy (0x44) + ambient (0x45) SHT31 |
| D2 | heater SSR |
| D3 | fogger MOSFET (24 V) |
| A5 | circ fan PWM (4-wire) |
| D4 | circ fan power MOSFET (true off) |
| A4 | vent fan MOSFET (2-wire) |

---

## Step 0 — Confirm the firmware is alive (zero wiring)

```bash
particle get <device-name> status
```
Expected (nothing wired yet):
```
ALARM no-sensor | heat=off fog=off circ=0% vent=0% [auto]
```
✅ The control loop and fail-safe work before any hardware exists.
❌ No response → device offline; fix connectivity (should breathe cyan).

---

## Step 1 — Sensors (3.3 V, I²C — safe on the breadboard)

Both SHT31s share the D0/D1 bus; they differ only by the **ADDR strap**.

| SHT31 | VIN | GND | SDA | SCL | ADDR | Address |
|-------|-----|-----|-----|-----|------|---------|
| canopy | 3V3 | GND | D0 | D1 | **GND** | 0x44 |
| ambient | 3V3 | GND | D0 | D1 | **3V3** | 0x45 |

⚠️ Power from **3V3**, not VIN (pull-ups must reference 3.3 V). Add 4.7 kΩ SDA/SCL→3V3
only if your breakout lacks them.

```bash
particle get <device-name> canopyTempF    # room temp °F
particle get <device-name> ambientTempF   # room temp °F
particle get <device-name> status         # ALARM gone: T=..F RH=..% VPD=.. | ...
```
Breathe on the canopy sensor — RH jumps within seconds.

✅ both read plausible room temp; `status` no longer shows `ALARM`.
❌ a sensor reads `-1` → wiring, wrong ADDR strap, or missing pull-ups.

---

## Step 2 — Fans (12 V)

Two fans, each on a low-side MOSFET (common ground to the Photon, fan current off
the breadboard). Test each with its override:

```bash
particle call <device-name> setCirc "100"   # circ fan full
particle call <device-name> setCirc "0"      # circ fan OFF
particle call <device-name> setVent "100"    # vent fan ON
particle call <device-name> setVent "0"      # vent fan OFF
particle call <device-name> clearOverrides ""
```

**Circ fan (4-wire):** PWM wire → A5, power MOSFET → D4. **`setCirc "0"` must
*dead-stop* it** (the D4 MOSFET cuts power). If it idles at min instead, the fan is
back-feeding through a signal wire — **cut and insulate the tach wire** (the firmware
already releases the PWM pin to Hi-Z when off to handle the PWM-wire path).

**Vent fan (2-wire):** MOSFET → A4, on/off only (full 12 V).

✅ both fans run at `"100"` and fully stop at `"0"`.
❌ won't spin → MOSFET not logic-level, or grounds not common.

---

## Step 3 — Fogger (24 V, on/off via MOSFET on D3)

⚠️ **Water in the reservoir before energizing** — a dry ultrasonic transducer dies
in seconds (no dry-run protection yet).

```bash
particle call <device-name> setFog "1"     # fogger ON (circ fan auto-runs with it)
particle call <device-name> setFog "0"     # fogger OFF
particle call <device-name> clearOverrides ""
```

✅ fogger fires on `setFog "1"`, stops on `"0"`, and the circ fan runs alongside it.

---

## Step 4 — Heater + SSR (120 VAC — LAST)

⚠️ **120 VAC is dangerous.** Mains never touches the breadboard.

**4a — DC side first (no mains):** wire SSR `DC+ → D2`, `DC− → GND`. Force it and
watch the SSR's LED / meter the DC side:
```bash
particle call <device-name> setHeat "1"    # heater ON
particle call <device-name> setHeat "0"    # heater OFF
```
**4b — AC side (only after 4a):** switch the **hot** conductor through the SSR's AC
terminals, fully enclosed and strain-relieved. Heat-sink if near current rating.

✅ heater toggles on `setHeat`; the `TEMP_SAFETY_F` cutoff kills it if the canopy
reads too hot.

---

## Bench-testing — manual override API

All overrides **auto-revert** to auto control (default 60 s, max 600 s). Convention:
**`0 = off`, nonzero = on**.

```bash
particle call <device-name> setFog  "1"      # fogger on  ("1,30" = 30s; "0" = off)
particle call <device-name> setHeat "0"      # heater off
particle call <device-name> setCirc "60"     # circ fan 60%   (0 = true off)
particle call <device-name> setVent "100"    # vent fan on
particle call <device-name> clearOverrides "" # cancel all, back to auto
particle get  <device-name> mode             # "manual" while any override active
```

**Safety still applies:** the overheat cutoff and no-valid-sensor lockout force the
heater off regardless. `setHeat "1"` is allowed with no sensor (to bench-test the SSR),
but overheat still wins if a sensor reads hot.

---

## Quick reference

### Cloud variables (`particle get <device-name> <variable>`)

| Variable | Meaning |
|----------|---------|
| `status` | one-line summary |
| `canopyTempF` / `canopyRH` / `vpd` | canopy temp °F / RH % / VPD kPa (`-1` = invalid) |
| `ambientTempF` / `ambientRH` / `ambientVPD` | ambient (reference) |
| `circ` / `vent` | fan duty % |
| `heat` / `fog` | actuator state (0/1) |
| `mode` | `auto` / `manual` |

### Cloud functions (`particle call <device-name> <fn> "<arg>"`)

| Function | Arg | Effect |
|----------|-----|--------|
| `setFog` | `1`/`0` [,sec] | fogger on/off |
| `setHeat` | `1`/`0` [,sec] | heater on/off |
| `setCirc` | `duty` [,sec] | circ fan % (0 = true off) |
| `setVent` | `duty` [,sec] | vent fan on/off |
| `clearOverrides` | — | back to auto |

### Events

| Event | When |
|-------|------|
| `everfresh/telemetry` | every 60 s — JSON (canopy + ambient + fans) |
| `everfresh/event` | actuator toggles (heat/fog/circ/vent) |
| `everfresh/alert` | alarm state change |
| `everfresh/cmd` | manual override |

---

## Bring-up checklist

- [ ] Step 0 — `status` shows `ALARM no-sensor` (firmware alive)
- [ ] Step 1 — canopy (0x44) **and** ambient (0x45) read room temp
- [ ] Step 2 — circ fan: `setCirc "100"` runs, `"0"` **dead-stops** (true off)
- [ ] Step 2 — vent fan: `setVent "100"`/`"0"` on/off
- [ ] Step 3 — fogger fires on `setFog "1"` (water in reservoir!)
- [ ] Step 4a — SSR DC side toggles on `setHeat "1"`
- [ ] Step 4b — heater AC side switched (hot leg, enclosed)
- [ ] Verified hard thermal cutoff (`TEMP_SAFETY_F`)
- [ ] Tach wire on the 4-wire circ fan cut/insulated
