#ifndef TRANSPORT_API_H
#define TRANSPORT_API_H

#include <Arduino.h>
#include <vector>
#include "types.h"

// Fetch upcoming departures for one stop. Consecutive calls reuse the same
// TLS session (keep-alive), so call transportEnd() once all stops are fetched.
// Returns true when the API answered and the payload parsed; `out` (and
// `lines`: LineRefs of the kept departures) is only written on success.
bool fetchDepartures(const char *url, const char *type, const char *filter, std::vector<departureType> &out,
                     std::vector<String> *lines = nullptr);

// Append the active incidents of one line ("STIF:Line::C01727:") that concern
// the whole line or the given stop (its MonitoringRef), blocking ones first.
// Shares the PRIM connection with fetchDepartures(). Returns false on failure.
bool fetchIncidents(const char *lineRef, const char *monitoringRef, std::vector<incidentType> &out);

// Close the shared PRIM connection.
void transportEnd();

#endif
