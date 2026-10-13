#include "weather_api.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <config.h>

#define USE_SERIAL Serial

// Map OpenWeatherMap icon codes to weathericons.h codes
static const char *mapWeatherIcon(const char *owmIcon)
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

bool fetchWeather(const char *city, weatherType &out)
{
    HTTPClient http;
    String url = String(WEATHER_API_URL) + "?q=" + city + "&appid=" + WEATHER_API_KEY + "&units=" + WEATHER_UNITS;

    http.setConnectTimeout(5000);
    http.setTimeout(8000);
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        USE_SERIAL.printf("[OWM] GET failed: %d\n", httpCode);
        http.end();
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getString());
    http.end();
    if (error) {
        USE_SERIAL.printf("[OWM] JSON error: %s\n", error.c_str());
        return false;
    }

    out.temperature = doc["main"]["temp"];
    out.feels_like = doc["main"]["feels_like"];
    out.humidity = doc["main"]["humidity"];
    out.description = doc["weather"][0]["description"].as<String>();
    out.icon = mapWeatherIcon(doc["weather"][0]["icon"] | "");
    out.wind_speed = doc["wind"]["speed"];
    return true;
}
