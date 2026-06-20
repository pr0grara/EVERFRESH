/*
 * EVERFRESH — Cojoba Angustifolia greenhouse controller
 * Target: Particle Photon (original) / Photon 2 / Argon
 *
 * Sensors : 2x SHT31 over I2C
 *   canopy  @ 0x44  — the CONTROL POINT (all decisions use this)
 *   ambient @ 0x45  — reference / logging only (room air the vent pulls in)
 * Actuators:
 *   HEAT  — 120VAC heater via SSR                    (on/off, D2)
 *   FOG   — 24VDC ultrasonic fogger transducer/MOSFET (on/off, D3)
 *   CIRC  — 4-pin PWM circulation fan (A5): INTERNAL mixing, no fresh air.
 *           Runs full during every fog cycle + gentler on a periodic mix schedule.
 *   VENT  — 4-pin PWM exchange fan (A4): fresh-air exchange with ambient.
 *           Periodic exchange + temp-high / RH-high overrides.
 *
 * 4-pin fans: PWM wire driven directly from the Photon (no MOSFET); 12V + GND
 * constant; tach unused. The fogger transducer (24V) keeps its MOSFET; heater its SSR.
 *
 * Control: hysteresis bang-bang for heat and fog (RH is the humidity loop). Fans run
 * on schedules + thresholds. VPD is computed/logged for future VPD-based control.
 *
 * SAFETY: heater is never energized without a valid canopy temperature, and is
 * force-OFF if the canopy reads dangerously hot.
 */

#include <math.h>   // NAN, isnan(), expf()

// ========================= CONFIG (edit me) =========================

// On/off load polarity (heater SSR, fogger MOSFET). SSR/MOSFET are active-HIGH.
const bool RELAY_ACTIVE_LOW = false;

// --- Pins ---
const int PIN_HEAT     = D2;   // heater SSR (on/off)
const int PIN_FOG      = D3;   // fogger transducer MOSFET (on/off, 24V)
const int PIN_CIRC_PWM = A5;   // circ fan (4-wire) PWM control wire — speed
const int PIN_CIRC_PWR = D4;   // circ fan power MOSFET — true on/off
const int PIN_VENT     = A4;   // vent fan (2-wire) MOSFET — on/off
const int FAN_PWM_FREQ = 25000;   // 25kHz PWM into the circ fan's control wire

// --- I2C sensor addresses (ADDR pin: low=0x44, high=0x45) ---
const uint8_t ADDR_CANOPY  = 0x44;
const uint8_t ADDR_AMBIENT = 0x45;

// --- Temperature setpoints (°F) ---  target band 75–85
const float HEAT_ON_F     = 76.0;   // heater ON below this
const float HEAT_OFF_F    = 78.5;   // heater OFF above this (hysteresis)
const float TEMP_SAFETY_F = 92.0;   // canopy above this => heater hard OFF
const float COOL_ON_F     = 84.0;   // fog-for-cooling ON above this
const float COOL_OFF_F    = 82.0;   // fog-for-cooling OFF below this

// --- Humidity setpoints (%RH) ---  target band 50–80
const float RH_FOG_ON  = 55.0;   // fog (humidity) ON below this
const float RH_FOG_OFF = 75.0;   // fog OFF above this
const float RH_CEILING = 90.0;   // never fog above this

// --- Circulation fan (internal mixing; no fresh air) ---
// Simple mode: runs ONLY during fog cycles (disperse mist). No standalone schedule.
const int CIRC_FOG_DUTY = 100;   // % while fogging

// --- Vent fan (fresh-air exchange with ambient) ---
const int VENT_DUTY = 100;       // % when exchanging
const unsigned long VENT_INTERVAL_MS = 120UL * 60 * 1000;  // exchange every 120 min
const unsigned long VENT_DURATION_MS =  2UL * 60 * 1000;  // for 2 min
// On-demand vent overrides (independent of the schedule), with hysteresis:
const float VENT_TEMP_ON_F  = 83.0;  // vent to cool above this
const float VENT_TEMP_OFF_F = 81.0;
const float VENT_RH_ON      = 88.0;  // vent to shed humidity above this
const float VENT_RH_OFF     = 80.0;

