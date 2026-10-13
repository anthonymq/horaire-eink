#include "power_policy.h"
#include <cstdio>

BatteryState batteryState(int level, bool onUsb)
{
    if (onUsb || level < 0) return BATTERY_OK;
    if (level <= BATTERY_CRITICAL_PCT) return BATTERY_CRITICAL;
    if (level <= BATTERY_LOW_PCT) return BATTERY_LOW;
    return BATTERY_OK;
}

uint32_t fetchIntervalS(long secsToNextDeparture, BatteryState battery)
{
    // A far-off departure barely moves between fetches, and a new one showing up
    // a few minutes late doesn't matter when the next one is 20 min away
    bool near = secsToNextDeparture >= 0 && secsToNextDeparture <= NEAR_DEPARTURE_S;
    uint32_t interval = near ? FETCH_INTERVAL_S : FETCH_IDLE_INTERVAL_S;
    return battery == BATTERY_OK ? interval : interval * 2;
}

uint32_t incidentIntervalS(BatteryState battery)
{
    return battery == BATTERY_OK ? INCIDENT_INTERVAL_S : INCIDENT_INTERVAL_S * 3;
}

void describeFetchError(const FetchError &error, char *out, size_t outSize)
{
    switch (error.stage) {
    case FETCH_OK:
        snprintf(out, outSize, "%s", "");
        return;
    case FETCH_WIFI:
        snprintf(out, outSize, "WiFi introuvable");
        return;
    case FETCH_PARSE:
        snprintf(out, outSize, "PRIM : reponse illisible");
        return;
    case FETCH_HTTP:
        break;
    }
    int code = error.code;
    if (code < 0) {
        // HTTPClient: connection refused/lost, TLS failure, timeout
        snprintf(out, outSize, "PRIM injoignable");
    } else if (code == 401 || code == 403) {
        snprintf(out, outSize, "PRIM : cle API refusee");
    } else if (code == 429) {
        snprintf(out, outSize, "PRIM : quota atteint");
    } else if (code >= 500) {
        snprintf(out, outSize, "PRIM en panne (%d)", code);
    } else {
        snprintf(out, outSize, "PRIM : erreur %d", code);
    }
}
