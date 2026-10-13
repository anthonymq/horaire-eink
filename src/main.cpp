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
 *  - Touching the screen wakes the device: tap = refresh, hold 5 s = setup portal.
 */
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiManager.h>  // tzapu/WiFiManager - captive portal for WiFi configuration
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <esp_sntp.h>
#include <driver/gpio.h>

#include <config.h>
#include <transport_api.h>
#include <weather_api.h>
#include <station_config.h>  // NVS-based station configuration (FR-SC1)
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
#define FULL_REFRESH_INTERVAL_S 1800    // Full e-ink refresh to clear ghosting
#define NTP_RESYNC_INTERVAL_S   (6 * 3600)  // The RTC slow clock drifts during light sleep
#define STALE_DEPARTURE_GRACE_S 30      // Keep a departure on screen this long after its time

// Quiet hours: no fetch, no redraw (local time, [start, end) ). Set equal to disable.
#define QUIET_START_HOUR        1
#define QUIET_END_HOUR          5
#define QUIET_TAP_OVERRIDE_S    600     // A tap during quiet hours shows live data this long

#define WIFI_FAST_CONNECT_MS    4000    // Reconnect to cached AP/channel (no scan)
#define WIFI_SCAN_CONNECT_MS    8000    // Fallback: WiFiMulti scan + connect

#define LONG_PRESS_MS           5000    // Hold to open the configuration portal
#define TOUCH_INT_PIN           GPIO_NUM_48  // GT911 INT on M5Paper S3 (active low)

#define CONFIG_PORTAL_TIMEOUT   120     // Portal timeout in seconds
#define AP_NAME "ProchainDepart-Setup"  // AP name for configuration

#define TZ_PARIS "CET-1CEST,M3.5.0,M10.5.0/3"
#define MIN_VALID_EPOCH 1700000000      // Anything earlier means the clock was never set

// ============================================================
// STATE (kept in RAM across light sleep)
// ============================================================
WiFiMulti wifiMulti;
WiFiManager wm;

static std::vector<departureType> departures[MAX_STATIONS];
static weatherType currentWeather = {};
static bool weatherValid = false;