// --- Min on/off times for on/off loads (anti-chatter, ms) ---
const unsigned long HEAT_MIN_ON = 60000, HEAT_MIN_OFF = 60000;
const unsigned long FOG_MIN_ON  = 20000, FOG_MIN_OFF  = 20000;

// --- Cadence ---
const unsigned long CONTROL_INTERVAL = 3000;
const unsigned long PUBLISH_INTERVAL = 60000;

// --- Manual override (testing / maintenance) ---
const unsigned long OVERRIDE_DEFAULT_S = 60;
const unsigned long OVERRIDE_MAX_S     = 600;

// ====================================================================

// Sensor readings
struct Reading { float tempF; float rh; bool ok; };
Reading canopy  = {NAN, NAN, false};
Reading ambient = {NAN, NAN, false};

// Control-point values (always the canopy)
float ctrlTempF = NAN, ctrlRH = NAN;
bool  sensorsValid = false;

// On/off actuator state + timers
bool heatOn = false, fogOn = false;
unsigned long heatLastChange = 0, fogLastChange = 0;

// Fan duties (%) and override-hysteresis memory
int  circDuty = 0, ventDuty = 0;
bool tempVentOn = false, rhVentOn = false;

// Schedules
unsigned long lastControl = 0, lastPublish = 0;
unsigned long ventCycleStart = 0;

// Manual overrides — while *Until is in the future, that actuator is forced.
unsigned long heatOverrideUntil = 0; bool heatOverrideOn  = false;
unsigned long fogOverrideUntil  = 0; bool fogOverrideOn   = false;
unsigned long circOverrideUntil = 0; int  circOverrideDuty = 0;
unsigned long ventOverrideUntil = 0; int  ventOverrideDuty = 0;

// Cloud-exposed values
double cloudCanopyT = 0, cloudCanopyRH = 0, cloudVPD = 0;
double cloudAmbientT = 0, cloudAmbientRH = 0, cloudAmbientVPD = 0;
int    cloudHeat = 0, cloudFog = 0, cloudCirc = 0, cloudVent = 0;
char   cloudMode[16]    = "auto";
char   cloudStatus[200] = "boot";
char   lastAlert[40]    = "";

// State-change event de-dup
bool prevHeat=false, prevFog=false, prevCirc=false, prevVent=false;
bool stateInit=false;

// Watchdog: reset if loop() hangs >60s.
ApplicationWatchdog *wd;

// -------------------- helpers --------------------

float cToF(float c) { return c * 9.0 / 5.0 + 32.0; }

// Air Vapor Pressure Deficit (kPa) from temp (F) + RH (%), via Tetens SVP.
float computeVPD(float tempF, float rh) {
  float tC  = (tempF - 32.0) * 5.0 / 9.0;
  float svp = 0.61078 * expf((17.27 * tC) / (tC + 237.3));
  return svp * (1.0 - rh / 100.0);
}

void writeRelay(int pin, bool on) {
  bool level = RELAY_ACTIVE_LOW ? !on : on;
  digitalWrite(pin, level ? HIGH : LOW);
}

// Circ fan (4-wire): power gated by a low-side MOSFET (true off) + speed via PWM.
// When OFF we release the PWM pin to high-impedance instead of driving it low —
// otherwise the fan back-feeds to ground through the PWM wire and idles at min.
void writeCirc(int duty) {
  if (duty < 0) duty = 0; if (duty > 100) duty = 100;
  circDuty = duty; cloudCirc = duty;
  if (duty > 0) {
    writeRelay(PIN_CIRC_PWR, true);                                       // power on
    pinMode(PIN_CIRC_PWM, OUTPUT);
    analogWrite(PIN_CIRC_PWM, map(duty, 0, 100, 0, 255), FAN_PWM_FREQ);   // speed
  } else {
    writeRelay(PIN_CIRC_PWR, false);   // cut power
    pinMode(PIN_CIRC_PWM, INPUT);      // Hi-Z: no sneak ground path through the PWM wire
  }
}
// Vent fan (2-wire) on a MOSFET: on/off only (full 12V), no PWM.
void writeVent(int duty) {
  if (duty < 0) duty = 0; if (duty > 100) duty = 100;
  ventDuty = (duty > 0) ? 100 : 0; cloudVent = ventDuty;
  writeRelay(PIN_VENT, duty > 0);
}

