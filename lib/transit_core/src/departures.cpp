#include "departures.h"
#include "text_utils.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <cstring>

static const size_t MAX_DEPARTURES = 8;  // Keep a few spare so the list survives between fetches
static const size_t MAX_LINES = 2;       // Lines remembered per stop (for the incident lookup)
static const size_t MAX_DESTINATIONS = 6;
static const size_t MAX_QUAYS = 6;
static const long MIN_DELAY_S = 90;      // Below this a train counts as on time ("+2" from 90 s)

long departureType::delayMinutes() const
{
    if (cancelled || aimed == 0 || expected - aimed < MIN_DELAY_S) return 0;
    return (long)(expected - aimed + 30) / 60;
}

// Only keep the fields we read: shrinks the JsonDocument and parsing time.
static void buildFilter(JsonDocument &filter)
{
    JsonObject delivery = filter["Siri"]["ServiceDelivery"].to<JsonObject>();
    delivery["ResponseTimestamp"] = true;
    JsonObject visit = delivery["StopMonitoringDelivery"][0]["MonitoredStopVisit"][0].to<JsonObject>();
    visit["MonitoringRef"] = true;
    JsonObject journey = visit["MonitoredVehicleJourney"].to<JsonObject>();
    journey["JourneyNote"] = true;
    journey["LineRef"] = true;
    journey["OperatorRef"] = true;
    journey["DestinationRef"] = true;
    JsonObject call = journey["MonitoredCall"].to<JsonObject>();
    call["ArrivalPlatformName"] = true;
    call["ExpectedArrivalTime"] = true;
    call["ExpectedDepartureTime"] = true;
    call["AimedArrivalTime"] = true;
    call["AimedDepartureTime"] = true;
    call["ArrivalStatus"] = true;
    call["DepartureStatus"] = true;
}

static void addUnique(std::vector<std::string> &list, const char *value, size_t max)
{
    if (value[0] && list.size() < max && std::find(list.begin(), list.end(), value) == list.end()) {
        list.push_back(value);
    }
}

// Trains are shown at arrival, buses at departure; terminus calls only have one of them
static const char *pick(bool arrivalFirst, const char *arrival, const char *departure)
{
    return arrivalFirst ? (arrival ? arrival : departure) : (departure ? departure : arrival);
}

bool parseDepartures(const char *json, size_t len, const char *type, const char *filter,
                     std::vector<departureType> &out, StopSeen *seen)
{
    JsonDocument filterDoc;
    buildFilter(filterDoc);
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json, len,
                                                 DeserializationOption::Filter(filterDoc),
                                                 DeserializationOption::NestingLimit(12));
    if (error) return false;

    JsonObject delivery = doc["Siri"]["ServiceDelivery"];
    time_t responseTime = parseIsoUtc(delivery["ResponseTimestamp"]);
    JsonArray stops = delivery["StopMonitoringDelivery"][0]["MonitoredStopVisit"];

    const bool isRer = strcmp(type, "RER") == 0;
    const bool matchAll = filter[0] == '\0';
    const bool byLine = !isRer || strncmp(filter, "STIF:", 5) == 0;
    std::vector<departureType> departures;
    StopSeen found;
    for (JsonObject stop : stops) {
        if (departures.size() >= MAX_DEPARTURES) break;
        JsonObject journey = stop["MonitoredVehicleJourney"];
        JsonObject call = journey["MonitoredCall"];
        const char *lineRef = journey["LineRef"]["value"] | "";

        if (!matchAll && byLine) {
            const char *operatorRef = journey["OperatorRef"]["value"] | "";
            if (strstr(lineRef, filter) == nullptr && strstr(operatorRef, filter) == nullptr) continue;
        } else if (!matchAll) {
            const char *platform = call["ArrivalPlatformName"]["value"] | "";
            if (strcmp(platform, filter) != 0) continue;
        }

        departureType dep;
        dep.cancelled = strcmp(call["DepartureStatus"] | "", "cancelled") == 0 ||
                        strcmp(call["ArrivalStatus"] | "", "cancelled") == 0;
        dep.expected = parseIsoUtc(pick(isRer, call["ExpectedArrivalTime"], call["ExpectedDepartureTime"]));
        dep.aimed = parseIsoUtc(pick(isRer, call["AimedArrivalTime"], call["AimedDepartureTime"]));
        // A cancelled call is shown at its timetabled time (it may have no expected one)
        if (dep.cancelled && dep.aimed) dep.expected = dep.aimed;
        if (dep.expected == 0 || dep.expected <= responseTime) continue;
        dep.name = journey["JourneyNote"][0]["value"] | "";
        departures.push_back(dep);

        if (dep.cancelled) continue;  // Says nothing about where trains actually go
        addUnique(found.lines, lineRef, MAX_LINES);
        addUnique(found.destinations, journey["DestinationRef"]["value"] | "", MAX_DESTINATIONS);
        addUnique(found.quays, stop["MonitoringRef"]["value"] | "", MAX_QUAYS);
    }
    if (seen) *seen = std::move(found);
    out = std::move(departures);
    return true;
}
