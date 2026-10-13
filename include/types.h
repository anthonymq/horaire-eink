#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>
#include <ctime>

struct departureType {
    String name;
    time_t expected;  // UTC epoch of expected departure/arrival
};

// Active traffic incident (PRIM Navitia line report) affecting a board
struct incidentType {
    String text;    // Short ASCII title, line prefix removed ("6 gares non desservies")
    bool blocking;  // No service / stops skipped / detour, as opposed to delays
};

struct weatherType {
    float temperature;
    float feels_like;
    int humidity;
    String description;
    String icon;
    float wind_speed;
};

#endif
