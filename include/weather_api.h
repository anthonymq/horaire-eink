#ifndef WEATHER_API_H
#define WEATHER_API_H

#include <Arduino.h>
#include "types.h"

#define WEATHER_NAME_LEN 32

// Weather location, set from the setup portal (NVS namespace "weather").
// Without saved coordinates the OpenWeatherMap city search uses `name`.
struct WeatherLocation {
    char name[WEATHER_NAME_LEN];  // Shown under the weather column (ASCII)
    float lat;
    float lon;
    bool hasCoords;
};

// Saved location, or WEATHER_CITY from config.h until one is saved.
const WeatherLocation& getWeatherLocation();
bool saveWeatherLocation(const WeatherLocation& loc);

// Returns true and fills `out` on success; leaves `out` untouched on failure.
bool fetchWeather(weatherType &out);

// OpenWeatherMap geocoding for the portal. Writes:
//   [{"name":"Vincennes","state":"Ile-de-France","country":"FR","lat":48.84,"lon":2.43}]
bool searchCities(const char *query, String &outJson);

#endif
