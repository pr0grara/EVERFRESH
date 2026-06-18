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
      sheet.appendRow(['published_at', 'event', 'tempF', 'rh',
                       'heat', 'fog', 'fan', 'mode', 'change', 'state', 'raw']);
    }

    var pick = function (k) { return d[k] !== undefined ? d[k] : ''; };

    sheet.appendRow([
      at, name,
      pick('ct'), pick('crh'),
      pick('heat'), pick('fog'), pick('fan'), pick('mode'),
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
  sheet.appendRow([new Date().toISOString(), 'TEST', 99.9, 42,
                   1, 0, 100, 'test', 'manual', 'on',
                   '{"note":"manual testWrite ok"}']);
  Logger.log('testWrite appended a row to sheet ' + SHEET_ID);
}
