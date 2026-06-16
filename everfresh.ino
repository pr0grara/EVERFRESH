/*
 * EVERFRESH — Cojoba Angustifolia greenhouse controller
 * Target: Particle Photon 2  (also compiles on original Photon / Argon)
 *
 * Sensors : 2x SHT31 over I2C   (canopy @ 0x44, trunk @ 0x45)
 * Actuators:
 *   HEAT  — 120VAC aquarium heater OR heat mat, via SSR/relay
 *   FOG   — 12VDC ultrasonic fogger + mist fan combo (does humidity AND cooling)
 *   CIRC  — 12VDC airflow fan @ 6V, runs 24/7 (MCU control optional)
 *
 * Control strategy: hysteresis (deadband) bang-bang control with
 * minimum on/off timers to protect equipment and stop relay chatter.
 * The fogger is dual-purpose, so demand for HUMIDITY and demand for
 * COOLING are arbitrated against a hard humidity ceiling.
 *
 * SAFETY: the heater is NEVER energized without a valid temperature
 * reading, and is force-OFF if either sensor reads dangerously hot.
 */

#include <math.h>   // NAN, isnan(), fabs()

// No external libraries needed — the SHT31 driver is inlined below (uses Wire).

// ========================= CONFIG (edit me) =========================

// --- Switching polarity ---
// Most cheap blue relay boards are ACTIVE-LOW (pin LOW = relay ON).
// Solid-state relays (SSR) and MOSFET drivers are usually ACTIVE-HIGH.
// Flip this one flag to match your hardware.
const bool RELAY_ACTIVE_LOW = false;   // false = active-HIGH (SSR/MOSFET)

// --- Pins ---  (Photon digital pins)
const int PIN_HEAT = D2;   // 120VAC heater  (USE AN SSR — relays wear out fast)
const int PIN_FOG  = D3;   // 12VDC fogger + mist fan combo

// --- Airflow fan: VARIABLE SPEED via PWM ---
// PIN_CIRC must be a PWM-capable pin on YOUR board (original Photon: A4/A5/WKP/D0-D3;
// Photon 2: check the datasheet — A5 is a safe bet on both).
// Wiring options:
//   2-wire 12V fan  -> drive a logic-level MOSFET low-side, PWM the gate from this pin.
//   4-wire PC fan   -> power the fan at 12V, PWM its dedicated control wire from this pin.
const int PIN_CIRC      = A5;
const int FAN_PWM_FREQ  = 25000;  // 25kHz: above hearing range, no audible fan whine
// Duty is % of supply voltage. From a 12V supply: ~50% ≈ 6V average (your current baseline).
// Tune FAN_MIN_DUTY until the fan reliably spins and matches your "min viable" airflow.
const int FAN_MIN_DUTY  = 50;     // % — always-on baseline (never drops below this)
const int FAN_MAX_DUTY  = 100;    // % — full speed for venting / destratifying
const float DESTRAT_GAP_F = 4.0;  // canopy/trunk temp gap that triggers a mixing boost

// --- Setpoints (degrees F) ---  target band: 75–85F
const float HEAT_ON_F   = 76.0;   // heater turns ON below this
const float HEAT_OFF_F  = 78.5;   // heater turns OFF above this  (hysteresis)
const float COOL_ON_F   = 84.0;   // fog-for-cooling turns ON above this
const float COOL_OFF_F  = 82.0;   // fog-for-cooling turns OFF below this
const float TEMP_SAFETY_F = 92.0; // ANY sensor above this => heater hard OFF

// --- Setpoints (percent RH) ---  target band: 50–80%
const float RH_FOG_ON   = 55.0;   // fog-for-humidity turns ON below this
const float RH_FOG_OFF  = 65.0;   // fog-for-humidity turns OFF above this
const float RH_CEILING  = 78.0;   // NEVER fog above this, even to cool

// --- Minimum on/off times (ms) — protect gear, stop chatter ---
const unsigned long HEAT_MIN_ON   = 60000;  // 60s
const unsigned long HEAT_MIN_OFF  = 60000;  // 60s
const unsigned long FOG_MIN_ON    = 20000;  // 20s
const unsigned long FOG_MIN_OFF   = 20000;  // 20s

