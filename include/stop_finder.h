#ifndef STOP_FINDER_H
#define STOP_FINDER_H

#include <Arduino.h>

/**
 * Stop lookup for the setup portal (needs a working STA connection).
 * Both calls go through PRIM with API_TOKEN and return compact JSON for the
 * /stops web page.
 */

// Navitia place search, stop areas only. Writes:
//   [{"id":"424396","name":"Ivry-sur-Seine (Ivry-sur-Seine)",
//     "lines":[{"id":"C01727","code":"C","mode":"RER","color":"FFCE00","text":"000000"}]}]
// `id` is the IDFM number: MonitoringRef = STIF:StopArea:SP:<id>:, LineRef = STIF:Line::<id>:
bool searchStopAreas(const char* query, String& outJson);

// Live departures at `monitoringRef`, grouped per (line, quay, platform) so the
// user can pick a direction. Writes:
//   [{"line":"C01287","ref":"STIF:StopPoint:Q:16918:","platform":"","dests":["Issy-Val de Seine RER"],"n":2}]
bool previewStop(const char* monitoringRef, String& outJson);

#endif
