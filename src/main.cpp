/**
 * Prochain Départ
 *
 *  Created on: 21.11.2024
 *  Migrated to M5Paper S3: 2026-01-11
 *  WiFi Manager added: 2026-01-11
 *  Power Management added: 2026-01-11
 *  StopPoint Configuration added: 2026-01-11
 *
 */
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiManager.h>  // tzapu/WiFiManager - captive portal for WiFi configuration

#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <ctime>
#include <esp_sleep.h>  // For light/deep sleep functions

#include <misc.h>
#include <string>

// M5Paper S3 uses M5Unified - no manual SPI/display setup needed
// Display accessed via M5.Display

// custom
#include <config.h>
#include <transport_api.h>
#include <station_config.h>  // NVS-based station configuration (FR-SC1)
#include "types.h"
#include "display.h"

using namespace std;

#define USE_SERIAL Serial

WiFiMulti wifiMulti;
WiFiManager wm;  // WiFi Manager for captive portal configuration

// RTC for timekeeping (will be synced via NTP)
ESP32Time rtc(0); // No offset - timezone handled by TZ env var
long lastSuccessfullCallEpoch;
long lastCallEpoch = 0;
long lastScreenRefreshEpoch = 0;
bool ntpSynced = false;
bool configPortalActive = false;  // Track if config portal is running
#define DELAY_BETWEEN_CALLS 30  // in seconds
#define CONFIG_PORTAL_TIMEOUT 120  // Portal timeout in seconds
#define AP_NAME "ProchainDepart-Setup"  // AP name for configuration

// Power Management Configuration (P4)
// Light sleep is used between refresh cycles to conserve battery.
// When charging via USB, sleep is disabled for better responsiveness.
#define POWER_SAVE_ENABLED true       // Master toggle for power saving
#define SLEEP_DURATION_US 25000000ULL // Sleep 25 seconds (25,000,000 microseconds)
                                       // This leaves ~5s for WiFi reconnect + data fetch
                                       // to hit the 30s refresh target

/**
 * Enter light sleep to conserve power between refresh cycles.
 * Light sleep preserves RAM and program state, allowing quick resume.
 * The ESP32-S3 wakes after the specified duration or on button press.
 * 
 * @param durationUs Sleep duration in microseconds
 * @return true if woke from sleep, false if sleep was skipped
 */
bool enterLightSleep(uint64_t durationUs) {
    // Skip sleep if:
    // 1. Power saving is disabled
    // 2. Device is charging (USB connected = more responsive operation)
    // 3. Config portal is active
    if (!POWER_SAVE_ENABLED || M5.Power.isCharging() || configPortalActive) {
        USE_SERIAL.println("Sleep skipped (charging or config active)");
        return false;
    }
    
    USE_SERIAL.printf("Entering light sleep for %.1f seconds...\n", durationUs / 1000000.0);
    USE_SERIAL.flush();  // Ensure serial output completes before sleep
    
    // Configure wake-up sources
    // 1. Timer wake-up after specified duration
    esp_sleep_enable_timer_wakeup(durationUs);
    
    // 2. Button wake-up (GPIO wake-up via M5Unified - uses EXT0/EXT1)
    // M5Paper S3 button is handled internally by M5Unified
    // Using M5.Power.lightSleep() which handles button wakeup automatically
    
    // Enter light sleep using M5Unified API (handles hardware properly)
    M5.Power.lightSleep(durationUs, false);
    
    // After waking up
    USE_SERIAL.println("Woke up from light sleep");
    
    // Update M5Unified state after wake
    M5.update();
    
    return true;
}

/**
 * Start WiFi Manager configuration portal (FR-WM1, FR-WM2, FR-WM5, FR-SC2)
 * Creates an open AP that user connects to for WiFi configuration.
 * Credentials are automatically saved to NVS/Preferences by WiFiManager.
 * Also allows configuration of transit stations (StopPoint configuration).
 */

// Global flag for station config save
static bool shouldSaveStationConfig = false;

