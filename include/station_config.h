#ifndef STATION_CONFIG_H
#define STATION_CONFIG_H

#include <Arduino.h>
#include <Preferences.h>

/**
 * StopPoint Configuration Module (FR-SC1, FR-SC2)
 * 
 * Provides NVS-based storage for station configurations, allowing users to
 * configure transit stops via WiFiManager portal instead of hardcoding in config.h.
 * 
 * Storage Schema (NVS namespace: "stations"):
 *   - station_count: int (0-3)
 *   - station_0_name, station_0_ref, station_0_type, etc.
 *   - station_1_name, station_1_ref, station_1_type, etc.
 *   - station_2_name, station_2_ref, station_2_type, etc.
 */

// Maximum number of configurable stations
#define MAX_STATIONS 3

// Maximum string lengths for station fields
#define STATION_NAME_LEN 32
#define STATION_REF_LEN 64
#define STATION_TYPE_LEN 8
#define STATION_FILTER_LEN 48
#define STATION_DISPLAY_LEN 24

/**
 * Runtime station configuration structure.
 * This is a copy-safe version using char arrays instead of const char*.
 */
struct RuntimeStationConfig {
    char name[STATION_NAME_LEN];           // Internal name
    char monitoringRef[STATION_REF_LEN];   // STIF:StopPoint:Q:XXXXX: format
    char type[STATION_TYPE_LEN];           // "RER", "BUS", "METRO", "TRAM"
    char filter[STATION_FILTER_LEN];       // Platform name or operator filter
    bool showNames;                         // Show destination names in UI
    int maxEntries;                        // Max departures to display (1-6)
    char displayName[STATION_DISPLAY_LEN]; // Name shown in UI banner
    bool isConfigured;                     // True if this slot has valid config
};

/**
 * Initialize the station configuration system.
 * Loads saved configs from NVS, or uses config.h defaults if none saved.
 * Must be called before using getStations() or getStationCount().
 */
void initStationConfig();

/**
 * Check if custom stations are configured in NVS.
 * @return true if NVS has saved station configs, false to use config.h fallback
 */
bool hasNVSStationConfig();

/**
 * Get the number of configured stations.
 * @return Number of stations (0-3)
 */
int getStationCount();

/**
 * Get a pointer to the station configurations array.
 * @return Pointer to RuntimeStationConfig array (MAX_STATIONS elements)
 */
RuntimeStationConfig* getStations();

/**
 * Build the full API endpoint URL from a MonitoringRef.
 * @param monitoringRef The stop reference (e.g., "STIF:StopPoint:Q:39570:")
 * @param outBuffer Buffer to write the full URL
 * @param bufferSize Size of the output buffer
 */
void buildEndpointUrl(const char* monitoringRef, char* outBuffer, size_t bufferSize);

/**
 * Save a station configuration to NVS.
 * @param index Station slot (0-2)
 * @param config Station configuration to save
 * @return true if save succeeded
 */
bool saveStationConfig(int index, const RuntimeStationConfig& config);

/**
 * Save the station count to NVS.
 * @param count Number of configured stations
 * @return true if save succeeded
 */
bool saveStationCount(int count);

/**
 * Clear all station configurations from NVS.
 * After clearing, config.h defaults will be used on next init.
 * @return true if clear succeeded
 */
bool clearStationConfigs();

/**
 * Load station configurations from NVS into the runtime array.
 * This is called automatically by initStationConfig().
 * @return Number of stations loaded
 */
int loadStationConfigs();

/**
 * Copy default station configurations from config.h to runtime array.
 * Used when NVS has no saved configs.
 */
void loadDefaultStations();

#endif // STATION_CONFIG_H
