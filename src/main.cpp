/**
 * Prochain Départ
 *
 *  Created on: 21.11.2024
 *  Migrated to M5Paper S3: 2026-01-11
 *  WiFi Manager added: 2026-01-11
 *  Power Management added: 2026-01-11
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
 * Start WiFi Manager configuration portal (FR-WM1, FR-WM2, FR-WM5)
 * Creates an open AP that user connects to for WiFi configuration.
 * Credentials are automatically saved to NVS/Preferences by WiFiManager.
 */
void startConfigPortal() {
    USE_SERIAL.println("Starting WiFi configuration portal...");
    
    // Display instructions on screen
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setFont(&FreeSansBold12pt7b);
    
    // Center title
    const char* title = "WiFi Configuration";
    int titleWidth = M5.Display.textWidth(title);
    M5.Display.setCursor((960 - titleWidth) / 2, 100);
    M5.Display.print(title);
    
    // Instructions
    M5.Display.setFont(&FreeSans9pt7b);
    int y = 180;
    int lineHeight = 30;
    
    M5.Display.setCursor(100, y);
    M5.Display.print("1. Connect to WiFi network:");
    y += lineHeight;
    
    M5.Display.setFont(&FreeSansBold9pt7b);
    M5.Display.setCursor(120, y);
    M5.Display.print(AP_NAME);
    y += lineHeight + 10;
    
    M5.Display.setFont(&FreeSans9pt7b);
    M5.Display.setCursor(100, y);
    M5.Display.print("2. Open browser to http://192.168.4.1");
    y += lineHeight;
    
    M5.Display.setCursor(100, y);
    M5.Display.print("3. Select your WiFi network and enter password");
    y += lineHeight;
    
    M5.Display.setCursor(100, y);
    M5.Display.print("4. Device will restart and connect automatically");
    y += lineHeight + 20;
    
    // Timeout info
    M5.Display.setTextColor(TFT_DARKGREY);
    char timeoutMsg[64];
    snprintf(timeoutMsg, sizeof(timeoutMsg), "Portal will close automatically after %d seconds", CONFIG_PORTAL_TIMEOUT);
    M5.Display.setCursor(100, y);
    M5.Display.print(timeoutMsg);
    
    M5.Display.display();
    
    // Configure WiFi Manager
    wm.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT);
    wm.setBreakAfterConfig(true);  // Exit portal after config saved
    
    // Callback when configuration is saved
    wm.setSaveConfigCallback([]() {
        USE_SERIAL.println("WiFi configuration saved!");
    });
    
    // Start the configuration portal
    // This is a blocking call - will return when:
    // 1. User saves valid credentials and connects
    // 2. Timeout expires
    // 3. User manually closes portal
    bool connected = wm.startConfigPortal(AP_NAME);
    
    if (connected) {
        USE_SERIAL.println("WiFi configured successfully, restarting...");
        displayLoader("WiFi configure! Redemarrage...");
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
            for (int i = 0; i < NUM_STATIONS; i++) {
                std::string endpoint = STATIONS[i].endpoint;
                std::string type = STATIONS[i].type;
                std::string filter = STATIONS[i].filter;
                
                std::vector<departureType> departures = fetchDepartures(endpoint, type, filter);
                // Process departures...
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
