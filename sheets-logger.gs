/*
 * EVERFRESH — Google Sheets logger (Google Apps Script)
 *
 * Receives Particle webhook POSTs and appends one row per event to a "log" sheet.
 * Handles all everfresh/* events: telemetry, event (actuator toggles), alert, cmd.
 *
 * Setup: see LOGGING.md. In short:
 *   1. Open your Google Sheet; copy its ID from the URL:
 *        https://docs.google.com/spreadsheets/d/<THIS_IS_THE_ID>/edit
 *      and paste it into SHEET_ID below.
 *   2. Extensions -> Apps Script, paste this whole file in, Save.
 *   3. Deploy -> New deployment -> Web app, execute as you, access "Anyone".
 *   4. Copy the /exec URL into a Particle webhook (event filter: "everfresh").
 *   --- After ANY later edit: Deploy -> Manage deployments -> Edit -> New version.
 */

var SHEET_ID = '11bLXCx34tZWXnYe-mk9Jl52WSTpmxNaIX0cW2fmCqa8';

// Your timezone — a named zone so daylight-saving is handled automatically.
// (https://en.wikipedia.org/wiki/List_of_tz_database_time_zones)
var TZ = 'America/Los_Angeles';

// VPD leaf-temperature offset (°C). 0 = "air VPD" (correct for a single air
// sensor). Set to about -2 to estimate "leaf VPD" (leaves run a touch cooler than
// air from transpiration), which is what many growers tune to.
var LEAF_OFFSET_C = 0;

// Vapor Pressure Deficit in kPa, from temp (°F) and RH (%). Returns '' if either
// is missing (e.g. alert/cmd rows). Uses the Tetens saturation-pressure equation.
function computeVPD(tempF, rh) {
  var t = Number(tempF), r = Number(rh);
  if (tempF === '' || rh === '' || isNaN(t) || isNaN(r)) return '';
  var sat = function (tc) { return 0.61078 * Math.exp((17.27 * tc) / (tc + 237.3)); }; // kPa
  var tAirC   = (t - 32) * 5 / 9;
  var avp     = sat(tAirC) * (r / 100);        // actual vapor pressure (from air RH)
  var svpLeaf = sat(tAirC + LEAF_OFFSET_C);    // saturation at leaf temp (offset 0 = air)
  return Math.round((svpLeaf - avp) * 100) / 100;   // kPa, 2 decimals
}

function doPost(e) {
  try {
    // Particle's default webhook body: { event|name, data, published_at, coreid }.
    var body = JSON.parse(e.postData.contents);
    var name = body.event || body.name || '';
    var at   = body.published_at || new Date().toISOString();
    var raw  = body.data != null ? body.data : '';

    // data is a string; telemetry/event payloads are JSON, alert/cmd are plain.
    var d = {};
    try { d = JSON.parse(raw); } catch (err) { d = {}; }

    var ss = SpreadsheetApp.openById(SHEET_ID);   // robust whether bound or standalone
    var sheet = ss.getSheetByName('log') || ss.insertSheet('log');

    if (sheet.getLastRow() === 0) {
      sheet.appendRow(['published_at', 'local', 'event',
                       'tempF', 'rh', 'vpd_kpa',
                       'atempF', 'arh', 'avpd_kpa',
                       'heat', 'fog', 'circ', 'vent', 'mode',
                       'change', 'state', 'raw']);
    }

    // Readable local time, DST-correct via the named zone. -> "6/18 2:58PM"
    var local = '';
    try { local = Utilities.formatDate(new Date(at), TZ, 'M/d h:mma'); } catch (err) {}

    var pick = function (k) { return d[k] !== undefined ? d[k] : ''; };

    sheet.appendRow([
      at, local, name,
      pick('ct'),  pick('crh'),  computeVPD(pick('ct'),  pick('crh')),   // canopy
      pick('at'),  pick('arh'),  computeVPD(pick('at'),  pick('arh')),   // ambient
      pick('heat'), pick('fog'), pick('circ'), pick('vent'), pick('mode'),
      pick('ev'), pick('state'),
      raw
    ]);

    return ContentService
      .createTextOutput(JSON.stringify({ ok: true }))
      .setMimeType(ContentService.MimeType.JSON);

  } catch (err) {
    // Surface failures into an "errors" tab so problems aren't silent.
    try {
      var dbg = SpreadsheetApp.openById(SHEET_ID).getSheetByName('errors')
                || SpreadsheetApp.openById(SHEET_ID).insertSheet('errors');
      dbg.appendRow([new Date().toISOString(), String(err),
                     (e && e.postData) ? e.postData.contents : '(no body)']);
    } catch (e2) { /* nothing else we can do */ }

    return ContentService
      .createTextOutput(JSON.stringify({ ok: false, error: String(err) }))
      .setMimeType(ContentService.MimeType.JSON);
  }
}

// Open the /exec URL in a browser to confirm the deployment is live.
function doGet(e) {
  return ContentService
    .createTextOutput('EVERFRESH logger is live. POST Particle events here.')
    .setMimeType(ContentService.MimeType.TEXT);
}

// Run this manually from the Apps Script editor (select testWrite -> Run).
// It forces the authorization prompt and proves the SHEET_ID + write path work,
// independent of the webhook. A row should land in the "log" tab.
function testWrite() {
  var ss = SpreadsheetApp.openById(SHEET_ID);
  var sheet = ss.getSheetByName('log') || ss.insertSheet('log');
  sheet.appendRow([new Date().toISOString(), 'now', 'TEST',
                   99.9, 42, computeVPD(99.9, 42),
                   72.0, 50, computeVPD(72.0, 50),
                   1, 0, 60, 100, 'test', 'manual', 'on',
                   '{"note":"manual testWrite ok"}']);
  Logger.log('testWrite appended a row to sheet ' + SHEET_ID);
}