void startConfigPortal() {
    USE_SERIAL.println("Starting WiFi configuration portal...");
    
    // Display instructions on screen
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setFont(&FreeSansBold12pt7b);
    
    // Center title
    const char* title = "Configuration Mode";
    int titleWidth = M5.Display.textWidth(title);
    M5.Display.setCursor((960 - titleWidth) / 2, 80);
    M5.Display.print(title);
    
    // Instructions
    M5.Display.setFont(&FreeSans9pt7b);
    int y = 150;
    int lineHeight = 28;
    
    M5.Display.setCursor(100, y);
    M5.Display.print("1. Connect to WiFi network:");
    y += lineHeight;
    
    M5.Display.setFont(&FreeSansBold9pt7b);
    M5.Display.setCursor(120, y);
    M5.Display.print(AP_NAME);
    y += lineHeight + 8;
    
    M5.Display.setFont(&FreeSans9pt7b);
    M5.Display.setCursor(100, y);
    M5.Display.print("2. Open browser to http://192.168.4.1");
    y += lineHeight;
    
    M5.Display.setCursor(100, y);
    M5.Display.print("3. Configure WiFi network and station settings");
    y += lineHeight;
    
    M5.Display.setCursor(100, y);
    M5.Display.print("4. Device will restart after configuration");
    y += lineHeight + 16;
    
    // Station config info
    M5.Display.setTextColor(TFT_DARKGREY);
    M5.Display.setCursor(100, y);
    M5.Display.print("StopPoint IDs: prim.iledefrance-mobilites.fr/fr/jeux-de-donnees/arrets-lignes");
    y += lineHeight;
    
    // Timeout info
    char timeoutMsg[64];
    snprintf(timeoutMsg, sizeof(timeoutMsg), "Portal closes after %d seconds of inactivity", CONFIG_PORTAL_TIMEOUT);
    M5.Display.setCursor(100, y);
    M5.Display.print(timeoutMsg);
    
    M5.Display.display();
    
    // Get current station configs for default values
    RuntimeStationConfig* currentStations = getStations();
    int currentCount = getStationCount();
    
    // Create WiFiManager custom parameters for station configuration (FR-SC2)
    // Station 1 (RER/primary station)
    char s1_name_val[STATION_NAME_LEN] = "";
    char s1_ref_val[STATION_REF_LEN] = "";
    char s1_type_val[STATION_TYPE_LEN] = "RER";
    char s1_filter_val[STATION_FILTER_LEN] = "";
    
    if (currentCount >= 1 && currentStations[0].isConfigured) {
        strncpy(s1_name_val, currentStations[0].displayName, sizeof(s1_name_val) - 1);
        strncpy(s1_ref_val, currentStations[0].monitoringRef, sizeof(s1_ref_val) - 1);
        strncpy(s1_type_val, currentStations[0].type, sizeof(s1_type_val) - 1);
        strncpy(s1_filter_val, currentStations[0].filter, sizeof(s1_filter_val) - 1);
    }
    
    // Station 2 (Bus left)
    char s2_name_val[STATION_NAME_LEN] = "";
    char s2_ref_val[STATION_REF_LEN] = "";
    char s2_type_val[STATION_TYPE_LEN] = "BUS";
    char s2_filter_val[STATION_FILTER_LEN] = "";
    
    if (currentCount >= 2 && currentStations[1].isConfigured) {
        strncpy(s2_name_val, currentStations[1].displayName, sizeof(s2_name_val) - 1);
        strncpy(s2_ref_val, currentStations[1].monitoringRef, sizeof(s2_ref_val) - 1);
        strncpy(s2_type_val, currentStations[1].type, sizeof(s2_type_val) - 1);
        strncpy(s2_filter_val, currentStations[1].filter, sizeof(s2_filter_val) - 1);
    }
    
    // Station 3 (Bus right)
    char s3_name_val[STATION_NAME_LEN] = "";
    char s3_ref_val[STATION_REF_LEN] = "";
    char s3_type_val[STATION_TYPE_LEN] = "BUS";
    char s3_filter_val[STATION_FILTER_LEN] = "";
    
    if (currentCount >= 3 && currentStations[2].isConfigured) {
        strncpy(s3_name_val, currentStations[2].displayName, sizeof(s3_name_val) - 1);
        strncpy(s3_ref_val, currentStations[2].monitoringRef, sizeof(s3_ref_val) - 1);
        strncpy(s3_type_val, currentStations[2].type, sizeof(s3_type_val) - 1);
        strncpy(s3_filter_val, currentStations[2].filter, sizeof(s3_filter_val) - 1);
    }
    
    // Custom HTML header for the station config section
    WiFiManagerParameter stationHeader("<br><hr><h3>Station Configuration</h3><p>Leave blank to use defaults. Format: STIF:StopPoint:Q:12345:</p>");
    
    // Station 1 parameters
    WiFiManagerParameter s1_header("<h4>Station 1 (Top Section - RER)</h4>");
    WiFiManagerParameter s1_name("s1_name", "Display Name", s1_name_val, STATION_NAME_LEN);
    WiFiManagerParameter s1_ref("s1_ref", "MonitoringRef (StopPoint ID)", s1_ref_val, STATION_REF_LEN);
    WiFiManagerParameter s1_type("s1_type", "Type (RER/BUS/METRO)", s1_type_val, STATION_TYPE_LEN);
    WiFiManagerParameter s1_filter("s1_filter", "Filter (platform/operator)", s1_filter_val, STATION_FILTER_LEN);
    
    // Station 2 parameters
    WiFiManagerParameter s2_header("<h4>Station 2 (Bottom Left - Bus)</h4>");
    WiFiManagerParameter s2_name("s2_name", "Display Name", s2_name_val, STATION_NAME_LEN);
    WiFiManagerParameter s2_ref("s2_ref", "MonitoringRef", s2_ref_val, STATION_REF_LEN);
    WiFiManagerParameter s2_type("s2_type", "Type", s2_type_val, STATION_TYPE_LEN);
    WiFiManagerParameter s2_filter("s2_filter", "Filter", s2_filter_val, STATION_FILTER_LEN);
    
    // Station 3 parameters
    WiFiManagerParameter s3_header("<h4>Station 3 (Bottom Right - Bus)</h4>");
    WiFiManagerParameter s3_name("s3_name", "Display Name", s3_name_val, STATION_NAME_LEN);
    WiFiManagerParameter s3_ref("s3_ref", "MonitoringRef", s3_ref_val, STATION_REF_LEN);
    WiFiManagerParameter s3_type("s3_type", "Type", s3_type_val, STATION_TYPE_LEN);
    WiFiManagerParameter s3_filter("s3_filter", "Filter", s3_filter_val, STATION_FILTER_LEN);
    
    // Add parameters to WiFiManager
    wm.addParameter(&stationHeader);
    wm.addParameter(&s1_header);
    wm.addParameter(&s1_name);
    wm.addParameter(&s1_ref);
    wm.addParameter(&s1_type);
    wm.addParameter(&s1_filter);
    wm.addParameter(&s2_header);
    wm.addParameter(&s2_name);
    wm.addParameter(&s2_ref);
    wm.addParameter(&s2_type);
    wm.addParameter(&s2_filter);
    wm.addParameter(&s3_header);
    wm.addParameter(&s3_name);
    wm.addParameter(&s3_ref);
    wm.addParameter(&s3_type);
    wm.addParameter(&s3_filter);
    
    // Configure WiFi Manager
    wm.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT);
    wm.setBreakAfterConfig(true);  // Exit portal after config saved
    
    // Callback when configuration is saved
    wm.setSaveConfigCallback([]() {
        USE_SERIAL.println("Configuration saved!");
        shouldSaveStationConfig = true;
    });
    
    // Start the configuration portal
    bool connected = wm.startConfigPortal(AP_NAME);
    
    // Save station configs if callback was triggered
    if (shouldSaveStationConfig) {
        USE_SERIAL.println("Saving station configurations to NVS...");
        
        int savedCount = 0;
        
        // Process Station 1
        const char* ref1 = s1_ref.getValue();
        if (ref1 && strlen(ref1) > 0) {
            RuntimeStationConfig cfg1 = {};
            strncpy(cfg1.name, s1_name.getValue(), STATION_NAME_LEN - 1);
            strncpy(cfg1.monitoringRef, ref1, STATION_REF_LEN - 1);
            strncpy(cfg1.type, s1_type.getValue(), STATION_TYPE_LEN - 1);
            strncpy(cfg1.filter, s1_filter.getValue(), STATION_FILTER_LEN - 1);
            strncpy(cfg1.displayName, s1_name.getValue(), STATION_DISPLAY_LEN - 1);
            cfg1.showNames = (strcmp(cfg1.type, "RER") == 0);  // Show names for RER
            cfg1.maxEntries = 6;
            cfg1.isConfigured = true;
            saveStationConfig(0, cfg1);
            savedCount++;
            USE_SERIAL.printf("  Station 1: %s (%s)\n", cfg1.displayName, cfg1.monitoringRef);
        }
        
        // Process Station 2
        const char* ref2 = s2_ref.getValue();
        if (ref2 && strlen(ref2) > 0) {
            RuntimeStationConfig cfg2 = {};
            strncpy(cfg2.name, s2_name.getValue(), STATION_NAME_LEN - 1);
            strncpy(cfg2.monitoringRef, ref2, STATION_REF_LEN - 1);
            strncpy(cfg2.type, s2_type.getValue(), STATION_TYPE_LEN - 1);
            strncpy(cfg2.filter, s2_filter.getValue(), STATION_FILTER_LEN - 1);
            strncpy(cfg2.displayName, s2_name.getValue(), STATION_DISPLAY_LEN - 1);
            cfg2.showNames = (strcmp(cfg2.type, "RER") == 0);
            cfg2.maxEntries = 6;
            cfg2.isConfigured = true;
            saveStationConfig(1, cfg2);
            savedCount++;
            USE_SERIAL.printf("  Station 2: %s (%s)\n", cfg2.displayName, cfg2.monitoringRef);
        }
        
        // Process Station 3
        const char* ref3 = s3_ref.getValue();
        if (ref3 && strlen(ref3) > 0) {
            RuntimeStationConfig cfg3 = {};
            strncpy(cfg3.name, s3_name.getValue(), STATION_NAME_LEN - 1);
            strncpy(cfg3.monitoringRef, ref3, STATION_REF_LEN - 1);
            strncpy(cfg3.type, s3_type.getValue(), STATION_TYPE_LEN - 1);
            strncpy(cfg3.filter, s3_filter.getValue(), STATION_FILTER_LEN - 1);
            strncpy(cfg3.displayName, s3_name.getValue(), STATION_DISPLAY_LEN - 1);
            cfg3.showNames = (strcmp(cfg3.type, "RER") == 0);
            cfg3.maxEntries = 6;
            cfg3.isConfigured = true;
            saveStationConfig(2, cfg3);
            savedCount++;
            USE_SERIAL.printf("  Station 3: %s (%s)\n", cfg3.displayName, cfg3.monitoringRef);
        }
        
        // If at least one station was configured, save the count
        if (savedCount > 0) {
            saveStationCount(savedCount);
            USE_SERIAL.printf("Saved %d station configurations to NVS\n", savedCount);
        }
        
        shouldSaveStationConfig = false;
    }
    
    if (connected) {
        USE_SERIAL.println("Configuration complete, restarting...");
        displayLoader("Configuration terminee! Redemarrage...");
        delay(2000);
        ESP.restart();
    } else {
        USE_SERIAL.println("Config portal timeout or cancelled, resuming normal operation");
        displayLoader("Configuration annulee...");
        delay(1000);
        // Force screen refresh after config portal closes
        lastCallEpoch = 0;
        lastScreenRefreshEpoch = 0;
    }
    
    configPortalActive = false;
}

