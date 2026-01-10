#include "weather_api.h"
#include <Arduino.h>

#define USE_SERIAL Serial

weatherType fetchWeather(const String& city) {
    weatherType weather = {}; // Initialize with default values
    HTTPClient http;
    
    // Construct the URL with the city name, API key, and units
    String url = String(WEATHER_API_URL) + "?q=" + city + "&appid=" + WEATHER_API_KEY + "&units=" + WEATHER_UNITS;
    
    USE_SERIAL.print("[HTTP] Weather API begin...\n");
    http.begin(url);
    
    USE_SERIAL.print("[HTTP] GET...\n");
    int httpCode = http.GET();
    
    if (httpCode > 0) {
        USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            USE_SERIAL.println("Weather response payload length: " + String(payload.length()));
            USE_SERIAL.println("First 1000 chars of payload: " + payload.substring(0, 1000));
            
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            
            if (error) {
                USE_SERIAL.print(F("deserializeJson() failed: "));
                USE_SERIAL.println(error.f_str());
                http.end();
                return weather;
            }
            
            // Extract weather data
            weather.temperature = doc["main"]["temp"];
            weather.feels_like = doc["main"]["feels_like"];
            weather.humidity = doc["main"]["humidity"];
            weather.description = doc["weather"][0]["description"].as<String>();
            // In fetchWeather function, replace this line:
            // weather.icon = doc["weather"][0]["icon"].as<String>();
            
            // With this:
            String owmIcon = doc["weather"][0]["icon"].as<String>();
            weather.icon = mapWeatherIcon(owmIcon);
            weather.wind_speed = doc["wind"]["speed"];
        }
    } else {
        USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
    return weather;
}

// Add this function to map OpenWeatherMap icon codes to weathericons.h codes
String mapWeatherIcon(String owmIcon) {
    // Map OpenWeatherMap icon codes to weathericons.h codes
    if (owmIcon == "01d" || owmIcon == "01n") return "sunny";
    if (owmIcon == "02d" || owmIcon == "02n") return "partlycloudy";
    if (owmIcon == "03d" || owmIcon == "03n") return "mostlycloudy";
    if (owmIcon == "04d" || owmIcon == "04n") return "cloudy";
    if (owmIcon == "09d" || owmIcon == "09n") return "chancerain";
    if (owmIcon == "10d" || owmIcon == "10n") return "rain";
    if (owmIcon == "11d" || owmIcon == "11n") return "tstorms";
    if (owmIcon == "13d" || owmIcon == "13n") return "snow";
    if (owmIcon == "50d" || owmIcon == "50n") return "fog";
    
    return "unknown"; // Default fallback
}