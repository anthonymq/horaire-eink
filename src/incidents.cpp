#include "incidents.h"
#include <ArduinoJson.h>
#include <algorithm>
#include "prim_client.h"
#include "gzip_reader.h"
#include "text_utils.h"
#include "misc.h"

#define USE_SERIAL Serial

static const size_t MAX_INCIDENTS = 4;            // Per stop
static const size_t MAX_GZIP_BODY = 1024 * 1024;  // Compressed line report (RER C ~ 60 KB)
static const size_t MAX_ROUTE_CACHE = 32;         // Route stop orders kept (for section incidents)

static const char *NAVITIA_BASE = "https://prim.iledefrance-mobilites.fr/marketplace/v2/navitia";

static void buildIncidentFilter(JsonDocument &filter)
{
    filter["status"] = true;
    filter["tags"] = true;
    filter["application_periods"] = true;
    filter["severity"]["effect"] = true;
    JsonObject message = filter["messages"][0].to<JsonObject>();
    message["text"] = true;
    message["channel"]["types"] = true;
    JsonObject impacted = filter["impacted_objects"][0].to<JsonObject>();
    impacted["pt_object"]["id"] = true;
    impacted["pt_object"]["embedded_type"] = true;
    for (const char *key : {"impacted_section", "impacted_rail_section"}) {
        JsonObject section = impacted[key].to<JsonObject>();
        section["from"]["id"] = true;
        section["to"]["id"] = true;
        section["routes"][0]["id"] = true;
    }
}

// Navitia local time "20261016T094000" (Europe/Paris, the device TZ) -> epoch
static time_t parseNavitiaLocal(const char *s)
{
    struct tm t = {};
    if (!s || sscanf(s, "%4d%2d%2dT%2d%2d%2d", &t.tm_year, &t.tm_mon, &t.tm_mday,
                     &t.tm_hour, &t.tm_min, &t.tm_sec) != 6) return 0;
    t.tm_year -= 1900;
    t.tm_mon -= 1;
    t.tm_isdst = -1;
    return mktime(&t);
}

// The period running now, as "9h40 - 16h00" when it ends the day it started,
// otherwise "jusqu'au 23/10". Empty if none matches (clock not set...).
static String currentPeriod(JsonArrayConst periods, time_t now)
{
    for (JsonObjectConst p : periods) {
        time_t begin = parseNavitiaLocal(p["begin"]);
        time_t end = parseNavitiaLocal(p["end"]);
        if (begin == 0 || end == 0 || now < begin || now >= end) continue;
        struct tm b, e;
        localtime_r(&begin, &b);
        localtime_r(&end, &e);
        char buf[24];
        if (b.tm_yday == e.tm_yday && b.tm_year == e.tm_year) {
            snprintf(buf, sizeof(buf), "%dh%02d - %dh%02d", b.tm_hour, b.tm_min, e.tm_hour, e.tm_min);
        } else {
            snprintf(buf, sizeof(buf), "jusqu'au %02d/%02d", e.tm_mday, e.tm_mon + 1);
        }
        return buf;
    }
    return "";
}

// "RER C : 6 gares non desservies" -> "6 gares non desservies"
static String stripLinePrefix(const String &title)
{
    int colon = title.indexOf(':');
    if (colon < 0 || colon > 14) return title;
    int start = colon + 1;
    while (start < (int)title.length() && title[start] == ' ') start++;
    return start < (int)title.length() ? title.substring(start) : title;
}

static int incidentLevel(const char *effect)
{
    static const char *const BLOCKING[] = {"NO_SERVICE", "REDUCED_SERVICE", "MODIFIED_SERVICE", "DETOUR"};
    static const char *const DISRUPTED[] = {"SIGNIFICANT_DELAYS", "STOP_MOVED"};
    for (const char *e : BLOCKING) if (strcmp(effect, e) == 0) return 2;
    for (const char *e : DISRUPTED) if (strcmp(effect, e) == 0) return 1;
    return 0;  // Information only, extra service...
}

static uint32_t fnv1a(const char *s)
{
    uint32_t h = 2166136261u;
    for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
    return h;
}

