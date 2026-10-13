/**
 * Prochain Départ
 *
 *  Created on: 21.11.2024
 *  Migrated to M5Paper S3: 2026-01-11
 *  WiFi Manager added: 2026-01-11
 *  Power Management added: 2026-01-11
 *  StopPoint Configuration added: 2026-01-11
 *  Power/battery rework: 2026-10-13
 *
 * Power model (see docs/power_optimization.md):
 *  - The CPU light-sleeps until the next minute boundary; each wake does at most
 *    one e-ink update (countdowns + clock are recomputed locally).
 *  - WiFi is only powered for a fetch every FETCH_INTERVAL_S, with fast
 *    reconnect to the last AP (no scan) and one TLS session for all PRIM calls.
 *  - Nothing is fetched or redrawn during quiet hours.
 *  - Touching the screen wakes the device: tap = refresh, hold 5 s = setup portal
 *    (QR code on screen; stop search when the device is online).
 */
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <esp_sntp.h>
#include <driver/gpio.h>
#include <driver/usb_serial_jtag.h>

#include <config.h>
#include <transport_api.h>
#include <weather_api.h>
#include <station_config.h>  // NVS-based station configuration (FR-SC1)
#include <setup_portal.h>
#include "types.h"
#include "display.h"

#define USE_SERIAL Serial

// ============================================================
// TIMING
// ============================================================
#define FETCH_INTERVAL_S        120     // Transit data refresh; countdowns tick locally in between
#define FETCH_MAX_BACKOFF_S     900     // Max retry delay when WiFi/API keeps failing
#define MIN_TAP_FETCH_GAP_S     20      // Ignore refresh taps closer than this (debounce / spurious INT)
#define WEATHER_INTERVAL_S      1800    // 30 minutes
#define INCIDENT_INTERVAL_S     300     // Traffic incidents (one gzip line report per line)
#define FULL_REFRESH_INTERVAL_S 1800    // Full e-ink refresh to clear ghosting
#define NTP_RESYNC_INTERVAL_S   (6 * 3600)  // The RTC slow clock drifts during light sleep
#define STALE_DEPARTURE_GRACE_S 30      // Keep a departure on screen this long after its time

// Quiet hours: no fetch, no redraw (local time, [start, end) ). Set equal to disable.
#define QUIET_START_HOUR        1
#define QUIET_END_HOUR          5
#define QUIET_TAP_OVERRIDE_S    600     // A tap during quiet hours shows live data this long

#define WIFI_FAST_CONNECT_MS    4000    // Reconnect to cached AP/channel (no scan)
#define WIFI_SCAN_CONNECT_MS    8000    // Fallback: WiFiMulti scan + connect

#define USB_HOST_PROBE_MS       500     // After light sleep, time for a computer to resume talking to the USB port
#define LONG_PRESS_MS           5000    // Hold to open the configuration portal
#define TOUCH_INT_PIN           GPIO_NUM_48  // GT911 INT on M5Paper S3 (active low)

#define TZ_PARIS "CET-1CEST,M3.5.0,M10.5.0/3"
#define MIN_VALID_EPOCH 1700000000      // Anything earlier means the clock was never set

// ============================================================
// STATE (kept in RAM across light sleep)
// ============================================================
WiFiMulti wifiMulti;

static std::vector<departureType> departures[MAX_STATIONS];
static std::vector<incidentType> incidents[MAX_STATIONS];
static StopSeen stationSeen[MAX_STATIONS];  // Lines and destinations seen at each stop
static weatherType currentWeather = {};
static bool weatherValid = false;

static time_t nextFetchEpoch = 0;
static time_t lastFetchAttemptEpoch = 0;
static time_t lastSuccessEpoch = 0;
static time_t lastWeatherEpoch = 0;
static time_t lastIncidentEpoch = 0;
static time_t lastNtpSyncEpoch = 0;
static time_t lastFullRefreshEpoch = 0;
static time_t quietOverrideUntil = 0;
static int consecutiveFailures = 0;
static int lastRssi = 0;
static bool needFullRefresh = true;
static bool quietScreenShown = false;
static bool wokeByTouch = false;

// Last AP we connected to, so the next connect can skip the full channel scan
static char cachedSsid[33];
static char cachedPass[65];
static uint8_t cachedBssid[6];
static int32_t cachedChannel = 0;

static volatile bool sntpSynced = false;

