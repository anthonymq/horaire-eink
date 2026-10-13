#ifndef TRANSPORT_API_H
#define TRANSPORT_API_H

#include <Arduino.h>
#include <vector>
#include "types.h"

// Fetch upcoming departures for one stop. Consecutive calls reuse the same
// TLS session (keep-alive), so call transportEnd() once all stops are fetched.
// Returns true when the API answered and the payload parsed; `out` is only
// written on success.
bool fetchDepartures(const char *url, const char *type, const char *filter, std::vector<departureType> &out);

// Close the shared PRIM connection.
void transportEnd();

#endif
