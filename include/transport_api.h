#ifndef TRANSPORT_API_H
#define TRANSPORT_API_H

#include <Arduino.h>
#include <vector>
#include "types.h"

// Fetch upcoming departures for one stop (see parseDepartures() for the filter
// rules). Consecutive calls reuse the same PRIM connection, so call primEnd()
// once all stops are fetched. Returns true when the API answered and the
// payload parsed; `out` (and `seen`) is only written on success, otherwise
// primLastError() says why.
bool fetchDepartures(const char *url, const char *type, const char *filter, std::vector<departureType> &out,
                     StopSeen *seen = nullptr);

#endif
