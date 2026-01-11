# StopPoint Configuration Specification

## Overview
Allow users to configure which transit stops to display without modifying source code.

## Implementation Status: COMPLETE (2026-01-11)

**Storage**: ESP32 Preferences library (NVS)  
**Interface**: WiFiManager custom parameters  
**Trigger**: Long press (5s) to enter config mode  
**Files**: `station_config.h`, `station_config.cpp`

## Current State
- ✅ Station configs stored in NVS (FR-SC1)
- ✅ Web interface for configuration (FR-SC2)
- Station presets not yet implemented (FR-SC3)
- API validation not yet implemented (FR-SC4)

## Target State
- Web interface for station configuration
- Station presets for common locations
- Custom StopPoint ID entry
- Configuration stored in NVS

## Functional Requirements

### FR-SC1: Station Configuration Storage
Store in NVS:
- Up to 3 station configurations
- Each station includes:
  - Display name
  - MonitoringRef (StopPoint/StopArea ID)
  - Type (RER, BUS, METRO, TRAM)
  - Filter (platform or operator)
  - Display settings (show names, max entries)

### FR-SC2: Web Configuration Interface
Extends WiFi Manager web interface:
- List current configured stations
- Add/edit/remove stations
- Station type selector
- MonitoringRef input field
- Filter input field

### FR-SC3: Station Presets
Pre-defined station configurations:
- Common RER stations in Ivry-sur-Seine area
- Bus stops near known locations
- User can select preset or enter custom

### FR-SC4: Validation
- Validate MonitoringRef format
- Test API call before saving
- Show preview of departures

## Data Structure

```cpp
struct StationConfig {
    String name;           // Display name
    String monitoringRef;  // STIF:StopPoint:Q:XXXXX: format
    String type;           // RER, BUS, METRO, TRAM
    String filter;         // Platform or operator filter
    bool showNames;        // Show destination names
    int maxEntries;        // Max departures to show (1-6)
};
```

## StopPoint ID Format

| Type | Format Example |
|------|----------------|
| StopArea | STIF:StopArea:SP:46366: |
| StopPoint | STIF:StopPoint:Q:39570: |

## Finding StopPoint IDs
- PRIM API dataset: https://prim.iledefrance-mobilites.fr/fr/jeux-de-donnees/arrets-lignes
- Download CSV and search for station name
- Use stop_id column value

## Acceptance Criteria

- [x] Station configs stored in NVS
- [x] Web interface lists configured stations
- [x] User can add new station with custom MonitoringRef
- [x] User can edit existing station configuration
- [ ] User can remove a station (clear NVS to reset)
- [ ] Station presets available for quick setup
- [ ] API validation before saving
- [x] Changes take effect on next restart

## Priority
P5 - Enhancement (COMPLETE)

## Dependencies
- WiFi Manager (FR-WM2 web interface)
- Preferences library for NVS storage
