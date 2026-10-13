#pragma once

// OpenWeatherMap 3-hour forecast (free /data/2.5/forecast endpoint).

#include <cstddef>
#include <ctime>

static const int FORECAST_SLOTS = 8;  // 24 h

struct ForecastSlot {
    time_t time;    // UTC epoch the step is forecast for
    float temp;
    int pop;        // Probability of precipitation, %
    float rainMm;   // Rain + snow over the 3 hours
    char icon[16];  // weathericons.h code ("rain", "sunny"...)
};

// OpenWeatherMap icon ("10d") -> weathericons.h code ("rain")
const char *mapWeatherIcon(const char *owmIcon);

// Parses up to `max` steps from a /forecast response. Returns how many, -1 on a JSON error.
int parseForecast(const char *json, size_t len, ForecastSlot *out, int max);

// Does this step bring rain worth taking an umbrella for?
bool isRainy(const ForecastSlot &slot);

// One line for the weather card, local time (TZ):
//   "Pluie vers 15h (70%)" / "Pas de pluie jusqu'a 2h"
//   brief: "Pluie vers 15h" / "Sec jusqu'a 2h"
// Only the first `hours` hours are considered. Empty when there is no forecast.
void rainSummary(const ForecastSlot *slots, int count, int hours, bool brief, char *out, size_t outSize);
