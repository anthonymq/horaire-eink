#pragma once

#include <Arduino.h>
#include <ctime>

// Parse an ISO-8601 UTC timestamp ("2026-01-11T18:12:00.000Z") into a UTC epoch.
// Returns 0 on parse failure.
time_t parseIsoUtc(const char *datetimeString);

// Percent-encode a query string value (keeps ':' so STIF refs stay readable).
String urlEncode(const char *s);

// Transliterate UTF-8 to the 7-bit ASCII the e-ink fonts can draw: accents are
// dropped, typographic quotes/dashes/spaces become plain ones, the rest is removed.
String asciiFold(const char *utf8);
