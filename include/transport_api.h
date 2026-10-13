#ifndef TRANSPORT_API_H
#define TRANSPORT_API_H

#include <Arduino.h>
#include <vector>
#include "types.h"

// Fetch upcoming departures for one stop. Consecutive calls reuse the same
// TLS session (keep-alive), so call transportEnd() once all stops are fetched.
// What the kept departures of a stop tell about it (for the incident lookup)
struct StopSeen {
    std::vector<String> lines;         // LineRefs ("STIF:Line::C01727:")
    std::vector<String> destinations;  // DestinationRefs ("STIF:StopArea:SP:44411:")
};

// Returns true when the API answered and the payload parsed; `out` (and
// `seen`) is only written on success.
bool fetchDepartures(const char *url, const char *type, const char *filter, std::vector<departureType> &out,
                     StopSeen *seen = nullptr);

// Append the active incidents of one line ("STIF:Line::C01727:") that concern
// the given stop (its MonitoringRef): at the stop, or on a part of the line
// through it in the direction of `destinations` (none known = any direction).
// Blocking ones first. Shares the PRIM connection with fetchDepartures().
// Returns false on failure.
bool fetchIncidents(const char *lineRef, const char *monitoringRef, const std::vector<String> &destinations,
                    std::vector<incidentType> &out);

// Close the shared PRIM connection.
void transportEnd();

#endif