bool valid(float t, float h) {
  if (isnan(t) || isnan(h)) return false;
  if (t < -40 || t > 257) return false;
  if (h < 0  || h > 100)  return false;
  return true;
}

// -------------------- sensors (inlined SHT31 driver) --------------------

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
bool sht31Read(uint8_t addr, float &tempC, float &rh) {
  Wire.beginTransmission(addr);
  Wire.write(0x24);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;
  delay(20);
  if (Wire.requestFrom(addr, (uint8_t)6) != 6) return false;
  uint8_t d[6];
  for (int i = 0; i < 6; i++) d[i] = Wire.read();
  if (sht31Crc(&d[0]) != d[2]) return false;
  if (sht31Crc(&d[3]) != d[5]) return false;
  uint16_t rawT = (d[0] << 8) | d[1];
  uint16_t rawH = (d[3] << 8) | d[4];
  tempC = -45.0 + 175.0 * ((float)rawT / 65535.0);
  rh    = 100.0 * ((float)rawH / 65535.0);
  return true;
}

void readSensors() {
  float c, h;
  if (sht31Read(ADDR_CANOPY, c, h))  { float f = cToF(c); canopy  = { f, h, valid(f, h) }; }
  else                               { canopy  = { NAN, NAN, false }; }
  if (sht31Read(ADDR_AMBIENT, c, h)) { float f = cToF(c); ambient = { f, h, valid(f, h) }; }
  else                               { ambient = { NAN, NAN, false }; }

  // Canopy is the sole control point.
  if (canopy.ok) { ctrlTempF = canopy.tempF; ctrlRH = canopy.rh; sensorsValid = true; }
  else           { sensorsValid = false; }

  cloudCanopyT   = canopy.ok  ? canopy.tempF  : -1;
  cloudCanopyRH  = canopy.ok  ? canopy.rh     : -1;
  cloudVPD       = canopy.ok  ? computeVPD(canopy.tempF, canopy.rh) : -1;
  cloudAmbientT  = ambient.ok ? ambient.tempF : -1;
  cloudAmbientRH = ambient.ok ? ambient.rh    : -1;
  cloudAmbientVPD= ambient.ok ? computeVPD(ambient.tempF, ambient.rh) : -1;
}

// -------------------- control --------------------

bool applyHysteresis(bool current, bool wantOn, unsigned long &lastChange,
                     unsigned long minOn, unsigned long minOff) {
  unsigned long since = millis() - lastChange;
  if (wantOn == current) return current;
  if (current && since < minOn)  return current;
  if (!current && since < minOff) return current;
  lastChange = millis();
  return wantOn;
}

// Circulation fan: ON (full) only while fogging, to disperse mist. Off otherwise.
int circAutoDuty(bool fogActive) {
  return fogActive ? CIRC_FOG_DUTY : 0;
}

// Vent fan: periodic fresh-air exchange, plus temp-high / RH-high overrides.
// (Ambient-aware decisions come later; for now it's schedule + thresholds.)
int ventAutoDuty(unsigned long now) {
  if (now - ventCycleStart >= VENT_INTERVAL_MS) ventCycleStart = now;
  bool exchangeWindow = (now - ventCycleStart) < VENT_DURATION_MS;

  if (sensorsValid) {
    if (!tempVentOn && ctrlTempF >= VENT_TEMP_ON_F)  tempVentOn = true;
    else if (tempVentOn && ctrlTempF <= VENT_TEMP_OFF_F) tempVentOn = false;
    if (!rhVentOn && ctrlRH >= VENT_RH_ON)   rhVentOn = true;
    else if (rhVentOn && ctrlRH <= VENT_RH_OFF) rhVentOn = false;
  } else {
    tempVentOn = false; rhVentOn = false;
  }

  return (exchangeWindow || tempVentOn || rhVentOn) ? VENT_DUTY : 0;
}

