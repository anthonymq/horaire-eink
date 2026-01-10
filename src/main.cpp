/**
 * Prochain Départ
 *
 *  Created on: 21.11.2024
 *
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>

#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <iostream>
#include <ctime>

#include <misc.h>
#include <string>

// EINK
#define USE_HSPI_FOR_EPD

#include <GxEPD2_BW.h>
// #include <GxEPD2_3C.h>
// #include <GxEPD2_4C.h>
// #include <GxEPD2_7C.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>

// custom
#include <config.h>
#include <transport_api.h>
#include "types.h"
#include "display.h"  // Add this include

#define GxEPD2_DISPLAY_CLASS GxEPD2_BW
// old screen 2.7
// GxEPD2_BW<GxEPD2_270_GDEY027T91, GxEPD2_270_GDEY027T91::HEIGHT> display(GxEPD2_270_GDEY027T91(/*CS=*/15, /*DC=*/27, /*RST=*/26, /*BUSY=*/25)); // GDEY027T91 176x264, SSD1680
GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(GxEPD2_420_GDEY042T81(/*CS=*/15, /*DC=*/27, /*RST=*/26, /*BUSY=*/25)); // 400x300, SSD1683

// #include "bitmaps/Bitmaps176x264.h" // 2.7"  b/w
SPIClass hspi(HSPI);
// /EINK

using namespace std;

#define USE_SERIAL Serial

////// button
#define BUTTON_PIN 12
int lastButtonState = HIGH; // the previous state from the input pin
int buttonState;            // the current reading from the input pin
bool screenBacklightState = true;

WiFiMulti wifiMulti;

// Change the offset to 3600 seconds (1 hour) for GMT+1
ESP32Time rtc(3600); // offset in seconds GMT+1
long lastSuccessfullCallEpoch;
long lastCallEpoch = 0;
long lastScreenRefreshEpoch = 0;
#define DELAY_BETWEEN_CALLS 30; // in seconds

// Remove the display functions that were moved to display.cpp

// Keep the setup and loop functions
void setup()
{
    // Set timezone to Central European Time (CET/CEST with automatic DST adjustment)
    // For France/Paris timezone
    setenv("TZ", "GMT+1GMT+2,M3.5.0,M10.5.0/3", 1);
    tzset();
    USE_SERIAL.begin(115200);

    // *** special handling for Waveshare ESP32 Driver board *** //
    // ********************************************************* //
    hspi.begin(13, 12, 14, 15); // remap hspi for EPD (swap pins)
    display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    // *** end of special handling for Waveshare ESP32 Driver board *** //
    // **************************************************************** //
    display.init(115200);

    displayLoader("Horaire...");
    Serial.println("setup done");

    // drawScreen(std::vector<departureType>());
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    USE_SERIAL.println(rtc.getEpoch());

    wifiMulti.addAP("WIFI_SSID_REMOVED", "WIFI_PASSWORD_REMOVED");
    lastSuccessfullCallEpoch = rtc.getEpoch();
}

// Add this include at the top with other includes
#include <weather_api.h>

// Modify the loop function to fetch weather data
// Add at the top with other global variables
long lastWeatherCallEpoch = 0;
#define WEATHER_CALL_INTERVAL 1800; // 30 minutes in seconds

void loop()
{
    buttonState = digitalRead(BUTTON_PIN);
    if (lastButtonState == LOW && buttonState == HIGH)
    {
        Serial.println("The state changed from LOW to HIGH");
    }
    lastButtonState = buttonState;

    if (wifiMulti.run() == WL_CONNECTED)
    {
        if ((rtc.getEpoch() - lastCallEpoch) > 29 || lastCallEpoch == 0)
        {
            std::vector<std::vector<departureType>> allDepartures;
            for (const auto& station : STATION_ENDPOINTS) {
                std::string stationName = station.first;
                std::string endpoint = station.second;
                std::string type = STATION_TYPES.at(stationName);
                std::string filter = STATION_FILTERS.at(stationName);
                
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

            bool fullScreenRefresh = (rtc.getEpoch() - lastScreenRefreshEpoch) > 300 || lastScreenRefreshEpoch == 0;
            drawScreen(allDepartures, currentWeather, fullScreenRefresh);  // Pass the array of vectors and weather data
            lastSuccessfullCallEpoch = rtc.getEpoch();
            // @TODO fix data freshness
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
