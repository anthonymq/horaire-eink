#include "station_config.h"
#include <config.h>

#define USE_SERIAL Serial

// NVS namespace for station configurations
#define NVS_NAMESPACE "stations"

// Static storage for runtime station configurations
static RuntimeStationConfig runtimeStations[MAX_STATIONS];
static int runtimeStationCount = 0;
static bool initialized = false;

// PRIM API base URL
static const char* API_BASE_URL = "https://prim.iledefrance-mobilites.fr/marketplace/stop-monitoring?MonitoringRef=";

void buildEndpointUrl(const char* monitoringRef, char* outBuffer, size_t bufferSize) {
    snprintf(outBuffer, bufferSize, "%s%s", API_BASE_URL, monitoringRef);
}

void loadDefaultStations() {
    USE_SERIAL.println("Loading default stations from config.h");
    
    // Clear runtime array first
    memset(runtimeStations, 0, sizeof(runtimeStations));
    runtimeStationCount = 0;
    
    // Copy from config.h STATIONS array
    int count = (NUM_STATIONS < MAX_STATIONS) ? NUM_STATIONS : MAX_STATIONS;
    
    for (int i = 0; i < count; i++) {
        RuntimeStationConfig& dest = runtimeStations[i];
        const StationConfig& src = STATIONS[i];
        
        // Copy string fields safely
        strncpy(dest.name, src.name, STATION_NAME_LEN - 1);
        dest.name[STATION_NAME_LEN - 1] = '\0';
        
        // For monitoringRef, we need to extract from the full URL
        // The config.h stores full endpoints, we need just the MonitoringRef part
        // Format: https://prim.../stop-monitoring?MonitoringRef=STIF:StopPoint:...
        const char* refStart = strstr(src.endpoint, "MonitoringRef=");
        if (refStart) {
            refStart += strlen("MonitoringRef=");
            strncpy(dest.monitoringRef, refStart, STATION_REF_LEN - 1);
            dest.monitoringRef[STATION_REF_LEN - 1] = '\0';
        } else {
            // Fallback: assume endpoint IS the monitoring ref
            strncpy(dest.monitoringRef, src.endpoint, STATION_REF_LEN - 1);
            dest.monitoringRef[STATION_REF_LEN - 1] = '\0';
        }
        
        strncpy(dest.type, src.type, STATION_TYPE_LEN - 1);
        dest.type[STATION_TYPE_LEN - 1] = '\0';
        
        strncpy(dest.filter, src.filter, STATION_FILTER_LEN - 1);
        dest.filter[STATION_FILTER_LEN - 1] = '\0';
        
        strncpy(dest.displayName, src.displayName, STATION_DISPLAY_LEN - 1);
        dest.displayName[STATION_DISPLAY_LEN - 1] = '\0';
        
        dest.showNames = src.showNames;
        dest.maxEntries = src.maxEntries;
        dest.isConfigured = true;
        
        runtimeStationCount++;
        
        USE_SERIAL.printf("  Loaded default station %d: %s (%s)\n", i, dest.displayName, dest.type);
    }
}

bool hasNVSStationConfig() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {  // Read-only mode
        return false;
    }
    
    int count = prefs.getInt("count", -1);
    prefs.end();
    
    return count >= 0;
}

int loadStationConfigs() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {  // Read-only mode
        USE_SERIAL.println("Failed to open NVS for reading");
        return 0;
    }
    
    int count = prefs.getInt("count", 0);
    if (count <= 0 || count > MAX_STATIONS) {
        prefs.end();
        return 0;
    }
    
    USE_SERIAL.printf("Loading %d stations from NVS\n", count);
    
    // Clear runtime array
    memset(runtimeStations, 0, sizeof(runtimeStations));
    runtimeStationCount = 0;
    
    for (int i = 0; i < count; i++) {
        char keyName[24];
        char keyRef[24];
        char keyType[24];
        char keyFilter[24];
        char keyDisplay[24];
        char keyShowNames[24];
        char keyMaxEntries[24];
        
        snprintf(keyName, sizeof(keyName), "s%d_name", i);
        snprintf(keyRef, sizeof(keyRef), "s%d_ref", i);
        snprintf(keyType, sizeof(keyType), "s%d_type", i);
        snprintf(keyFilter, sizeof(keyFilter), "s%d_filter", i);
        snprintf(keyDisplay, sizeof(keyDisplay), "s%d_disp", i);
        snprintf(keyShowNames, sizeof(keyShowNames), "s%d_names", i);
        snprintf(keyMaxEntries, sizeof(keyMaxEntries), "s%d_max", i);
        
        RuntimeStationConfig& station = runtimeStations[i];
        
        // Read string values
        String name = prefs.getString(keyName, "");
        String ref = prefs.getString(keyRef, "");
        String type = prefs.getString(keyType, "BUS");
        String filter = prefs.getString(keyFilter, "");
        String display = prefs.getString(keyDisplay, "");
        
        if (name.isEmpty() || ref.isEmpty()) {
            USE_SERIAL.printf("  Station %d has invalid config, skipping\n", i);
            continue;
        }
        
        // Copy to runtime struct
        strncpy(station.name, name.c_str(), STATION_NAME_LEN - 1);
        station.name[STATION_NAME_LEN - 1] = '\0';
        
        strncpy(station.monitoringRef, ref.c_str(), STATION_REF_LEN - 1);
        station.monitoringRef[STATION_REF_LEN - 1] = '\0';
        
        strncpy(station.type, type.c_str(), STATION_TYPE_LEN - 1);
        station.type[STATION_TYPE_LEN - 1] = '\0';
        
        strncpy(station.filter, filter.c_str(), STATION_FILTER_LEN - 1);
        station.filter[STATION_FILTER_LEN - 1] = '\0';
        
        strncpy(station.displayName, display.isEmpty() ? name.c_str() : display.c_str(), STATION_DISPLAY_LEN - 1);
        station.displayName[STATION_DISPLAY_LEN - 1] = '\0';
        
        station.showNames = prefs.getBool(keyShowNames, type == "RER");
        station.maxEntries = prefs.getInt(keyMaxEntries, 6);
        station.isConfigured = true;
        
        runtimeStationCount++;
        
        USE_SERIAL.printf("  Loaded NVS station %d: %s (%s) ref=%s\n", 
                          i, station.displayName, station.type, station.monitoringRef);
    }
    
    prefs.end();
    return runtimeStationCount;
}

