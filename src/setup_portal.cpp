#include "setup_portal.h"
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiManager.h>  // tzapu/WiFiManager - captive portal for WiFi configuration
#include <ESPmDNS.h>
#include <ArduinoJson.h>

#include "display.h"
#include "station_config.h"
#include "stop_finder.h"
#include "weather_api.h"
#include "stops_page.h"

#define USE_SERIAL Serial

#define AP_NAME                 "ProchainDepart-Setup"
#define AP_PORTAL_TIMEOUT_S     300     // Captive portal closes after this long without a request
#define LAN_PORTAL_IDLE_S       600     // Home-network portal closes after this long without a request
#define MDNS_HOST               "prochain-depart"

static WiFiManager wm;
static uint32_t lastActivityMs = 0;
static bool exitRequested = false;

// ============================================================
// HTTP HANDLERS (/stops page + JSON API)
// ============================================================
static void sendJson(int code, const String& body) {
    wm.server->sendHeader("Cache-Control", "no-store");
    wm.server->send(code, "application/json", body);
}

static void sendError(int code, const char* message) {
    JsonDocument doc;
    doc["error"] = message;
    String body;
    serializeJson(doc, body);
    sendJson(code, body);
}

static void handleStopsPage() {
    lastActivityMs = millis();
    wm.server->send(200, "text/html", STOPS_PAGE);
}

static void handleGetStations() {
    lastActivityMs = millis();
    static const char* const defaultTypes[MAX_STATIONS] = { "RER", "BUS", "BUS" };
    RuntimeStationConfig* stations = getStations();
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < MAX_STATIONS; i++) {
        const RuntimeStationConfig& s = stations[i];
        JsonObject o = arr.add<JsonObject>();
        o["name"] = s.isConfigured ? s.displayName : "";
        o["ref"] = s.isConfigured ? s.monitoringRef : "";
        o["type"] = s.isConfigured ? s.type : defaultTypes[i];
        o["filter"] = s.isConfigured ? s.filter : "";
    }
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

static void handleSaveStations() {
    lastActivityMs = millis();
    JsonDocument doc;
    if (deserializeJson(doc, wm.server->arg("plain")) || !doc.is<JsonArray>()) {
        sendError(400, "JSON invalide");
        return;
    }
    // Stations keep their slot (slot 0 = top board). The count is the highest
    // configured slot + 1 so that skipped slots don't hide later ones.
    int highestSlot = -1;
    for (int i = 0; i < MAX_STATIONS; i++) {
        JsonObject s = doc[i];
        const char* ref = s["ref"] | "";
        if (ref[0] == '\0') {
            clearStationSlot(i);
            continue;
        }
        RuntimeStationConfig cfg = {};
        const char* name = s["name"] | "";
        if (name[0] == '\0') name = ref;
        strlcpy(cfg.name, name, sizeof(cfg.name));
        strlcpy(cfg.displayName, name, sizeof(cfg.displayName));
        strlcpy(cfg.monitoringRef, ref, sizeof(cfg.monitoringRef));
        strlcpy(cfg.type, s["type"] | "BUS", sizeof(cfg.type));
        strlcpy(cfg.filter, s["filter"] | "", sizeof(cfg.filter));
        cfg.showNames = (strcmp(cfg.type, "RER") == 0);
        cfg.maxEntries = 6;
        cfg.isConfigured = true;
        saveStationConfig(i, cfg);
        highestSlot = i;
        USE_SERIAL.printf("  Station %d: %s (%s, %s)\n", i + 1, cfg.displayName, cfg.monitoringRef, cfg.filter);
    }
    saveStationCount(highestSlot + 1);
    sendJson(200, "{\"ok\":true}");
}

