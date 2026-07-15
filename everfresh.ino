/*
 * EVERFRESH — Cojoba Angustifolia greenhouse controller
 * Version: v1.2.6 (2026-07-14) — see CHANGELOG.md
 * Target: Particle Photon (original) / Photon 2 / Argon
 *
 * Sensors : 2x SHT31, each on its OWN 2-wire bus (both fixed at addr 0x44)
 *   canopy  @ hardware I2C (D0/D1) — the CONTROL POINT (all decisions use this)
 *   ambient @ software I2C (D5/D6) — reference + gates vent cooling. On its own
 *           bit-banged bus because the sealed waterproof module is fixed at 0x44,
 *           same as the canopy sensor, so the two cannot share one bus.
 * Actuators:
 *   HEAT  — 120VAC heater via SSR                    (on/off, D2)
 *   FOG   — 24VDC ultrasonic fogger transducer/MOSFET (on/off, D3)
 *   CIRC  — 4-pin PWM circulation fan (PWM A5 + power MOSFET D4): INTERNAL mixing.
 *           Runs full during every fog cycle + continuous gentle mixing between cycles.
 *   VENT  — 4-pin PWM exchange fan (PWM WKP + power MOSFET A4): fresh-air exchange
 *           with ambient. On-demand: cooling + high-RH safety. Periodic exchange
 *           disabled (leaky chamber) — re-enable VENT_SCHEDULE_ENABLED once sealed.
 *
 * 4-pin fans: speed via 25kHz PWM on the control wire, power gated by a low-side
 * MOSFET for a true hard-off (the PWM wire is released to Hi-Z when off so the fan
 * can't back-feed to ground and idle at min); 12V + GND otherwise constant; tach
 * unused. The fogger transducer (24V) keeps its MOSFET; heater its SSR.
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
const int PIN_VENT_PWM = WKP;  // (A7) vent fan (4-wire) PWM control wire — speed (25kHz)
const int PIN_VENT_PWR = A4;   // vent fan power MOSFET — true on/off (hard off)
const int FAN_PWM_FREQ = 25000;   // 25kHz PWM into both 4-wire fans' control wires

// Ambient sensor's dedicated software-I2C bus (see sensor section). Both pins need
// a pull-up (4.7k–10k) to 3.3V; most SHT31 breakouts include them onboard.
const int PIN_AMB_SDA  = D5;   // ambient SHT31 — software I2C data
const int PIN_AMB_SCL  = D6;   // ambient SHT31 — software I2C clock

// --- I2C sensor addresses --- both SHT31s are fixed at 0x44; they don't collide
// because each lives on its own bus (canopy on hardware Wire, ambient bit-banged).
const uint8_t ADDR_CANOPY  = 0x44;
const uint8_t ADDR_AMBIENT = 0x44;

// --- Temperature setpoints (°F) ---  target band 75–85
const float HEAT_ON_F     = 76.0;   // heater ON below this
const float HEAT_OFF_F    = 78.5;   // heater OFF above this (hysteresis)
const float TEMP_SAFETY_F = 92.0;   // canopy above this => heater hard OFF + "hot" alarm (no longer force-vents)
const float COOL_ON_F     = 90.0;   // cooling (fog + vent) ON above this — raised 84→90 (7/14): a hot
                                    // tropical afternoon regularly rides 85-100°F; don't fight heat until 90
const float COOL_OFF_F    = 88.0;   // cooling OFF below this (fixed floor; vent adds an elastic ambient floor below)

// --- Humidity setpoints (%RH) ---  target band 50–80
const float RH_FOG_ON  = 65.0;   // fog (humidity) ON below this
const float RH_FOG_OFF = 90.0;   // fog OFF above this
const float RH_CEILING = 95.0;   // never fog above this

// --- Circulation fan (internal mixing; no fresh air) ---
const int CIRC_FOG_DUTY = 100;   // % while fogging (disperse mist)
// Between fog cycles: CONTINUOUS gentle mixing. Recirculating over the damp floor
// homogenizes the air AND evaporates standing water back into the chamber — this
// sustains RH between fog cycles and dries the floor (no fresh air pulled in).
// Proven on 6/20: with mixing running, canopy RH decayed far slower and the fogger
// stopped thrashing; with mixing off, RH crashed and refired every ~5 min.
const int CIRC_MIX_DUTY = 1;    // % continuous mix speed (lower if floor over-dries / too much leaf airflow; 0 = mixing OFF)
// Night idle floor (7/14): mode-3 night USED to hard-zero circ (avoid re-wetting the floor pool during
// dry-down). But the temp regime rose ~10°F — nights now run ~80°F / ~82% RH, so the risk inverted from
// re-wetting to STAGNATION (still, warm, saturated air = fungal boundary layer on the leaves). Hold a
// gentle real airflow at night instead of near-zero. Below the 35% desiccation-concern ceiling; tune freely.
const int CIRC_NIGHT_MIN = 20;  // % circ floor at night in mode 3 (was hard 0). Raise for more stir / lower if it over-humidifies
// At night circ is the PRIMARY RH lever (fog is off overnight): running it over the floor pool evaporates
// water and holds RH up, no fogger needed. But if that drives canopy VPD under the wet floor (VPD_NIGHT_LO)
// it's over-humidifying — instead of killing circ (stagnation risk), PULSE it: brief ON keeps air moving
// for anti-stagnation while cutting continuous pool evaporation; the export vent does the actual drying.
const unsigned long CIRC_NIGHT_WET_PULSE_ON_MS  =  60UL * 1000;   // too-wet: circ 1 min on...
const unsigned long CIRC_NIGHT_WET_PULSE_OFF_MS = 240UL * 1000;   // ...then 4 min off (~20% time-duty), repeat
// RH-assist: as canopy RH sags from the top of the assist band down toward the fog-on
// point, ramp circ speed from CIRC_MIX_DUTY up to CIRC_RH_ASSIST_MAX — driving more
// floor-water evaporation to prop RH up *before* the fogger fires. Goal: flatten the
// RH decay (straighter lines, fewer/shallower sawteeth) and stretch the inter-fog
// interval. Kept gentle so sustained airflow doesn't desiccate the foliage.
const int   CIRC_RH_ASSIST_MAX = 35;    // % ceiling for the humidity-assist ramp
const float RH_ASSIST_BAND     = 10.0;  // RH span above RH_FOG_ON over which circ ramps up (e.g. 65→75)
// Heater circ interlock: a radiant element (40W ceramic on D2) in still air stratifies —
// heat pools up top while the canopy SHT31, the ONLY overtemp sensor (TEMP_SAFETY_F), reads
// cooler air below and may never trip. So whenever the heater is energized, hold at least this
// circ duty so the fan is never fully stopped next to a live element. At 1 this is the same
// idle-mix trickle circ runs by day — the blade turns but moves little air, so it guards
// against a dead-still hot pocket, NOT full convective mixing. Raise it (toward CIRC_DRYDOWN
// = 35) if the ceramic-on-D2 run shows the upper chamber running away from the canopy sensor.
const int   HEAT_CIRC_MIN = 1;          // % circ floor while the heater is on

// --- Vent fan (fresh-air exchange with ambient) ---
// Chamber is leaky, so leaks already supply fresh air — the timed exchange is off
// for now and the vent is purely on-demand (cooling + high-RH safety, below).
// Re-enable the schedule once the chamber is sealed.
const bool VENT_SCHEDULE_ENABLED = false;
const int VENT_DUTY = 100;       // % when exchanging
const unsigned long VENT_INTERVAL_MS = 120UL * 60 * 1000;  // exchange every 120 min (when enabled)
const unsigned long VENT_DURATION_MS =  2UL * 60 * 1000;  // for 2 min
// On-demand cooling vent. PRIORITY RULE (7/09): the vent is the PREFERRED cooler because it
// sheds heat WITHOUT adding water — but only when it actually cools, i.e. the room is cooler
// than the canopy (ventCools(): canopy-ambient >= VENT_AMBIENT_DELTA_F). So it engages for
// cooling from COOL_ON_F up whenever ventCools() holds, and fog is forced for cooling ONLY when
// venting can't help (room >= canopy — the common case during the 4-8 PM spike, where the room
// co-heats; empirically venting was the right move only ~16.5% of cooling-needed samples). The
// 6/20 finding still stands — venting crashes RH/VPD (VPD ~2.5 vented vs ~1.25 fog) — so even as
// primary cooler it PULSES (below) to let RH recover, and fog keeps its independent RH/VPD job.
const float VENT_TEMP_ON_F  = COOL_ON_F;   // vent-cool from here up when ventCools() (was 94: vent-only-at-peak)
const float VENT_TEMP_OFF_F = COOL_OFF_F;  // ...release under here OR when the elastic ambient floor is hit (below)
const float VENT_EMERGENCY_F = 99.0; // hard backstop: force vent full OVER manual (runaway guard)
// Above VENT_TEMP_ON_F the cooling vent runs, but ON is CAPPED: it may run at most VENT_PULSE_ON_MS
// continuously, then takes a forced VENT_PULSE_OFF_MS break before it's allowed to resume. The break
// is a fog/RH-recovery + closed-chamber-efficiency window, NOT a fixed duty cycle — the vent releases
// the instant cooling is satisfied (tempVentOn drops when ctrlTempF <= VENT_TEMP_OFF_F or !ventCools()),
// so a mid-burst temp drop at, say, 75 s turns it OFF immediately; the 3-min figure only bounds a
// SUSTAINED excursion. This is looser than the old 25% pulse because v1.2.4's independent RH-hold fog
// now backfills humidity continuously (fog is the direct humidity lever), so the vent no longer has to
// stay mostly-off to protect RH — it only pauses briefly so fog gets efficient closed-chamber windows.
// (VENT_EMERGENCY_F still wins.) Driven ON/OFF here (time-pulsing) even though the 4-wire fan can PWM.
const unsigned long VENT_PULSE_ON_MS  = 180UL * 1000;  // vent up to 3 min continuous...
const unsigned long VENT_PULSE_OFF_MS =  60UL * 1000;  // ...then a forced 1 min off before it may resume
const float VENT_RH_ON      = 88.0;  // vent to shed humidity above this...
const unsigned long VENT_RH_DWELL_MS = 5UL * 60 * 1000;  // ...but only if held >=5 min
const float VENT_RH_OFF     = 80.0;
// Vent only cools if it pulls in cooler air, and it can never pull the canopy BELOW ambient — so
// the cooling band is elastic against the room temperature (7/14). Two decoupled deltas:
//   _ON  (3°): don't START venting unless the canopy leads the room by at least this — a gap worth exploiting.
//   _OFF (1°): once running, keep chasing DOWN until the canopy is within this of ambient, then release —
//              the last degree of approach costs the most airflow for the least drop, so quit early.
// Net vent release = max(VENT_TEMP_OFF_F, ambient + VENT_AMBIENT_DELTA_OFF): ambient 92 → stop ~93;
// cool room (ambient 78) → the fixed 88° floor takes over. ventCools() (the engage/fog-arbiter gate) uses _ON.
// With no valid ambient reading both guards fail closed → cooling hands back to fog (6/22: the room runs
// HOTTER than the tent during the spike, so the old "room is cooler" assumption was backwards).
const float VENT_AMBIENT_DELTA_F   = 3.0;   // engage gap (also the ventCools() arbiter threshold)
const float VENT_AMBIENT_DELTA_OFF = 1.0;   // release gap — how close to ambient the vent is allowed to chase

// --- Min on/off times for on/off loads (anti-chatter, ms) ---
const unsigned long HEAT_MIN_ON = 60000, HEAT_MIN_OFF = 60000;
const unsigned long FOG_MIN_ON  = 20000, FOG_MIN_OFF  = 20000;

// --- Cadence ---
const unsigned long CONTROL_INTERVAL = 3000;
const unsigned long PUBLISH_INTERVAL = 60000;

// --- Manual override (testing / maintenance) ---
const unsigned long OVERRIDE_DEFAULT_S = 60;
const unsigned long OVERRIDE_MAX_S     = 600;

// --- Humidity control mode (RUNTIME-selectable; the "guide back") ---
// 0 = original RH bang-bang (RH_FOG_ON/OFF)   — most conservative fallback
// 1 = diurnal VPD bang-bang (vpdWantFog/vpdWantDryVent)
// 2 = moisture-regime-gated PI (dew-point gap + PI + dry-down)
// 3 = wide-hysteresis excursion bang-bang (regime-gated; overdry/overmoisten + rest) — default
// Change live with the setControlMode cloud function (no reflash). Heat is NEVER driven
// by humidity control in any mode (the temp loop owns it). See VPD_PI_CONTROL.md.
int controlMode = 3;
const int MODE_RH = 0, MODE_VPD_BANGBANG = 1, MODE_REGIME_PI = 2, MODE_EXCURSION = 3;

// Cloud-time zone (America/Los_Angeles); base offset + explicit US DST (no TZ db on device).
const int   TZ_BASE_OFFSET = -8;          // PST hours from UTC
const bool  TZ_USE_DST     = true;        // add +1h during the US DST window

// Coarse phase boundaries (local hour, half-open [start, next))
const int   PHASE_MORNING_START   = 6;    // [6,11)  morning
const int   PHASE_AFTERNOON_START = 11;   // [11,16) afternoon
const int   PHASE_EVENING_START   = 16;   // [16,20) evening (sun-gated pulse)
const int   PHASE_NIGHT_START     = 20;   // [20,6)  night

// Per-phase canopy VPD bands (kPa)
// 7/09 de-stress: pinnae not unfurling fully since the ceramic element went in (dry-air stress
// during frond expansion). Whole ladder pulled down ~0.25-0.35 kPa + evening spike crushed so the
// fog loop holds humidity up against the heater's drying. Frond expansion now sits in the 0.55-0.8
// zone it wants. Gentle diurnal climb retained (still some chop) but capped well under the stress band.
const float VPD_NIGHT_LO = 0.55, VPD_NIGHT_HI = 1.10;  // wide night deadband (7/14): floor 0.55 keeps the
// wet-side export; HI 0.70→1.10 lets the fogger stay out until the night is genuinely dry. Plant is asleep
// (stomata closed, leaflets folded) so a loose VPD costs nothing, and nights now run ~10°F warmer — the old
// tight band implicitly demanded ~82% RH. This ONLY quiets night fog; it does NOT dry the tent (that's LO).
const float VPD_MORN_LO  = 0.55, VPD_MORN_HI  = 0.80;  // was 0.8/1.1
const float VPD_AFT_LO   = 0.70, VPD_AFT_HI   = 0.95;  // was 1.0/1.3
const float VPD_EVE_LO   = 0.85, VPD_EVE_HI   = 1.15;  // was 1.3/1.8; only when sun detected
const float VPD_EVE_CAP  = 1.40;                       // was 2.2; intervene-above ceiling during the spike
const float VPD_DEADBAND = 0.05;                      // anti-chatter on the VPD loop

// Sun-detection gate for the evening pulse: sun = ambient OR canopy crosses threshold, sustained.
const float SUN_AMB_TEMP_ON_F    = 88.0, SUN_AMB_TEMP_OFF_F    = 84.0;  // room heats hard under west sun
const float SUN_CANOPY_TEMP_ON_F = 85.0, SUN_CANOPY_TEMP_OFF_F = 82.0;
const float SUN_CANOPY_VPD_ON    = 1.45, SUN_CANOPY_VPD_OFF    = 1.30;
const unsigned long SUN_DETECT_DWELL_MS = 5UL * 60 * 1000;

// Active-dry vent pulse (raises VPD when chamber too humid). Gated on ambient actually being drier.
const unsigned long VPD_DRY_DWELL_MS = 10UL * 60 * 1000;  // VPD below floor this long before drying
const float VPD_DRY_AMBIENT_MARGIN   = 0.15;              // require ambientVPD - canopyVPD >= this (kPa)
const unsigned long VPD_DRY_PULSE_ON_MS  =  60UL * 1000;  // ~20% duty: 1 min on...
const unsigned long VPD_DRY_PULSE_OFF_MS = 240UL * 1000;  // ...4 min off

// --- Moisture-regime-gated PI control (controlMode 2) ---
// Regime from the inside-outside dew-point gap dpGap = canopyDP - ambientDP (°F). Dual
// hysteresis so the regime latches without chatter.
const float DPGAP_WET_ON_F  = 4.0, DPGAP_WET_OFF_F = 2.5;  // WET = water-loaded, venting exports moisture
const float DPGAP_DRY_ON_F  = 1.0, DPGAP_DRY_OFF_F = 2.0;  // DRY = room ~as wet, venting futile → fog
const float DPGAP_VENT_FULL_F = 6.0;   // vent authority = clamp(dpGap/this,0,1); fades to 0 as gap→0

// PI on canopy VPD error vs band midpoint. Effort is unitless 0..1 per direction.
const float VPD_KP = 1.20;             // effort per kPa of error
const float VPD_KI = 0.020;            // effort per kPa per control cycle
const float VPD_I_CLAMP = 1.50;        // |integral| hard clamp (effort units)
const float VPD_DEADBAND_PI = 0.04;    // |error| below this → zero effort, hold integral (kPa)

// On/off actuators (fog, 2-wire vent) → time-duty: effort maps to ON-fraction of this window.
const unsigned long VPD_PI_WINDOW_MS = 300UL * 1000;   // 5-min PWM-in-time window
const float VPD_FOG_MIN_EFFORT  = 0.10;                // below this, don't fire fog (keeps ON >= FOG_MIN_ON)
const float VPD_VENT_MIN_EFFORT = 0.10;                // below this, don't open the vent for drying

// Active dry-down pump (WET + VPD well below setpoint): circ UP with the vent to evaporate +
// exhaust the floor reservoir so drying isn't transitory.
const float VPD_DRYDOWN_TRIGGER_KPA = 0.15;            // VPD this far below setpoint → engage pump
const int   CIRC_DRYDOWN_DUTY = 35;                    // moderated circ during dry-down (== assist cap)

// --- Wide-hysteresis excursion control (controlMode 3) --------------------------------
// The hormetic alternative to the PI servo: instead of pinning VPD to the band midpoint
// (which micro-cycles the vent every few seconds and never lets the plant settle), this
// commits to ONE big, deliberate swing and then RESTS. Too wet → dry HARD until VPD
// overshoots a fixed margin PAST the band's dry edge, then release and let it drift back
// for a guaranteed rest plateau. Too dry → fog HARD until VPD overshoots past the wet
// edge, then rest. Larger variance is the point (see GROW_PHILOSOPHY.md); the regime
// interlock + below-room stop from mode 2 still pick the feasible lever. Heat untouched.
const float EXC_OVERSHOOT_KPA   = 0.15;                // drive this far PAST the far band edge before resting
const float EXC_TRIGGER_MARGIN  = 0.02;               // arm once VPD leaves the band by this (kPa)
const unsigned long EXC_REST_MS     = 12UL * 60 * 1000;  // minimum rest plateau after a swing (no humidity servo)
const unsigned long EXC_MAX_DRIVE_MS = 20UL * 60 * 1000; // safety: end an excursion that can't reach overshoot
// Don't START a dry swing unless the room is meaningfully drier — a positive dpGap isn't enough
// (6/23: gap 1.7°F, ambient VPD ≈ canopy → vent churned near-equal air at 100% for the full
// max-drive with zero gain). Continuation relies on canVent + the stall detector, so conditions
// can soften mid-swing without re-arming into futility.
const float EXC_DRY_ARM_GAP_F   = 2.5;                // min canopy−ambient dew-point gap (°F) to arm a dry swing
// Arm dwell: the trigger condition must HOLD this long before committing to a swing, so a single
// boot/RH transient can't kick one off (6/23: a 1-sample RH spike to ~67% armed a vent 30s after
// flash). Same "sustained, not instantaneous" guard as the RH-relief vent / mode-1 dry-vent.
const unsigned long EXC_ARM_DWELL_MS = 2UL * 60 * 1000;    // trigger must persist this long to arm a swing
// Room-floor stop: if VPD won't move toward the target (vent can't dry past the incoming air /
// fog can't keep up), give up rather than run the lever wide-open to the max-drive timeout. Judged
// every control cycle (CONTROL_INTERVAL, 3s) — far finer than the telemetry's 60s log cadence.
const float EXC_STALL_MIN_KPA   = 0.03;               // VPD progress toward target that re-arms the stall clock
const unsigned long EXC_STALL_WINDOW_MS = 1UL * 60 * 1000;  // no such progress this long → stalled, end the swing

// --- Overnight moisture-export vent (controlMode 3 only) ------------------------------
// The excursion's dry swing gives up at night: the floor reservoir re-humidifies as fast as the
// vent exports, so the VPD-progress stall detector fires within ~1 min and the 12-min rest plateau
// lets VPD collapse below 0.4 kPa for hours (6/26-27 telemetry: ~6 h/night < 0.4). But the lever
// isn't actually futile — dpGap ~10°F means room air is genuinely drier, so exchanging it removes
// water mass over the night; the stall test just can't see it on a 1-min VPD timescale. This runs a
// gentle export during NIGHT phase gated on dpGap (real export), not VPD progress, and only below a
// deep-wet floor (0.55) so the excursion still owns the hormetic chop in the 0.55–0.9 band. Heat is
// untouched: the temp loop reheats the cooler incoming air, so vent+reheat = net dehumidify. Run at
// vent speed 1 — the lowest duty (~1% PWM): visibly flutters the leaves but is near-silent, so it can
// run continuously across the whole wet trough without the fan-runtime QoL cost of a 100% blast.
const bool  NIGHT_EXPORT_ENABLED      = true;
const int   NIGHT_EXPORT_DUTY         = 1;      // vent speed 1: gentle leaf-flutter exchange, near-silent
const float NIGHT_EXPORT_VPD_ON       = 0.35;   // 7/09: dropped from 0.55 so export won't fight the lower night band...
const float NIGHT_EXPORT_VPD_OFF      = 0.55;   // ...only strips moisture in a genuinely-wet trough now (was 0.85)
const float NIGHT_EXPORT_TEMP_FLOOR_F = 64.0;   // skip if canopy this cold — don't cold-stress; let heat win first
// Short debounce only (not a long dwell): the 0.55/0.85 hysteresis band already prevents chatter, so this
// just rejects single-sample sensor noise. Keeping it short means the gentle export re-engages almost
// immediately when the air sags back wet, holding a tight floor instead of sawing down during a re-arm wait.
const unsigned long NIGHT_EXPORT_DWELL_MS = 60UL * 1000;  // VPD below ON this long first (debounce sensor noise)

// --- Stagnancy safeguard --------------------------------------------------------------
// At night the excursion strategy holds circ OFF (below): the gentle vent/export moves the air, and a
// running circ just fans the floor reservoir back into the air, fighting the drying. But with circ off,
// a long calm stretch (no vent firing) can let the chamber air stratify and pool moisture on cold
// surfaces. So if the vent (any exchange: cooling, RH relief, dry swing, or the gentle night export)
// hasn't fired for STAGNANT_VENT_GAP_MS, restore a brief mold-safe circ pulse, until the vent runs
// again (which resets the clock).
const unsigned long STAGNANT_VENT_GAP_MS    = 1UL * 60 * 60 * 1000;  // no vent this long → start periodic stirs
const unsigned long STAGNANT_STIR_PERIOD_MS = 30UL * 60 * 1000;      // one stir every 30 min while stagnant...
const unsigned long STAGNANT_STIR_ON_MS     =  3UL * 60 * 1000;      // ...each stir runs circ for 3 min
const int   STAGNANT_STIR_DUTY = 1;                                  // stir at speed 1 (the mold-safe floor) — gentle, vs circ OFF

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
unsigned long ventPulseStart = 0;   // anchors the cool-vent ON/OFF pulse to peak entry
unsigned long rhHighSince = 0;      // when RH first crossed VENT_RH_ON (0 = not high); gates the dwell
unsigned long nightExportSince = 0; // dwell clock while VPD below the night-export floor (0 = not low)
bool   nightExportLatched = false;  // hysteresis latch between VPD_ON (0.55) and VPD_OFF (0.75)
bool   nightExportOn = false;       // this cycle's export demand (drives vent @ speed 1, idles circ, status)
unsigned long ventLastFired = 0;    // last time the vent moved air (any reason); gates the stagnancy stir
bool   stagnantStirOn = false;      // this cycle's stagnancy-stir demand (status/telemetry)

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
char   cloudStatus[240] = "boot";
char   lastAlert[40]    = "";
char   cloudVersion[16] = "v1.2.6";    // firmware build id — exposed as the "version" cloud var so a flash is verifiable remotely

// State-change event de-dup
bool prevHeat=false, prevFog=false, prevCirc=false, prevVent=false;
bool stateInit=false;

// Diurnal VPD-control runtime state
enum Phase { PH_NIGHT, PH_MORNING, PH_AFTERNOON, PH_EVENING };
Phase  curPhase = PH_AFTERNOON;        // resolved each control cycle (safe mid-range default)
bool   sunDetected = false;            // latched evening-spike flag (hysteresis)
unsigned long sunHighSince = 0;        // dwell clock for sun detection (0 = not high)
float  vpdTargetLo = NAN, vpdTargetHi = NAN;   // active band (for telemetry/status)
bool   timeValid = false;              // mirror of Time.isValid() at resolve time
unsigned long vpdLowSince = 0;         // dwell clock while disarmed (0 = not below floor)
unsigned long vpdDryStart = 0;         // pulse-phase anchor, set when the dry-vent arms
bool   vpdDryVentOn = false;           // latched dry-vent request
int    forcedPhase = -1;               // test override: -1=live clock, 0..3 = force PH_*
bool   forcedSun = false;              // test override: force sun-detected in forced evening
double cloudVpdTarget = 0;             // active band midpoint (Particle.variable + telemetry)
char   cloudPhase[12] = "afternoon";   // current phase name (Particle.variable)

// Moisture-regime-gated PI state (controlMode 2)
float  canopyDP = NAN, ambientDP = NAN, dpGap = NAN;   // dew points (°F) + inside-outside gap
bool   dpGapValid = false;             // true only when BOTH sensors valid
enum Regime { RG_DRY, RG_NEUTRAL, RG_WET };
Regime curRegime = RG_NEUTRAL;         // latched, hysteretic
int    forcedRegime = -1;              // test override: -1=live, 0/1/2 = force DRY/NEUTRAL/WET
struct PIState { float integ; float fogEffort; float ventEffort; } piState = {0, 0, 0};
unsigned long fogPiWin = 0, ventPiWin = 0;             // time-duty window anchors
bool   ventBelowRoom = false;          // status flag: want to dry but gap≈0 (needs heat, can't)

// Wide-hysteresis excursion state (controlMode 3)
enum ExcState { EX_REST, EX_DRY, EX_MOIST };
ExcState excState = EX_REST;           // latched: resting / drying-hard / moistening-hard
unsigned long excDriveStart = 0;       // when the current swing began (max-drive safety clock)
unsigned long excRestUntil  = 0;       // earliest time the next swing may arm (rest plateau)
bool   excWantVent = false, excWantFog = false;   // this cycle's humidity-servo demands
float  excStallVpd = 0;                // VPD anchor for the stall detector
unsigned long excStallSince = 0;       // when the stall anchor was last set
unsigned long excArmSince = 0;         // dwell clock: when the arm trigger first became true (0 = not pending)
double cloudCanopyDP = 0, cloudAmbientDP = 0, cloudDpGap = 0;   // telemetry mirrors

// Watchdog: reset if loop() hangs >60s.
ApplicationWatchdog *wd;

// -------------------- helpers --------------------

float cToF(float c) { return c * 9.0 / 5.0 + 32.0; }
float constrainf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Air Vapor Pressure Deficit (kPa) from temp (F) + RH (%), via Tetens SVP.
float computeVPD(float tempF, float rh) {
  float tC  = (tempF - 32.0) * 5.0 / 9.0;
  float svp = 0.61078 * expf((17.27 * tC) / (tC + 237.3));
  return svp * (1.0 - rh / 100.0);
}

// Dew point (°F) from temp (F) + RH (%), Magnus (a=17.62, b=243.12°C).
float computeDewPoint(float tempF, float rh) {
  float tC = (tempF - 32.0) * 5.0 / 9.0;
  if (rh < 1.0) rh = 1.0;                       // guard ln(0)
  float a = 17.62, b = 243.12;
  float g = logf(rh / 100.0) + (a * tC) / (b + tC);
  return cToF((b * g) / (a - g));
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
// Vent fan (4-wire): same drive pattern as the circ fan — power gated by a low-side
// MOSFET (true hard-off) + speed via PWM. When OFF, release the PWM pin to Hi-Z so
// the fan can't back-feed to ground through the control wire and idle at min.
// The on-demand control logic below still time-pulses ON/OFF (writeVent(100)/(0));
// the PWM wire additionally enables real speed control via setVent and any future
// analog vent effort.
void writeVent(int duty) {
  if (duty < 0) duty = 0; if (duty > 100) duty = 100;
  ventDuty = duty; cloudVent = duty;
  if (duty > 0) {
    writeRelay(PIN_VENT_PWR, true);                                       // power on
    pinMode(PIN_VENT_PWM, OUTPUT);
    analogWrite(PIN_VENT_PWM, map(duty, 0, 100, 0, 255), FAN_PWM_FREQ);   // speed
  } else {
    writeRelay(PIN_VENT_PWR, false);   // cut power (hard off)
    pinMode(PIN_VENT_PWM, INPUT);      // Hi-Z: no sneak ground path through the PWM wire
  }
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

// ---- software (bit-banged) I2C for the ambient sensor ----
// Open-drain emulation: a line is pulled LOW by driving the pin as an output, and
// released HIGH by switching it to a high-impedance input (the external pull-up
// raises it). ~100 kHz. The SHT31 is read in no-clock-stretch mode, so the master
// owns the clock; swSclHigh still tolerates brief stretching with a timeout.
inline void swDelay()   { delayMicroseconds(5); }
inline void swSdaHigh() { pinMode(PIN_AMB_SDA, INPUT); }
inline void swSdaLow()  { digitalWrite(PIN_AMB_SDA, LOW); pinMode(PIN_AMB_SDA, OUTPUT); }
inline void swSclLow()  { digitalWrite(PIN_AMB_SCL, LOW); pinMode(PIN_AMB_SCL, OUTPUT); }
inline void swSclHigh() {
  pinMode(PIN_AMB_SCL, INPUT);
  for (int i = 0; i < 1000 && digitalRead(PIN_AMB_SCL) == LOW; i++) delayMicroseconds(1);
}

void swI2CInit() {            // idle bus = both lines released high
  swSdaHigh();
  swSclHigh();
}

void swStart() {             // SDA falls while SCL is high
  swSdaHigh(); swSclHigh(); swDelay();
  swSdaLow();  swDelay();
  swSclLow();  swDelay();
}

void swStop() {              // SDA rises while SCL is high
  swSdaLow();  swDelay();
  swSclHigh(); swDelay();
  swSdaHigh(); swDelay();
}

bool swWrite(uint8_t b) {    // returns true if the slave ACKed
  for (int i = 0; i < 8; i++) {
    if (b & 0x80) swSdaHigh(); else swSdaLow();
    swDelay();
    swSclHigh(); swDelay();
    swSclLow();  swDelay();
    b <<= 1;
  }
  swSdaHigh();               // release SDA so the slave can drive the ACK bit
  swDelay();
  swSclHigh(); swDelay();
  bool ack = (digitalRead(PIN_AMB_SDA) == LOW);
  swSclLow();  swDelay();
  return ack;
}

uint8_t swRead(bool ack) {   // ack=true -> ACK (more bytes), false -> NACK (last byte)
  uint8_t b = 0;
  swSdaHigh();               // let the slave drive the data line
  for (int i = 0; i < 8; i++) {
    swDelay();
    swSclHigh(); swDelay();
    b = (uint8_t)((b << 1) | (digitalRead(PIN_AMB_SDA) ? 1 : 0));
    swSclLow();
  }
  if (ack) swSdaLow(); else swSdaHigh();
  swDelay();
  swSclHigh(); swDelay();
  swSclLow();  swDelay();
  swSdaHigh();
  return b;
}

// Single-shot, high-repeatability, no clock stretching (cmd 0x2400) over software I2C.
bool sht31ReadSW(uint8_t addr, float &tempC, float &rh) {
  swStart();
  if (!swWrite((uint8_t)(addr << 1)))       { swStop(); return false; }  // write
  if (!swWrite(0x24))                        { swStop(); return false; }
  if (!swWrite(0x00))                        { swStop(); return false; }
  swStop();
  delay(20);
  swStart();
  if (!swWrite((uint8_t)((addr << 1) | 1)))  { swStop(); return false; }  // read
  uint8_t d[6];
  for (int i = 0; i < 6; i++) d[i] = swRead(i < 5);   // ACK first 5, NACK the last
  swStop();
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
  if (sht31Read(ADDR_CANOPY, c, h))    { float f = cToF(c); canopy  = { f, h, valid(f, h) }; }
  else                                 { canopy  = { NAN, NAN, false }; }
  if (sht31ReadSW(ADDR_AMBIENT, c, h)) { float f = cToF(c); ambient = { f, h, valid(f, h) }; }
  else                                 { ambient = { NAN, NAN, false }; }

  // Canopy is the sole control point.
  if (canopy.ok) { ctrlTempF = canopy.tempF; ctrlRH = canopy.rh; sensorsValid = true; }
  else           { sensorsValid = false; }

  cloudCanopyT   = canopy.ok  ? canopy.tempF  : -1;
  cloudCanopyRH  = canopy.ok  ? canopy.rh     : -1;
  cloudVPD       = canopy.ok  ? computeVPD(canopy.tempF, canopy.rh) : -1;
  cloudAmbientT  = ambient.ok ? ambient.tempF : -1;
  cloudAmbientRH = ambient.ok ? ambient.rh    : -1;
  cloudAmbientVPD= ambient.ok ? computeVPD(ambient.tempF, ambient.rh) : -1;

  // Moisture-load sense organ: inside-outside dew-point gap (needs BOTH sensors).
  canopyDP  = canopy.ok  ? computeDewPoint(canopy.tempF, canopy.rh)   : NAN;
  ambientDP = ambient.ok ? computeDewPoint(ambient.tempF, ambient.rh) : NAN;
  dpGapValid = canopy.ok && ambient.ok;
  dpGap = dpGapValid ? (canopyDP - ambientDP) : NAN;
  cloudCanopyDP  = canopy.ok  ? canopyDP  : -100;
  cloudAmbientDP = ambient.ok ? ambientDP : -100;
  cloudDpGap     = dpGapValid ? dpGap     : -100;
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

// -------------------- diurnal VPD control --------------------

const char* phaseName(Phase p) {
  switch (p) {
    case PH_NIGHT:     return "night";
    case PH_MORNING:   return "morning";
    case PH_AFTERNOON: return "afternoon";
    default:           return "evening";
  }
}

// US DST window: 2nd Sunday of March 02:00 .. 1st Sunday of November 02:00. Pure date math
// (device has no TZ database). Ignores the 02:00 cutover hour on the two transition days —
// a ≤1h phase-boundary slip then, which is cosmetic.
bool usDstActive() {
  int m = Time.month(), d = Time.day(), wd = Time.weekday();  // weekday: 1=Sun..7=Sat
  if (m < 3 || m > 11) return false;
  if (m > 3 && m < 11) return true;
  int prevSunday = d - (wd - 1);          // date of the most recent Sunday (<1 => prior month)
  if (m == 3)  return prevSunday >= 8;     // March: on/after the 2nd Sunday
  return prevSunday < 1;                    // November: before the 1st Sunday
}

// Latch the evening spike: sun = ambient OR canopy crossing threshold, sustained, with
// on/off hysteresis (passing clouds don't toggle it). Mirrors the rhHighSince dwell idiom.
void resolveSun() {
  if (forcedPhase >= 0) { sunDetected = forcedSun; sunHighSince = 0; return; }
  if (curPhase != PH_EVENING || !sensorsValid) { sunDetected = false; sunHighSince = 0; return; }
  float vpd = computeVPD(ctrlTempF, ctrlRH);
  bool hot = (ambient.ok && ambient.tempF >= SUN_AMB_TEMP_ON_F)
             || ctrlTempF >= SUN_CANOPY_TEMP_ON_F
             || vpd >= SUN_CANOPY_VPD_ON;
  bool ambCool = !ambient.ok || ambient.tempF <= SUN_AMB_TEMP_OFF_F;
  bool cool = ambCool && ctrlTempF <= SUN_CANOPY_TEMP_OFF_F && vpd <= SUN_CANOPY_VPD_OFF;
  unsigned long now = millis();
  if (hot) {
    if (sunHighSince == 0) sunHighSince = now;
    if (now - sunHighSince >= SUN_DETECT_DWELL_MS) sunDetected = true;
  } else {
    sunHighSince = 0;                        // not currently hot → restart the dwell
    if (sunDetected && cool) sunDetected = false;   // clear the latch only when clearly cool
  }
}

// Set the active VPD band from the phase (+ evening sun gate) and publish its midpoint.
void resolveBand() {
  switch (curPhase) {
    case PH_NIGHT:     vpdTargetLo = VPD_NIGHT_LO; vpdTargetHi = VPD_NIGHT_HI; break;
    case PH_MORNING:   vpdTargetLo = VPD_MORN_LO;  vpdTargetHi = VPD_MORN_HI;  break;
    case PH_AFTERNOON: vpdTargetLo = VPD_AFT_LO;   vpdTargetHi = VPD_AFT_HI;   break;
    case PH_EVENING:
      if (sunDetected) { vpdTargetLo = VPD_EVE_LO; vpdTargetHi = VPD_EVE_HI; }  // ride the spike
      else             { vpdTargetLo = VPD_AFT_LO; vpdTargetHi = VPD_AFT_HI; }  // overcast = rest day
      break;
  }
  cloudVpdTarget = (vpdTargetLo + vpdTargetHi) / 2.0;
  strncpy(cloudPhase, phaseName(curPhase), sizeof(cloudPhase) - 1);
  cloudPhase[sizeof(cloudPhase) - 1] = '\0';
}

// Resolve clock phase (test-force > unsynced-fallback > wall clock), then sun + band.
void resolvePhase() {
  timeValid = Time.isValid();
  if (forcedPhase >= 0) {
    curPhase = (Phase)forcedPhase;
  } else if (!timeValid) {
    curPhase = PH_AFTERNOON;                 // before cloud time syncs — never block control
  } else {
    int h = Time.hour();                      // base-offset local (Time.zone set in setup)
    if (TZ_USE_DST && usDstActive()) h = (h + 1) % 24;
    if      (h >= PHASE_NIGHT_START || h < PHASE_MORNING_START) curPhase = PH_NIGHT;
    else if (h <  PHASE_AFTERNOON_START)                        curPhase = PH_MORNING;
    else if (h <  PHASE_EVENING_START)                          curPhase = PH_AFTERNOON;
    else                                                        curPhase = PH_EVENING;
  }
  resolveSun();
  resolveBand();
}

// ---- moisture-regime-gated PI (controlMode 2) ----
const char* regimeName(Regime r) { return r == RG_WET ? "WET" : (r == RG_DRY ? "DRY" : "NEUTRAL"); }
// Excursion phase for telemetry; "" unless mode 3 is active.
const char* excTag() {
  if (controlMode != MODE_EXCURSION) return "";
  if (nightExportOn) return " ex=EXPORT";
  return excState == EX_DRY ? " ex=DRYING" : (excState == EX_MOIST ? " ex=MOIST" : " ex=REST");
}

// Classify the moisture regime from the inside-outside dew-point gap, dual hysteresis.
// WET = chamber water-loaded (venting exports moisture, fog unsafe); DRY = room ~as wet
// (venting futile, fog is the safe lever); NEUTRAL between. No gap / forced → safe handling.
void resolveRegime() {
  if (forcedRegime >= 0) { curRegime = (Regime)forcedRegime; return; }
  if (!dpGapValid)       { curRegime = RG_NEUTRAL; return; }
  switch (curRegime) {
    case RG_WET:
      if (dpGap < DPGAP_WET_OFF_F) curRegime = (dpGap <= DPGAP_DRY_ON_F) ? RG_DRY : RG_NEUTRAL;
      break;
    case RG_DRY:
      if (dpGap > DPGAP_DRY_OFF_F) curRegime = (dpGap >= DPGAP_WET_ON_F) ? RG_WET : RG_NEUTRAL;
      break;
    default: // RG_NEUTRAL
      if      (dpGap >= DPGAP_WET_ON_F) curRegime = RG_WET;
      else if (dpGap <= DPGAP_DRY_ON_F) curRegime = RG_DRY;
      break;
  }
}

// PI on canopy VPD error vs the band midpoint. Output splits by sign into one-directional
// fog/vent efforts (0..1), each gated by the regime interlock + feasibility. Heat untouched.
// Anti-windup: clamp + conditional integration + bleed when the active lever is suppressed.
// Vent authority fades as dpGap→0 (can't dry below room dew point — that needs heat).
void vpdPIUpdate() {
  ventBelowRoom = false;
  if (!sensorsValid) { piState.integ = 0; piState.fogEffort = 0; piState.ventEffort = 0; return; }
  float vpd = computeVPD(ctrlTempF, ctrlRH);
  if (vpd < 0) { piState.fogEffort = 0; piState.ventEffort = 0; return; }
  float sp = (vpdTargetLo + vpdTargetHi) / 2.0;     // band midpoint setpoint
  float e  = sp - vpd;                              // +e too humid (vent), -e too dry (fog)

  if (fabsf(e) < VPD_DEADBAND_PI) { piState.fogEffort = 0; piState.ventEffort = 0; return; }  // hold integral

  bool  wantVent = (e > 0);                         // too humid → dry by venting
  bool  wantFog  = (e < 0);                         // too dry  → fog
  float ventAuth = dpGapValid ? constrainf(dpGap / DPGAP_VENT_FULL_F, 0.0, 1.0) : 0.0;
  bool  ventFeasible = wantVent && (curRegime != RG_DRY) && (ventAuth > 0.0);
  // Fog gate is canopy-local only (below condensation ceiling) — NOT regime-gated. See the
  // excursionUpdate() note: dpGap/regime governs the VENT lever (room-air exchange), but fog
  // is a source and can moisten regardless of how wet the room is. Dropping curRegime != RG_WET
  // stops WET-regime nights/floor-evaporation from stranding VPD above target.
  bool  fogFeasible  = wantFog  && (ctrlRH < RH_CEILING);
  if (wantVent && ventAuth <= 0.0) ventBelowRoom = true;   // want to dry but gap≈0 → needs heat

  float p = VPD_KP * e;
  float iTrial = piState.integ + VPD_KI * e;
  bool  activeFeasible = (wantVent && ventFeasible) || (wantFog && fogFeasible);
  if (!activeFeasible) {
    piState.integ *= 0.90;                          // bleed stale demand when suppressed/infeasible
  } else {
    float outTrial = p + iTrial;
    bool sat = fabsf(outTrial) >= 1.0;
    if (!(sat && ((outTrial > 0) == (e > 0))))      // conditional integration: not further into sat
      piState.integ = constrainf(iTrial, -VPD_I_CLAMP, VPD_I_CLAMP);
  }

  float out = constrainf(p + piState.integ, -1.0, 1.0);
  piState.ventEffort = ventFeasible ? constrainf(out,  0.0, 1.0) * ventAuth : 0.0;
  piState.fogEffort  = fogFeasible  ? constrainf(-out, 0.0, 1.0)            : 0.0;
}

// Map effort (0..1) to an ON window-fraction over VPD_PI_WINDOW_MS (anchored, not per-cycle).
bool piPulseOn(float effort, float minEffort, unsigned long &winStart, unsigned long now) {
  if (effort < minEffort) return false;
  if (winStart == 0 || now - winStart >= VPD_PI_WINDOW_MS) winStart = now;
  unsigned long onMs = (unsigned long)(effort * (float)VPD_PI_WINDOW_MS);
  return (now - winStart) < onMs;
}

// WET regime + VPD well below setpoint → run circ UP (with the vent) to evaporate+exhaust the
// floor reservoir so drying isn't transitory. Bounded by the desiccation cap; mold floor sacred.
bool vpdWantDryDown() {
  if (controlMode != MODE_REGIME_PI || !sensorsValid || curRegime != RG_WET) return false;
  float vpd = computeVPD(ctrlTempF, ctrlRH);
  if (vpd < 0) return false;
  float sp = (vpdTargetLo + vpdTargetHi) / 2.0;
  return (sp - vpd) >= VPD_DRYDOWN_TRIGGER_KPA;
}

// Stall detector for an excursion drive (the room-floor stop). Anchors VPD + time, and re-anchors
// whenever VPD makes EXC_STALL_MIN_KPA of progress toward the target (rising while drying, falling
// while moistening). If no such progress for EXC_STALL_WINDOW_MS, the lever can't move VPD any
// further (vent already at the incoming-air floor, or fog can't keep up) → report stalled.
void excResetStall(float vpd, unsigned long now) { excStallVpd = vpd; excStallSince = now; }
bool excStalled(float vpd, unsigned long now, bool drying) {
  float progress = drying ? (vpd - excStallVpd) : (excStallVpd - vpd);
  if (progress >= EXC_STALL_MIN_KPA) { excResetStall(vpd, now); return false; }   // still working
  return (now - excStallSince) >= EXC_STALL_WINDOW_MS;
}

// Wide-hysteresis excursion controller (controlMode 3). One deliberate swing, then rest:
// commit to drying (or fogging) HARD until VPD overshoots a fixed margin past the FAR band
// edge, then drop the humidity servo for a guaranteed rest plateau and let VPD drift back.
// Same regime interlock + below-room stop as mode 2 choose the feasible lever; heat untouched.
// Sets excWantVent/excWantFog for this cycle and mirrors them into veff/feff for telemetry.
void excursionUpdate() {
  unsigned long now = millis();
  excWantVent = false; excWantFog = false; ventBelowRoom = false;
  if (!sensorsValid) { excState = EX_REST; piState.ventEffort = 0; piState.fogEffort = 0; return; }
  float vpd = computeVPD(ctrlTempF, ctrlRH);
  if (vpd < 0) { excState = EX_REST; piState.ventEffort = 0; piState.fogEffort = 0; return; }

  float bandLo = vpdTargetLo, bandHi = vpdTargetHi;
  // Lever feasibility — identical interlocks to the PI mode. Venting can only export moisture
  // while the room is drier (dpGap > 0); fog is unsafe when water-loaded or already at the ceiling.
  float ventAuth = dpGapValid ? constrainf(dpGap / DPGAP_VENT_FULL_F, 0.0, 1.0) : 0.0;
  bool  canVent  = (curRegime != RG_DRY) && (ventAuth > 0.0);
  // Fog is a SOURCE, not a room-air exchange, so room-relative moisture (dpGap/regime)
  // is irrelevant to it — it can raise canopy RH no matter how wet the room is. Its only
  // real limits are canopy-local: don't fog above the condensation ceiling. "Too dry" is
  // established by the VPD band (wantMoist / EX_MOIST) below. The old `curRegime != RG_WET`
  // clause was a venting interlock wrongly inherited by fog: it locked fog out whenever the
  // chamber read wetter than the room (high dpGap from floor evaporation), stranding VPD
  // above target — solving a non-problem (fog can't over-wet vs the room) by creating a real
  // one. dpGap stays on VENT (canVent), where room-relative moisture is the actual physics.
  bool  canFog   = (ctrlRH < RH_CEILING);
  // Stricter gate to START a dry swing: need a real dew-point gap, not just dpGap > 0, or the
  // vent churns near-equal air to the max-drive timeout for nothing. Continuation uses canVent.
  bool  armDry   = canVent && dpGapValid && (dpGap >= EXC_DRY_ARM_GAP_F);

  switch (excState) {
    case EX_DRY:   // drive until VPD overshoots PAST the dry edge, else the lever quit / stalled / timed out
      if (vpd >= bandHi + EXC_OVERSHOOT_KPA) { excState = EX_REST; excRestUntil = now + EXC_REST_MS; }
      else if (!canVent || excStalled(vpd, now, true) || (now - excDriveStart) >= EXC_MAX_DRIVE_MS) {
        ventBelowRoom = true;                          // drying can't progress (at the room floor) → needs heat
        excState = EX_REST; excRestUntil = now + EXC_REST_MS;
      } else excWantVent = true;
      break;
    case EX_MOIST: // drive until VPD overshoots PAST the wet edge, else fog infeasible / stalled / timed out
      if (vpd <= bandLo - EXC_OVERSHOOT_KPA || !canFog || excStalled(vpd, now, false)
          || (now - excDriveStart) >= EXC_MAX_DRIVE_MS) {
        excState = EX_REST; excRestUntil = now + EXC_REST_MS;
      } else excWantFog = true;
      break;
    default:       // EX_REST: humidity servo OFF, VPD drifts. Re-arm only after the rest plateau,
                   // AND only once the trigger HOLDS for EXC_ARM_DWELL_MS — so a boot/RH transient
                   // on a single sample can't kick off a swing (cf. v1.0.6 "sustained, not instant").
      if (now >= excRestUntil) {
        bool wantDry   = (vpd < bandLo - EXC_TRIGGER_MARGIN);
        bool wantMoist = (vpd > bandHi + EXC_TRIGGER_MARGIN);
        if (wantDry && armDry) {                          // water-loaded + real gap → arm a dry swing after the dwell
          if (excArmSince == 0) excArmSince = now;
          if (now - excArmSince >= EXC_ARM_DWELL_MS) { excState = EX_DRY;   excDriveStart = now; excResetStall(vpd, now); excWantVent = true; excArmSince = 0; }
        } else if (wantMoist && canFog) {                 // too dry → arm a moisten swing after the dwell
          if (excArmSince == 0) excArmSince = now;
          if (now - excArmSince >= EXC_ARM_DWELL_MS) { excState = EX_MOIST; excDriveStart = now; excResetStall(vpd, now); excWantFog  = true; excArmSince = 0; }
        } else {
          excArmSince = 0;                                // trigger broke (transient) or gap too small → reset dwell
          if (wantDry && canVent) ventBelowRoom = true;   // too wet but gap < arm gate → flag and wait
        }
      }
      break;
  }
  piState.ventEffort = excWantVent ? 1.0 : 0.0;   // reuse veff/feff telemetry for the active lever
  piState.fogEffort  = excWantFog  ? 1.0 : 0.0;
}

// Desired fog state from the active canopy VPD band. Fog LOWERS VPD; heat is never touched
// here (the temp loop owns it). Center-restoring hysteresis: fire when VPD leaves the band
// on the dry side (above band-hi, or above the cap during a detected evening spike), then
// fog until VPD is pulled back to the band MIDPOINT (not just the edge) so it doesn't drift
// straight back out and chatter.
bool vpdWantFog(bool fogCurrent) {
  if (!sensorsValid) return fogCurrent;
  float vpd = computeVPD(ctrlTempF, ctrlRH);
  if (vpd < 0) return fogCurrent;
  bool spike = (curPhase == PH_EVENING && sunDetected);
  float mid  = (vpdTargetLo + vpdTargetHi) / 2.0;
  float onAt = spike ? VPD_EVE_CAP : vpdTargetHi;               // fire when VPD leaves the band
  if (!fogCurrent && vpd > onAt + VPD_DEADBAND) return true;    // too dry → fog
  if ( fogCurrent && vpd <= mid)               return false;    // pulled back to centre → stop
  return fogCurrent;                                            // in band → hold
}

// Active-dry vent: pulse the vent to RAISE VPD when the chamber has sat too humid, but only
// when venting would actually dry (ambient air drier in deficit terms). Pulsed (~20% duty)
// so it nudges without crashing temp / over-drying. Returns true when the pulse is ON.
bool vpdWantDryVent(unsigned long now) {
  if (controlMode != MODE_VPD_BANGBANG || !sensorsValid) { vpdLowSince = 0; vpdDryVentOn = false; return false; }
  float vpd = computeVPD(ctrlTempF, ctrlRH);
  if (vpd < 0) { vpdLowSince = 0; vpdDryVentOn = false; return false; }
  float mid = (vpdTargetLo + vpdTargetHi) / 2.0;          // aim for band centre, not the floor
  bool ambientDrier = ambient.ok && (cloudAmbientVPD - vpd) >= VPD_DRY_AMBIENT_MARGIN;
  unsigned long period = VPD_DRY_PULSE_ON_MS + VPD_DRY_PULSE_OFF_MS;

  if (!ambientDrier) {
    vpdLowSince = 0; vpdDryVentOn = false;               // venting can't dry → stand down
  } else if (!vpdDryVentOn) {
    // Arm after a sustained spell BELOW the floor. Reset the dwell only on a clear recovery
    // back to the floor (not on tiny noise), so VPD hovering just under the floor still arms.
    if (vpd < vpdTargetLo) {
      if (vpdLowSince == 0) vpdLowSince = now;
      if (now - vpdLowSince >= VPD_DRY_DWELL_MS) { vpdDryVentOn = true; vpdDryStart = now; }
    } else {
      vpdLowSince = 0;
    }
  } else {
    // Armed. Judge success ONLY on a SETTLED reading — late in an OFF window, vent not running —
    // so the vent+circ inrush during a pulse can't spike the canopy past the target and self-disarm.
    unsigned long ph = (now - vpdDryStart) % period;
    bool settled = ph >= (VPD_DRY_PULSE_ON_MS + VPD_DRY_PULSE_OFF_MS / 2);
    if (settled && vpd >= mid) { vpdDryVentOn = false; vpdLowSince = 0; }
  }

  if (!vpdDryVentOn) return false;
  return ((now - vpdDryStart) % period) < VPD_DRY_PULSE_ON_MS;   // full 1-min ON each cycle
}

// Circulation fan speed, three regimes:
//   1. Active cooling/mist (fog or vent) → full, to disperse mist / feed the vent.
//   2. RH-assist — RH sagging toward the fog-on point → ramp from the idle mix speed
//      up to CIRC_RH_ASSIST_MAX, evaporating more floor water to prop RH up before the
//      fogger fires (flatten the decay; fewer, shallower sawteeth).
//   3. Idle → continuous gentle mixing (homogenize + slowly dry the floor).
int circAutoDuty(bool fogActive, bool ventActive, bool dryDown) {
  if (fogActive)               return CIRC_FOG_DUTY;     // (1) mist dispersion needs full
  if (dryDown)                 return CIRC_DRYDOWN_DUTY; // dry-down: moderated evaporate+exhaust (not 100% → desiccation)
  if (ventActive)              return CIRC_FOG_DUTY;     // genuine cooling vent → feed it full
  if (!sensorsValid)           return CIRC_MIX_DUTY;     // no RH reading → gentle idle
  // (2) RH-assist ramp across [RH_FOG_ON, RH_FOG_ON + RH_ASSIST_BAND].
  float top = RH_FOG_ON + RH_ASSIST_BAND;
  if (ctrlRH >= top)       return CIRC_MIX_DUTY;          // (3) RH healthy → idle mix
  if (ctrlRH <= RH_FOG_ON) return CIRC_RH_ASSIST_MAX;     // at fog-on → max assist
  float frac = (top - ctrlRH) / RH_ASSIST_BAND;          // 0 at top → 1 at fog-on
  return CIRC_MIX_DUTY + (int)((CIRC_RH_ASSIST_MAX - CIRC_MIX_DUTY) * frac);
}

// Does venting actually cool right now? Only if it pulls in cooler air: require a real
// canopy-minus-ambient gap from a VALID ambient reading. Without the sensor we return false —
// we can't confirm venting cools, and the old "room is cooler during the spike" assumption is
// BACKWARDS (6/22 finding: the room runs HOTTER than the tent then). So a sensor failure hands
// cooling back to FOG rather than importing heat; the 99°F emergency backstop still forces the
// vent on a true runaway. This is also the arbiter that suppresses fog-for-cooling in control().
bool ventCools() {
  if (ambient.ok) return (ctrlTempF - ambient.tempF) >= VENT_AMBIENT_DELTA_F;
  return false;
}

// Overnight moisture-export gate (controlMode 3). Fills the hole where the excursion's dry swing
// stalls out at night and rests, letting VPD park below 0.4 kPa while the floor reservoir keeps
// evaporating. Decides ON/OFF only — the duty (NIGHT_EXPORT_DUTY, speed 1) is applied in
// ventAutoDuty. Runs on dpGap (room genuinely drier → exchange exports water mass), latched between
// 0.55 and the night band floor, with the same "sustained, not instantaneous" dwell as every other
// vent gate. Heat is untouched (the temp loop reheats the incoming air → net dehumidify).
bool nightExportVentOn(unsigned long now) {
  nightExportOn = false;
  if (!NIGHT_EXPORT_ENABLED || controlMode != MODE_EXCURSION || !sensorsValid || curPhase != PH_NIGHT) {
    nightExportLatched = false; nightExportSince = 0; return false;
  }
  // Physical export gate: room must be meaningfully drier in dew point (the same gate that arms a dry
  // swing) and not in the DRY regime, or venting just churns near-equal air. Cold floor → let heat win.
  bool canExport = dpGapValid && (dpGap >= EXC_DRY_ARM_GAP_F) && (curRegime != RG_DRY)
                   && (ctrlTempF >= NIGHT_EXPORT_TEMP_FLOOR_F);
  float vpd = computeVPD(ctrlTempF, ctrlRH);
  if (!canExport || vpd < 0) { nightExportLatched = false; nightExportSince = 0; return false; }

  // Latched hysteresis: arm once VPD has held below the wet-trough floor for the dwell; hold until
  // pulled back up to the band floor so it doesn't chatter around a single threshold.
  if (!nightExportLatched) {
    if (vpd < NIGHT_EXPORT_VPD_ON) {
      if (nightExportSince == 0) nightExportSince = now;
      if (now - nightExportSince >= NIGHT_EXPORT_DWELL_MS) nightExportLatched = true;
    } else nightExportSince = 0;
  } else if (vpd >= NIGHT_EXPORT_VPD_OFF) {
    nightExportLatched = false; nightExportSince = 0;
  }
  nightExportOn = nightExportLatched;
  return nightExportOn;
}

// Vent fan (reactive): above VENT_TEMP_ON_F, PULSE-cool (short bursts so RH recovers)
// while venting can actually help; plus an always-allowed continuous high-RH relief
// valve. (Periodic exchange schedule optional.)
int ventAutoDuty(unsigned long now) {
  bool exchangeWindow = false;
  if (VENT_SCHEDULE_ENABLED) {
    if (now - ventCycleStart >= VENT_INTERVAL_MS) ventCycleStart = now;
    exchangeWindow = (now - ventCycleStart) < VENT_DURATION_MS;
  }

  bool coolPulseOn = false;
  if (sensorsValid) {
    // Cool-vent: above VENT_TEMP_ON_F enter PULSE mode (tempVentOn); leave under
    // VENT_TEMP_OFF_F or when venting can't cool. Anchor the pulse to entry so the first
    // ON burst hits the peak immediately, then cycle ON_MS on / OFF_MS off while in mode.
    bool canCoolOn  = ventCools();   // >= _ON gap: a lead worth STARTING to vent on
    // Once running, keep chasing down to the tighter _OFF gap (elastic ambient floor). No valid
    // ambient → fail closed (release), same as the engage gate.
    bool canCoolOff = ambient.ok && (ctrlTempF - ambient.tempF) >= VENT_AMBIENT_DELTA_OFF;
    if (!tempVentOn && canCoolOn && ctrlTempF >= VENT_TEMP_ON_F)          { tempVentOn = true; ventPulseStart = now; }
    else if (tempVentOn && (!canCoolOff || ctrlTempF <= VENT_TEMP_OFF_F)) tempVentOn = false;
    if (tempVentOn) {
      unsigned long period = VENT_PULSE_ON_MS + VENT_PULSE_OFF_MS;
      coolPulseOn = ((now - ventPulseStart) % period) < VENT_PULSE_ON_MS;
    }
    // RH-relief: humidity safety valve (last resort — open the box). Only opens once RH
    // has stayed at/above VENT_RH_ON for VENT_RH_DWELL_MS, so brief spikes (e.g. a fog
    // burst) are ignored. Exit hysteresis: keep venting until RH falls under VENT_RH_OFF.
    if (ctrlRH >= VENT_RH_ON) {
      if (rhHighSince == 0) rhHighSince = now;                  // start the dwell clock
      if (now - rhHighSince >= VENT_RH_DWELL_MS) rhVentOn = true;
    } else {
      rhHighSince = 0;                                          // sustained-high broken → reset
      if (rhVentOn && ctrlRH <= VENT_RH_OFF) rhVentOn = false;
    }
  } else {
    tempVentOn = false; rhVentOn = false; rhHighSince = 0;
  }

  // Humidity dry-vent: mode 2 = regime-gated PI effort (time-duty); mode 1 = legacy pulse;
  // mode 0 = none. (piState efforts are refreshed in control() before this runs.)
  bool dryVentOn = false;
  if      (controlMode == MODE_EXCURSION)    dryVentOn = excWantVent;
  else if (controlMode == MODE_REGIME_PI)    dryVentOn = piPulseOn(piState.ventEffort, VPD_VENT_MIN_EFFORT, ventPiWin, now);
  else if (controlMode == MODE_VPD_BANGBANG) dryVentOn = vpdWantDryVent(now);

  // Overnight gentle export runs the vent at speed 1 — but any full-duty reason (cooling, RH relief,
  // active dry swing, scheduled exchange) outranks it. Always evaluate it so its latch + circ-idle
  // flag stay current even when a full-duty reason is also firing.
  bool nightExport = nightExportVentOn(now);
  if (exchangeWindow || coolPulseOn || rhVentOn || dryVentOn) return VENT_DUTY;
  return nightExport ? NIGHT_EXPORT_DUTY : 0;
}

void control() {
  unsigned long now = millis();
  resolvePhase();   // updates curPhase / sunDetected / vpdTargetLo,Hi for the VPD layer
  resolveRegime();  // moisture regime from the dew-point gap (drives the mode-2 interlock)
  if      (controlMode == MODE_REGIME_PI) vpdPIUpdate();    // refresh PI fog + dry-vent efforts
  else if (controlMode == MODE_EXCURSION) excursionUpdate(); // refresh excursion lever demands
  bool overheat = sensorsValid && (ctrlTempF >= TEMP_SAFETY_F);

  // ===== 1) AUTOMATIC heat/fog decisions (hysteresis) =====
  bool autoHeat = false, autoFog = false;
  if (sensorsValid) {
    if (overheat)                               autoHeat = false;
    else if (!heatOn && ctrlTempF < HEAT_ON_F)  autoHeat = true;
    else if (heatOn  && ctrlTempF > HEAT_OFF_F) autoHeat = false;
    else                                        autoHeat = heatOn;

    if (controlMode == MODE_EXCURSION) {
      // Excursion fog demand (moistening swing). Fog cools ONLY when venting can't (vent-first); ceiling down.
      autoFog = excWantFog;
      // Independent RH/VPD-hold (7/10). The excursion's MOIST swing rests for EXC_REST_MS (+ re-arm dwell)
      // after every swing, and its stall detector quits a swing in ~1 min whenever a cooling-vent pulse
      // keeps re-drying the air (both levers share the canopy VPD signal, so venting reads as "fog isn't
      // progressing"). That starved fog during the 7/09-10 solar bakes — canopy VPD parked at 3-4 kPa
      // while fog sat in a 12-min rest and RH fell to ~32%. This term keeps fog servoing the active VPD
      // band CONTINUOUSLY (center-restoring, evening-cap aware — same logic mode 1 uses), with NO
      // reference to excState/rest/stall, so a solar spike gets near-constant fog for RH while the
      // excursion/vent independently own temperature. It only fires when too DRY (VPD above band), so it's
      // a no-op in the overnight wet trough; fog-for-cooling (below) and the RH ceiling are untouched.
      if (vpdWantFog(fogOn))                      autoFog = true;
      // Night: fog OFF — circ over the floor pool is the night RH lever now (7/14), plant is asleep
      // (stomata closed) so a loose VPD costs nothing. Placed BEFORE the cooling override so a freak
      // hot night can still re-enable fog for cooling; the RH-hold/excursion night fog is what's killed.
      if (curPhase == PH_NIGHT)                   autoFog = false;
      if (ctrlTempF > COOL_ON_F && !ventCools()) autoFog = true;
      if (ctrlRH >= RH_CEILING)  autoFog = false;
    } else if (controlMode == MODE_REGIME_PI) {
      // PI fog effort → time-duty. Regime interlock already zeroed fogEffort in WET. Fog cools
      // ONLY when venting can't (vent-first); RH ceiling stays a hard wet backstop.
      autoFog = piPulseOn(piState.fogEffort, VPD_FOG_MIN_EFFORT, fogPiWin, now);
      if (ctrlTempF > COOL_ON_F && !ventCools()) autoFog = true;
      if (ctrlRH >= RH_CEILING)  autoFog = false;
    } else if (controlMode == MODE_VPD_BANGBANG) {
      // Diurnal VPD bang-bang (center-restoring). Fog cools ONLY when venting can't (vent-first).
      autoFog = vpdWantFog(fogOn);
      if (ctrlTempF > COOL_ON_F && !ventCools()) autoFog = true;
      if (ctrlRH >= RH_CEILING)  autoFog = false;
    } else {
      // mode 0: original RH/temp bang-bang.
      bool wantHumidity;
      if (!fogOn && ctrlRH < RH_FOG_ON)       wantHumidity = true;
      else if (fogOn && ctrlRH > RH_FOG_OFF)  wantHumidity = false;
      else                                    wantHumidity = fogOn;

      bool wantCooling;
      if (ctrlTempF > COOL_ON_F)       wantCooling = true;
      else if (ctrlTempF < COOL_OFF_F) wantCooling = false;
      else                             wantCooling = fogOn;
      if (ventCools()) wantCooling = false;   // vent-first: let the vent shed heat, keep fog on RH duty

      autoFog = (wantHumidity || wantCooling);
      if (ctrlRH >= RH_CEILING) autoFog = false;
    }
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

  // ===== 5) fans — vent first (primary heat removal), then circ moves air to it =====
  int ventReq = ventManual ? ventOverrideDuty : ventAutoDuty(now);
  // Mode-3 dry excursion: hold circ at the idle mix floor instead of letting the vent ramp it
  // to 100%. Forced convection over the standing floor water evaporates it straight back into
  // the air (RH up, VPD down — worst at night), fighting the excursion. Let the vent dry; circ
  // just idles at the mold-safe floor. (Fog still wins for mist dispersion if temp-cooling fires.)
  bool excDrying = (controlMode == MODE_EXCURSION && (excState == EX_DRY || nightExportOn));
  int circReq;
  if      (circManual)          circReq = circOverrideDuty;
  else if (excDrying && !fogOn)  circReq = CIRC_MIX_DUTY;
  else                           circReq = circAutoDuty(fogOn, ventReq > 0, vpdWantDryDown());
  // NIGHT circ = the primary RH lever (fog is off overnight). Hold continuous at CIRC_NIGHT_MIN to
  // evaporate the floor pool and keep RH up + air moving (anti-stagnation; nights now run ~80°F/~82% RH,
  // so stagnant saturated air is the fungal risk the old hard-zero ignored). ONLY back off if circ has
  // pulled canopy VPD under the wet floor (VPD_NIGHT_LO) — and even then PULSE, never hard-off, so there's
  // always some stir; the export vent does the real drying. Floor only RAISES a too-low idle; fog/vent-feed
  // (100) and RH-assist (35) still win.
  if (controlMode == MODE_EXCURSION && curPhase == PH_NIGHT && !circManual) {
    int nightFloor = CIRC_NIGHT_MIN;
    if (sensorsValid && computeVPD(ctrlTempF, ctrlRH) < VPD_NIGHT_LO) {   // over-wet → pulse instead of continuous
      unsigned long period = CIRC_NIGHT_WET_PULSE_ON_MS + CIRC_NIGHT_WET_PULSE_OFF_MS;
      nightFloor = (now % period) < CIRC_NIGHT_WET_PULSE_ON_MS ? CIRC_NIGHT_MIN : 0;
    }
    if (circReq < nightFloor) circReq = nightFloor;
  }

  // ===== 5a) HEATER CIRC INTERLOCK — circ must move air whenever the heater is on =====
  // Above the night hard-zero (and above a manual hold: safety only RAISES the duty, so a manual
  // setCirc >= HEAT_CIRC_MIN still wins, but you can't manually kill circ while the element cooks).
  // Fog/vent-feed (100) already exceed this floor, so it only bites on the idle/off cases. Keyed on
  // heatOn = the D2 relay state — so it covers the ceramic only once it's on D2 (external thermostat
  // tonight = firmware can't see it, circ may still be zero while it fires).
  if (heatOn && circReq < HEAT_CIRC_MIN) circReq = HEAT_CIRC_MIN;

  // ===== 5b) EMERGENCY BACKSTOP — only at a true runaway temp =====
  // Normal high-temp cooling is the vent hysteresis (VENT_TEMP_ON_F) + fog. This forces
  // the vent full OVER a manual hold only past VENT_EMERGENCY_F, so "vent less" can't
  // become "no protection" if fog fails on a brutal spike while unattended. Rarely fires
  // (the 94° hysteresis already has the vent full by here); it just adds override-manual.
  // (Heater is already locked off at TEMP_SAFETY_F above; circ rides full with the vent.)
  if (sensorsValid && ctrlTempF >= VENT_EMERGENCY_F) { ventReq = VENT_DUTY; circReq = CIRC_FOG_DUTY; }

  // ===== 5c) STAGNANCY STIR — if the vent hasn't moved air for an hour, restore a brief circ pulse =====
  // With circ OFF at night (above), a long calm stretch can stratify the air. The clock resets whenever
  // the vent fires for ANY reason (cooling, RH relief, dry swing, or the gentle night export); after
  // STAGNANT_VENT_GAP_MS with no vent, run a short speed-1 circ stir every STAGNANT_STIR_PERIOD_MS.
  // Manual circ holds and anything already faster (fog / vent feed) win — the stir only raises a too-low idle.
  stagnantStirOn = false;
  if (ventReq > 0) {
    ventLastFired = now;
  } else if (sensorsValid && !circManual && (now - ventLastFired) >= STAGNANT_VENT_GAP_MS) {
    if ((now - ventLastFired) % STAGNANT_STIR_PERIOD_MS < STAGNANT_STIR_ON_MS
        && circReq < STAGNANT_STIR_DUTY) {
      circReq = STAGNANT_STIR_DUTY;
      stagnantStirOn = true;
    }
  }

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
      "T=%.1fF RH=%.0f%% VPD=%.2f | heat=%s fog=%s circ=%d%% vent=%d%% [%s] m%d %s gap=%.1f %s→%.2f%s%s%s%s%s%s",
      ctrlTempF, ctrlRH, cloudVPD,
      heatOn ? "ON":"off", fogOn ? "ON":"off", circReq, ventReq, cloudMode,
      controlMode, regimeName(curRegime), cloudDpGap,
      phaseName(curPhase), cloudVpdTarget, sunDetected ? "(sun)" : "",
      overheat ? " OVERHEAT" : "",
      (ctrlRH >= RH_CEILING) ? " RH-ceiling" : "",
      ventBelowRoom ? " BELOW-ROOM" : "",
      stagnantStirOn ? " STIR" : "", excTag());
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
// setPhase("-1") => live clock; setPhase("0".."3") => force PH_NIGHT/MORNING/AFTERNOON/EVENING;
// setPhase("3,sun") => force evening WITH sun detected (test the hardening pulse on the bench).
int fnSetPhase(String arg) {
  arg.trim();
  int comma = arg.indexOf(',');
  String p = (comma >= 0 ? arg.substring(0, comma) : arg); p.trim();
  int n = (p.length() ? p.toInt() : -1);
  if (n < -1) n = -1; if (n > 3) n = 3;
  forcedPhase = n;
  String s = (comma >= 0 ? arg.substring(comma + 1) : ""); s.trim();
  forcedSun = (s == "1" || s.equalsIgnoreCase("sun"));
  Particle.publish("everfresh/cmd", String::format("phase %d sun=%d", forcedPhase, forcedSun ? 1 : 0), PRIVATE);
  return forcedPhase;
}
// setControlMode("0|1|2|3") -> 0 RH bang-bang, 1 VPD bang-bang, 2 regime-PI, 3 excursion. The
// "guide back": flip to 2/1/0 live (no reflash) if the excursion layer misbehaves. See VPD_PI_CONTROL.md.
int fnSetControlMode(String arg) {
  arg.trim();
  int m = arg.toInt();
  if (m < 0) m = 0; if (m > 3) m = 3;
  controlMode = m;
  piState.integ = 0; piState.fogEffort = 0; piState.ventEffort = 0;   // clean handoff
  excState = EX_REST; excRestUntil = 0; excArmSince = 0; excWantVent = false; excWantFog = false;
  nightExportLatched = false; nightExportSince = 0; nightExportOn = false;   // clean handoff
  Particle.publish("everfresh/cmd", String::format("controlMode %d", controlMode), PRIVATE);
  return controlMode;
}
// setRegime("-1") => live (dew-point gap); "0|1|2" => force DRY/NEUTRAL/WET for bench testing
// without spoofing the ambient sensor.
int fnSetRegime(String arg) {
  arg.trim();
  int r = (arg.length() ? arg.toInt() : -1);
  if (r < -1) r = -1; if (r > 2) r = 2;
  forcedRegime = r;
  Particle.publish("everfresh/cmd", String::format("regime %d", forcedRegime), PRIVATE);
  return forcedRegime;
}

// -------------------- telemetry --------------------

void publishTelemetry() {
  char json[440];
  snprintf(json, sizeof(json),
    "{\"ct\":%.1f,\"crh\":%.0f,\"vpd\":%.2f,"
    "\"at\":%.1f,\"arh\":%.0f,\"avpd\":%.2f,"
    "\"heat\":%d,\"fog\":%d,\"circ\":%d,\"vent\":%d,\"mode\":\"%s\","
    "\"phase\":\"%s\",\"vtgt\":%.2f,\"sun\":%d,"
    "\"cm\":%d,\"cdp\":%.1f,\"adp\":%.1f,\"dpgap\":%.1f,\"rg\":\"%s\",\"veff\":%.2f,\"feff\":%.2f,"
    "\"fw\":\"%s\"}",
    cloudCanopyT, cloudCanopyRH, cloudVPD,
    cloudAmbientT, cloudAmbientRH, cloudAmbientVPD,
    cloudHeat, cloudFog, circDuty, ventDuty, cloudMode,
    phaseName(curPhase), cloudVpdTarget, sunDetected ? 1 : 0,
    controlMode, cloudCanopyDP, cloudAmbientDP, cloudDpGap, regimeName(curRegime),
    piState.ventEffort, piState.fogEffort, cloudVersion);
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
  pinMode(PIN_VENT_PWM, OUTPUT);
  pinMode(PIN_VENT_PWR, OUTPUT);

  // Known-safe boot state: everything off.
  writeRelay(PIN_HEAT, false);
  writeRelay(PIN_FOG,  false);
  writeCirc(0);   // circ MOSFET off + PWM 0
  writeVent(0);   // vent MOSFET off

  Wire.begin();   // canopy SHT31 on the hardware bus
  swI2CInit();    // ambient SHT31 on its dedicated software bus (D5/D6)

  Time.zone(TZ_BASE_OFFSET);   // base local offset; DST added per-cycle in resolvePhase()

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
  Particle.variable("phase",        cloudPhase);
  Particle.variable("vpdTarget",    cloudVpdTarget);
  Particle.variable("controlMode",  controlMode);
  Particle.variable("dpGap",        cloudDpGap);
  Particle.variable("version",      cloudVersion);   // read with: particle get <device> version

  Particle.function("setFog",         fnSetFog);         // arg: "1"/"0" [,seconds]
  Particle.function("setHeat",        fnSetHeat);        // arg: "1"/"0" [,seconds]
  Particle.function("setCirc",        fnSetCirc);        // arg: "duty" [,seconds]
  Particle.function("setVent",        fnSetVent);        // arg: "duty" [,seconds]
  Particle.function("clearOverrides", fnClearOverrides); // arg: (ignored)
  Particle.function("setPhase",       fnSetPhase);       // arg: "-1" live | "0..3" | "3,sun"
  Particle.function("setControlMode", fnSetControlMode); // arg: "0" RH | "1" VPD | "2" regime-PI
  Particle.function("setRegime",      fnSetRegime);      // arg: "-1" live | "0..2" force DRY/NEUT/WET

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