// Sync time via NTP
void syncTimeWithNTP() {
    USE_SERIAL.println("Syncing time with NTP...");
    
    // Configure NTP with Paris timezone
    // TZ string: CET-1CEST,M3.5.0,M10.5.0/3
    // CET = Central European Time (standard time name)
    // -1 = UTC+1 (note: POSIX uses inverted sign)
    // CEST = Central European Summer Time (DST name)
    // M3.5.0 = DST starts last Sunday of March at 00:00
    // M10.5.0/3 = DST ends last Sunday of October at 03:00
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
    
    // Wait for time sync (max 10 seconds)
    int retry = 0;
    while (time(nullptr) < 1700000000 && retry < 20) {  // Wait until we get a reasonable time (after 2023)
        delay(500);
        retry++;
        USE_SERIAL.print(".");
    }
    USE_SERIAL.println();
    
    if (time(nullptr) > 1700000000) {
        // Update ESP32Time RTC with the synced time
        struct tm timeinfo;
        getLocalTime(&timeinfo);
        rtc.setTimeStruct(timeinfo);
        USE_SERIAL.printf("NTP sync successful: %s", asctime(&timeinfo));
        ntpSynced = true;
    } else {
        USE_SERIAL.println("NTP sync failed - using default time");
    }
}

void setup()
{
    // Initialize M5Paper S3 with auto-detection
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);

    // M5Paper S3 display is automatically initialized by M5.begin()
    // Set rotation (0=portrait, 1=landscape, 2=portrait inverted, 3=landscape inverted)
    M5.Display.setRotation(1);  // Landscape mode

    displayLoader("Horaire...");
    USE_SERIAL.println("setup done");
    
    // Initialize station configuration from NVS or config.h fallback (FR-SC1)
    initStationConfig();
    USE_SERIAL.printf("Loaded %d station configurations\n", getStationCount());

    // Add all configured WiFi networks from config.h
    for (int i = 0; i < NUM_WIFI_CREDENTIALS; i++) {
        wifiMulti.addAP(WIFI_CREDENTIALS[i].ssid, WIFI_CREDENTIALS[i].password);
    }
    lastSuccessfullCallEpoch = rtc.getEpoch();
}