void control() {
  unsigned long now = millis();
  bool overheat = sensorsValid && (ctrlTempF >= TEMP_SAFETY_F);

  // ===== 1) AUTOMATIC heat/fog decisions (hysteresis) =====
  bool autoHeat = false, autoFog = false;
  if (sensorsValid) {
    if (overheat)                               autoHeat = false;
    else if (!heatOn && ctrlTempF < HEAT_ON_F)  autoHeat = true;
    else if (heatOn  && ctrlTempF > HEAT_OFF_F) autoHeat = false;
    else                                        autoHeat = heatOn;

    bool wantHumidity;
    if (!fogOn && ctrlRH < RH_FOG_ON)       wantHumidity = true;
    else if (fogOn && ctrlRH > RH_FOG_OFF)  wantHumidity = false;
    else                                    wantHumidity = fogOn;

    bool wantCooling;
    if (ctrlTempF > COOL_ON_F)       wantCooling = true;
    else if (ctrlTempF < COOL_OFF_F) wantCooling = false;
    else                             wantCooling = fogOn;

    autoFog = (wantHumidity || wantCooling);
    if (ctrlRH >= RH_CEILING) autoFog = false;
  }

  // ===== 2) MANUAL OVERRIDE layer =====
  bool heatReq = autoHeat, fogReq = autoFog;
  bool heatManual = (now < heatOverrideUntil);
  bool fogManual  = (now < fogOverrideUntil);
  bool circManual = (now < circOverrideUntil);
  bool ventManual = (now < ventOverrideUntil);
  if (heatManual) heatReq = heatOverrideOn;
  if (fogManual)  fogReq  = fogOverrideOn;
  strcpy(cloudMode, (heatManual||fogManual||circManual||ventManual) ? "manual" : "auto");

  // ===== 3) HARD SAFETY — always wins =====
  if (overheat) heatReq = false;
  if (!sensorsValid && !heatManual) heatReq = false;

  // ===== 4) apply on/off loads (manual = immediate; auto = min-time protected) =====
  if (heatManual) { heatOn = heatReq; heatLastChange = now; }
  else            heatOn = applyHysteresis(heatOn, heatReq, heatLastChange, HEAT_MIN_ON, HEAT_MIN_OFF);
  if (fogManual)  { fogOn = fogReq; fogLastChange = now; }
  else            fogOn  = applyHysteresis(fogOn,  fogReq,  fogLastChange,  FOG_MIN_ON, FOG_MIN_OFF);
  writeRelay(PIN_HEAT, heatOn);
  writeRelay(PIN_FOG,  fogOn);

  // ===== 5) fans (PWM) =====
  int circReq = circManual ? circOverrideDuty : circAutoDuty(fogOn);
  int ventReq = ventManual ? ventOverrideDuty : ventAutoDuty(now);
  writeCirc(circReq);
  writeVent(ventReq);

  cloudHeat = heatOn ? 1 : 0;
  cloudFog  = fogOn  ? 1 : 0;

  // ===== 6) status string =====
  if (!sensorsValid) {
    snprintf(cloudStatus, sizeof(cloudStatus),
      "ALARM no-sensor | heat=%s fog=%s circ=%d%% vent=%d%% [%s]",
      heatOn ? "ON":"off", fogOn ? "ON":"off", circReq, ventReq, cloudMode);
  } else {
    snprintf(cloudStatus, sizeof(cloudStatus),
      "T=%.1fF RH=%.0f%% VPD=%.2f | heat=%s fog=%s circ=%d%% vent=%d%% [%s]%s%s",
      ctrlTempF, ctrlRH, cloudVPD,
      heatOn ? "ON":"off", fogOn ? "ON":"off", circReq, ventReq, cloudMode,
      overheat ? " OVERHEAT" : "",
      (ctrlRH >= RH_CEILING) ? " RH-ceiling" : "");
  }
}