// Stops of a route in travel order (hashed stop point and stop area ids)
struct RouteStops {
    uint32_t route;
    std::vector<uint32_t> points;
    std::vector<uint32_t> areas;
};
static std::vector<RouteStops> routeCache;

// A SIRI stop ref as Navitia route stop ids (hashed). Refs are quays
// ("STIF:StopPoint:Q:N:" -> stop_point:IDFM:N) or stop places ("STIF:StopArea:SP:N:"
// -> stop_point:IDFM:monomodalStopPlace:N, or stop_area:IDFM:N when picked in the portal).
struct StopKey {
    uint32_t point = 0;
    uint32_t area = 0;

    explicit StopKey(const char *stifRef) {
        char nav[48];
        if (!stifToNavitia(stifRef, nav, sizeof(nav))) return;
        if (strncmp(nav, "stop_point:", 11) == 0) {
            point = fnv1a(nav);
            return;
        }
        area = fnv1a(nav);
        char place[64];
        snprintf(place, sizeof(place), "stop_point:IDFM:monomodalStopPlace:%s", nav + strlen("stop_area:IDFM:"));
        point = fnv1a(place);
    }

    bool valid() const { return point || area; }

    // Index of this stop on the route at or after `from`, -1 if absent
    int indexOn(const RouteStops &r, size_t from = 0) const {
        for (size_t i = from; i < r.points.size(); i++) {
            if ((point && r.points[i] == point) || (area && r.areas[i] == area)) return i;
        }
        return -1;
    }
};

// Ordered stops of a route, fetched once (route_schedules lists every stop of the
// route in order; ~90 KB for an RER route, filtered down to the ids)
static const RouteStops *routeStops(const char *routeId)
{
    uint32_t route = fnv1a(routeId);
    for (const auto &r : routeCache) {
        if (r.route == route) return &r;
    }

    char url[224];
    snprintf(url, sizeof(url), "%s/routes/%s/route_schedules?depth=0&items_per_schedule=1&disable_geojson=true",
             NAVITIA_BASE, routeId);
    PsramBody body(MAX_GZIP_BODY);
    bool gzipped = false;
    if (!primGetGzip(url, body, gzipped)) return nullptr;

    JsonDocument filter;
    JsonObject point = filter["route_schedules"][0]["table"]["rows"][0]["stop_point"].to<JsonObject>();
    point["id"] = true;
    point["stop_area"]["id"] = true;
    JsonDocument doc;
    DeserializationError error;
    if (gzipped) {
        GzipReader reader;
        if (!reader.begin(body.data, body.len)) return nullptr;
        error = deserializeJson(doc, reader, DeserializationOption::Filter(filter));
    } else {
        error = deserializeJson(doc, (const char *)body.data, body.len, DeserializationOption::Filter(filter));
    }
    if (error) {
        USE_SERIAL.printf("[INCIDENTS] route JSON error: %s\n", error.c_str());
        return nullptr;
    }

    RouteStops r{route, {}, {}};
    for (JsonObjectConst row : doc["route_schedules"][0]["table"]["rows"].as<JsonArrayConst>()) {
        r.points.push_back(fnv1a(row["stop_point"]["id"] | ""));
        r.areas.push_back(fnv1a(row["stop_point"]["stop_area"]["id"] | ""));
    }
    if (r.areas.empty()) return nullptr;
    if (routeCache.size() >= MAX_ROUTE_CACHE) routeCache.clear();
    routeCache.push_back(std::move(r));
    USE_SERIAL.printf("[INCIDENTS] route %s: %u stops\n", routeId, routeCache.back().areas.size());
    return &routeCache.back();
}

// Where the incident lookup stands: our stop (and its quays), and where its trains go
struct StopContext {
    StopKey stop;
    std::vector<StopKey> destinations;
    std::vector<String> stopIds;  // Navitia ids an incident at our stop would name
};

