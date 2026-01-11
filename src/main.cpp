/**
 * Prochain Départ
 *
 *  Created on: 21.11.2024
 *  Migrated to M5Paper S3: 2026-01-11
 *
 */
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiMulti.h>

#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <ctime>

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

// RTC for timekeeping (will be synced via NTP)
ESP32Time rtc(0); // No offset - timezone handled by TZ env var
long lastSuccessfullCallEpoch;
long lastCallEpoch = 0;
long lastScreenRefreshEpoch = 0;
bool ntpSynced = false;
#define DELAY_BETWEEN_CALLS 30; // in seconds

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

void loop()
{
    // Update M5Unified (handles button states, etc.)
    M5.update();

    // Check for button press using M5Unified API
    // M5Paper S3 has one physical button
    if (M5.BtnA.wasPressed()) {
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
            
            drawScreen(allDepartures, currentWeather, fullScreenRefresh, timeStr, batteryLevel, isCharging);
            lastSuccessfullCallEpoch = rtc.getEpoch();
            displayDataFreshness(true, lastSuccessfullCallEpoch, rtc.getEpoch());
            lastCallEpoch = rtc.getEpoch();
            if (fullScreenRefresh)
            {
                lastScreenRefreshEpoch = rtc.getEpoch();
            }
        }
    }
    else
    {
        displayLoader("Connection au WIFI...");
    }
}
