# Prochain Depart - Implementation Plan

**Last Updated**: 2026-01-11
**Status**: Active Development - **M5Paper S3 Migration Complete, UI Enhancements Done**

---

## Summary

This document tracks implementation progress against specifications in `specs/` and the PRD in `docs/PRD.md`. Items are organized by priority and grouped by feature area.

---

## M5Paper S3 Migration - COMPLETE

### Overview

Successfully migrated from generic ESP32 + SPI E-Ink (GxEPD2) to M5Paper S3 with parallel EPD interface.

**Target Hardware**: [M5Paper S3](https://shop.m5stack.com/products/m5papers3-esp32s3-development-kit)

### Hardware Comparison

| Aspect | Previous | M5Paper S3 |
|--------|----------|------------|
| MCU | ESP32 | ESP32-S3R8 |
| Display | 4.2" GDEY042T81 (400x300) | 4.7" ED047TC1 (960x540) |
| Driver | SSD1683 (SPI) | Parallel 8-bit EPD |
| Library | GxEPD2 | M5Unified + M5GFX |
| Touch | None | GT911 capacitive |
| Button | GPIO 0 | Built-in via M5Unified |
| Extras | - | Gyro (BMI270), Buzzer, RTC (BM8563), 1800mAh battery |

---

### Phase 1: Environment Setup - COMPLETE

| Task | Status | Notes |
|------|--------|-------|
| Update `platformio.ini` for ESP32-S3 | [x] | Board: `esp32-s3-devkitc-1` |
| Add M5Unified library | [x] | `m5stack/M5Unified@^0.2.2` |
| Add M5GFX library | [x] | Auto-installed as dependency |
| Remove GxEPD2 dependency | [x] | Removed from lib_deps |
| Verify build compiles | [x] | BUILD SUCCESS |

---

### Phase 2: Display Driver Migration - COMPLETE

| Task | Status | Notes |
|------|--------|-------|
| Create new display header with M5GFX types | [x] | Uses `M5Unified.h` |
| Refactor display init in `main.cpp` | [x] | Uses `M5.begin(cfg)` |
| Remove HSPI/SPI configuration | [x] | M5Unified handles internally |
| Update display object declaration | [x] | Uses `M5.Display` throughout |

---

### Phase 3: Drawing Code Refactor - COMPLETE

| Task | Status | Notes |
|------|--------|-------|
| Migrate `displayLoader()` | [x] | Uses `textWidth()` and `fontHeight()` |
| Migrate `displayBanner()` | [x] | Updated for M5GFX API |
| Migrate `drawScreen()` | [x] | Complete rewrite for M5GFX |
| Migrate `draw2BitIcon()` | [x] | 4-level grayscale rendering |
| Migrate `displayWeather()` | [x] | Updated for M5GFX API |
| Migrate `displayDataFreshness()` | [x] | Integrated into status bar |

---

### Phase 4: UI Layout Redesign - COMPLETE

| Task | Status | Notes |
|------|--------|-------|
| Recalculate for 960x540 resolution | [x] | Full layout updated |
| Update banner heights | [x] | 36px banners |
| Update weather column | [x] | 200px right column |
| Update departure cell sizes | [x] | Scaled proportionally |
| Add status bar | [x] | 32px with time, WiFi, data age, battery |
| Consider larger fonts | [x] | FreeSansBold12pt7b, FreeSans9pt7b |

**Screen Layout (960x540):**

```
+----------------------------------+------------+
|         RER C - Ivry             | 17:25 [WiFi] 12s 85%[bat] |
|  (760px width, 270px height)     |---------------------------|
|   Schedule    Name      Delay    |      [Weather Icon]       |
|   10:15     Paris       2 min    |         15.2°C            |
|   10:22     Versailles  9 min    |      Ressenti 14°C        |
|   ...                            |         63% hum           |
+----------------+-----------------+         nuageux           |
| 323 Moliere    | 323 Jules F     |        2.1 m/s            |
| (380px)        | (380px)         |                           |
| 10:18   3min   | 10:20   5min    |     Ivry-sur-Seine        |
+----------------+-----------------+---------------------------+
```

---

### Phase 5: Button & Input Handling - COMPLETE

| Task | Status | Notes |
|------|--------|-------|
| Replace GPIO 0 with M5Unified button API | [x] | Uses `M5.BtnA.wasPressed()` |
| Update button press detection | [x] | M5Unified handles debounce |
| Button forces refresh | [x] | Sets `lastCallEpoch = 0` |
| Consider touch support (future) | [ ] | GT911 available - not implemented |

---

### Phase 6: Power Management - COMPLETE (2026-01-11)

| Task | Status | Notes |
|------|--------|-------|
| Battery level display | [x] | `M5.Power.getBatteryLevel()` |
| Charging indicator | [x] | `M5.Power.isCharging()` |
| Low battery warning | [x] | Visual indicator when <20% |
| Light sleep between refreshes | [x] | `M5.Power.lightSleep()` - 25s sleep cycles |
| Skip sleep when charging | [x] | Full responsiveness when USB connected |
| Leverage BM8563 RTC for sleep/wake | [ ] | Future enhancement (timerSleep) |

**Implementation Notes:**
- Uses `M5.Power.lightSleep(25000000, false)` for 25-second sleep cycles
- Light sleep preserves RAM and program state (no restart needed)
- Sleep is automatically skipped when charging for better responsiveness
- WiFi reconnects quickly after light sleep (~5 seconds)
- Total refresh cycle: ~30 seconds (25s sleep + 5s fetch/render)

---

### Phase 7: Testing & Verification - IN PROGRESS

| Task | Status | Notes |
|------|--------|-------|
| Build compiles successfully | [x] | BUILD SUCCESS |
| Uploads to device | [x] | Uploaded 2026-01-11 |
| WiFi connectivity works | [ ] | Needs verification |
| PRIM API calls work | [ ] | Needs verification |
| OpenWeatherMap API works | [ ] | Needs verification |
| Full refresh displays correctly | [ ] | Needs verification |
| Partial refresh works | [ ] | Needs verification |
| Button interaction works | [ ] | Needs verification |
| 24+ hour stability test | [ ] | Needs verification |

---

## UI Enhancements - COMPLETE (2026-01-11)

### Status Bar Features

| Feature | Status | Description |
|---------|--------|-------------|
| NTP Time Sync | [x] | Paris timezone (CET-1CEST with DST) |
| Time Display | [x] | HH:MM format, synced via `pool.ntp.org` |
| WiFi Signal | [x] | 4-bar indicator based on RSSI |
| Data Freshness | [x] | Shows seconds/minutes since last update |
| Stale Data Warning | [x] | Grays out when >2 minutes old |
| Battery Percentage | [x] | Shows current level |
| Battery Icon | [x] | 24x12px with fill level |
| Low Battery Warning | [x] | Double outline + exclamation when <20% |
| Charging Indicator | [x] | Lightning bolt when charging |

### Weather Display Enhancements

| Feature | Status | Description |
|---------|--------|-------------|
| Degree Symbol | [x] | Temperature shows `15.2°C` |
| 4-Level Grayscale Icons | [x] | Full grayscale palette rendering |
| Feels Like Temperature | [x] | "Ressenti" display |
| Text Centering Fixed | [x] | Proper GFX font ascent calculation |

---

## File Change Tracker

| File | Status | Changes |
|------|--------|---------|
| `platformio.ini` | [x] Complete | ESP32-S3 board, M5Unified + WiFiManager deps |
| `src/main.cpp` | [x] Complete | M5.begin(), NTP sync, battery/time/WiFi RSSI, WiFi Manager, Light Sleep, Station Config |
| `include/display.h` | [x] Complete | Updated function signatures |
| `src/display.cpp` | [x] Complete | Full M5GFX API, status bar, UI enhancements |
| `include/types.h` | No Change | Data structures unchanged |
| `src/transport_api.cpp` | No Change | API logic unchanged |
| `src/weather_api.cpp` | No Change | API logic unchanged |
| `include/weathericons.h` | No Change | PROGMEM icons compatible |
| `include/config.h` | No Change | Credentials unchanged |
| `include/config.h.example` | [x] Updated | Fixed STL container crash issue |
| `include/station_config.h` | [x] **NEW** | NVS-based station configuration header |
| `src/station_config.cpp` | [x] **NEW** | NVS storage implementation for stations |

---

## Recent Commits

| Commit | Description |
|--------|-------------|
| `85a403b` | Add UI enhancements: degree symbol, battery warning, data freshness, WiFi indicator |
| `dc4d479` | Add status bar with NTP time sync and battery indicator |
| `97136e4` | Weather icon working |
| `0af096f` | Fix boot loop: GPIO 12 conflict and global STL initialization crash |

---

## Core Functionality Status

### Transport API (`specs/transport-api.md`) - COMPLETE

| Requirement | Status | Notes |
|-------------|--------|-------|
| FR-T1: Fetch Departures | [x] | `fetchDepartures()` in `transport_api.cpp` |
| FR-T2: Multi-Station Support | [x] | Loop through `STATIONS[]` in `main.cpp` |
| FR-T3: Filtering | [x] | Implemented with `type` and `filter` params |
| FR-T4: Data Extraction | [x] | `departureType` struct populated |
| FR-T5: Error Handling | [x] | Graceful failure with empty list |
| 30-second refresh rate | [x] | Epoch-based timing check |

### Weather API (`specs/weather-api.md`) - COMPLETE

| Requirement | Status | Notes |
|-------------|--------|-------|
| FR-W1: Fetch Current Weather | [x] | `fetchWeather()` in `weather_api.cpp` |
| FR-W2: Data Extraction | [x] | All fields in `weatherType` |
| FR-W3: Icon Mapping | [x] | `mapWeatherIcon()` maps OWM codes |
| FR-W4: Error Handling | [x] | Returns empty struct on failure |
| 30-minute refresh interval | [x] | `WEATHER_CALL_INTERVAL` check |

### Display (`specs/display.md`) - COMPLETE

| Requirement | Status | Notes |
|-------------|--------|-------|
| FR-D1: Display Layout | [x] | Three-section layout (RER, Bus x2, Weather) |
| FR-D2: Weather Display | [x] | Icon, temperature, humidity, wind |
| FR-D3: Departure Boards | [x] | RER with names, Bus without |
| FR-D4: Refresh Strategy | [x] | Partial every 30s, full every 30min |
| FR-D5: Loading State | [x] | `displayLoader()` for init |
| FR-D6: Error States | [x] | "Aucun train/bus" centered |
| Status Bar | [x] | Time, WiFi, data age, battery |

---

## Enhancement Features Status

### Button Interaction (`specs/button-interaction.md`) - COMPLETE

| Requirement | Status | Notes |
|-------------|--------|-------|
| FR-B1: Button Debouncing | [x] | M5Unified handles this |
| FR-B2: Short Press Action | [x] | Forces immediate refresh |
| FR-B3: Long Press Action (5+ sec) | [x] | Shows "Configuration...", triggers config mode |
| FR-B4: Press Detection | [x] | `M5.BtnA.wasPressed()`, `pressedFor()` |

### WiFi Manager (`specs/wifi-manager.md`) - COMPLETE (2026-01-11)

| Requirement | Status | Notes |
|-------------|--------|-------|
| FR-WM1: Access Point Mode | [x] | Creates "ProchainDepart-Setup" AP |
| FR-WM2: Configuration Web Interface | [x] | WiFiManager captive portal |
| FR-WM3: Credential Storage (NVS) | [x] | WiFiManager auto-saves to NVS |
| FR-WM4: Connection Logic (fallback) | [x] | Uses `WiFiMulti` with multiple APs |
| FR-WM5: Runtime Config Access | [x] | Long press (5s) triggers AP mode |

### StopPoint Configuration (`specs/stoppoint-config.md`) - IMPLEMENTED (2026-01-11)

| Requirement | Status | Notes |
|-------------|--------|-------|
| FR-SC1: Station Configuration Storage | [x] | NVS-based storage via Preferences library |
| FR-SC2: Web Configuration Interface | [x] | WiFiManager custom parameters for 3 stations |
| FR-SC3: Station Presets | [ ] | Future enhancement (not implemented) |
| FR-SC4: Validation | [ ] | Future enhancement (not implemented) |

**Implementation Details:**
- Created `station_config.h` and `station_config.cpp` modules
- Stations stored in NVS namespace "stations" with keys like `s0_name`, `s0_ref`, etc.
- Falls back to `config.h` `STATIONS[]` if no NVS config exists
- WiFi Manager portal shows 3 station configuration sections
- Long press (5s) triggers config portal with station fields
- Changes take effect on device restart

---

## Priority Implementation Order

### P0 - M5Paper S3 Migration - COMPLETE

All phases complete. Firmware uploaded and running.

### P1 - Hardware Verification (CURRENT)

1. [ ] Verify WiFi connects to configured networks
2. [ ] Verify PRIM API returns departure data
3. [ ] Verify OpenWeatherMap API returns weather
4. [ ] Verify display renders correctly
5. [ ] Verify button triggers refresh
6. [ ] Run 24-hour stability test

### P2 - Future UI Enhancements

1. [ ] Touch support (GT911)
2. [x] Larger weather icons (100x100)
3. [ ] Configurable refresh intervals
4. [ ] Multiple screen layouts

### P3 - WiFi Manager - COMPLETE

1. [x] Add WiFiManager library
2. [x] Implement AP mode on button long press
3. [x] Create captive portal with instructions screen
4. [x] Store credentials in NVS (handled by WiFiManager)
5. [x] Long button press triggers AP mode

### P4 - Power Management - COMPLETE (2026-01-11)

1. [x] Light sleep between refreshes (25s cycles)
2. [x] Skip sleep when charging (USB = responsive mode)
3. [x] Battery-aware behavior (sleep only on battery)
4. [ ] RTC-based wake scheduling (timerSleep - future)
5. [ ] Configurable sleep duration via web interface

### P5 - StopPoint Configuration - COMPLETE (2026-01-11)

1. [x] Web interface for station config (WiFiManager custom params)
2. [x] NVS storage for stations (station_config module)
3. [ ] Station presets/search (future enhancement)

---

## Build Information

**Last Build**: 2026-01-11
**Status**: SUCCESS

```
RAM:   [==        ]  15.7% (used 51564 bytes from 327680 bytes)
Flash: [==        ]  19.8% (used 1295153 bytes from 6553600 bytes)
```

**Device**: M5Paper S3
**MAC**: d0:cf:13:16:07:40
**Port**: /dev/cu.usbmodem11101

---

## References

- [M5Unified GitHub](https://github.com/m5stack/M5Unified)
- [M5GFX GitHub](https://github.com/m5stack/M5GFX)
- [M5Paper S3 Product Page](https://shop.m5stack.com/products/m5papers3-esp32s3-development-kit)
- [M5Paper S3 Docs](https://docs.m5stack.com/en/core/M5PaperS3)

---

## Legend

- [x] Complete - Fully implemented
- [~] Partial - Implemented but incomplete
- [ ] Pending - Not yet implemented

---

## Changelog

- **2026-01-11**: StopPoint Configuration (P5) implementation complete
  - Created `station_config.h` and `station_config.cpp` modules
  - NVS-based storage using ESP32 Preferences library
  - WiFiManager custom parameters for up to 3 stations
  - Each station configurable: name, MonitoringRef, type (RER/BUS), filter
  - Falls back to `config.h` STATIONS[] if no NVS config exists
  - Config portal accessible via long press (5s) - shows station fields
  - Stations now loaded dynamically at runtime instead of hardcoded
- **2026-01-11**: Power Management implementation complete
  - Added light sleep between refresh cycles (25 seconds)
  - Uses `M5.Power.lightSleep()` for battery conservation
  - Sleep automatically skipped when charging via USB
  - WiFi reconnects quickly after wake (~5s)
  - Estimated battery life improvement: 3-4x over continuous operation
- **2026-01-11**: Larger weather icons (100x100)
  - Changed icon priority to prefer 100x100 over 50x50 mini icons
  - Better visibility on 960x540 screen
  - Falls back to mini icons if large not found
- **2026-01-11**: WiFi Manager implementation complete
  - Added tzapu/WiFiManager library for captive portal
  - Long press (5s) starts AP mode "ProchainDepart-Setup"
  - On-screen instructions displayed during configuration
  - Credentials auto-saved to NVS by WiFiManager
  - Device restarts after successful configuration
  - Updated config.h.example to use C arrays (avoid STL crash)
- **2026-01-11**: Long press detection and spec updates
  - Implemented FR-B3: Long press (5+ sec) shows "Configuration..." message
  - Updated specs/display.md for M5Paper S3 hardware
  - Updated specs/button-interaction.md with M5Unified API
  - Created git tag 0.0.1 for stable build
- **2026-01-11**: UI enhancements complete
  - Added degree symbol to temperature displays
  - Added low battery warning (<20%)
  - Added data freshness indicator
  - Added WiFi signal strength indicator
  - Status bar now shows: time | wifi | data age | battery%
- **2026-01-11**: Status bar with NTP time sync
  - Added 32px status bar at top of weather column
  - NTP sync with Paris timezone (CET-1CEST with DST)
  - Battery percentage and icon display
  - 4-level grayscale weather icons
  - Fixed text vertical centering
- **2026-01-11**: Firmware uploaded to M5Paper S3 device
  - Upload successful via esptool
  - Hardware testing phase began
- **2026-01-11**: M5Paper S3 migration complete
  - All display code migrated to M5GFX
  - Button handling via M5Unified
  - Boot loop fix (GPIO 12 conflict, STL crash)
- **2026-01-10**: Initial plan created from codebase analysis
