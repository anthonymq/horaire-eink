# Power Optimization & Loop Logic

How the Prochain Départ display on the M5Paper S3 saves battery. Every constant
mentioned here is defined at the top of `src/main.cpp`.

## Principles

1. **Wake once per minute, at most one e-ink update per wake.** The CPU light-sleeps
   until the next minute boundary. Departure countdowns ("4 min") and the clock are
   recomputed locally from the stored departure times, so the screen stays current
   without any network traffic.
2. **WiFi only during a fetch** (`FETCH_INTERVAL_S` = 120 s). The radio is off the rest
   of the time.
   - Fast reconnect: the device reuses the last AP's BSSID and channel, so it skips the
     full WiFiMulti scan. It only scans if that reconnect fails.
   - One TLS session for all PRIM requests (HTTP keep-alive). A TLS 1.3 handshake
     costs more than the request itself.
   - The SIRI JSON is parsed through an ArduinoJson filter that keeps only the fields
     the display uses.
   - Weather is fetched every 30 min. NTP resyncs every 6 h, because the RTC slow
     clock drifts during light sleep.
   - On failure, retries back off exponentially (60 s → 15 min). The last good data
     stays on screen; departures whose time has passed drop off.
3. **Quiet hours** (`QUIET_START_HOUR`–`QUIET_END_HOUR`, 01:00–05:00 by default). The
   device shows a standby screen and sleeps through the whole window: no fetches, no
   redraws.
4. **Cheap e-ink updates.** Only rows whose text changed are redrawn. Partial updates
   use the `epd_text` waveform. A full `epd_quality` refresh runs every 30 min to clear
   ghosting.
5. **Wait for the panel before sleeping.** M5GFX drives the EPD from a background task
   with the high-voltage rails on. `M5.Display.waitDisplay()` must finish before
   `esp_light_sleep_start()`; otherwise the image gets corrupted and the rails stay
   powered during sleep.

## Interaction

The M5Paper S3 has no user button: `M5.BtnA` never fires unless touch buttons are
configured. The touch panel's INT pin (GPIO 48, active low) is used as a light-sleep
wake source.

| Gesture | Effect |
|---------|--------|
| Tap | Fetch now (debounced to once per 20 s). During quiet hours, shows live data for 10 min |
| Hold 5 s | WiFi/station configuration portal |

When on USB (charging), the device doesn't sleep. Instead it polls touch every 50 ms.

## Loop

```text
loop():
  read touch ─ hold 5s → config portal
             └ tap     → force fetch
  quiet hours? → standby screen (once) → light sleep until end → return
  fetch due?   → WiFi on → [NTP] → PRIM ×N (one TLS session) → [weather] → WiFi off
  render()     → one partial (or periodic full) e-ink update
  waitDisplay → light sleep until next minute or next fetch (timer or touch wake)
```

## Tuning knobs

| Constant | Default | Trade-off |
|----------|---------|-----------|
| `FETCH_INTERVAL_S` | 120 s | Main battery lever: each fetch is the most expensive thing the device does |
| `QUIET_START_HOUR` / `QUIET_END_HOUR` | 1 / 5 | Set them equal to disable quiet hours |
| `FULL_REFRESH_INTERVAL_S` | 30 min | Longer saves energy but leaves more ghosting |
| `WEATHER_INTERVAL_S` | 30 min | |
