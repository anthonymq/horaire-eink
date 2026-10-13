#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <M5Unified.h>
#include <vector>
#include "types.h"

// M5Paper S3 uses M5.Display (M5GFX) - no separate display object needed

struct BoardView {
    const char* title;
    const std::vector<departureType>* departures;  // Only upcoming departures, soonest first
};

struct StatusInfo {
    const char* timeStr;
    int batteryLevel;
    bool isCharging;
    int dataAgeSecs;
    int wifiRssi;
};

// boards[0] = top (RER, shows names), boards[1]/[2] = bottom left/right
void drawScreen(const BoardView boards[3], const weatherType& weather, const StatusInfo& status,
                time_t now, bool fullRefresh);
void displayLoader(const char* message);

#endif