// -------------------- cloud functions (manual override) --------------------

unsigned long clampSeconds(String arg) {
  long s = arg.trim().length() ? arg.toInt() : 0;
  if (s <= 0) s = OVERRIDE_DEFAULT_S;
  if ((unsigned long)s > OVERRIDE_MAX_S) s = OVERRIDE_MAX_S;
  return (unsigned long)s;
}

int parseDuty(String s) {
  int d = s.toInt();
  if (d < 0) d = 0; if (d > 100) d = 100;
  return d;
}

// Parse "state[,seconds]". state: 1/on => on, anything else (incl. 0) => off.
bool parseState(String arg, unsigned long &secs) {
  arg.trim();
  int comma = arg.indexOf(',');
  String st = (comma >= 0 ? arg.substring(0, comma) : arg);
  st.trim();
  secs = clampSeconds(comma >= 0 ? arg.substring(comma + 1) : "");
  return (st == "1" || st.equalsIgnoreCase("on") || st.toInt() >= 1);
}

// setFog("1") / setFog("0") [,seconds] -> force fogger ON/OFF for a window, then auto.
int fnSetFog(String arg) {
  unsigned long s; bool on = parseState(arg, s);
  fogOverrideOn = on; fogOverrideUntil = millis() + s * 1000;
  Particle.publish("everfresh/cmd", String::format("fog %s %lus", on ? "on" : "off", s), PRIVATE);
  return on ? 1 : 0;
}
// setHeat("1") / setHeat("0") [,seconds] -> force heater ON/OFF (overheat cutoff still applies).
int fnSetHeat(String arg) {
  unsigned long s; bool on = parseState(arg, s);
  heatOverrideOn = on; heatOverrideUntil = millis() + s * 1000;
  Particle.publish("everfresh/cmd", String::format("heat %s %lus", on ? "on" : "off", s), PRIVATE);
  return on ? 1 : 0;
}
// setCirc("80") or setCirc("80,120") -> hold circ fan at 80% for 120s.
int fnSetCirc(String arg) {
  int comma = arg.indexOf(',');
  int duty = parseDuty(comma >= 0 ? arg.substring(0, comma) : arg);
  unsigned long s = clampSeconds(comma >= 0 ? arg.substring(comma + 1) : "");
  circOverrideDuty = duty; circOverrideUntil = millis() + s * 1000;
  Particle.publish("everfresh/cmd", String::format("circ %d%% %lus", duty, s), PRIVATE);
  return duty;
}
// setVent("100") or setVent("100,300") -> hold vent fan at 100% for 300s.
int fnSetVent(String arg) {
  int comma = arg.indexOf(',');
  int duty = parseDuty(comma >= 0 ? arg.substring(0, comma) : arg);
  unsigned long s = clampSeconds(comma >= 0 ? arg.substring(comma + 1) : "");
  ventOverrideDuty = duty; ventOverrideUntil = millis() + s * 1000;
  Particle.publish("everfresh/cmd", String::format("vent %d%% %lus", duty, s), PRIVATE);
  return duty;
}
int fnClearOverrides(String arg) {
  heatOverrideUntil = fogOverrideUntil = circOverrideUntil = ventOverrideUntil = 0;
  Particle.publish("everfresh/cmd", "cleared", PRIVATE);
  return 0;
}

// -------------------- telemetry --------------------

void publishTelemetry() {
  char json[255];
  snprintf(json, sizeof(json),
    "{\"ct\":%.1f,\"crh\":%.0f,\"vpd\":%.2f,"
    "\"at\":%.1f,\"arh\":%.0f,\"avpd\":%.2f,"
    "\"heat\":%d,\"fog\":%d,\"circ\":%d,\"vent\":%d,\"mode\":\"%s\"}",
    cloudCanopyT, cloudCanopyRH, cloudVPD,
    cloudAmbientT, cloudAmbientRH, cloudAmbientVPD,
    cloudHeat, cloudFog, circDuty, ventDuty, cloudMode);
  Particle.publish("everfresh/telemetry", json, PRIVATE);
}