// ============================================================
// WIFI / TIME
// ============================================================
static bool connectWifi() {
    WiFi.mode(WIFI_STA);
    
    if (cachedChannel > 0) {
        WiFi.begin(cachedSsid, cachedPass, cachedChannel, cachedBssid);
        if (WiFi.waitForConnectResult(WIFI_FAST_CONNECT_MS) == WL_CONNECTED) {
            return true;
        }
        USE_SERIAL.println("Fast reconnect failed, scanning");
        cachedChannel = 0;
        WiFi.disconnect();
    }
    
    if (wifiMulti.run(WIFI_SCAN_CONNECT_MS) == WL_CONNECTED) {
        strlcpy(cachedSsid, WiFi.SSID().c_str(), sizeof(cachedSsid));
        strlcpy(cachedPass, WiFi.psk().c_str(), sizeof(cachedPass));
        memcpy(cachedBssid, WiFi.BSSID(), sizeof(cachedBssid));
        cachedChannel = WiFi.channel();
        return true;
    }
    return false;
}

static void wifiOff() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

static void onSntpSync(struct timeval*) {
    sntpSynced = true;
}

// Needs WiFi. Restarts SNTP so it sends a request right away.
static void syncTime() {
    sntpSynced = false;
    configTzTime(TZ_PARIS, "pool.ntp.org", "time.nist.gov");
    uint32_t start = millis();
    while (!sntpSynced && millis() - start < 5000) {
        delay(50);
    }
    if (sntpSynced) {
        lastNtpSyncEpoch = time(nullptr);
        USE_SERIAL.println("NTP sync ok");
    } else {
        USE_SERIAL.println("NTP sync failed");
        time_t now = time(nullptr);
        if (now >= MIN_VALID_EPOCH) {
            // Clock still usable: retry in 30 min rather than on every fetch
            lastNtpSyncEpoch = now - NTP_RESYNC_INTERVAL_S + 1800;
        }
    }
}

static bool isQuietTime(time_t now) {
    if (QUIET_START_HOUR == QUIET_END_HOUR) return false;
    struct tm local;
    localtime_r(&now, &local);
    int h = local.tm_hour;
    if (QUIET_START_HOUR < QUIET_END_HOUR) return h >= QUIET_START_HOUR && h < QUIET_END_HOUR;
    return h >= QUIET_START_HOUR || h < QUIET_END_HOUR;
}

static uint32_t secondsUntilQuietEnd(time_t now) {
    struct tm local;
    localtime_r(&now, &local);
    int secsOfDay = local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec;
    int end = QUIET_END_HOUR * 3600;
    int delta = end - secsOfDay;
    if (delta <= 0) delta += 24 * 3600;
    return (uint32_t)delta;
}

// ============================================================
// FETCH CYCLE (the only time WiFi is powered)
// ============================================================
// Incidents of the lines each stop shows (the line of its filter, or the lines
// of its recent departures), in the direction its trains go. A failed lookup
// keeps the previous list.
static void fetchStationIncidents() {
    RuntimeStationConfig* stations = getStations();
    bool allOk = true;
    for (int i = 0; i < MAX_STATIONS; i++) {
        if (!stations[i].isConfigured) {
            incidents[i].clear();
            continue;
        }
        std::vector<String> lines;
        if (strncmp(stations[i].filter, "STIF:Line::", 11) == 0) {
            lines.push_back(stations[i].filter);
        } else {
            lines = stationSeen[i].lines;
        }
        if (lines.empty()) continue;  // No departure seen yet: nothing to look up

        std::vector<incidentType> found;
        bool ok = true;
        for (const String& line : lines) {
            ok &= fetchIncidents(line.c_str(), stations[i].monitoringRef, stationSeen[i], found);
        }
        if (ok) incidents[i] = std::move(found);
        allOk &= ok;
    }
    if (allOk) lastIncidentEpoch = time(nullptr);
}

static void runFetchCycle() {
    lastFetchAttemptEpoch = time(nullptr);
    uint32_t startMs = millis();
    
    bool ok = connectWifi();
    if (ok) {
        lastRssi = WiFi.RSSI();
        time_t now = time(nullptr);
        if (now < MIN_VALID_EPOCH || now - lastNtpSyncEpoch >= NTP_RESYNC_INTERVAL_S) {
            syncTime();
        }
        
        RuntimeStationConfig* stations = getStations();
        for (int i = 0; i < MAX_STATIONS; i++) {
            if (!stations[i].isConfigured) continue;
            char endpoint[128];
            buildEndpointUrl(stations[i].monitoringRef, endpoint, sizeof(endpoint));
            USE_SERIAL.printf("Fetching station %d: %s\n", i, stations[i].displayName);
            // On failure the previous list is kept; past departures drop off at render time
            StopSeen seen;
            if (fetchDepartures(endpoint, stations[i].type, stations[i].filter, departures[i], &seen)) {
                // Keep what was last seen when no departure is announced
                if (!seen.lines.empty()) stationSeen[i] = std::move(seen);
            } else {
                ok = false;
            }
        }
        
        now = time(nullptr);
        if (ok && now - lastIncidentEpoch >= INCIDENT_INTERVAL_S) {
            fetchStationIncidents();
        }
        transportEnd();
        
        now = time(nullptr);
        if (!weatherValid || now - lastWeatherEpoch >= WEATHER_INTERVAL_S) {
            if (fetchWeather(currentWeather)) {
                weatherValid = true;
                lastWeatherEpoch = now;
            }
        }
    } else {
        USE_SERIAL.println("WiFi connection failed");
    }
    wifiOff();
    
    time_t now = time(nullptr);
    if (ok) {
        lastSuccessEpoch = now;
        consecutiveFailures = 0;
        // Count from the start of the cycle so fetches stay on the per-minute wakes
        nextFetchEpoch = lastFetchAttemptEpoch + FETCH_INTERVAL_S;
    } else {
        consecutiveFailures++;
        long backoff = 60L << min(consecutiveFailures - 1, 4);
        nextFetchEpoch = now + min(backoff, (long)FETCH_MAX_BACKOFF_S);
    }
    USE_SERIAL.printf("Fetch cycle %s in %lu ms\n", ok ? "ok" : "failed", millis() - startMs);
}

