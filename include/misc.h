#pragma once

#include <Arduino.h>
#include <ctime>

// Parse an ISO-8601 UTC timestamp ("2026-01-11T18:12:00.000Z") into a UTC epoch.
// Returns 0 on parse failure.
time_t parseIsoUtc(const char *datetimeString);

// Percent-encode a query string value (keeps ':' so STIF refs stay readable).
String urlEncode(const char *s);
