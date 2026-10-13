#pragma once

// SIRI stop-monitoring parsing (PRIM), independent of the HTTP client so it can
// be tested on the host against saved responses.

#include <cstddef>
#include <ctime>
#include <string>
#include <vector>

struct departureType {
    std::string name;  // Journey note (RER mission code, e.g. "ROMI"); may be empty
    time_t expected;   // UTC epoch of expected departure/arrival (the aimed time when cancelled)
    time_t aimed;      // UTC epoch of the timetabled departure/arrival (0 = unknown)
    bool cancelled;    // DepartureStatus/ArrivalStatus "cancelled"

    // Minutes late (rounded), 0 when on time, early or unknown
    long delayMinutes() const;
};

// What the kept departures of a stop tell about it (for the incident lookup)
struct StopSeen {
    std::vector<std::string> lines;         // LineRefs ("STIF:Line::C01727:")
    std::vector<std::string> destinations;  // DestinationRefs ("STIF:StopArea:SP:44411:")
    std::vector<std::string> quays;         // MonitoringRefs of the visits: the quays of a stop area
};

// Parse a stop-monitoring response. `type` is the station type ("RER", "BUS"...)
// and `filter` its filter: empty = every departure; a full LineRef
// ("STIF:Line::C01727:") matches the line for any type; otherwise RER filters on
// the platform name and other types on a LineRef/OperatorRef substring.
// Departures already gone at the response time are dropped (cancelled ones by
// their aimed time). Returns false when the JSON doesn't parse.
bool parseDepartures(const char *json, size_t len, const char *type, const char *filter,
                     std::vector<departureType> &out, StopSeen *seen);