// ============================================================
// SETUP PORTAL
// ============================================================
static void runSetup() {
    // Online: stops are picked from the home network with live search.
    // Offline: access point first to choose a network.
    displayLoader("Configuration - connexion au WiFi...");
    bool online = connectWifi();
    runSetupPortal(wifiMulti, online);
    wifiOff();
    
    // Stations, weather location or network may have changed: start over from fresh data
    cachedChannel = 0;
    for (int i = 0; i < MAX_STATIONS; i++) {
        departures[i].clear();
        incidents[i].clear();
        stationSeen[i] = StopSeen();
    }
    lastIncidentEpoch = 0;
    weatherValid = false;  // Location may have changed
    consecutiveFailures = 0;
    needFullRefresh = true;
    nextFetchEpoch = 0;
}

// ============================================================
// RENDER (one e-ink update per wake)
// ============================================================
static void render(time_t now) {
    RuntimeStationConfig* stations = getStations();
    std::vector<departureType> visible[MAX_STATIONS];
    BoardView boards[3];
    for (int i = 0; i < 3; i++) {
        if (i < MAX_STATIONS) {
            for (const auto& d : departures[i]) {
                if (d.expected + STALE_DEPARTURE_GRACE_S >= now) visible[i].push_back(d);
            }
        }
        boards[i].title = (i < MAX_STATIONS && stations[i].isConfigured) ? stations[i].displayName : "";
        boards[i].departures = &visible[i];
        boards[i].incidents = i < MAX_STATIONS ? &incidents[i] : nullptr;
    }
    
    struct tm local;
    localtime_r(&now, &local);
    char timeStr[6];
    strftime(timeStr, sizeof(timeStr), "%H:%M", &local);
    
    StatusInfo status;
    status.timeStr = timeStr;
    status.batteryLevel = M5.Power.getBatteryLevel();
    status.isCharging = M5.Power.isCharging();
    status.dataAgeSecs = lastSuccessEpoch ? (int)(now - lastSuccessEpoch) : -1;
    status.wifiRssi = lastRssi;
    
    bool full = needFullRefresh || now - lastFullRefreshEpoch >= FULL_REFRESH_INTERVAL_S;
    drawScreen(boards, currentWeather, status, now, full);
    if (full) {
        lastFullRefreshEpoch = now;
        needFullRefresh = false;
    }
}

// ============================================================
// TOUCH + SLEEP
// ============================================================
enum TouchAction { TOUCH_NONE, TOUCH_TAP, TOUCH_LONG };

static TouchAction readTouch() {
    bool touched = M5.Touch.getCount() > 0;
    if (!touched) {
        // A quick tap may be over by the time we are awake and polling
        return wokeByTouch ? TOUCH_TAP : TOUCH_NONE;
    }
    uint32_t start = millis();
    while (M5.Touch.getCount() > 0) {
        if (millis() - start >= LONG_PRESS_MS) return TOUCH_LONG;
        delay(20);
        M5.update();
    }
    return TOUCH_TAP;
}

// True when plugged into a computer (it polls the USB port with SOF packets; a
// charger or power bank never does). M5.Power.isCharging() alone isn't enough:
// the PaperS3 has no VBUS sense, and its charge pin goes idle once the battery
// is full. Light sleep drops the USB link, so after a wake the host needs a
// moment to see the device again before this can return true.
static bool usbHostConnected() {
    uint32_t start = millis();
    while (!usb_serial_jtag_is_connected()) {
        if (millis() - start >= USB_HOST_PROBE_MS) return false;
        delay(20);
    }
    return true;
}