// --- Loop / reporting cadence ---
const unsigned long CONTROL_INTERVAL = 3000;    // run control every 3s
const unsigned long PUBLISH_INTERVAL = 60000;   // cloud publish every 60s

// ====================================================================

// I2C addresses (ADDR pin: low = 0x44, high = 0x45)
const uint8_t ADDR_CANOPY = 0x44;
const uint8_t ADDR_TRUNK  = 0x45;

// Sensor state
struct Reading { float tempF; float rh; bool ok; };
Reading canopy = {NAN, NAN, false};
Reading trunk  = {NAN, NAN, false};

// Control-point values actually used for decisions
float ctrlTempF = NAN;
float ctrlRH    = NAN;
bool  sensorsValid = false;

// Actuator state + timers
bool heatOn = false, fogOn = false;
unsigned long heatLastChange = 0, fogLastChange = 0;
unsigned long lastControl = 0, lastPublish = 0;

int fanCurrentDuty = FAN_MIN_DUTY;

// Cloud-exposed strings/numbers (Particle dashboard)
double cloudCanopyT = 0, cloudCanopyRH = 0, cloudTrunkT = 0, cloudTrunkRH = 0;
double cloudFanDuty = 0;
char   cloudStatus[160] = "boot";

// Application watchdog: if the loop hangs >60s, reset the device.
ApplicationWatchdog *wd;

// -------------------- helpers --------------------

float cToF(float c) { return c * 9.0 / 5.0 + 32.0; }

void writeRelay(int pin, bool on) {
  // Translate logical ON/OFF into the correct electrical level.
  bool level = RELAY_ACTIVE_LOW ? !on : on;
  digitalWrite(pin, level ? HIGH : LOW);
}

void writeFan(int dutyPct) {
  // Map 0–100% to the Photon's 8-bit PWM range at an inaudible frequency.
  if (dutyPct < 0)   dutyPct = 0;
  if (dutyPct > 100) dutyPct = 100;
  fanCurrentDuty = dutyPct;
  cloudFanDuty   = dutyPct;
  analogWrite(PIN_CIRC, map(dutyPct, 0, 100, 0, 255), FAN_PWM_FREQ);
}

bool valid(float t, float h) {
  // SHT31 returns NAN on read failure; also sanity-bound the values.
  if (isnan(t) || isnan(h)) return false;
  if (t < -40 || t > 257)  return false;   // sensor's own range, in F
  if (h < 0  || h > 100)   return false;
  return true;
}

// -------------------- sensors (inlined SHT31 driver) --------------------

