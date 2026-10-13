#include "forecast.h"
#include <ArduinoJson.h>
#include <cstdio>
#include <cstring>

static const int RAIN_POP_PCT = 40;
static const float RAIN_MM = 0.3f;

const char *mapWeatherIcon(const char *owmIcon)
{
    if (strncmp(owmIcon, "01", 2) == 0) return "sunny";
    if (strncmp(owmIcon, "02", 2) == 0) return "partlycloudy";
    if (strncmp(owmIcon, "03", 2) == 0) return "mostlycloudy";
    if (strncmp(owmIcon, "04", 2) == 0) return "cloudy";
    if (strncmp(owmIcon, "09", 2) == 0) return "chancerain";
    if (strncmp(owmIcon, "10", 2) == 0) return "rain";
    if (strncmp(owmIcon, "11", 2) == 0) return "tstorms";
    if (strncmp(owmIcon, "13", 2) == 0) return "snow";
    if (strncmp(owmIcon, "50", 2) == 0) return "fog";
    return "unknown";
}

int parseForecast(const char *json, size_t len, ForecastSlot *out, int max)
{
    JsonDocument filter;
    JsonObject step = filter["list"][0].to<JsonObject>();
    step["dt"] = true;
    step["main"]["temp"] = true;
    step["pop"] = true;
    step["rain"]["3h"] = true;
    step["snow"]["3h"] = true;
    step["weather"][0]["icon"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, json, len, DeserializationOption::Filter(filter))) return -1;

    int n = 0;
    for (JsonObject s : doc["list"].as<JsonArray>()) {
        if (n >= max) break;
        ForecastSlot &slot = out[n++];
        slot.time = (time_t)(s["dt"] | 0L);
        slot.temp = s["main"]["temp"] | 0.0f;
        slot.pop = (int)((s["pop"] | 0.0f) * 100 + 0.5f);
        slot.rainMm = (s["rain"]["3h"] | 0.0f) + (s["snow"]["3h"] | 0.0f);
        snprintf(slot.icon, sizeof(slot.icon), "%s", mapWeatherIcon(s["weather"][0]["icon"] | ""));
    }
    return n;
}

bool isRainy(const ForecastSlot &slot)
{
    return slot.pop >= RAIN_POP_PCT || slot.rainMm >= RAIN_MM;
}

void rainSummary(const ForecastSlot *slots, int count, int hours, bool brief, char *out, size_t outSize)
{
    out[0] = '\0';
    if (count == 0) return;
    time_t limit = slots[0].time + hours * 3600;
    int last = 0;
    for (int i = 0; i < count && slots[i].time < limit; i++) {
        last = i;
        if (!isRainy(slots[i])) continue;
        struct tm local;
        localtime_r(&slots[i].time, &local);
        if (slots[i].pop > 0 && !brief) {
            snprintf(out, outSize, "Pluie vers %dh (%d%%)", local.tm_hour, slots[i].pop);
        } else {
            snprintf(out, outSize, "Pluie vers %dh", local.tm_hour);
        }
        return;
    }
    // Each step covers the 3 hours that follow it
    time_t end = slots[last].time + 3 * 3600;
    struct tm local;
    localtime_r(&end, &local);
    snprintf(out, outSize, brief ? "Sec jusqu'a %dh" : "Pas de pluie jusqu'a %dh", local.tm_hour);
}
