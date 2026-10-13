#include "transport_api.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <config.h>
#include "misc.h"
#include <miniz.h>  // ROM inflate (tinfl)
#include <algorithm>

#define USE_SERIAL Serial

static const size_t MAX_DEPARTURES = 8;  // Keep a few spare so the list survives between fetches
static const size_t MAX_LINES = 2;       // Lines remembered per stop (for the incident lookup)
static const size_t MAX_DESTINATIONS = 6;
static const size_t MAX_INCIDENTS = 4;   // Per stop
static const size_t MAX_GZIP_BODY = 1024 * 1024;  // Compressed line report (RER C ~ 60 KB)
static const size_t MAX_ROUTE_CACHE = 32;         // Route stop orders kept (for section incidents)

static const char *NAVITIA_BASE = "https://prim.iledefrance-mobilites.fr/marketplace/v2/navitia";

// One TLS connection shared by all stop requests: a TLS 1.3 handshake costs far
// more CPU/radio time than the request itself, so we only pay it once per cycle.
static WiFiClientSecure client;
static HTTPClient http;
static bool clientReady = false;

static void ensureClient()
{
    if (!clientReady) {
        client.setInsecure();  // No CA pinned (same as before); see review notes
        http.setReuse(true);
        http.setConnectTimeout(5000);
        http.setTimeout(8000);
        clientReady = true;
    }
}

// Only keep the fields we read: shrinks the JsonDocument and parsing time.
static void buildFilter(JsonDocument &filter)
{
    JsonObject delivery = filter["Siri"]["ServiceDelivery"].to<JsonObject>();
    delivery["ResponseTimestamp"] = true;
    JsonObject journey = delivery["StopMonitoringDelivery"][0]["MonitoredStopVisit"][0]["MonitoredVehicleJourney"].to<JsonObject>();
    journey["JourneyNote"] = true;
    journey["LineRef"] = true;
    journey["OperatorRef"] = true;
    journey["DestinationRef"] = true;
    JsonObject call = journey["MonitoredCall"].to<JsonObject>();
    call["ArrivalPlatformName"] = true;
    call["ExpectedArrivalTime"] = true;
    call["ExpectedDepartureTime"] = true;
}

bool fetchDepartures(const char *url, const char *type, const char *filter, std::vector<departureType> &out,
                     StopSeen *seen)
{
    ensureClient();
    if (!http.begin(client, url)) {
        USE_SERIAL.println("[PRIM] begin failed");
        return false;
    }
    http.addHeader("apiKey", API_TOKEN);
    http.setAcceptEncoding("identity");

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        USE_SERIAL.printf("[PRIM] GET failed: %d %s\n", httpCode, httpCode < 0 ? http.errorToString(httpCode).c_str() : "");
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();  // Keeps the socket open thanks to setReuse(true)

    JsonDocument filterDoc;
    buildFilter(filterDoc);
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload,
                                                 DeserializationOption::Filter(filterDoc),
                                                 DeserializationOption::NestingLimit(12));
    if (error) {
        USE_SERIAL.printf("[PRIM] JSON error: %s (%u bytes)\n", error.c_str(), payload.length());
        return false;
    }

    JsonObject delivery = doc["Siri"]["ServiceDelivery"];
    time_t responseTime = parseIsoUtc(delivery["ResponseTimestamp"]);
    JsonArray stops = delivery["StopMonitoringDelivery"][0]["MonitoredStopVisit"];

    const bool isRer = strcmp(type, "RER") == 0;
    // Empty filter = every departure. A full LineRef ("STIF:Line::C01727:", as set
    // by the portal stop finder) matches the line for any type. Otherwise RER
    // filters on the platform name and other types on a LineRef/OperatorRef part.
    const bool matchAll = filter[0] == '\0';
    const bool byLine = !isRer || strncmp(filter, "STIF:", 5) == 0;
    std::vector<departureType> departures;
    StopSeen found;
    for (JsonObject stop : stops) {
        if (departures.size() >= MAX_DEPARTURES) break;
        JsonObject journey = stop["MonitoredVehicleJourney"];
        JsonObject call = journey["MonitoredCall"];
        const char *lineRef = journey["LineRef"]["value"] | "";

        if (!matchAll && byLine) {
            const char *operatorRef = journey["OperatorRef"]["value"] | "";
            if (strstr(lineRef, filter) == nullptr && strstr(operatorRef, filter) == nullptr) continue;
        } else if (!matchAll) {
            const char *platform = call["ArrivalPlatformName"]["value"] | "";
            if (strcmp(platform, filter) != 0) continue;
        }

        // Trains are shown at arrival, buses at departure; terminus calls only have one of them
        const char *arrival = call["ExpectedArrivalTime"];
        const char *departure = call["ExpectedDepartureTime"];
        const char *expectedTime = isRer ? (arrival ? arrival : departure) : (departure ? departure : arrival);

        time_t expected = parseIsoUtc(expectedTime);
        if (expected == 0 || expected <= responseTime) continue;

        departureType dep;
        dep.name = journey["JourneyNote"][0]["value"] | "";
        dep.expected = expected;
        departures.push_back(dep);

        const char *destination = journey["DestinationRef"]["value"] | "";
        if (lineRef[0] && found.lines.size() < MAX_LINES &&
            std::find(found.lines.begin(), found.lines.end(), lineRef) == found.lines.end()) {
            found.lines.push_back(lineRef);
        }
        if (destination[0] && found.destinations.size() < MAX_DESTINATIONS &&
            std::find(found.destinations.begin(), found.destinations.end(), destination) == found.destinations.end()) {
            found.destinations.push_back(destination);
        }
    }
    if (seen) *seen = std::move(found);

    USE_SERIAL.printf("[PRIM] %u departures (%u bytes)\n", departures.size(), payload.length());
    out = std::move(departures);
    return true;
}