void publishAlerts() {
  const char *alert = "";
  if (!sensorsValid)                   alert = "no-sensor";
  else if (ctrlTempF >= TEMP_SAFETY_F) alert = "overheat";
  if (strcmp(alert, lastAlert) != 0) {
    strncpy(lastAlert, alert, sizeof(lastAlert) - 1);
    if (alert[0]) Particle.publish("everfresh/alert", alert, PRIVATE);
    else          Particle.publish("everfresh/alert", "cleared", PRIVATE);
  }
}

void publishStateChange(const char *what, bool on) {
  char ev[140];
  snprintf(ev, sizeof(ev),
    "{\"ev\":\"%s\",\"state\":\"%s\",\"ct\":%.1f,\"crh\":%.0f}",
    what, on ? "on" : "off", cloudCanopyT, cloudCanopyRH);
  Particle.publish("everfresh/event", ev, PRIVATE);
}

void publishStateChanges() {
  bool circOn = circDuty > 0, ventOn = ventDuty > 0;
  if (!stateInit) {
    prevHeat=heatOn; prevFog=fogOn; prevCirc=circOn; prevVent=ventOn; stateInit=true;
    return;
  }
  if (heatOn != prevHeat) { prevHeat = heatOn; publishStateChange("heat", heatOn); }
  if (fogOn  != prevFog)  { prevFog  = fogOn;  publishStateChange("fog",  fogOn);  }
  if (circOn != prevCirc) { prevCirc = circOn; publishStateChange("circ", circOn); }
  if (ventOn != prevVent) { prevVent = ventOn; publishStateChange("vent", ventOn); }
}

// -------------------- setup / loop --------------------

void setup() {
  pinMode(PIN_HEAT,     OUTPUT);
  pinMode(PIN_FOG,      OUTPUT);
  pinMode(PIN_CIRC_PWM, OUTPUT);
  pinMode(PIN_CIRC_PWR, OUTPUT);
  pinMode(PIN_VENT,     OUTPUT);

  // Known-safe boot state: everything off.
  writeRelay(PIN_HEAT, false);
  writeRelay(PIN_FOG,  false);
  writeCirc(0);   // circ MOSFET off + PWM 0
  writeVent(0);   // vent MOSFET off

  Wire.begin();   // both SHT31s read via the inline driver

  Particle.variable("canopyTempF",  cloudCanopyT);
  Particle.variable("canopyRH",     cloudCanopyRH);
  Particle.variable("vpd",          cloudVPD);
  Particle.variable("ambientTempF", cloudAmbientT);
  Particle.variable("ambientRH",    cloudAmbientRH);
  Particle.variable("ambientVPD",   cloudAmbientVPD);
  Particle.variable("circ",         cloudCirc);
  Particle.variable("vent",         cloudVent);
  Particle.variable("heat",         cloudHeat);
  Particle.variable("fog",          cloudFog);
  Particle.variable("mode",         cloudMode);
  Particle.variable("status",       cloudStatus);

  Particle.function("setFog",         fnSetFog);         // arg: "1"/"0" [,seconds]
  Particle.function("setHeat",        fnSetHeat);        // arg: "1"/"0" [,seconds]
  Particle.function("setCirc",        fnSetCirc);        // arg: "duty" [,seconds]
  Particle.function("setVent",        fnSetVent);        // arg: "duty" [,seconds]
  Particle.function("clearOverrides", fnClearOverrides); // arg: (ignored)

  wd = new ApplicationWatchdog(60000, System.reset, 1536);

  unsigned long now = millis();
  heatLastChange = fogLastChange = now - 60000;
  ventCycleStart = now;
}

void loop() {
  unsigned long now = millis();

  if (now - lastControl >= CONTROL_INTERVAL) {
    lastControl = now;
    readSensors();
    control();
    publishAlerts();
    publishStateChanges();
  }

  if (now - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = now;
    publishTelemetry();
  }

  if (wd) wd->checkin();
}
