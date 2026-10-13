#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <M5Unified.h>
#include <vector>
#include "types.h"
#include "board_layout.h"

// M5Paper S3 uses M5.Display (M5GFX) - no separate display object needed

struct BoardView {
    const char* title;
    const std::vector<departureType>* departures;  // Only upcoming departures, soonest first
    const std::vector<incidentType>* incidents;    // Active incidents, blocking first (may be null)
    bool showNames;                                // Show the journey names (large boards)
    bool isTrain;                                  // "Aucun train" rather than "Aucun passage"
};

struct StatusInfo {
    const char* timeStr;
    int batteryLevel;
    bool isCharging;
    bool batteryLow;     // Battery saving mode: shown in bold
    int dataAgeSecs;     // -1 = never fetched
    int wifiRssi;
    const char* error;   // Why the last fetch failed, or empty
};

// `boards` is indexed by station slot (layout.boards[i].station). A layout
// change forces a full refresh.
void drawScreen(const ScreenLayout& layout, const BoardView* boards, const weatherType& weather,
                bool weatherValid, const StatusInfo& status, time_t now, bool fullRefresh);
void displayLoader(const char* message);

struct SetupQr {
    const char* payload;  // Encoded in the QR code
    const char* caption;  // Shown under it
};

// Setup portal screen: QR codes on the left, instruction lines on the right.
// A line starting with '*' is drawn bold, an empty line is a spacer. Text must be
// ASCII (the GFX fonts have no accents) and short enough for a ~330px column.
// With no QR code the lines start at the left margin.
void drawSetupScreen(const char* title, const SetupQr* qrs, int qrCount,
                     const char* const* lines, int lineCount);

#endif