static time_t nextFetchEpoch = 0;
static time_t lastFetchAttemptEpoch = 0;
static time_t lastSuccessEpoch = 0;
static time_t lastWeatherEpoch = 0;
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
// CONFIG PORTAL
// ============================================================
static void startConfigPortal() {
    USE_SERIAL.println("Starting WiFi configuration portal...");
    
    // Display instructions on screen
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK);
    
    // Title with icon-like box
    M5.Display.fillRect(0, 0, 960, 60, TFT_BLACK);
    M5.Display.setFont(&FreeSansBold12pt7b);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextDatum(textdatum_t::middle_center);
    M5.Display.drawString("Mode Configuration WiFi", 480, 30);
    M5.Display.setTextDatum(textdatum_t::top_left);
    M5.Display.setTextColor(TFT_BLACK);
    
    int y = 100;
    int lineHeight = 32;
    int leftMargin = 80;
    
    // Step 1
    M5.Display.setFont(&FreeSansBold12pt7b);
    M5.Display.setCursor(leftMargin, y);
    M5.Display.print("1. Sur votre telephone ou ordinateur:");
    y += lineHeight;
    
    M5.Display.setFont(&FreeSans9pt7b);
    M5.Display.setCursor(leftMargin + 30, y);
    M5.Display.print("Connectez-vous au reseau WiFi:");
    y += lineHeight;
    
    // AP name in a box
    M5.Display.setFont(&FreeSansBold12pt7b);
    int apNameWidth = M5.Display.textWidth(AP_NAME);
    int boxX = leftMargin + 30;
    M5.Display.drawRect(boxX, y - 5, apNameWidth + 20, 35, TFT_BLACK);
    M5.Display.setCursor(boxX + 10, y + 18);
    M5.Display.print(AP_NAME);
    y += 50;
    
    // Step 2
    M5.Display.setFont(&FreeSansBold12pt7b);
    M5.Display.setCursor(leftMargin, y);
    M5.Display.print("2. Ouvrez votre navigateur web:");
    y += lineHeight;
    
    M5.Display.setFont(&FreeSans9pt7b);
    M5.Display.setCursor(leftMargin + 30, y);
    M5.Display.print("Allez a l'adresse:");
    y += lineHeight;
    
    // URL in a box
    M5.Display.setFont(&FreeSansBold12pt7b);
    const char* url = "http://192.168.4.1";
    int urlWidth = M5.Display.textWidth(url);
    M5.Display.drawRect(boxX, y - 5, urlWidth + 20, 35, TFT_BLACK);
    M5.Display.setCursor(boxX + 10, y + 18);
    M5.Display.print(url);
    y += 50;
    
    // Step 3
    M5.Display.setFont(&FreeSansBold12pt7b);
    M5.Display.setCursor(leftMargin, y);
    M5.Display.print("3. Configurez votre WiFi et vos arrets");
    y += lineHeight + 10;
    
    // Step 4
    M5.Display.setCursor(leftMargin, y);
    M5.Display.print("4. L'appareil redemarrera automatiquement");
    y += lineHeight + 20;
    
    // Timeout info at bottom
    M5.Display.setFont(&FreeSans9pt7b);
    M5.Display.setTextColor(0x7BEF);  // Gray
    char timeoutMsg[80];
    snprintf(timeoutMsg, sizeof(timeoutMsg), "Le portail se fermera apres %d secondes d'inactivite", CONFIG_PORTAL_TIMEOUT);
    M5.Display.setCursor(leftMargin, y);
    M5.Display.print(timeoutMsg);
    y += lineHeight;
    
    // StopPoint info
    M5.Display.setCursor(leftMargin, y);
    M5.Display.print("IDs des arrets: prim.iledefrance-mobilites.fr/fr/jeux-de-donnees/arrets-lignes");
    
    M5.Display.setTextColor(TFT_BLACK);  // Reset
    M5.Display.display();
    
    RuntimeStationConfig* currentStations = getStations();
    
    static const char* const slotLabels[MAX_STATIONS] = {
        "<h4>Station 1 (Top Section - RER)</h4>",
        "<h4>Station 2 (Bottom Left - Bus)</h4>",
        "<h4>Station 3 (Bottom Right - Bus)</h4>",
    };
    static const char* const defaultTypes[MAX_STATIONS] = { "RER", "BUS", "BUS" };
    
    // Parameter IDs must outlive the portal; they are string literals.
    static const char* const nameIds[MAX_STATIONS]   = { "s1_name", "s2_name", "s3_name" };
    static const char* const refIds[MAX_STATIONS]    = { "s1_ref", "s2_ref", "s3_ref" };
    static const char* const typeIds[MAX_STATIONS]   = { "s1_type", "s2_type", "s3_type" };
    static const char* const filterIds[MAX_STATIONS] = { "s1_filter", "s2_filter", "s3_filter" };
    
    WiFiManagerParameter stationHeader("<br><hr><h3>Station Configuration</h3><p>Leave blank to use defaults. Format: STIF:StopPoint:Q:12345:</p>");
    wm.addParameter(&stationHeader);
    
    WiFiManagerParameter* params[MAX_STATIONS][5];
    for (int i = 0; i < MAX_STATIONS; i++) {
        const RuntimeStationConfig& cur = currentStations[i];
        bool has = cur.isConfigured;
        params[i][0] = new WiFiManagerParameter(slotLabels[i]);
        params[i][1] = new WiFiManagerParameter(nameIds[i], "Display Name", has ? cur.displayName : "", STATION_DISPLAY_LEN);
        params[i][2] = new WiFiManagerParameter(refIds[i], "MonitoringRef (StopPoint ID)", has ? cur.monitoringRef : "", STATION_REF_LEN);
        params[i][3] = new WiFiManagerParameter(typeIds[i], "Type (RER/BUS/METRO)", has ? cur.type : defaultTypes[i], STATION_TYPE_LEN);
        params[i][4] = new WiFiManagerParameter(filterIds[i], "Filter (RER platform / bus LineRef)", has ? cur.filter : "", STATION_FILTER_LEN);
        for (auto* p : params[i]) wm.addParameter(p);
    }
    
    static bool shouldSaveStationConfig;
    shouldSaveStationConfig = false;
    wm.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT);
    wm.setBreakAfterConfig(true);  // Exit portal after config saved
    wm.setSaveConfigCallback([]() {
        USE_SERIAL.println("Configuration saved!");
        shouldSaveStationConfig = true;
    });
    
    bool connected = wm.startConfigPortal(AP_NAME);
    
    if (shouldSaveStationConfig) {
        // Stations keep their slot (slot 0 = top board). The count is the highest
        // configured slot + 1 so that skipped slots don't hide later ones.
        int highestSlot = -1;
        for (int i = 0; i < MAX_STATIONS; i++) {
            const char* ref = params[i][2]->getValue();
            if (ref == nullptr || ref[0] == '\0') continue;
            
            RuntimeStationConfig cfg = {};
            const char* name = params[i][1]->getValue();
            if (name == nullptr || name[0] == '\0') name = ref;
            strlcpy(cfg.name, name, sizeof(cfg.name));
            strlcpy(cfg.displayName, name, sizeof(cfg.displayName));
            strlcpy(cfg.monitoringRef, ref, sizeof(cfg.monitoringRef));
            strlcpy(cfg.type, params[i][3]->getValue(), sizeof(cfg.type));
            strlcpy(cfg.filter, params[i][4]->getValue(), sizeof(cfg.filter));
            cfg.showNames = (strcmp(cfg.type, "RER") == 0);
            cfg.maxEntries = 6;
            cfg.isConfigured = true;
            saveStationConfig(i, cfg);
            highestSlot = i;
            USE_SERIAL.printf("  Station %d: %s (%s)\n", i + 1, cfg.displayName, cfg.monitoringRef);
        }
        if (highestSlot >= 0) {
            saveStationCount(highestSlot + 1);
        }
    }
    
    if (connected) {
        USE_SERIAL.println("Configuration complete, restarting...");
        displayLoader("Configuration terminee! Redemarrage...");
        M5.Display.waitDisplay();
        delay(1000);
        ESP.restart();
    }
    
    // ESP.restart() never returns, so the parameters only need freeing here
    for (int i = 0; i < MAX_STATIONS; i++) {
        for (auto* p : params[i]) delete p;
    }
    
    USE_SERIAL.println("Config portal timeout or cancelled, resuming normal operation");
    WiFi.mode(WIFI_OFF);
    needFullRefresh = true;
    nextFetchEpoch = 0;
}

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
            ok &= fetchDepartures(endpoint, stations[i].type, stations[i].filter, departures[i]);
        }
        transportEnd();
        
        now = time(nullptr);
        if (!weatherValid || now - lastWeatherEpoch >= WEATHER_INTERVAL_S) {
            if (fetchWeather(WEATHER_CITY, currentWeather)) {
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

// Sleep (or idle when on USB) for `seconds`, waking early on touch.
static void idleFor(uint32_t seconds) {
    // The EPD waveform runs in a background task with the panel's high-voltage
    // rails on: sleeping before it finishes corrupts the image and leaves the
    // rails powered for the whole sleep.
    M5.Display.waitDisplay();
    wokeByTouch = false;
    if (seconds == 0) return;
    
    if (M5.Power.isCharging()) {
        // On USB: stay responsive, but don't spin at full speed
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
        startConfigPortal();
        return;
    }
    bool forceFetch = false;
    if (touch == TOUCH_TAP && now - lastFetchAttemptEpoch >= MIN_TAP_FETCH_GAP_S) {
        USE_SERIAL.println("Tap - forcing refresh");
        forceFetch = true;
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