// Sleep (or idle when on USB) for `seconds`, waking early on touch.
static void idleFor(uint32_t seconds) {
    // The EPD waveform runs in a background task with the panel's high-voltage
    // rails on: sleeping before it finishes corrupts the image and leaves the
    // rails powered for the whole sleep.
    M5.Display.waitDisplay();
    wokeByTouch = false;
    if (seconds == 0) return;
    
    if (M5.Power.isCharging() || usbHostConnected()) {
        // On USB: stay awake so the port stays up for flashing/serial and
        // touch stays responsive, but don't spin at full speed
        uint32_t until = millis() + seconds * 1000UL;
        while ((int32_t)(until - millis()) > 0) {
            M5.update();
            if (M5.Touch.getCount() > 0) return;
            delay(50);
        }
        return;
    }
    
    // Only arm touch wake if INT is idle (high); otherwise we'd wake instantly forever
    bool touchWake = gpio_get_level(TOUCH_INT_PIN) == 1;
    if (touchWake) {
        gpio_wakeup_enable(TOUCH_INT_PIN, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();
    }
    esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
    USE_SERIAL.flush();
    esp_light_sleep_start();
    
    wokeByTouch = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO;
    if (touchWake) {
        gpio_wakeup_disable(TOUCH_INT_PIN);
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    }
}

static uint32_t secondsToNextMinute(time_t now) {
    // +1 s so we land just after the minute flips
    return (uint32_t)(60 - (now % 60)) + 1;
}

// ============================================================
// SETUP / LOOP
// ============================================================
void setup()
{
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);
    M5.Display.setRotation(1);  // Landscape
    
    setenv("TZ", TZ_PARIS, 1);
    tzset();
    sntp_set_time_sync_notification_cb(onSntpSync);
    
    displayLoader("Horaire...");
    
    initStationConfig();
    USE_SERIAL.printf("Loaded %d station configurations\n", getStationCount());
    
    for (int i = 0; i < NUM_WIFI_CREDENTIALS; i++) {
        wifiMulti.addAP(WIFI_CREDENTIALS[i].ssid, WIFI_CREDENTIALS[i].password);
    }
    // Also try the network saved by the WiFiManager portal
    WiFi.mode(WIFI_STA);
    wifi_config_t saved;
    if (esp_wifi_get_config(WIFI_IF_STA, &saved) == ESP_OK && saved.sta.ssid[0] != 0) {
        wifiMulti.addAP((const char*)saved.sta.ssid, (const char*)saved.sta.password);
        USE_SERIAL.printf("Added saved WiFi: %s\n", (const char*)saved.sta.ssid);
    }
    WiFi.persistent(false);  // Don't rewrite WiFi config to flash on every connect
    WiFi.mode(WIFI_OFF);
}

void loop()
{
    M5.update();
    
    TouchAction touch = readTouch();
    time_t now = time(nullptr);
    if (touch == TOUCH_LONG) {
        USE_SERIAL.println("Long press - entering configuration mode");
        runSetup();
        return;
    }
    bool forceFetch = false;
    if (touch == TOUCH_TAP && now - lastFetchAttemptEpoch >= MIN_TAP_FETCH_GAP_S) {
        USE_SERIAL.println("Tap - forcing refresh");
        forceFetch = true;
        lastIncidentEpoch = 0;  // A tap asks for everything fresh
        quietOverrideUntil = now + QUIET_TAP_OVERRIDE_S;
    }
    
    bool clockValid = now >= MIN_VALID_EPOCH;
    
    if (clockValid && !forceFetch && now >= quietOverrideUntil && isQuietTime(now)) {
        if (!quietScreenShown) {
            char msg[80];
            snprintf(msg, sizeof(msg), "En veille jusqu'a %02d:00 - touchez l'ecran pour actualiser", QUIET_END_HOUR);
            displayLoader(msg);
            quietScreenShown = true;
        }
        idleFor(secondsUntilQuietEnd(now) + 1);
        return;
    }
    if (quietScreenShown) {
        quietScreenShown = false;
        needFullRefresh = true;
        nextFetchEpoch = 0;
    }
    
    if (forceFetch || !clockValid || now >= nextFetchEpoch) {
        runFetchCycle();
        now = time(nullptr);
        clockValid = now >= MIN_VALID_EPOCH;
    }
    
    if (clockValid) {
        render(now);
        // Wake at the next minute, or earlier if a fetch is due before then
        uint32_t sleepS = secondsToNextMinute(now);
        if (nextFetchEpoch > now && (uint32_t)(nextFetchEpoch - now) < sleepS) {
            sleepS = (uint32_t)(nextFetchEpoch - now);
        }
        idleFor(sleepS);
    } else {
        // No clock means no WiFi yet: say so once, then retry with backoff
        if (consecutiveFailures == 1) {
            displayLoader("Pas de connexion WiFi - nouvel essai bientot");
            needFullRefresh = true;
        }
        idleFor((uint32_t)max<long>(nextFetchEpoch - now, 30));
    }
}
