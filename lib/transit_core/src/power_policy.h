#pragma once

// Battery and fetch-rate decisions (see docs/power_optimization.md).

#include <cstddef>
#include <cstdint>

static const uint32_t FETCH_INTERVAL_S = 120;       // Transit data refresh while a departure is near
static const uint32_t FETCH_IDLE_INTERVAL_S = 300;  // Nothing within NEAR_DEPARTURE_S (or nothing at all)
static const long NEAR_DEPARTURE_S = 20 * 60;
static const uint32_t INCIDENT_INTERVAL_S = 300;    // Traffic incidents (one gzip line report per line)

static const int BATTERY_LOW_PCT = 15;       // Fetch half as often
static const int BATTERY_CRITICAL_PCT = 5;   // "Charge me" screen, then power off

enum BatteryState { BATTERY_OK, BATTERY_LOW, BATTERY_CRITICAL };

// `level` is 0-100, or negative when unknown. On USB the battery never limits anything.
BatteryState batteryState(int level, bool onUsb);

// Seconds until the next fetch. `secsToNextDeparture` is the soonest upcoming
// (not cancelled) departure over all boards, -1 when there is none.
uint32_t fetchIntervalS(long secsToNextDeparture, BatteryState battery);
uint32_t incidentIntervalS(BatteryState battery);

// Why the last fetch cycle failed, for the header
enum FetchStage { FETCH_OK, FETCH_WIFI, FETCH_HTTP, FETCH_PARSE };

struct FetchError {
    FetchStage stage;
    int code;  // HTTP status, or the HTTPClient error (< 0) for FETCH_HTTP
};

// Short ASCII text: "WiFi introuvable", "PRIM : cle API refusee", "PRIM : quota atteint"...
// Empty for FETCH_OK.
void describeFetchError(const FetchError &error, char *out, size_t outSize);
