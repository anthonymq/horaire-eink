#include "transport_api.h"
#include "prim_client.h"

#define USE_SERIAL Serial

bool fetchDepartures(const char *url, const char *type, const char *filter, std::vector<departureType> &out,
                     StopSeen *seen)
{
    String payload;
    if (!primGet(url, payload)) return false;

    if (!parseDepartures(payload.c_str(), payload.length(), type, filter, out, seen)) {
        USE_SERIAL.printf("[PRIM] JSON error (%u bytes)\n", payload.length());
        primSetParseError();
        return false;
    }
    USE_SERIAL.printf("[PRIM] %u departures (%u bytes)\n", out.size(), payload.length());
    return true;
}
