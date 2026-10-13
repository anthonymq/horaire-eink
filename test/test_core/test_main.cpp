// Host tests for lib/transit_core: pio test -e native
#include <unity.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "board_layout.h"
#include "departures.h"
#include "forecast.h"
#include "power_policy.h"
#include "text_utils.h"

// Saved PRIM responses in docs/, found from this file's path (relative to the
// project directory, where the tests run, or absolute)
static std::string readDoc(const char *name)
{
    std::string file = __FILE__;
    size_t testDir = file.rfind("test/test_core/");
    std::string path = file.substr(0, testDir) + "docs/" + name;
    std::ifstream in(path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void setUp() {}
void tearDown() {}

// ------------------------------------------------------------
// text_utils
// ------------------------------------------------------------
static void test_parse_iso_utc()
{
    TEST_ASSERT_EQUAL_INT64(1768155120, parseIsoUtc("2026-01-11T18:12:00.000Z"));
    TEST_ASSERT_EQUAL_INT64(0, parseIsoUtc("garbage"));
    TEST_ASSERT_EQUAL_INT64(0, parseIsoUtc(nullptr));
    TEST_ASSERT_EQUAL_INT64(1768155120, utcEpoch(2026, 1, 11, 18, 12, 0));
}

static void test_ascii_fold()
{
    TEST_ASSERT_EQUAL_STRING("Gare d'Ivry - Eole", asciiFoldStd("Gare d\xE2\x80\x99Ivry \xE2\x80\x93 \xC3\x89ole").c_str());
    TEST_ASSERT_EQUAL_STRING("coeur...", asciiFoldStd("c\xC5\x93ur\xE2\x80\xA6").c_str());
    TEST_ASSERT_EQUAL_STRING("plain", asciiFoldStd("plain").c_str());
}

static void test_stif_to_navitia()
{
    char out[48];
    TEST_ASSERT_TRUE(stifToNavitia("STIF:Line::C01727:", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("line:IDFM:C01727", out);
    TEST_ASSERT_TRUE(stifToNavitia("STIF:StopPoint:Q:39570:", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("stop_point:IDFM:39570", out);
    TEST_ASSERT_TRUE(stifToNavitia("STIF:StopArea:SP:46366:", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("stop_area:IDFM:46366", out);
    TEST_ASSERT_FALSE(stifToNavitia("STIF:Line:::", out, sizeof(out)));
    TEST_ASSERT_FALSE(stifToNavitia("RATP:whatever", out, sizeof(out)));
}

// ------------------------------------------------------------
// departures
// ------------------------------------------------------------
static void test_parse_rer_sample()
{
    std::string json = readDoc("prim.json");
    TEST_ASSERT_TRUE_MESSAGE(json.size() > 0, "docs/prim.json not found");
    std::vector<departureType> deps;
    StopSeen seen;
    TEST_ASSERT_TRUE(parseDepartures(json.data(), json.size(), "RER", "1B", deps, &seen));
    TEST_ASSERT_EQUAL(8, deps.size());  // Capped
    TEST_ASSERT_EQUAL_STRING("ROMI", deps[0].name.c_str());
    TEST_ASSERT_EQUAL_INT64(1732019702, deps[0].expected);  // Arrival time for trains
    TEST_ASSERT_EQUAL_INT64(1732019590, deps[0].aimed);
    TEST_ASSERT_FALSE(deps[0].cancelled);
    TEST_ASSERT_EQUAL(2, deps[0].delayMinutes());  // 112 s late
    TEST_ASSERT_EQUAL(0, deps[1].delayMinutes());  // 10 s late = on time
    TEST_ASSERT_EQUAL(1, seen.lines.size());
    TEST_ASSERT_EQUAL_STRING("STIF:Line::C01727:", seen.lines[0].c_str());
    TEST_ASSERT_TRUE(seen.destinations.size() > 0);
}

static void test_parse_bus_sample_line_filter()
{
    std::string json = readDoc("exemple.json");
    TEST_ASSERT_TRUE_MESSAGE(json.size() > 0, "docs/exemple.json not found");
    std::vector<departureType> deps;
    TEST_ASSERT_TRUE(parseDepartures(json.data(), json.size(), "BUS", "STIF:Line::C01287:", deps, nullptr));
    TEST_ASSERT_EQUAL(4, deps.size());
    TEST_ASSERT_EQUAL_INT64(1742565535, deps[0].expected);  // Departure time for buses
    TEST_ASSERT_EQUAL_INT64(0, deps[0].aimed);
    TEST_ASSERT_EQUAL(0, deps[0].delayMinutes());
}

static const char *CANCELLED_JSON = R"({"Siri":{"ServiceDelivery":{
  "ResponseTimestamp":"2026-01-11T18:00:00.000Z",
  "StopMonitoringDelivery":[{"MonitoredStopVisit":[
    {"MonitoringRef":{"value":"STIF:StopPoint:Q:1:"},"MonitoredVehicleJourney":{
      "LineRef":{"value":"STIF:Line::C01727:"},"JourneyNote":[{"value":"ROMI"}],
      "DestinationRef":{"value":"STIF:StopArea:SP:9:"},
      "MonitoredCall":{"AimedArrivalTime":"2026-01-11T18:05:00.000Z","ArrivalStatus":"cancelled",
                       "DepartureStatus":"cancelled","ArrivalPlatformName":{"value":"1B"}}}},
    {"MonitoringRef":{"value":"STIF:StopPoint:Q:1:"},"MonitoredVehicleJourney":{
      "LineRef":{"value":"STIF:Line::C01727:"},"JourneyNote":[{"value":"MONA"}],
      "DestinationRef":{"value":"STIF:StopArea:SP:8:"},
      "MonitoredCall":{"AimedArrivalTime":"2026-01-11T18:10:00.000Z","ExpectedArrivalTime":"2026-01-11T18:16:20.000Z",
                       "ArrivalStatus":"delayed","ArrivalPlatformName":{"value":"1B"}}}},
    {"MonitoringRef":{"value":"STIF:StopPoint:Q:1:"},"MonitoredVehicleJourney":{
      "LineRef":{"value":"STIF:Line::C01727:"},"JourneyNote":[{"value":"GONE"}],
      "MonitoredCall":{"AimedArrivalTime":"2026-01-11T17:50:00.000Z","ArrivalStatus":"cancelled",
                       "ArrivalPlatformName":{"value":"1B"}}}}
  ]}]}}})";

static void test_cancelled_and_delayed()
{
    std::vector<departureType> deps;
    StopSeen seen;
    TEST_ASSERT_TRUE(parseDepartures(CANCELLED_JSON, strlen(CANCELLED_JSON), "RER", "1B", deps, &seen));
    TEST_ASSERT_EQUAL(2, deps.size());  // The cancelled train already past is dropped

    TEST_ASSERT_TRUE(deps[0].cancelled);
    TEST_ASSERT_EQUAL_STRING("ROMI", deps[0].name.c_str());
    TEST_ASSERT_EQUAL_INT64(parseIsoUtc("2026-01-11T18:05:00Z"), deps[0].expected);  // At its aimed time
    TEST_ASSERT_EQUAL(0, deps[0].delayMinutes());

    TEST_ASSERT_FALSE(deps[1].cancelled);
    TEST_ASSERT_EQUAL(6, deps[1].delayMinutes());  // 6 min 20 s

    // Cancelled trains don't tell where trains go (incident direction filter)
    TEST_ASSERT_EQUAL(1, seen.destinations.size());
    TEST_ASSERT_EQUAL_STRING("STIF:StopArea:SP:8:", seen.destinations[0].c_str());
}

static void test_parse_error()
{
    std::vector<departureType> deps(1);
    TEST_ASSERT_FALSE(parseDepartures("{not json", 9, "BUS", "", deps, nullptr));
    TEST_ASSERT_EQUAL(1, deps.size());  // Untouched on failure
}

// ------------------------------------------------------------
// layout
// ------------------------------------------------------------
static void test_layout_default_matches_classic_screen()
{
    bool configured[] = {true, true, true, false};
    bool large[] = {true, false, false, false};
    ScreenLayout l = computeLayout(configured, large, 4);
    TEST_ASSERT_EQUAL(3, l.count);
    // RER board: full-height left column, 5 rows of 88
    TEST_ASSERT_EQUAL(0, l.boards[0].station);
    TEST_ASSERT_EQUAL(LEFT_X, l.boards[0].rect.x);
    TEST_ASSERT_EQUAL(BODY_BOTTOM - BODY_Y, l.boards[0].rect.h);
    TEST_ASSERT_EQUAL(5, l.boards[0].rows);
    TEST_ASSERT_EQUAL(88, l.boards[0].rowH);
    // Two bus boards of 3 rows of 46, weather 104 tall at y 424
    TEST_ASSERT_EQUAL(SIDE_X, l.boards[1].rect.x);
    TEST_ASSERT_EQUAL(170, l.boards[1].rect.h);
    TEST_ASSERT_EQUAL(3, l.boards[2].rows);
    TEST_ASSERT_EQUAL(46, l.boards[2].rowH);
    TEST_ASSERT_EQUAL(424, l.weather.y);
    TEST_ASSERT_EQUAL(104, l.weather.h);
}

static void test_layout_all_compact_goes_left()
{
    bool configured[] = {true, true, false, false};
    bool large[] = {false, false, false, false};
    ScreenLayout l = computeLayout(configured, large, 4);
    TEST_ASSERT_EQUAL(2, l.count);
    for (int i = 0; i < l.count; i++) TEST_ASSERT_EQUAL(LEFT_X, l.boards[i].rect.x);
    TEST_ASSERT_EQUAL(BODY_Y, l.weather.y);  // Whole right column for the weather
    TEST_ASSERT_EQUAL(BODY_BOTTOM - BODY_Y, l.weather.h);
}

static void test_layout_four_boards_fit()
{
    bool configured[] = {true, true, true, true};
    bool large[] = {false, true, false, false};  // Slot order kept within each column
    ScreenLayout l = computeLayout(configured, large, 4);
    TEST_ASSERT_EQUAL(4, l.count);
    TEST_ASSERT_EQUAL(1, l.boards[0].station);
    TEST_ASSERT_EQUAL(0, l.boards[1].station);
    TEST_ASSERT_EQUAL(2, l.boards[2].station);
    TEST_ASSERT_EQUAL(3, l.boards[3].station);
    for (int i = 1; i < 4; i++) {
        TEST_ASSERT_EQUAL(SIDE_X, l.boards[i].rect.x);
        TEST_ASSERT_TRUE(l.boards[i].rows >= 2);
        TEST_ASSERT_TRUE(l.boards[i].rowH >= 36);
    }
    TEST_ASSERT_TRUE(l.weather.h >= WEATHER_MIN_H);
    TEST_ASSERT_TRUE(l.weather.y + l.weather.h <= BODY_BOTTOM);
}

static void test_layout_single_compact_gives_tall_weather()
{
    bool configured[] = {true, true, false, false};
    bool large[] = {true, false, false, false};
    ScreenLayout l = computeLayout(configured, large, 4);
    TEST_ASSERT_EQUAL(4, l.boards[1].rows);
    TEST_ASSERT_TRUE(l.weather.h > 200);  // Room for a forecast row
}

static void test_layout_two_large_boards()
{
    bool configured[] = {true, true, false, false};
    bool large[] = {true, true, false, false};
    ScreenLayout l = computeLayout(configured, large, 4);
    TEST_ASSERT_EQUAL(2, l.count);
    TEST_ASSERT_EQUAL(3, l.boards[0].rows);
    TEST_ASSERT_EQUAL(l.boards[0].rect.y + l.boards[0].rect.h + CARD_GAP, l.boards[1].rect.y);
    TEST_ASSERT_TRUE(l.boards[1].rect.y + l.boards[1].rect.h <= BODY_BOTTOM);
}

static void test_layout_empty()
{
    bool configured[] = {false, false, false, false};
    bool large[] = {false, false, false, false};
    ScreenLayout l = computeLayout(configured, large, 4);
    TEST_ASSERT_EQUAL(0, l.count);
    TEST_ASSERT_EQUAL(BODY_BOTTOM - BODY_Y, l.weather.h);
}

// ------------------------------------------------------------
// power policy
// ------------------------------------------------------------
static void test_battery_state()
{
    TEST_ASSERT_EQUAL(BATTERY_OK, batteryState(80, false));
    TEST_ASSERT_EQUAL(BATTERY_LOW, batteryState(15, false));
    TEST_ASSERT_EQUAL(BATTERY_CRITICAL, batteryState(5, false));
    TEST_ASSERT_EQUAL(BATTERY_OK, batteryState(3, true));   // On USB
    TEST_ASSERT_EQUAL(BATTERY_OK, batteryState(-1, false)); // Unknown
}

static void test_fetch_interval()
{
    TEST_ASSERT_EQUAL(FETCH_INTERVAL_S, fetchIntervalS(5 * 60, BATTERY_OK));
    TEST_ASSERT_EQUAL(FETCH_INTERVAL_S, fetchIntervalS(NEAR_DEPARTURE_S, BATTERY_OK));
    TEST_ASSERT_EQUAL(FETCH_IDLE_INTERVAL_S, fetchIntervalS(NEAR_DEPARTURE_S + 1, BATTERY_OK));
    TEST_ASSERT_EQUAL(FETCH_IDLE_INTERVAL_S, fetchIntervalS(-1, BATTERY_OK));
    TEST_ASSERT_EQUAL(FETCH_INTERVAL_S * 2, fetchIntervalS(60, BATTERY_LOW));
    TEST_ASSERT_EQUAL(INCIDENT_INTERVAL_S * 3, incidentIntervalS(BATTERY_LOW));
}

static void test_fetch_error_text()
{
    char buf[48];
    describeFetchError({FETCH_OK, 0}, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("", buf);
    describeFetchError({FETCH_WIFI, 0}, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("WiFi introuvable", buf);
    describeFetchError({FETCH_HTTP, 401}, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("PRIM : cle API refusee", buf);
    describeFetchError({FETCH_HTTP, 429}, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("PRIM : quota atteint", buf);
    describeFetchError({FETCH_HTTP, 503}, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("PRIM en panne (503)", buf);
    describeFetchError({FETCH_HTTP, -11}, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("PRIM injoignable", buf);
    describeFetchError({FETCH_PARSE, 0}, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("PRIM : reponse illisible", buf);
}

// ------------------------------------------------------------
// forecast
// ------------------------------------------------------------
static const char *FORECAST_JSON = R"({"cod":"200","list":[
  {"dt":1768154400,"main":{"temp":7.4},"weather":[{"icon":"04d"}],"pop":0.1},
  {"dt":1768165200,"main":{"temp":5.6},"weather":[{"icon":"10n"}],"pop":0.72,"rain":{"3h":1.2}},
  {"dt":1768176000,"main":{"temp":4.1},"weather":[{"icon":"13n"}],"pop":0,"snow":{"3h":0.4}}
]})";

static void test_parse_forecast()
{
    ForecastSlot slots[FORECAST_SLOTS];
    int n = parseForecast(FORECAST_JSON, strlen(FORECAST_JSON), slots, FORECAST_SLOTS);
    TEST_ASSERT_EQUAL(3, n);
    TEST_ASSERT_EQUAL_INT64(1768154400, slots[0].time);
    TEST_ASSERT_EQUAL_STRING("cloudy", slots[0].icon);
    TEST_ASSERT_EQUAL(10, slots[0].pop);
    TEST_ASSERT_FALSE(isRainy(slots[0]));
    TEST_ASSERT_EQUAL_STRING("rain", slots[1].icon);
    TEST_ASSERT_EQUAL(72, slots[1].pop);
    TEST_ASSERT_TRUE(isRainy(slots[1]));
    TEST_ASSERT_TRUE(isRainy(slots[2]));  // Snow volume
    TEST_ASSERT_EQUAL(-1, parseForecast("[", 1, slots, FORECAST_SLOTS));
}

static void test_rain_summary()
{
    setenv("TZ", "UTC0", 1);
    tzset();
    ForecastSlot slots[FORECAST_SLOTS];
    int n = parseForecast(FORECAST_JSON, strlen(FORECAST_JSON), slots, FORECAST_SLOTS);
    char buf[48];
    rainSummary(slots, n, 12, false, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Pluie vers 21h (72%)", buf);  // 1768165200 = 21:00 UTC

    rainSummary(slots, 1, 12, false, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Pas de pluie jusqu'a 21h", buf);  // 18:00 step covers until 21:00

    rainSummary(slots, n, 3, false, buf, sizeof(buf));  // Rain beyond the window
    TEST_ASSERT_EQUAL_STRING("Pas de pluie jusqu'a 21h", buf);

    rainSummary(slots, n, 12, true, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Pluie vers 21h", buf);
    rainSummary(slots, 1, 12, true, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Sec jusqu'a 21h", buf);

    rainSummary(slots, 0, 12, false, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("", buf);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_iso_utc);
    RUN_TEST(test_ascii_fold);
    RUN_TEST(test_stif_to_navitia);
    RUN_TEST(test_parse_rer_sample);
    RUN_TEST(test_parse_bus_sample_line_filter);
    RUN_TEST(test_cancelled_and_delayed);
    RUN_TEST(test_parse_error);
    RUN_TEST(test_layout_default_matches_classic_screen);
    RUN_TEST(test_layout_all_compact_goes_left);
    RUN_TEST(test_layout_four_boards_fit);
    RUN_TEST(test_layout_single_compact_gives_tall_weather);
    RUN_TEST(test_layout_two_large_boards);
    RUN_TEST(test_layout_empty);
    RUN_TEST(test_battery_state);
    RUN_TEST(test_fetch_interval);
    RUN_TEST(test_fetch_error_text);
    RUN_TEST(test_parse_forecast);
    RUN_TEST(test_rain_summary);
    return UNITY_END();
}
