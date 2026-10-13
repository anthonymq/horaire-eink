#ifndef INCIDENTS_H
#define INCIDENTS_H

#include <Arduino.h>
#include <vector>
#include "types.h"

// Append the active incidents of one line ("STIF:Line::C01727:") that concern
// the given stop (its MonitoringRef): at the stop or one of `seen.quays` (and
// titled for this line), or on a part of the line through it in the direction
// of `seen.destinations` (none known = any direction). Blocking ones first.
// Shares the PRIM connection with fetchDepartures(). Returns false on failure.
bool fetchIncidents(const char *lineRef, const char *monitoringRef, const StopSeen &seen,
                    std::vector<incidentType> &out);

#endif
