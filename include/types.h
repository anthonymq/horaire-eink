#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>
#include <ctime>
#include "departures.h"  // departureType, StopSeen (lib/transit_core)
#include "forecast.h"    // ForecastSlot (lib/transit_core)

// Active traffic incident (PRIM Navitia line report) affecting a board
struct incidentType {
    String text;    // Short ASCII title, line prefix removed ("6 gares non desservies")
    bool blocking;  // No service / stops skipped / detour, as opposed to delays
    String when;    // Current application period: "9h40 - 16h00", "jusqu'au 23/10" (may be empty)
};

struct weatherType {
    float temperature;
    float feels_like;
    int humidity;
    String description;
    String icon;
    float wind_speed;
    ForecastSlot forecast[FORECAST_SLOTS];  // Next 3-hour steps, soonest first
    int forecastCount;                       // 0 when the forecast fetch failed
};

#endif
