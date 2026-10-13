#include "stop_finder.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <config.h>
#include "misc.h"

#define USE_SERIAL Serial

static const char* PRIM_BASE = "https://prim.iledefrance-mobilites.fr/marketplace";
static const size_t MAX_PREVIEW_GROUPS = 16;
static const size_t MAX_GROUP_DESTS = 3;

// One-off GET on PRIM (the portal is interactive, so no connection reuse needed)
static bool primGet(const String& url, String& payload)
{
    WiFiClientSecure client;
    client.setInsecure();  // Same as transport_api: no CA pinned
    HTTPClient http;
    http.setConnectTimeout(5000);
    http.setTimeout(8000);
    if (!http.begin(client, url)) {
        USE_SERIAL.println("[Finder] begin failed");
        return false;
    }
    http.addHeader("apiKey", API_TOKEN);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        USE_SERIAL.printf("[Finder] GET %s failed: %d\n", url.c_str(), code);
        http.end();
        return false;
    }
    payload = http.getString();
    http.end();
    return true;
}

// "line:IDFM:C01727" or "STIF:Line::C01727:" -> "C01727"
static String lineCode(const char* ref)
{
    String s = ref ? ref : "";
    if (s.endsWith(":")) s.remove(s.length() - 1);
    int colon = s.lastIndexOf(':');
    return colon >= 0 ? s.substring(colon + 1) : s;
}

bool searchStopAreas(const char* query, String& outJson)
{
    String url = String(PRIM_BASE) + "/v2/navitia/places?type%5B%5D=stop_area&count=8&disable_geojson=true&q=" + urlEncode(query);
    String payload;
    if (!primGet(url, payload)) return false;

    JsonDocument filter;
    JsonObject place = filter["places"][0].to<JsonObject>();
    place["id"] = true;
    place["name"] = true;
    JsonObject line = place["stop_area"]["lines"][0].to<JsonObject>();
    line["id"] = true;
    line["code"] = true;
    line["color"] = true;
    line["text_color"] = true;
    line["commercial_mode"]["name"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    if (err) {
        USE_SERIAL.printf("[Finder] search JSON error: %s\n", err.c_str());
        return false;
    }

    JsonDocument out;
    JsonArray results = out.to<JsonArray>();
    for (JsonObject p : doc["places"].as<JsonArray>()) {
        JsonObject r = results.add<JsonObject>();
        r["id"] = lineCode(p["id"]);  // "stop_area:IDFM:424396" -> "424396"
        r["name"] = p["name"];
        JsonArray lines = r["lines"].to<JsonArray>();
        for (JsonObject l : p["stop_area"]["lines"].as<JsonArray>()) {
            JsonObject o = lines.add<JsonObject>();
            o["id"] = lineCode(l["id"]);
            o["code"] = l["code"];
            o["mode"] = l["commercial_mode"]["name"];
            o["color"] = l["color"];
            o["text"] = l["text_color"];
        }
    }
    outJson = "";
    serializeJson(out, outJson);
    return true;
}

bool previewStop(const char* monitoringRef, String& outJson)
{
    String url = String(PRIM_BASE) + "/stop-monitoring?MonitoringRef=" + urlEncode(monitoringRef);
    String payload;
    if (!primGet(url, payload)) return false;

    JsonDocument filter;
    JsonObject visit = filter["Siri"]["ServiceDelivery"]["StopMonitoringDelivery"][0]["MonitoredStopVisit"][0].to<JsonObject>();
    visit["MonitoringRef"] = true;
    JsonObject journey = visit["MonitoredVehicleJourney"].to<JsonObject>();
    journey["LineRef"] = true;
    journey["DirectionName"] = true;
    journey["DestinationName"] = true;
    journey["MonitoredCall"]["ArrivalPlatformName"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload,
                                               DeserializationOption::Filter(filter),
                                               DeserializationOption::NestingLimit(12));
    if (err) {
        USE_SERIAL.printf("[Finder] preview JSON error: %s\n", err.c_str());
        return false;
    }

    JsonDocument out;
    JsonArray groups = out.to<JsonArray>();
    JsonArray visits = doc["Siri"]["ServiceDelivery"]["StopMonitoringDelivery"][0]["MonitoredStopVisit"];
    for (JsonObject v : visits) {
        JsonObject j = v["MonitoredVehicleJourney"];
        String line = lineCode(j["LineRef"]["value"]);
        const char* ref = v["MonitoringRef"]["value"] | monitoringRef;
        const char* platform = j["MonitoredCall"]["ArrivalPlatformName"]["value"] | "";
        const char* dest = j["DirectionName"][0]["value"] | (j["DestinationName"][0]["value"] | "");

        JsonObject group;
        for (JsonObject g : groups) {
            if (line == g["line"].as<const char*>() && strcmp(ref, g["ref"]) == 0 && strcmp(platform, g["platform"]) == 0) {
                group = g;
                break;
            }
        }
        if (group.isNull()) {
            if (groups.size() >= MAX_PREVIEW_GROUPS) continue;
            group = groups.add<JsonObject>();
            group["line"] = line;
            group["ref"] = ref;
            group["platform"] = platform;
            group["dests"].to<JsonArray>();
            group["n"] = 0;
        }
        group["n"] = group["n"].as<int>() + 1;
        JsonArray dests = group["dests"];
        bool seen = dest[0] == '\0';
        for (const char* d : dests) {
            if (strcmp(d, dest) == 0) { seen = true; break; }
        }
        if (!seen && dests.size() < MAX_GROUP_DESTS) dests.add(dest);
    }
    outJson = "";
    serializeJson(out, outJson);
    return true;
}
