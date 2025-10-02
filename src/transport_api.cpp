#include "transport_api.h"
#define USE_SERIAL Serial
#include <string>

std::vector<departureType> fetchDepartures(const std::string &url, const std::string &type, const std::string &filter)
{
    std::vector<departureType> departures;
    HTTPClient http;

    USE_SERIAL.print("[HTTP] begin...\n");
    http.begin(url.c_str());
    http.addHeader("apiKey", API_TOKEN);

    USE_SERIAL.print("[HTTP] GET...\n");
    int httpCode = http.GET();

    if (httpCode > 0)
    {
        USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

        if (httpCode == HTTP_CODE_OK)
        {
            String payload = http.getString();
            USE_SERIAL.println("Response payload length: " + String(payload.length()));
            USE_SERIAL.println("First 100 chars of payload: " + payload.substring(0, 100));
            
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload, DeserializationOption::NestingLimit(11));
        
            if (error)
            {
                Serial.print(F("deserializeJson() failed: "));
                Serial.println(error.f_str());
                // Fix the type mismatch by casting both arguments to the same type
                int lastCharsPos = (payload.length() > 100) ? (payload.length() - 100) : 0;
                Serial.println("Last 100 chars of payload: " + payload.substring(lastCharsPos));
                http.end();
                return departures;
            }
            
            const char *responseTimestamp = doc["Siri"]["ServiceDelivery"]["ResponseTimestamp"];
            const char *format = "%Y-%m-%dT%H:%M:%S.000Z";
            time_t currentTimestamp = parseDateTime(responseTimestamp, format);
            JsonArray stops = doc["Siri"]["ServiceDelivery"]["StopMonitoringDelivery"][0]["MonitoredStopVisit"].as<JsonArray>();

            for (JsonObject stop : stops)
            {
                if (departures.size() > 5)
                {
                    break;
                }
                JsonObject journey = stop["MonitoredVehicleJourney"];
                String name = journey["JourneyNote"][0]["value"];

                const char *expectedTime = nullptr;
                if (type == "RER")
                {
                    std::string voie = journey["MonitoredCall"]["ArrivalPlatformName"]["value"];
                    expectedTime = journey["MonitoredCall"]["ExpectedArrivalTime"];
                    if (voie != filter)
                    {
                        continue;
                    }
                }
                else if (type == "BUS")
                {
                    std::string lineName = journey["OperatorRef"]["value"];
                    expectedTime = journey["MonitoredCall"]["ExpectedDepartureTime"];
                    if(lineName.find(filter) == std::string::npos){
                        continue;
                    }
                }

                Serial.println(name);
                if (expectedTime != nullptr)
                {
                    Serial.println(expectedTime);
                    const char *format = "%Y-%m-%dT%H:%M:%S.000Z";
                    const char *shortFormat = "%H:%M";
                    time_t parsedTime = parseDateTime(expectedTime, format);
                    String formattedTime(toDateTime(parsedTime, format));
                    String shortFormattedTime(toDateTime(parsedTime, shortFormat));

                    tm *tm_gmt = gmtime(&parsedTime);
                    time_t delayInSeconds = parsedTime - currentTimestamp;

                    departureType currentDeparture;
                    currentDeparture.name = name;
                    char scheduleBuffer[5];
                    sprintf(scheduleBuffer, "%.2d:%.2d", tm_gmt->tm_hour, tm_gmt->tm_min);
                    currentDeparture.schedule = scheduleBuffer;
                    currentDeparture.delay = delayInSeconds / 60;

                    if (delayInSeconds > 0)
                    {
                        departures.push_back(currentDeparture);
                    }
                }
            }
        }
    }
    else
    {
        USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    return departures;
}