// ============================================================
// INCIDENTS (Navitia line reports)
// ============================================================
// Response body collected in PSRAM; HTTPClient::writeToStream() also undoes
// chunked transfer encoding.
class PsramBody : public Stream {
public:
    ~PsramBody() { free(data); }
    size_t write(uint8_t b) override { return write(&b, 1); }
    size_t write(const uint8_t *buf, size_t n) override {
        if (len + n > cap) {
            size_t newCap = max(cap * 2, len + n + 8192);
            if (newCap > MAX_GZIP_BODY) return 0;
            uint8_t *p = (uint8_t *)ps_realloc(data, newCap);
            if (!p) return 0;
            data = p;
            cap = newCap;
        }
        memcpy(data + len, buf, n);
        len += n;
        return n;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }

    uint8_t *data = nullptr;
    size_t len = 0;
    size_t cap = 0;
};

// Inflates a gzip body on the fly for ArduinoJson (a reader needs read() and
// readBytes()), so the ~1 MB of JSON is never held in memory at once. The 32 KB
// inflate window doubles as the output buffer.
class GzipReader {
public:
    ~GzipReader() {
        free(dict);
        free(inflator);
    }

    bool begin(const uint8_t *gz, size_t n) {
        // RFC 1952 header: magic, deflate, flags, then optional fields
        if (n < 18 || gz[0] != 0x1F || gz[1] != 0x8B || gz[2] != 8) return false;
        uint8_t flags = gz[3];
        size_t pos = 10;
        if (flags & 0x04) pos += 2 + (gz[pos] | (gz[pos + 1] << 8));  // FEXTRA
        if (flags & 0x08) while (pos < n && gz[pos++]) {}             // FNAME
        if (flags & 0x10) while (pos < n && gz[pos++]) {}             // FCOMMENT
        if (flags & 0x02) pos += 2;                                   // FHCRC
        if (pos >= n) return false;
        in = gz + pos;
        inLeft = n - pos;

        dict = (uint8_t *)ps_malloc(TINFL_LZ_DICT_SIZE);
        inflator = (tinfl_decompressor *)ps_malloc(sizeof(tinfl_decompressor));
        if (!dict || !inflator) return false;
        tinfl_init(inflator);
        return true;
    }

    int read() {
        if (pushedBack >= 0) {
            int c = pushedBack;
            pushedBack = -1;
            return c;
        }
        if (outPos == outEnd && !refill()) return -1;
        return dict[outPos++];
    }

    size_t readBytes(char *buf, size_t n) {
        size_t got = 0;
        while (got < n) {
            int c = read();
            if (c < 0) break;
            buf[got++] = (char)c;
        }
        return got;
    }

    void unread(int c) { pushedBack = c; }

    int readNonSpace() {
        int c;
        do c = read(); while (c == ' ' || c == '\n' || c == '\r' || c == '\t');
        return c;
    }

    // Consume input up to and including `pattern`
    bool skipPast(const char *pattern) {
        size_t matched = 0, len = strlen(pattern);
        int c;
        while ((c = read()) >= 0) {
            if (c == pattern[matched]) {
                if (++matched == len) return true;
            } else {
                matched = c == pattern[0] ? 1 : 0;
            }
        }
        return false;
    }

    bool failed() const { return error; }

private:
    bool refill() {
        while (!finished) {
            dictOfs = (dictOfs + produced) & (TINFL_LZ_DICT_SIZE - 1);
            size_t inSize = inLeft, outSize = TINFL_LZ_DICT_SIZE - dictOfs;
            tinfl_status status = tinfl_decompress(inflator, in, &inSize, dict, dict + dictOfs, &outSize, 0);
            in += inSize;
            inLeft -= inSize;
            produced = outSize;
            if (status != TINFL_STATUS_HAS_MORE_OUTPUT) {
                finished = true;
                error = status != TINFL_STATUS_DONE;
            }
            outPos = dictOfs;
            outEnd = dictOfs + outSize;
            if (outSize) return true;
        }
        return false;
    }

    const uint8_t *in = nullptr;
    size_t inLeft = 0;
    uint8_t *dict = nullptr;
    tinfl_decompressor *inflator = nullptr;
    size_t dictOfs = 0, produced = 0, outPos = 0, outEnd = 0;
    int pushedBack = -1;
    bool finished = false, error = false;
};

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