bool saveStationConfig(int index, const RuntimeStationConfig& config) {
    if (index < 0 || index >= MAX_STATIONS) {
        return false;
    }
    
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {  // Read-write mode
        USE_SERIAL.println("Failed to open NVS for writing");
        return false;
    }
    
    char keyName[24];
    char keyRef[24];
    char keyType[24];
    char keyFilter[24];
    char keyDisplay[24];
    char keyShowNames[24];
    char keyMaxEntries[24];
    
    snprintf(keyName, sizeof(keyName), "s%d_name", index);
    snprintf(keyRef, sizeof(keyRef), "s%d_ref", index);
    snprintf(keyType, sizeof(keyType), "s%d_type", index);
    snprintf(keyFilter, sizeof(keyFilter), "s%d_filter", index);
    snprintf(keyDisplay, sizeof(keyDisplay), "s%d_disp", index);
    snprintf(keyShowNames, sizeof(keyShowNames), "s%d_names", index);
    snprintf(keyMaxEntries, sizeof(keyMaxEntries), "s%d_max", index);
    
    prefs.putString(keyName, config.name);
    prefs.putString(keyRef, config.monitoringRef);
    prefs.putString(keyType, config.type);
    prefs.putString(keyFilter, config.filter);
    prefs.putString(keyDisplay, config.displayName);
    prefs.putBool(keyShowNames, config.showNames);
    prefs.putInt(keyMaxEntries, config.maxEntries);
    
    prefs.end();
    
    USE_SERIAL.printf("Saved station %d to NVS: %s\n", index, config.displayName);
    
    // Also update runtime array
    runtimeStations[index] = config;
    
    return true;
}

bool clearStationSlot(int index) {
    if (index < 0 || index >= MAX_STATIONS) {
        return false;
    }
    
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        USE_SERIAL.println("Failed to open NVS for clearing slot");
        return false;
    }
    
    static const char* const fields[] = { "name", "ref", "type", "filter", "disp", "names", "max" };
    for (const char* field : fields) {
        char key[24];
        snprintf(key, sizeof(key), "s%d_%s", index, field);
        prefs.remove(key);
    }
    prefs.end();
    
    memset(&runtimeStations[index], 0, sizeof(runtimeStations[index]));
    USE_SERIAL.printf("Cleared station %d from NVS\n", index);
    return true;
}

bool saveStationCount(int count) {
    if (count < 0 || count > MAX_STATIONS) {
        return false;
    }
    
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        USE_SERIAL.println("Failed to open NVS for writing count");
        return false;
    }
    
    prefs.putInt("count", count);
    prefs.end();
    
    runtimeStationCount = count;
    
    USE_SERIAL.printf("Saved station count to NVS: %d\n", count);
    return true;
}

bool clearStationConfigs() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        USE_SERIAL.println("Failed to open NVS for clearing");
        return false;
    }
    
    prefs.clear();
    prefs.end();
    
    // Reset runtime state
    memset(runtimeStations, 0, sizeof(runtimeStations));
    runtimeStationCount = 0;
    initialized = false;
    
    USE_SERIAL.println("Cleared all station configs from NVS");
    return true;
}

void initStationConfig() {
    if (initialized) {
        return;
    }
    
    USE_SERIAL.println("Initializing station configuration...");
    
    // Try to load from NVS first. A saved count of 0 means the user cleared
    // every slot, which is kept rather than replaced by the config.h defaults.
    if (hasNVSStationConfig()) {
        int loaded = loadStationConfigs();
        USE_SERIAL.printf("Loaded %d stations from NVS\n", loaded);
        initialized = true;
        return;
    }
    
    // Fallback to config.h defaults
    USE_SERIAL.println("No NVS config found, using config.h defaults");
    loadDefaultStations();
    initialized = true;
}

int getStationCount() {
    if (!initialized) {
        initStationConfig();
    }
    return runtimeStationCount;
}

RuntimeStationConfig* getStations() {
    if (!initialized) {
        initStationConfig();
    }
    return runtimeStations;
}