// SHT31 CRC-8: polynomial 0x31, init 0xFF, over the 2 data bytes.
uint8_t sht31Crc(const uint8_t *data) {
  uint8_t crc = 0xFF;
  for (int i = 0; i < 2; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

// Single-shot, high-repeatability, no clock stretching (cmd 0x2400).
// Returns true and fills tempC/rh on a good read with valid CRCs.
bool sht31Read(uint8_t addr, float &tempC, float &rh) {
  Wire.beginTransmission(addr);
  Wire.write(0x24);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;

  delay(20);  // high-repeatability conversion ~15ms

  if (Wire.requestFrom(addr, (uint8_t)6) != 6) return false;
  uint8_t d[6];
  for (int i = 0; i < 6; i++) d[i] = Wire.read();

  if (sht31Crc(&d[0]) != d[2]) return false;   // temp CRC
  if (sht31Crc(&d[3]) != d[5]) return false;   // humidity CRC

  uint16_t rawT = (d[0] << 8) | d[1];
  uint16_t rawH = (d[3] << 8) | d[4];
  tempC = -45.0 + 175.0 * ((float)rawT / 65535.0);
  rh    = 100.0 * ((float)rawH / 65535.0);
  return true;
}

void readSensors() {
  float c, h;

  if (sht31Read(ADDR_CANOPY, c, h)) { float f = cToF(c); canopy = { f, h, valid(f, h) }; }
  else                              { canopy = { NAN, NAN, false }; }

  if (sht31Read(ADDR_TRUNK, c, h))  { float f = cToF(c); trunk = { f, h, valid(f, h) }; }
  else                              { trunk = { NAN, NAN, false }; }

  // Control point: prefer canopy (where leaves transpire); fall back to trunk.
  if (canopy.ok)      { ctrlTempF = canopy.tempF; ctrlRH = canopy.rh; sensorsValid = true; }
  else if (trunk.ok)  { ctrlTempF = trunk.tempF;  ctrlRH = trunk.rh;  sensorsValid = true; }
  else                { sensorsValid = false; }

  // mirror to cloud variables
  cloudCanopyT  = canopy.ok ? canopy.tempF : -1;
  cloudCanopyRH = canopy.ok ? canopy.rh    : -1;
  cloudTrunkT   = trunk.ok  ? trunk.tempF  : -1;
  cloudTrunkRH  = trunk.ok  ? trunk.rh     : -1;
}

// Highest valid temp across both sensors — used for safety cutoff.
float hottestReading() {
  float h = -1000;
  if (canopy.ok && canopy.tempF > h) h = canopy.tempF;
  if (trunk.ok  && trunk.tempF  > h) h = trunk.tempF;
  return h;
}

// -------------------- control --------------------

// Returns the desired state, respecting hysteresis + minimum on/off timing.
bool applyHysteresis(bool current, bool wantOn,
                     unsigned long &lastChange,
                     unsigned long minOn, unsigned long minOff) {
  unsigned long since = millis() - lastChange;
  if (wantOn == current) return current;             // no change requested
  if (current && since < minOn)  return current;     // must stay ON longer
  if (!current && since < minOff) return current;    // must stay OFF longer
  lastChange = millis();
  return wantOn;
}

// Desired airflow fan speed (%). Baseline always-on, ramped up when the air
// needs to move — this fan is our ONLY active lever to shed excess humidity.
int fanDuty() {
  if (!sensorsValid) return FAN_MIN_DUTY;     // benign: keep air moving

  int duty = FAN_MIN_DUTY;

  // 1) Vent excess humidity. Ramp linearly from baseline (at RH_FOG_OFF, where
  //    we stop adding moisture) up to full speed at the RH ceiling. Below
  //    RH_FOG_OFF we leave it at baseline so we don't blow away humidity we want.
  if (ctrlRH > RH_FOG_OFF) {
    float frac = (ctrlRH - RH_FOG_OFF) / (RH_CEILING - RH_FOG_OFF);   // 0..1
    if (frac > 1.0) frac = 1.0;
    int ventDuty = FAN_MIN_DUTY + (int)(frac * (FAN_MAX_DUTY - FAN_MIN_DUTY));
    if (ventDuty > duty) duty = ventDuty;
  }

  // 2) Hot AND too humid to fog: the fan is the only cooling/venting tool left.
  if (ctrlTempF > COOL_ON_F && ctrlRH >= RH_CEILING) duty = FAN_MAX_DUTY;

  // 3) Destratify: large canopy/trunk temperature gap => bump airflow to mix.
  if (canopy.ok && trunk.ok && fabs(canopy.tempF - trunk.tempF) > DESTRAT_GAP_F) {
    if (duty < 70) duty = 70;
  }

  return duty;
}

void control() {
  // ---- SAFETY FIRST ----
  // No trustworthy temperature => kill heat and fog. Never heat blind.
  if (!sensorsValid) {
    heatOn = applyHysteresis(heatOn, false, heatLastChange, 0, 0);
    fogOn  = applyHysteresis(fogOn,  false, fogLastChange,  0, 0);
    writeRelay(PIN_HEAT, false);
    writeRelay(PIN_FOG,  false);
    writeFan(FAN_MIN_DUTY);   // keep baseline airflow even when blind
    strcpy(cloudStatus, "ALARM: no valid sensor — heat/fog OFF");
    return;
  }

  // Hard thermal cutoff regardless of control point.
  bool overheat = hottestReading() >= TEMP_SAFETY_F;

  // ---- HEAT demand (hysteresis around HEAT_ON/HEAT_OFF) ----
  bool wantHeat;
  if (overheat)               wantHeat = false;
  else if (!heatOn && ctrlTempF < HEAT_ON_F)  wantHeat = true;
  else if (heatOn  && ctrlTempF > HEAT_OFF_F) wantHeat = false;
  else                        wantHeat = heatOn;     // inside deadband: hold

  // ---- FOG demand: dual purpose (humidity OR cooling) ----
  // Reason A: humidity too low.
  bool wantHumidity;
  if (!fogOn && ctrlRH < RH_FOG_ON)  wantHumidity = true;
  else if (fogOn && ctrlRH > RH_FOG_OFF) wantHumidity = false;
  else wantHumidity = fogOn;

  // Reason B: too hot AND we have humidity headroom (evaporative cooling).
  bool wantCooling;
  if (ctrlTempF > COOL_ON_F)       wantCooling = true;
  else if (ctrlTempF < COOL_OFF_F) wantCooling = false;
  else                             wantCooling = fogOn;

  bool wantFog = (wantHumidity || wantCooling);
  // Hard humidity ceiling: never push RH out the top, even to cool.
  if (ctrlRH >= RH_CEILING) wantFog = false;

  // ---- apply with timing protection ----
  heatOn = applyHysteresis(heatOn, wantHeat, heatLastChange, HEAT_MIN_ON, HEAT_MIN_OFF);
  fogOn  = applyHysteresis(fogOn,  wantFog,  fogLastChange,  FOG_MIN_ON,  FOG_MIN_OFF);

  int duty = fanDuty();

  writeRelay(PIN_HEAT, heatOn);
  writeRelay(PIN_FOG,  fogOn);
  writeFan(duty);

  // ---- status string for the cloud ----
  snprintf(cloudStatus, sizeof(cloudStatus),
           "T=%.1fF RH=%.0f%% | heat=%s fog=%s fan=%d%%%s%s",
           ctrlTempF, ctrlRH,
           heatOn ? "ON" : "off",
           fogOn  ? "ON" : "off",
           duty,
           overheat ? " [OVERHEAT]" : "",
           (ctrlRH >= RH_CEILING) ? " [RH-ceiling]" : "");
}

// -------------------- setup / loop --------------------

void setup() {
  pinMode(PIN_HEAT, OUTPUT);
  pinMode(PIN_FOG,  OUTPUT);
  pinMode(PIN_CIRC, OUTPUT);

  // Everything OFF before anything else (relays settle into known state).
  writeRelay(PIN_HEAT, false);
  writeRelay(PIN_FOG,  false);

  // Circulation fan: 24/7, variable speed. Kick to full briefly so a low
  // baseline duty doesn't leave it stalled (stiction), then drop to baseline.
  writeFan(100);
  delay(800);
  writeFan(FAN_MIN_DUTY);

  Wire.begin();   // SHT31s are read directly via the inline driver — no begin() per sensor

  // Cloud monitoring (Particle console / mobile app / webhooks)
  Particle.variable("canopyTempF", cloudCanopyT);
  Particle.variable("canopyRH",    cloudCanopyRH);
  Particle.variable("trunkTempF",  cloudTrunkT);
  Particle.variable("trunkRH",     cloudTrunkRH);
  Particle.variable("fanDuty",     cloudFanDuty);
  Particle.variable("status",      cloudStatus);

  // Reset the board if loop() ever stalls for 60s (hung I2C, etc.).
  wd = new ApplicationWatchdog(60000, System.reset, 1536);

  // Prime timers so min-off doesn't block the first legitimate action.
  unsigned long now = millis();
  heatLastChange = fogLastChange = now - 60000;
}

void loop() {
  unsigned long now = millis();

  if (now - lastControl >= CONTROL_INTERVAL) {
    lastControl = now;
    readSensors();
    control();
  }

  if (now - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = now;
    // Publish a compact event for logging / alerts / IFTTT / webhooks.
    Particle.publish("everfresh", cloudStatus, PRIVATE);
  }

  if (wd) wd->checkin();   // pet the watchdog
}
