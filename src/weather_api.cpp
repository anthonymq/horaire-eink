#include "weather_api.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <config.h>
#include "misc.h"

#define USE_SERIAL Serial
#define NVS_NAMESPACE "weather"

static WeatherLocation location;
static bool locationLoaded = false;

const WeatherLocation& getWeatherLocation()
{
    if (locationLoaded) return location;
    locationLoaded = true;
    strlcpy(location.name, WEATHER_CITY, sizeof(location.name));
    location.hasCoords = false;

    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, true)) {  // Fails (read-only) until something is saved
        String name = prefs.getString("name", "");
        if (!name.isEmpty()) {
            strlcpy(location.name, name.c_str(), sizeof(location.name));
            location.hasCoords = prefs.getBool("coords", false);
            location.lat = prefs.getFloat("lat", 0);
            location.lon = prefs.getFloat("lon", 0);
        }
        prefs.end();
    }
    return location;
}

bool saveWeatherLocation(const WeatherLocation& loc)
{
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        USE_SERIAL.println("Failed to open NVS for weather location");
        return false;
    }
    prefs.putString("name", loc.name);
    prefs.putBool("coords", loc.hasCoords);
    prefs.putFloat("lat", loc.lat);
    prefs.putFloat("lon", loc.lon);
    prefs.end();
    location = loc;
    locationLoaded = true;
    USE_SERIAL.printf("Saved weather location: %s (%.4f, %.4f)\n", loc.name, loc.lat, loc.lon);
    return true;
}

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

bool fetchWeather(weatherType &out)
{
    const WeatherLocation &loc = getWeatherLocation();
    HTTPClient http;
    String url = String(WEATHER_API_URL) + "?appid=" + WEATHER_API_KEY + "&units=" + WEATHER_UNITS;
    if (loc.hasCoords) {
        url += "&lat=" + String(loc.lat, 4) + "&lon=" + String(loc.lon, 4);
    } else {
        url += "&q=" + urlEncode(loc.name);
    }

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

bool searchCities(const char *query, String &outJson)
{
    HTTPClient http;
    String url = String("https://api.openweathermap.org/geo/1.0/direct?limit=5&appid=") + WEATHER_API_KEY + "&q=" + urlEncode(query);

    http.setConnectTimeout(5000);
    http.setTimeout(8000);
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        USE_SERIAL.printf("[OWM] geocoding failed: %d\n", httpCode);
        http.end();
        return false;
    }

    JsonDocument filter;
    JsonObject place = filter[0].to<JsonObject>();
    place["name"] = true;
    place["local_names"]["fr"] = true;
    place["state"] = true;
    place["country"] = true;
    place["lat"] = true;
    place["lon"] = true;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getString(), DeserializationOption::Filter(filter));
    http.end();
    if (error) {
        USE_SERIAL.printf("[OWM] geocoding JSON error: %s\n", error.c_str());
        return false;
    }

    JsonDocument out;
    JsonArray results = out.to<JsonArray>();
    for (JsonObject p : doc.as<JsonArray>()) {
        JsonObject r = results.add<JsonObject>();
        r["name"] = p["local_names"]["fr"] | (p["name"] | "");
        r["state"] = p["state"] | "";
        r["country"] = p["country"] | "";
        r["lat"] = p["lat"];
        r["lon"] = p["lon"];
    }
    outJson = "";
    serializeJson(out, outJson);
    return true;
}