// Does a section of the line (from/to stop areas, on some routes) include our
// station, on a route that then reaches one of our trains' destinations (i.e.
// in our direction)? Unknown cases count as yes: better a stray alert than a
// missed one.
static bool sectionCoversStop(JsonObjectConst section, const StopContext &ctx)
{
    JsonArrayConst routes = section["routes"];
    if (routes.size() == 0 || !ctx.stop.valid()) return true;
    uint32_t from = fnv1a(section["from"]["id"] | "");
    uint32_t to = fnv1a(section["to"]["id"] | "");
    for (JsonObjectConst route : routes) {
        const RouteStops *r = routeStops(route["id"] | "");
        if (!r) return true;
        int ours = ctx.stop.indexOn(*r);
        if (ours < 0) continue;  // Another branch
        if (!ctx.destinations.empty()) {
            bool ourWay = false;
            for (const StopKey &dest : ctx.destinations) ourWay |= dest.indexOn(*r, ours + 1) >= 0;
            if (!ourWay) continue;  // Trains that don't go where ours go (other direction)
        }
        int a = -1, b = -1;
        for (size_t i = 0; i < r->areas.size(); i++) {
            if (a < 0 && r->areas[i] == from) a = i;
            if (b < 0 && r->areas[i] == to) b = i;
        }
        if (a < 0 || b < 0) return true;
        if (min(a, b) <= ours && ours <= max(a, b)) return true;
    }
    return false;
}

enum Concern { CONCERN_NONE, CONCERN_LINE, CONCERN_STOP };

// Is this incident about our part of the line, or about our stop?
static Concern concernsStop(JsonArrayConst objects, const StopContext &ctx)
{
    Concern concern = CONCERN_NONE;
    for (JsonObjectConst o : objects) {
        const char *type = o["pt_object"]["embedded_type"] | "";
        const char *id = o["pt_object"]["id"] | "";
        if (strcmp(type, "line") == 0 || strcmp(type, "route") == 0 || strcmp(type, "network") == 0) {
            JsonObjectConst section = o["impacted_rail_section"];
            if (section.isNull()) section = o["impacted_section"];
            if (section.isNull() || sectionCoversStop(section, ctx)) return CONCERN_LINE;
        } else if (concern == CONCERN_NONE) {
            for (const String &stopId : ctx.stopIds) {
                if (stopId == id) concern = CONCERN_STOP;
            }
        }
    }
    return concern;
}

// Line named by a title prefix: "Bus 27 : Manifestation..." -> "27"
static String titleLine(const String &title)
{
    int colon = title.indexOf(':');
    if (colon < 0 || colon > 14) return "";
    String prefix = title.substring(0, colon);
    prefix.trim();
    int space = prefix.lastIndexOf(' ');
    return space >= 0 ? prefix.substring(space + 1) : prefix;
}

static void addIncident(std::vector<incidentType> &list, const String &text, bool blocking, const String &when)
{
    for (auto &inc : list) {
        if (inc.text == text) {  // The same notice is often published once per direction
            inc.blocking |= blocking;
            return;
        }
    }
    list.push_back({text, blocking, when});
}

