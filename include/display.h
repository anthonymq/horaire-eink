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
    const std::vector<incidentType>* incidents;    // Active incidents, blocking first (may be null)
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

struct SetupQr {
    const char* payload;  // Encoded in the QR code
    const char* caption;  // Shown under it
};

// Setup portal screen: QR codes on the left, instruction lines on the right.
// A line starting with '*' is drawn bold, an empty line is a spacer. Text must be
// ASCII (the GFX fonts have no accents) and short enough for a ~330px column.
void drawSetupScreen(const char* title, const SetupQr* qrs, int qrCount,
                     const char* const* lines, int lineCount);

#endif