static void handleSearch() {
    lastActivityMs = millis();
    if (WiFi.status() != WL_CONNECTED) {
        sendError(503, "hors ligne");
        return;
    }
    String q = wm.server->arg("q");
    q.trim();
    if (q.length() < 2 || q.length() > 64) {
        sendError(400, "requete invalide");
        return;
    }
    String body;
    if (!searchStopAreas(q.c_str(), body)) {
        sendError(502, "PRIM");
        return;
    }
    sendJson(200, body);
}

static void handlePreview() {
    lastActivityMs = millis();
    if (WiFi.status() != WL_CONNECTED) {
        sendError(503, "hors ligne");
        return;
    }
    String ref = wm.server->arg("ref");
    if (!ref.startsWith("STIF:") || ref.length() >= STATION_REF_LEN) {
        sendError(400, "ref invalide");
        return;
    }
    String body;
    if (!previewStop(ref.c_str(), body)) {
        sendError(502, "PRIM");
        return;
    }
    sendJson(200, body);
}

static void handleGetWeather() {
    lastActivityMs = millis();
    const WeatherLocation& loc = getWeatherLocation();
    JsonDocument doc;
    doc["name"] = loc.name;
    if (loc.hasCoords) {
        doc["lat"] = loc.lat;
        doc["lon"] = loc.lon;
    }
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

static void handleSaveWeather() {
    lastActivityMs = millis();
    JsonDocument doc;
    if (deserializeJson(doc, wm.server->arg("plain")) || !doc["name"].is<const char*>()) {
        sendError(400, "JSON invalide");
        return;
    }
    WeatherLocation loc = {};
    strlcpy(loc.name, doc["name"], sizeof(loc.name));
    loc.hasCoords = doc["lat"].is<float>() && doc["lon"].is<float>();
    loc.lat = doc["lat"] | 0.0f;
    loc.lon = doc["lon"] | 0.0f;
    if (loc.name[0] == '\0' || !saveWeatherLocation(loc)) {
        sendError(400, "lieu invalide");
        return;
    }
    sendJson(200, "{\"ok\":true}");
}

static void handleCities() {
    lastActivityMs = millis();
    if (WiFi.status() != WL_CONNECTED) {
        sendError(503, "hors ligne");
        return;
    }
    String q = wm.server->arg("q");
    q.trim();
    if (q.length() < 2 || q.length() > 64) {
        sendError(400, "requete invalide");
        return;
    }
    String body;
    if (!searchCities(q.c_str(), body)) {
        sendError(502, "OpenWeatherMap");
        return;
    }
    sendJson(200, body);
}

static void handleExit() {
    sendJson(200, "{\"ok\":true}");
    exitRequested = true;
}

static void setupWifiManager() {
    static bool done = false;
    if (done) return;
    done = true;

    wm.setTitle("Prochain Départ");
    static const char* menu[] = { "custom", "wifi", "info", "exit" };
    wm.setMenu(menu, sizeof(menu) / sizeof(menu[0]));
    wm.setCustomMenuHTML("<form action='/stops' method='get'><button>Choisir les arrêts</button></form><br/>\n");
    // Registered before WiFiManager's own routes, so these take precedence
    wm.setWebServerCallback([]() {
        wm.server->on("/stops", HTTP_GET, handleStopsPage);
        wm.server->on("/api/stations", HTTP_GET, handleGetStations);
        wm.server->on("/api/stations", HTTP_POST, handleSaveStations);
        wm.server->on("/api/search", HTTP_GET, handleSearch);
        wm.server->on("/api/preview", HTTP_GET, handlePreview);
        wm.server->on("/api/weather", HTTP_GET, handleGetWeather);
        wm.server->on("/api/weather", HTTP_POST, handleSaveWeather);
        wm.server->on("/api/cities", HTTP_GET, handleCities);
        wm.server->on("/api/exit", HTTP_POST, handleExit);
    });
}

// ============================================================
// PORTAL MODES
// ============================================================
static void waitTouchRelease() {
    do {
        M5.update();
        delay(20);
    } while (M5.Touch.getCount() > 0);
}

// Access point + captive portal. Returns true once a network is saved and joined.
static bool runApPortal(WiFiMulti& wifiMulti) {
    USE_SERIAL.println("Starting access point portal");
    static const SetupQr qrs[] = {
        { "WIFI:T:nopass;S:" AP_NAME ";;", "1. Rejoindre le WiFi" },
        { "http://192.168.4.1/", "2. Ouvrir la page" },
    };
    static const char* const lines[] = {
        "*Configuration WiFi",
        "Scannez le QR 1 avec l'appareil",
        "photo pour rejoindre le reseau",
        AP_NAME ".",
        "",
        "La page s'ouvre toute seule.",
        "Sinon, scannez le QR 2.",
        "",
        "*Ensuite",
        "Choisissez votre WiFi : un",
        "nouveau QR code apparaitra pour",
        "choisir vos arrets.",
        "",
        "Fermeture apres 5 min d'inactivite",
    };
    drawSetupScreen("Configuration", qrs, 2, lines, sizeof(lines) / sizeof(lines[0]));

    wm.setConfigPortalTimeout(AP_PORTAL_TIMEOUT_S);
    wm.setBreakAfterConfig(true);  // Exit portal after config saved
    if (!wm.startConfigPortal(AP_NAME)) {
        USE_SERIAL.println("Access point portal closed without a new network");
        return false;
    }
    USE_SERIAL.printf("Joined %s\n", WiFi.SSID().c_str());
    wifiMulti.addAP(WiFi.SSID().c_str(), WiFi.psk().c_str());
    return true;
}

// Web portal on the home network, so the phone keeps Internet and the device can
// query PRIM for the stop search.
static void runLanPortal(WiFiMulti& wifiMulti) {
    String url = "http://" + WiFi.localIP().toString() + "/stops";
    USE_SERIAL.printf("Starting web portal at %s\n", url.c_str());
    bool mdns = MDNS.begin(MDNS_HOST);
    if (mdns) MDNS.addService("http", "tcp", 80);

    String ssidLine = "au WiFi \"" + WiFi.SSID() + "\", puis";
    const SetupQr qr = { url.c_str(), url.c_str() };
    const char* const lines[] = {
        "*Choix des arrets",
        "Scannez le QR code avec un telephone",
        ssidLine.c_str(),
        "choisissez arrets et meteo.",
        "",
        mdns ? "Ou ouvrez dans le navigateur :" : "",
        mdns ? "http://" MDNS_HOST ".local/stops" : "",
        "",
        "*Touchez l'ecran pour quitter",
        "Fermeture apres 10 min d'inactivite",
    };
    drawSetupScreen("Configuration", &qr, 1, lines, sizeof(lines) / sizeof(lines[0]));

    exitRequested = false;
    lastActivityMs = millis();
    wm.startWebPortal();
    waitTouchRelease();

    while (true) {
        if (wm.process()) {
            // A new network was saved from /wifi
            wifiMulti.addAP(WiFi.SSID().c_str(), WiFi.psk().c_str());
        }
        if (exitRequested || !wm.getWebPortalActive()) break;
        if (millis() - lastActivityMs > LAN_PORTAL_IDLE_S * 1000UL) {
            USE_SERIAL.println("Web portal idle timeout");
            break;
        }
        M5.update();
        if (M5.Touch.getCount() > 0) break;
        delay(5);
    }

    delay(200);  // Let the last response reach the phone
    wm.stopWebPortal();
    if (mdns) MDNS.end();
    waitTouchRelease();  // Don't let the exit tap count as a refresh tap
}

void runSetupPortal(WiFiMulti& wifiMulti, bool online) {
    setupWifiManager();
    if (!online && !runApPortal(wifiMulti)) return;
    if (WiFi.status() != WL_CONNECTED) return;
    runLanPortal(wifiMulti);
    USE_SERIAL.println("Setup portal closed");
}
