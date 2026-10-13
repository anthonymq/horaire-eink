# Power Optimization & Loop Logic

How the Prochain Départ display on the M5Paper S3 saves battery. Constants are
defined at the top of `src/main.cpp`, except the fetch/incident intervals and
battery thresholds, which are in `lib/transit_core/src/power_policy.h` (unit-tested).

## Principles

1. **Wake once per minute, at most one e-ink update per wake.** The CPU light-sleeps
   until the next minute boundary. Departure countdowns ("4 min") and the clock are
   recomputed locally from the stored departure times, so the screen stays current
   without any network traffic.
2. **WiFi only during a fetch.** The radio is off the rest of the time. The interval
   adapts (`fetchIntervalS()`):
   - 120 s (`FETCH_INTERVAL_S`) while a departure is within 20 min,
   - 300 s (`FETCH_IDLE_INTERVAL_S`) when nothing runs within 20 min (or nothing at all),
   - twice as long on low battery. Incidents: every 5 min, every 15 min on low battery.
   - Fast reconnect: the device reuses the last AP's BSSID and channel, so it skips the
     full WiFiMulti scan. It only scans if that reconnect fails.
   - One TLS session for all PRIM requests (HTTP keep-alive). A TLS 1.3 handshake
     costs more than the request itself.
   - The SIRI JSON is parsed through an ArduinoJson filter that keeps only the fields
     the display uses.
   - Weather (current + 24 h forecast) is fetched every 30 min.
   - Time: each NTP sync is written to the BM8563 RTC (UTC). After every light-sleep
     wake the system clock is reset from the RTC, because the ESP32 slow clock drifts
     during light sleep. NTP then only resyncs every 24 h (6 h if the RTC is missing or
     lost power: its voltage-low flag is checked at boot).
   - On failure, retries back off exponentially (60 s → 15 min). The last good data
     stays on screen; departures whose time has passed drop off.
3. **Quiet hours** (`QUIET_START_HOUR`–`QUIET_END_HOUR`, 01:00–05:00 by default). The
   device shows a standby screen, then:
   - on battery, with a valid RTC: **powers off** (`M5.Power.timerSleep()`); the RTC
     alarm powers it back on at the end. The side button powers it on early; a boot
     during quiet hours counts as a tap (live data for 10 min). An ESP32 timer wake is
     armed too, in case the power latch doesn't cut.
   - otherwise (USB, or no RTC time): light-sleeps through the window as before
     (touch wakes it).
4. **Battery modes** (`batteryState()`, ignored on USB; the level is read at wake,
   before WiFi, because the voltage sags under radio load):
   - ≤ 15 % (`BATTERY_LOW_PCT`): fetches and incidents slow down, the percentage is bold.
   - ≤ 5 % (`BATTERY_CRITICAL_PCT`) on two readings in a row: a "Batterie vide" screen,
     then power off. Plugging in USB (or the side button) powers it back on.
5. **Cheap e-ink updates.** Only rows whose text changed are redrawn. Partial updates
   use the `epd_text` waveform. A full `epd_quality` refresh runs every 30 min to clear
   ghosting.
6. **Wait for the panel before sleeping.** M5GFX drives the EPD from a background task
   with the high-voltage rails on. `M5.Display.waitDisplay()` must finish before
   `esp_light_sleep_start()`; otherwise the image gets corrupted and the rails stay
   powered during sleep.

## Interaction

The M5Paper S3 has no user button: `M5.BtnA` never fires unless touch buttons are
configured. The touch panel's INT pin (GPIO 48, active low) is used as a light-sleep
wake source.

| Gesture | Effect |
|---------|--------|
| Tap | Fetch now (debounced to once per 20 s). During quiet hours on USB, shows live data for 10 min |
| Hold 5 s | WiFi/station configuration portal, firmware update (`/update`) |
| Side button | Powers on after a quiet-hours or battery power-off |

When on USB (charging), the device doesn't sleep. Instead it polls touch every 50 ms.

## Loop

```text
loop():
  read touch ─ hold 5s → config portal
             └ tap     → force fetch
  read power   → battery empty twice → "charge me" screen → power off
  quiet hours? → standby screen (once) → battery: power off until RTC alarm
                                        USB: idle until end → return
  fetch due?   → WiFi on → [NTP → RTC] → PRIM ×N (one TLS session) → [incidents] → [weather] → WiFi off
                 next fetch in 2 / 5 min (×2 on low battery)
  render()     → one partial (or periodic full) e-ink update
  waitDisplay → light sleep until next minute or next fetch (timer or touch wake) → clock from RTC
```

## Tuning knobs

| Constant | Default | Trade-off |
|----------|---------|-----------|
| `FETCH_INTERVAL_S` | 120 s | Main battery lever: each fetch is the most expensive thing the device does |
| `FETCH_IDLE_INTERVAL_S` / `NEAR_DEPARTURE_S` | 300 s / 20 min | Slower fetches when nothing is about to leave |
| `BATTERY_LOW_PCT` / `BATTERY_CRITICAL_PCT` | 15 % / 5 % | Save mode / power off |
| `QUIET_START_HOUR` / `QUIET_END_HOUR` | 1 / 5 | Set them equal to disable quiet hours |
| `FULL_REFRESH_INTERVAL_S` | 30 min | Longer saves energy but leaves more ghosting |
| `WEATHER_INTERVAL_S` | 30 min | |
