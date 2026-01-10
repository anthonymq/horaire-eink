#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

struct departureType {
    String name;
    String schedule;
    int delay;
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