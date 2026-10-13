#pragma once

// Plain C++ helpers (no Arduino dependency) so they can be unit-tested on the host.

#include <cstddef>
#include <ctime>
#include <string>

// Epoch of a UTC calendar time (month 1-12), whatever the TZ
time_t utcEpoch(int year, int month, int day, int hour, int minute, int second);

// Parse an ISO-8601 UTC timestamp ("2026-01-11T18:12:00.000Z") into a UTC epoch.
// Returns 0 on parse failure.
time_t parseIsoUtc(const char *datetimeString);

// Transliterate UTF-8 to the 7-bit ASCII the e-ink fonts can draw: accents are
// dropped, typographic quotes/dashes/spaces become plain ones, the rest is removed.
std::string asciiFoldStd(const char *utf8);

// "STIF:Line::C01727:" -> "line:IDFM:C01727", "STIF:StopPoint:Q:39570:" ->
// "stop_point:IDFM:39570", "STIF:StopArea:SP:46366:" -> "stop_area:IDFM:46366".
// Returns false for any other ref.
bool stifToNavitia(const char *stif, char *out, size_t outSize);
