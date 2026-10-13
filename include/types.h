#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>
#include <ctime>

struct departureType {
    String name;
    time_t expected;  // UTC epoch of expected departure/arrival
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
