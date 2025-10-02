#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

struct departureType {
    String name;
    String schedule;
    int delay;
};

#endif