bool fetchIncidents(const char *lineRef, const char *monitoringRef, const StopSeen &seen,
                    std::vector<incidentType> &out)
{
    char lineId[32];
    if (!stifToNavitia(lineRef, lineId, sizeof(lineId))) return false;

    char url[192];
    snprintf(url, sizeof(url), "%s/lines/%s/line_reports?filter_status%%5B%%5D=active&disable_geojson=true",
             NAVITIA_BASE, lineId);

    // Line reports are large (~1 MB for an RER line, mostly lift outages) but compress ~15x
    PsramBody body(MAX_GZIP_BODY);
    bool gzipped = false;
    if (!primGetGzip(url, body, gzipped)) return false;
    if (!gzipped) {
        USE_SERIAL.println("[INCIDENTS] line report not gzipped");
        return false;
    }

    GzipReader reader;
    if (!reader.begin(body.data, body.len) || !reader.skipPast("\"disruptions\":") || reader.readNonSpace() != '[') {
        USE_SERIAL.println("[INCIDENTS] no disruptions array");
        return false;
    }

    StopContext ctx{StopKey(monitoringRef), {}, {}};
    for (const std::string &d : seen.destinations) {
        StopKey key(d.c_str());
        if (key.valid()) ctx.destinations.push_back(key);
    }
    char nav[48];
    if (stifToNavitia(monitoringRef, nav, sizeof(nav))) ctx.stopIds.push_back(nav);
    for (const std::string &q : seen.quays) {
        if (stifToNavitia(q.c_str(), nav, sizeof(nav))) ctx.stopIds.push_back(nav);
    }

    // Stop-level incidents of a line report also come from the other lines
    // serving its stops ("Bus 27 : ..." in the 132 report); only the title says
    // which line, so they are checked against the line code once it is read.
    struct Candidate {
        incidentType incident;
        String titleLine;  // Set for stop-level incidents
    };
    std::vector<Candidate> candidates;

    // One disruption at a time keeps the parse memory to a few KB
    JsonDocument filter;
    buildIncidentFilter(filter);
    JsonDocument doc;
    std::vector<incidentType> found;
    int parsed = 0;
    for (;;) {
        int c = reader.readNonSpace();
        if (c == ']') break;
        if (c == ',') continue;
        if (c != '{') {
            USE_SERIAL.printf("[INCIDENTS] unexpected '%c'\n", c);
            return false;
        }
        reader.unread(c);
        DeserializationError error = deserializeJson(doc, reader, DeserializationOption::Filter(filter),
                                                     DeserializationOption::NestingLimit(24));
        if (error) {
            USE_SERIAL.printf("[INCIDENTS] JSON error: %s\n", error.c_str());
            return false;
        }
        parsed++;

        if (strcmp(doc["status"] | "", "active") != 0) continue;
        bool lift = false;
        for (const char *tag : doc["tags"].as<JsonArrayConst>()) {
            if (tag && strcmp(tag, "Ascenseur") == 0) lift = true;
        }
        if (lift) continue;  // Lift outages: dozens per RER line, not about departures
        int level = incidentLevel(doc["severity"]["effect"] | "");
        if (level == 0) continue;
        Concern concern = concernsStop(doc["impacted_objects"], ctx);
        if (concern == CONCERN_NONE) continue;

        // Short plain-text title; "pids" (station screens) is even shorter but vaguer
        const char *title = nullptr, *notification = nullptr, *pids = nullptr;
        for (JsonObjectConst m : doc["messages"].as<JsonArrayConst>()) {
            for (const char *t : m["channel"]["types"].as<JsonArrayConst>()) {
                if (!t) continue;
                if (strcmp(t, "title") == 0) title = m["text"];
                else if (strcmp(t, "notification") == 0) notification = m["text"];
                else if (strcmp(t, "pids") == 0) pids = m["text"];
            }
        }
        const char *text = title ? title : notification ? notification : pids;
        if (!text) continue;
        String full = asciiFold(text);
        String folded = stripLinePrefix(full);
        folded.trim();
        if (folded.length()) {
            candidates.push_back({{folded, level == 2, currentPeriod(doc["application_periods"], time(nullptr))},
                                  concern == CONCERN_STOP ? titleLine(full) : String()});
        }
    }
    if (reader.failed()) {
        USE_SERIAL.println("[INCIDENTS] inflate error");
        return false;
    }

    // The line's own code comes after the disruptions: line_reports[0].line.code
    String lineCode;
    if (reader.skipPast("\"line_reports\":") && reader.readNonSpace() == '[') {
        JsonDocument codeFilter;
        codeFilter["line"]["code"] = true;
        if (!deserializeJson(doc, reader, DeserializationOption::Filter(codeFilter),
                             DeserializationOption::NestingLimit(24))) {
            lineCode = doc["line"]["code"] | "";
        }
    }

    for (const Candidate &c : candidates) {
        // "RERC" still matches "C"; no code or no prefix: keep
        if (lineCode.length() && c.titleLine.length() && !c.titleLine.endsWith(lineCode)) continue;
        addIncident(found, c.incident.text, c.incident.blocking, c.incident.when);
    }

    std::stable_sort(found.begin(), found.end(),
                     [](const incidentType &a, const incidentType &b) { return a.blocking > b.blocking; });
    for (auto &inc : found) {
        if (out.size() >= MAX_INCIDENTS) break;
        bool dup = false;
        for (auto &o : out) dup |= o.text == inc.text;
        if (!dup) out.push_back(inc);
    }
    USE_SERIAL.printf("[INCIDENTS] %s: %d reports, %u kept (%u bytes gzip)\n", lineId, parsed, found.size(), body.len);
    return true;
}
