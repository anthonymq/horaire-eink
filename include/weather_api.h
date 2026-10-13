#ifndef WEATHER_API_H
#define WEATHER_API_H

#include <Arduino.h>
#include "types.h"

// Returns true and fills `out` on success; leaves `out` untouched on failure.
bool fetchWeather(const char *city, weatherType &out);

#endif