// "STIF:Line::C01727:" -> "line:IDFM:C01727", "STIF:StopPoint:Q:39570:" ->
// "stop_point:IDFM:39570", "STIF:StopArea:SP:46366:" -> "stop_area:IDFM:46366"
static bool stifToNavitia(const char *stif, char *out, size_t outSize)
{
    static const struct { const char *prefix; const char *kind; } MAP[] = {
        {"STIF:Line::", "line"},
        {"STIF:StopPoint:Q:", "stop_point"},
        {"STIF:StopArea:SP:", "stop_area"},
    };
    for (const auto &m : MAP) {
        size_t n = strlen(m.prefix);
        if (strncmp(stif, m.prefix, n) != 0) continue;
        const char *code = stif + n;
        size_t codeLen = strcspn(code, ":");
        if (codeLen == 0) return false;
        snprintf(out, outSize, "%s:IDFM:%.*s", m.kind, (int)codeLen, code);
        return true;
    }
    return false;
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

// GET a Navitia URL into `body`, gzip-compressed when the server agrees
static bool navitiaGet(const char *url, PsramBody &body, bool &gzipped)
{
    ensureClient();
    if (!http.begin(client, url)) {
        USE_SERIAL.println("[INCIDENTS] begin failed");
        return false;
    }
    http.addHeader("apiKey", API_TOKEN);
    http.setAcceptEncoding("gzip");
    const char *wanted[] = {"Content-Encoding"};
    http.collectHeaders(wanted, 1);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        USE_SERIAL.printf("[INCIDENTS] GET failed: %d %s\n", httpCode, httpCode < 0 ? http.errorToString(httpCode).c_str() : "");
        http.end();
        return false;
    }
    gzipped = http.header("Content-Encoding").equalsIgnoreCase("gzip");
    int written = http.writeToStream(&body);
    http.end();
    if (written < 0) {
        USE_SERIAL.printf("[INCIDENTS] body failed: %d\n", written);
        return false;
    }
    return true;
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
    PsramBody body;
    bool gzipped = false;
    if (!navitiaGet(url, body, gzipped)) return nullptr;

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

// Where the incident lookup stands: our stop, and where its trains go
struct StopContext {
    StopKey stop;
    std::vector<StopKey> destinations;
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

// Is this incident about our part of the line, or about our stop?
static bool concernsStop(JsonArrayConst objects, const char *stopId, const StopContext &ctx)
{
    for (JsonObjectConst o : objects) {
        const char *type = o["pt_object"]["embedded_type"] | "";
        const char *id = o["pt_object"]["id"] | "";
        if (strcmp(type, "line") == 0 || strcmp(type, "route") == 0 || strcmp(type, "network") == 0) {
            JsonObjectConst section = o["impacted_rail_section"];
            if (section.isNull()) section = o["impacted_section"];
            if (section.isNull() || sectionCoversStop(section, ctx)) return true;
        } else if (stopId[0] && strcmp(id, stopId) == 0) {
            return true;
        }
    }
    return false;
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

bool fetchIncidents(const char *lineRef, const char *monitoringRef, const std::vector<String> &destinations,
                    std::vector<incidentType> &out)
{
    char lineId[32], stopId[40] = "";
    if (!stifToNavitia(lineRef, lineId, sizeof(lineId))) return false;
    stifToNavitia(monitoringRef, stopId, sizeof(stopId));

    char url[192];
    snprintf(url, sizeof(url), "%s/lines/%s/line_reports?filter_status%%5B%%5D=active&disable_geojson=true",
             NAVITIA_BASE, lineId);

    // Line reports are large (~1 MB for an RER line, mostly lift outages) but compress ~15x
    PsramBody body;
    bool gzipped = false;
    if (!navitiaGet(url, body, gzipped)) return false;
    if (!gzipped) {
        USE_SERIAL.println("[INCIDENTS] line report not gzipped");
        return false;
    }

    GzipReader reader;
    if (!reader.begin(body.data, body.len) || !reader.skipPast("\"disruptions\":") || reader.readNonSpace() != '[') {
        USE_SERIAL.println("[INCIDENTS] no disruptions array");
        return false;
    }

    StopContext ctx{StopKey(monitoringRef), {}};
    for (const String &d : destinations) {
        StopKey key(d.c_str());
        if (key.valid()) ctx.destinations.push_back(key);
    }

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
        if (level == 0 || !concernsStop(doc["impacted_objects"], stopId, ctx)) continue;

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
        String folded = stripLinePrefix(asciiFold(text));
        folded.trim();
        if (folded.length()) {
            addIncident(found, folded, level == 2, currentPeriod(doc["application_periods"], time(nullptr)));
        }
    }
    if (reader.failed()) {
        USE_SERIAL.println("[INCIDENTS] inflate error");
        return false;
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

void transportEnd()
{
    http.end();
    client.stop();
}
