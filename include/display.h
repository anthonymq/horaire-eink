#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <M5Unified.h>
#include <vector>
#include "types.h"

// M5Paper S3 uses M5.Display (M5GFX) - no separate display object needed
// Access via M5.Display.methodName()

void displayBanner(int posx, int posy, int height, int width, String text);
void displayDepartureBoard(int posx, int posy, int height, int width, std::vector<departureType> departures, bool showNames, String banner);
void drawScreen(std::vector<std::vector<departureType>> allDepartures, weatherType weather, bool fullRefresh, const char* timeStr, int batteryLevel, bool isCharging);
void displayWeather(int posx, int posy, int width, int height, weatherType weather);
void displayLoader(String message);
void displayDataFreshness(bool success, long lastSuccess, long currentEpoch);

#endif