#include <weather_api.h>

// Weather refresh interval
long lastWeatherCallEpoch = 0;
#define WEATHER_CALL_INTERVAL 1800; // 30 minutes in seconds

// Long press detection state
bool longPressHandled = false;
#define LONG_PRESS_DURATION 5000  // 5 seconds in milliseconds

void loop()
{
    // Update M5Unified (handles button states, etc.)
    M5.update();

    // Check for button press using M5Unified API
    // M5Paper S3 has one physical button
    
    // Long press detection: 5+ seconds triggers configuration mode
    if (M5.BtnA.pressedFor(LONG_PRESS_DURATION)) {
        if (!longPressHandled) {
            longPressHandled = true;
            USE_SERIAL.println("Long press detected - entering configuration mode");
            configPortalActive = true;
            startConfigPortal();  // Blocking call - handles all portal logic
            longPressHandled = false;  // Reset after portal closes
        }
    } else if (M5.BtnA.wasReleased()) {
        if (longPressHandled && !configPortalActive) {
            // Button was released after long press (outside portal) - reset state
            longPressHandled = false;
            USE_SERIAL.println("Button released");
            lastCallEpoch = 0;  // Force screen refresh
        }
    } else if (M5.BtnA.wasPressed()) {
        USE_SERIAL.println("Button pressed - forcing refresh");
        lastCallEpoch = 0;  // Force immediate refresh
    }

    if (wifiMulti.run() == WL_CONNECTED)
    {
        // Sync time with NTP on first WiFi connection
        if (!ntpSynced) {
            syncTimeWithNTP();
        }
        
        if ((rtc.getEpoch() - lastCallEpoch) > 29 || lastCallEpoch == 0)
        {
            std::vector<std::vector<departureType>> allDepartures;
            
            // Get stations from NVS config or config.h fallback (FR-SC1)
            RuntimeStationConfig* stations = getStations();
            int stationCount = getStationCount();
            
            for (int i = 0; i < stationCount; i++) {
                if (!stations[i].isConfigured) continue;
                
                // Build the full endpoint URL from the MonitoringRef
                char endpoint[128];
                buildEndpointUrl(stations[i].monitoringRef, endpoint, sizeof(endpoint));
                
                std::string endpointStr = endpoint;
                std::string type = stations[i].type;
                std::string filter = stations[i].filter;
                
                USE_SERIAL.printf("Fetching station %d: %s (%s)\n", i, stations[i].displayName, type.c_str());
                std::vector<departureType> departures = fetchDepartures(endpointStr, type, filter);
                allDepartures.push_back(departures);
            }
            
            // Static weather data that persists between calls
            static weatherType currentWeather = {};
            
            // Fetch weather data only every 30 minutes
            if ((rtc.getEpoch() - lastWeatherCallEpoch) > 1800 || lastWeatherCallEpoch == 0)
            {
                USE_SERIAL.println("Fetching weather data...");
                currentWeather = fetchWeather(WEATHER_CITY);
                lastWeatherCallEpoch = rtc.getEpoch();
            }

            // Full refresh every 30 minutes to clear ghosting (1800 seconds)
            // Partial refresh handles individual row updates
            bool fullScreenRefresh = (rtc.getEpoch() - lastScreenRefreshEpoch) > 1800 || lastScreenRefreshEpoch == 0;
            
            // Get current time using standard C library (respects timezone from configTzTime)
            struct tm timeinfo;
            getLocalTime(&timeinfo);
            char timeStr[6];
            strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
            
            // Get battery status
            int batteryLevel = M5.Power.getBatteryLevel();
            bool isCharging = M5.Power.isCharging();
            
            // Calculate data age (seconds since last successful call)
            int dataAgeSecs = (int)(rtc.getEpoch() - lastSuccessfullCallEpoch);
            if (dataAgeSecs < 0) dataAgeSecs = 0;  // Handle edge case
            
            // Get WiFi signal strength
            int wifiRssi = WiFi.RSSI();
            
            drawScreen(allDepartures, currentWeather, fullScreenRefresh, timeStr, batteryLevel, isCharging, dataAgeSecs, wifiRssi);
            lastSuccessfullCallEpoch = rtc.getEpoch();
            displayDataFreshness(true, lastSuccessfullCallEpoch, rtc.getEpoch());
            lastCallEpoch = rtc.getEpoch();
            if (fullScreenRefresh)
            {
                lastScreenRefreshEpoch = rtc.getEpoch();
            }
            
            // Power Management: Enter light sleep to conserve battery
            // Sleep for ~25 seconds, leaving time for WiFi reconnect + fetch
            // Light sleep is skipped when charging for better responsiveness
            enterLightSleep(SLEEP_DURATION_US);
        }
    }
    else
    {
        displayLoader("Connection au WIFI...");
        // Brief delay when not connected to avoid busy loop
        delay(500);
    }
}
