# EVERFRESH — daily chart snapshots

Renders temp / RH / VPD charts (1h, 3h, 24h windows) from the Google Sheet log into
a dated folder, once a day. Re-plotted from the logged data with matplotlib — not a
screenshot of the Google chart, but the same three series.

```
snapshots/
  2026-06-21/
    1h.png
    3h.png
    24h.png
  2026-06-22/
    ...
```

## One-time setup

**1. Make the sheet readable.** In the Sheet: **Share → General access → Anyone with the
link → Viewer** (or File → Share → Publish to web). View-only is enough; the script never
writes. Without this the script exits with a clear "not publicly readable" error.

**2. Create a venv + install matplotlib** (paths match the launchd plist):

```bash
cd /Users/arabaghdassarian/Desktop/Work/EVRFRSH/tools
python3 -m venv .venv
.venv/bin/pip install --upgrade pip matplotlib
```

**3. Test it once by hand:**

```bash
.venv/bin/python3 sheet_snapshots.py
```

You should see PNGs land in `EVERFRESH/snapshots/<today>/`.

**4. Schedule it daily (launchd):**

```bash
cp com.everfresh.snapshots.plist ~/Library/LaunchAgents/
launchctl load ~/Library/LaunchAgents/com.everfresh.snapshots.plist
launchctl start com.everfresh.snapshots    # fire once now to confirm
```

It then runs every day at **21:00** (9 PM). To retime, edit `Hour`/`Minute` in the plist,
then `launchctl unload` + `load` it again.

## The snapshot moment vs. the run time (sleep-proof)

Each snapshot represents a **fixed daily moment** — `TARGET_HOUR` (default 21:00 / 9 PM)
— *not* whenever the script happens to run. So if the Mac is asleep at 9 PM and launchd
runs the job when you wake it the next morning, it still renders the **9 PM** view
(folder named by that date). The telemetry itself lives in Google Sheets 24/7 regardless
of the Mac, so no data is ever lost — only the local image render depends on this machine.

**Multi-day backfill.** A default run doesn't just do the latest day — it walks back and
renders **every missing daily 9 PM** since the last one it produced, oldest-first, then
stops at the first already-rendered day. So one catch-up run after a multi-day sleep fills
the whole gap. Bounded by `MAX_BACKFILL_DAYS` (30) and by how far back the sheet's data
goes; days the device was offline near 9 PM are skipped.

Manual control:

```bash
.venv/bin/python3 sheet_snapshots.py                         # backfill all missing days
.venv/bin/python3 sheet_snapshots.py --force                 # re-render existing days too
.venv/bin/python3 sheet_snapshots.py --asof now              # just current data, one shot
.venv/bin/python3 sheet_snapshots.py --asof 2026-06-18T21:00 # one specific past moment
```

## Tuning

- **Snapshot moment:** `TARGET_HOUR` / `TARGET_MINUTE` in `sheet_snapshots.py`
  (keep in sync with the plist's run time).
- **Run time:** `Hour`/`Minute` in the plist.
- **Time windows:** edit `WINDOWS` in `sheet_snapshots.py` (e.g. add `"48h": 48`).
- **Backfill cap:** `MAX_BACKFILL_DAYS` in `sheet_snapshots.py`.
- **Output location / colors / timezone:** the `CONFIG` block in `sheet_snapshots.py`.

## Notes

- Logs: `tools/snapshots.out.log` and `tools/snapshots.err.log`.
- The window is bounded by the anchor moment on both ends, and column names are matched
  leniently (works against both the old `VPD`/`fan` sheet and the new `vpd_kpa`/`circ`/`vent` one).
- `snapshots/` and `tools/.venv/` are gitignored — they're local artifacts.
