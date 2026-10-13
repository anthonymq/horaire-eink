#include "transport_api.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <config.h>
#include "misc.h"

#define USE_SERIAL Serial

static const size_t MAX_DEPARTURES = 8;  // Keep a few spare so the list survives between fetches

// One TLS connection shared by all stop requests: a TLS 1.3 handshake costs far
// more CPU/radio time than the request itself, so we only pay it once per cycle.
static WiFiClientSecure client;
static HTTPClient http;
static bool clientReady = false;

// Only keep the fields we read: shrinks the JsonDocument and parsing time.
static void buildFilter(JsonDocument &filter)
{
    JsonObject delivery = filter["Siri"]["ServiceDelivery"].to<JsonObject>();
    delivery["ResponseTimestamp"] = true;
    JsonObject journey = delivery["StopMonitoringDelivery"][0]["MonitoredStopVisit"][0]["MonitoredVehicleJourney"].to<JsonObject>();
    journey["JourneyNote"] = true;
    journey["LineRef"] = true;
    journey["OperatorRef"] = true;
    JsonObject call = journey["MonitoredCall"].to<JsonObject>();
    call["ArrivalPlatformName"] = true;
    call["ExpectedArrivalTime"] = true;
    call["ExpectedDepartureTime"] = true;
}

bool fetchDepartures(const char *url, const char *type, const char *filter, std::vector<departureType> &out)
{
    if (!clientReady) {
        client.setInsecure();  // No CA pinned (same as before); see review notes
        http.setReuse(true);
        http.setConnectTimeout(5000);
        http.setTimeout(8000);
        clientReady = true;
    }

    if (!http.begin(client, url)) {
        USE_SERIAL.println("[PRIM] begin failed");
        return false;
    }
    http.addHeader("apiKey", API_TOKEN);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        USE_SERIAL.printf("[PRIM] GET failed: %d %s\n", httpCode, httpCode < 0 ? http.errorToString(httpCode).c_str() : "");
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();  // Keeps the socket open thanks to setReuse(true)

    JsonDocument filterDoc;
    buildFilter(filterDoc);
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload,
                                                 DeserializationOption::Filter(filterDoc),
                                                 DeserializationOption::NestingLimit(12));
    if (error) {
        USE_SERIAL.printf("[PRIM] JSON error: %s (%u bytes)\n", error.c_str(), payload.length());
        return false;
    }

    JsonObject delivery = doc["Siri"]["ServiceDelivery"];
    time_t responseTime = parseIsoUtc(delivery["ResponseTimestamp"]);
    JsonArray stops = delivery["StopMonitoringDelivery"][0]["MonitoredStopVisit"];

    const bool isRer = strcmp(type, "RER") == 0;
    std::vector<departureType> departures;
    for (JsonObject stop : stops) {
        if (departures.size() >= MAX_DEPARTURES) break;
        JsonObject journey = stop["MonitoredVehicleJourney"];
        JsonObject call = journey["MonitoredCall"];
        const char *expectedTime;

        if (isRer) {
            const char *platform = call["ArrivalPlatformName"]["value"] | "";
            if (strcmp(platform, filter) != 0) continue;
            expectedTime = call["ExpectedArrivalTime"];
        } else {
            // Filter matches LineRef (e.g. "C01287" for 323, stable) or OperatorRef (legacy, IDs change)
            const char *lineRef = journey["LineRef"]["value"] | "";
            const char *operatorRef = journey["OperatorRef"]["value"] | "";
            if (strstr(lineRef, filter) == nullptr && strstr(operatorRef, filter) == nullptr) continue;
            expectedTime = call["ExpectedDepartureTime"];
        }

        time_t expected = parseIsoUtc(expectedTime);
        if (expected == 0 || expected <= responseTime) continue;

        departureType dep;
        dep.name = journey["JourneyNote"][0]["value"] | "";
        dep.expected = expected;
        departures.push_back(dep);
    }

    USE_SERIAL.printf("[PRIM] %u departures (%u bytes)\n", departures.size(), payload.length());
    out = std::move(departures);
    return true;
}

void transportEnd()
{
    http.end();
    client.stop();
}
