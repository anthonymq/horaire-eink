#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <vector>
#include "types.h"

// Forward declaration of the display object
extern GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display;

void displayBanner(int posx, int posy, int height, int width, String text);
void displayDepartureBoard(int posx, int posy, int height, int width, std::vector<departureType> departures, bool showNames, String banner);
void drawScreen(std::vector<std::vector<departureType>> allDepartures, bool fullRefresh);
void displayLoader(String message);
void displayDataFreshness(bool success, long lastSuccess, long currentEpoch);

#endif