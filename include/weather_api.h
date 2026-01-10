#ifndef WEATHER_API_H
#define WEATHER_API_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "types.h"
#include "config.h"

// Add this declaration
String mapWeatherIcon(String owmIcon);

weatherType fetchWeather(const String& city);

#endif