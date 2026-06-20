# EVERFRESH — Data Logging to Google Sheets

Logs temp/RH and every actuator on/off event to a Google Sheet, for daily history
and charting. Pipeline:

```
Photon  --publish-->  Particle Cloud  --webhook-->  Google Apps Script  --append-->  Google Sheet
```

The firmware already publishes everything needed (no code changes):

| Event | When | Example payload |
|-------|------|-----------------|
| `everfresh/telemetry` | every 60 s | `{"ct":78.4,"crh":61,"heat":0,"fog":0,"fan":0,"mode":"auto"}` |
| `everfresh/event`     | actuator toggles | `{"ev":"fog","state":"on","ct":78.2,"crh":54}` |
| `everfresh/alert`     | alarm change | `overheat` |
| `everfresh/cmd`       | manual override | `fogger 10s` |

---

## Step 1 — Create the Sheet + Apps Script

1. Create a new Google Sheet (name it e.g. **EVERFRESH log**).
2. **Copy its ID** from the URL — the part between `/d/` and `/edit`:
   `https://docs.google.com/spreadsheets/d/`**`<SHEET_ID>`**`/edit`
3. **Extensions → Apps Script.**
4. Delete the stub `myFunction`, paste in the contents of
   [sheets-logger.gs](sheets-logger.gs), **paste your Sheet ID into `SHEET_ID`** at
   the top, and **Save**.

> The script opens the sheet by ID (`openById`) rather than
> `getActiveSpreadsheet()`, so it works whether the script is bound to the sheet or
> standalone — `getActiveSpreadsheet()` returning null is a common silent failure.

## Step 2 — Deploy as a Web App

1. **Deploy → New deployment.**
2. Gear icon → select type **Web app**.
3. Settings:
   - **Execute as:** Me (your account)
   - **Who has access:** **Anyone** ← must be anonymous so Particle can POST
4. **Deploy**, authorize when prompted (it'll warn it's an unverified app — it's
   your own script; continue).
5. Copy the **Web app URL** — it ends in `/exec`. This is your webhook target.

> Sanity check: paste that `/exec` URL into a browser. You should see
> *"EVERFRESH logger is live."* (that's the `doGet` handler).

## Step 3 — Create the Particle webhook

In the [Particle Console](https://console.particle.io) → **Integrations → New
Integration → Webhook**:

- **Event name:** `everfresh`  ← prefix match, catches all `everfresh/*` events
- **URL:** your `/exec` URL from Step 2
- **Request type:** `POST`
- **Request format:** `JSON`
- **Device:** your Photon (or "Any")
- Leave the request body **at its default** — Particle then sends
  `{ "event", "data", "published_at", "coreid" }` with `data` properly escaped,
  which is exactly what the script expects.

Save (and **Enable** it).

## Step 4 — Verify

Trigger something and watch a row land in the Sheet:

```bash
particle call <device-name> runFogger "10"     # fires everfresh/cmd + everfresh/event
```

Within a few seconds you should see rows appear. The 60 s telemetry will steadily
add rows on its own. Columns:

| published_at | local | event | tempF | rh | vpd_kpa | heat | fog | fan | mode | change | state | raw |
|---|---|---|---|---|---|---|---|---|---|---|---|---|

`vpd_kpa` is Vapor Pressure Deficit (kPa), computed from `tempF`/`rh` — a better
humidity-stress metric than raw RH because it factors in temperature. Set
`LEAF_OFFSET_C` in the script to ~`-2` if you'd rather log *leaf* VPD than air VPD.

- **Telemetry rows** fill `tempF…mode`.
- **Event rows** fill `change` (heat/fog/vent) + `state` (on/off), plus `tempF/rh`
  at that instant.
- **alert/cmd rows** land in `raw` (plus the `event` column tells you which).

## Charting

Select the `tempF` / `rh` columns → **Insert → Chart → Line chart**. For a live
view, base the chart on the whole column so it grows as rows append.

---

## Notes & gotchas

- **Apps Script redirects:** Web-app POSTs return a 302 to
  `script.googleusercontent.com`; Particle's webhook follows it automatically.
  If rows never appear, re-check that access is **Anyone** (anonymous), not
  "Anyone with Google account."
- **Re-deploying the script (IMPORTANT):** editing the code does **nothing** to the
  live endpoint until you redeploy. Use **Deploy → Manage deployments → Edit
  (pencil) → Version: New version → Deploy.** The same `/exec` URL keeps working.
  This is the #1 reason a "fixed" script still misbehaves — the old version is
  still live.
- **`errors` tab:** if a webhook arrives but something throws (e.g. a wrong
  `SHEET_ID`), the script logs the error and the raw body to an `errors` tab
  instead of failing silently. Check there if the `log` tab stays empty.
- **Volume:** 60 s telemetry = ~1,440 rows/day. A Sheet holds plenty for months;
  archive or roll to a new tab occasionally if you want to keep it snappy.
- **Particle rate limit:** ~1 published event/sec average (burst 4). The firmware
  is well under this — telemetry is once a minute and actuator toggles are rare.
- **Time zone:** `published_at` is UTC (ISO 8601). The script also writes a
  readable **`local`** column (e.g. `6/18 2:58PM`) using `Utilities.formatDate`
  with the `TZ` constant — set `TZ` to your zone (e.g. `America/Los_Angeles`);
  named zones apply daylight-saving automatically.
