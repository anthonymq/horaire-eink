# Prochain Depart - Implementation Plan

**Last Updated**: 2026-01-11
**Status**: Active Development - **M5Paper S3 Migration - Firmware Uploaded, Hardware Testing In Progress**

---

## Summary

This document tracks implementation progress against specifications in `specs/` and the PRD in `docs/PRD.md`. Items are organized by priority and grouped by feature area.

---

## M5Paper S3 Migration (ACTIVE)

### Overview

Migrating from generic ESP32 + SPI E-Ink (GxEPD2) to M5Paper S3 with parallel EPD interface.

**Target Hardware**: [M5Paper S3](https://shop.m5stack.com/products/m5papers3-esp32s3-development-kit)

### Hardware Comparison

| Aspect | Current | M5Paper S3 |
|--------|---------|------------|
| MCU | ESP32 | ESP32-S3R8 |
| Display | 4.2" GDEY042T81 (400x300) | 4.7" ED047TC1 (960x540) |
| Driver | SSD1683 (SPI) | Parallel 8-bit EPD |
| Library | GxEPD2 | M5Unified + M5GFX |
| Touch | None | GT911 capacitive |
| Button | GPIO 0 | Built-in via M5Unified |
| Extras | - | Gyro (BMI270), Buzzer, RTC (BM8563), 1800mAh battery |

### Critical Note

> **GxEPD2 is NOT compatible with M5Paper S3.**
> The M5Paper S3 uses a parallel 8-bit EPD interface, not SPI.
> Must migrate to M5Unified/M5GFX or FastEPD library.

---

### Phase 1: Environment Setup

| Task | Status | Notes |
|------|--------|-------|
| Update `platformio.ini` for ESP32-S3 | [x] | Board: `esp32-s3-devkitc-1` (M5Unified auto-detects) |
| Add M5Unified library | [x] | `m5stack/M5Unified@^0.2.2` |
| Add M5GFX library | [x] | Auto-installed as M5Unified dependency |
| Remove GxEPD2 dependency | [x] | Removed from lib_deps |
| Verify build compiles | [x] | BUILD SUCCESS |

**Actual platformio.ini:**
```ini
[env:M5PaperS3]
platform = espressif32@^6.5.0
board = esp32-s3-devkitc-1
framework = arduino
board_build.arduino.memory_type = qio_opi
board_upload.flash_size = 16MB
board_build.partitions = default_16MB.csv
build_flags = 
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_CDC_ON_BOOT=1
lib_deps = 
    m5stack/M5Unified@^0.2.2
    bblanchon/ArduinoJson@^7.2.1
    fbiego/ESP32Time@^2.0.6
```

---

### Phase 2: Display Driver Migration

| Task | Status | Notes |
|------|--------|-------|
| Create new display header with M5GFX types | [x] | Replaced `GxEPD2_BW` include with `M5Unified.h` |
| Refactor display init in `main.cpp` | [x] | Uses `M5.begin(cfg)` |
| Remove HSPI/SPI configuration | [x] | M5Unified handles internally |
| Update display object declaration | [x] | Uses `M5.Display` throughout |

**API Mapping:**

| GxEPD2 | M5GFX (M5Unified) |
|--------|-------------------|
| `display.init()` | `M5.begin()` |
| `display.setRotation(3)` | `M5.Display.setRotation(3)` |
| `display.setFullWindow()` | `M5.Display.startWrite()` |
| `display.firstPage()/nextPage()` | Direct draw + `M5.Display.display()` |
| `display.fillScreen(GxEPD_WHITE)` | `M5.Display.fillScreen(TFT_WHITE)` |
| `display.drawRect()` | `M5.Display.drawRect()` |
| `display.setCursor()` | `M5.Display.setCursor()` |
| `display.setTextColor(GxEPD_BLACK)` | `M5.Display.setTextColor(TFT_BLACK)` |
| `display.width()` | `M5.Display.width()` |
| `display.height()` | `M5.Display.height()` |

---

### Phase 3: Drawing Code Refactor

| Task | Status | Notes |
|------|--------|-------|
| Migrate `displayLoader()` | [x] | Uses `textWidth()` and `fontHeight()` |
| Migrate `displayBanner()` | [x] | Updated for M5GFX API |
| Migrate `drawScreen()` | [x] | Complete rewrite for M5GFX |
| Migrate `draw2BitIcon()` | [x] | Works with `M5.Display.drawPixel()` |
| Migrate `displayWeather()` | [x] | Updated for M5GFX API |
| Migrate `displayDataFreshness()` | [x] | Placeholder (TODO in code) |

---

### Phase 4: UI Layout Redesign

| Task | Status | Notes |
|------|--------|-------|
| Recalculate for 960x540 resolution | [x] | Full layout updated |
| Update banner heights | [x] | 44 -> 80px |
| Update weather section | [x] | 55 -> 100px |
| Update departure cell sizes | [x] | Scaled proportionally |
| Consider larger fonts | [x] | Using FreeSansBold12pt7b |
| Consider more departure rows | [x] | 5 -> 6 rows |

**Resolution Scaling:**

| Element | Current (400x300) | New (960x540) |
|---------|-------------------|---------------|
| Weather Bar Height | 55px | 100px |
| Banner Height | 44px | 80px |
| Icon Size | 50x50 | 80x80 or 100x100 |
| Departure Rows | 5 | 6-7 |

---

### Phase 5: Button & Input Handling

| Task | Status | Notes |
|------|--------|-------|
| Replace GPIO 0 with M5Unified button API | [x] | Uses `M5.BtnA.wasPressed()` |
| Update button press detection | [x] | M5Unified handles debounce |
| Consider touch support (future) | [ ] | GT911 available - not implemented yet |

---

### Phase 6: Power Management

| Task | Status | Notes |
|------|--------|-------|
| Leverage BM8563 RTC for sleep/wake | [ ] | Future enhancement |
| Battery monitoring | [ ] | Future enhancement |
| Update power-off logic | [ ] | Future enhancement |

---

### Phase 7: Testing & Verification

| Task | Status | Notes |
|------|--------|-------|
| Build compiles successfully | [x] | **BUILD SUCCESS** |
| WiFi connectivity works | [ ] | Needs hardware test |
| PRIM API calls work | [ ] | Needs hardware test |
| OpenWeatherMap API works | [ ] | Needs hardware test |
| Full refresh displays correctly | [ ] | Needs hardware test |
| Partial refresh works | [ ] | Needs hardware test |
| Button interaction works | [ ] | Needs hardware test |
| 24+ hour stability test | [ ] | Needs hardware test |

---

### File Change Tracker (M5Paper S3)

| File | Status | Changes |
|------|--------|---------|
| `platformio.ini` | [x] Complete | ESP32-S3 board, M5Unified deps |
| `src/main.cpp` | [x] Complete | M5.begin(), M5.update(), button API |
| `include/display.h` | [x] Complete | M5Unified.h include, removed GxEPD2 |
| `src/display.cpp` | [x] Complete | Full M5GFX API migration |
| `include/types.h` | No Change | Data structures unchanged |
| `src/transport_api.cpp` | No Change | API logic unchanged |
| `src/weather_api.cpp` | No Change | API logic unchanged |
| `include/weathericons.h` | No Change | PROGMEM icons compatible |
| `include/config.h` | No Change | Credentials unchanged |

---

### Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| M5GFX API differences | Medium | API similar to Adafruit_GFX |
| Partial refresh behavior | Medium | Test on device, M5Paper S3 supports it |
| Font rendering differences | Low | Same Adafruit GFX fonts work |
| Weather icon scaling | Medium | May need larger icons |

---

### Migration Success Criteria

- [x] App compiles without errors
- [x] Uploads to M5Paper S3 successfully (uploaded 2026-01-11 via `/dev/cu.usbmodem11101`)
- [ ] WiFi connects and APIs return data
- [ ] Display shows departures correctly
- [ ] Display shows weather correctly
- [ ] Button triggers refresh
- [ ] Partial refresh works
- [ ] Full refresh clears ghosting
- [ ] Stable for 24+ hours

---

### References

- [M5Unified GitHub](https://github.com/m5stack/M5Unified)
- [M5GFX GitHub](https://github.com/m5stack/M5GFX)
- [M5Paper S3 Product Page](https://shop.m5stack.com/products/m5papers3-esp32s3-development-kit)
- [FastEPD (alternative)](https://github.com/bitbank2/FastEPD)
- [M5Paper S3 Docs](https://docs.m5stack.com/en/core/M5PaperS3)

---

## Legend

- [x] Complete - Fully implemented and matches specification
- [~] Partial - Implemented but incomplete or has known issues
- [ ] Pending - Not yet implemented

---

## Core Functionality Status

### Transport API (`specs/transport-api.md`) - MOSTLY COMPLETE

| Requirement | Status | Notes |
|-------------|--------|-------|
| FR-T1: Fetch Departures | [x] | `fetchDepartures()` in `transport_api.cpp` |
| FR-T2: Multi-Station Support | [x] | Loop through `STATION_ENDPOINTS` in `main.cpp` |
| FR-T3: Filtering (RER by platform, BUS by operator) | [x] | Implemented with `type` and `filter` params |
| FR-T4: Data Extraction | [x] | `departureType` struct populated correctly |
| FR-T5: Error Handling | [x] | Graceful failure with empty list return |
| 30-second refresh rate | [x] | `rtc.getEpoch() - lastCallEpoch > 29` |

### Weather API (`specs/weather-api.md`) - COMPLETE

| Requirement | Status | Notes |
|-------------|--------|-------|
| FR-W1: Fetch Current Weather | [x] | `fetchWeather()` in `weather_api.cpp` |
| FR-W2: Data Extraction | [x] | All fields populated in `weatherType` |
| FR-W3: Icon Mapping | [x] | `mapWeatherIcon()` maps OWM codes |
| FR-W4: Error Handling | [x] | Returns empty struct on failure |
| 30-minute refresh interval | [x] | `WEATHER_CALL_INTERVAL` check in `main.cpp` |

### Display (`specs/display.md`) - MOSTLY COMPLETE

| Requirement | Status | Notes |
|-------------|--------|-------|
| FR-D1: Display Layout | [x] | Three-section layout implemented |
| FR-D2: Weather Display | [x] | Shows icon, temperature, humidity |
| FR-D3: Departure Boards | [~] | **Issue**: RER C banner not filled (line 130 commented out) |
| FR-D4: Refresh Strategy | [x] | Partial every 30s, full every 5min |
| FR-D5: Loading State | [x] | `displayLoader()` for init messages |
| FR-D6: Error States | [x] | "Aucun train" centered in board |
| HSPI initialization | [x] | Pins 13/12/14/15 configured |

**Known Issues:**
1. `display.fillRect(posx, posy, width, BANNER_HEIGHT, GxEPD_BLACK);` is **commented out** on line 130 of `display.cpp` - RER C banner background is not black
2. `displayDepartureBoard()` declared in header but not implemented as separate function (code is inline)

---

## Enhancement Features Status

### Button Interaction (`specs/button-interaction.md`) - PARTIAL

| Requirement | Status | Notes |
|-------------|--------|-------|
| FR-B1: Button Debouncing | [x] | `lastButtonState` tracking in `main.cpp` |
| FR-B2: Short Press Action | [ ] | **NOT IMPLEMENTED** - Logs but no action |
| FR-B3: Long Press Action (5+ sec) | [ ] | **NOT IMPLEMENTED** - No timing logic |
| FR-B4: Press Detection | [x] | State transition detected |

**Current State**: Button press is detected and logged ("The state changed from LOW to HIGH") but **no action is triggered**. Need to:
1. Add short press handler to trigger immediate refresh
2. Add press duration tracking with `millis()`
3. Add long press (5s) handler for config mode

### WiFi Manager (`specs/wifi-manager.md`) - NOT IMPLEMENTED

| Requirement | Status | Notes |
|-------------|--------|-------|
| FR-WM1: Access Point Mode | [ ] | Not implemented |
| FR-WM2: Configuration Web Interface | [ ] | Not implemented |
| FR-WM3: Credential Storage (NVS) | [ ] | Not implemented |
| FR-WM4: Connection Logic (fallback) | [ ] | Not implemented |
| FR-WM5: Runtime Config Access | [ ] | Not implemented |

**Current State**: WiFi credentials are hardcoded in `main.cpp` line 93:
```cpp
wifiMulti.addAP("Freebox-35649A", "cd652qkv2xqvrtnwq96724");
```
The `WIFI_CREDENTIALS` map in `config.h` is **not used**.

### StopPoint Configuration (`specs/stoppoint-config.md`) - NOT IMPLEMENTED

| Requirement | Status | Notes |
|-------------|--------|-------|
| FR-SC1: Station Configuration Storage | [ ] | Hardcoded in `config.h` |
| FR-SC2: Web Configuration Interface | [ ] | Depends on WiFi Manager |
| FR-SC3: Station Presets | [ ] | Not implemented |
| FR-SC4: Validation | [ ] | Not implemented |

**Current State**: Station endpoints, types, filters all hardcoded in `config.h`.

---

## Future Roadmap Features (from PRD)

### Version 1.1 Features

| Feature | Priority | Status | Notes |
|---------|----------|--------|-------|
| WiFi Manager | P1 | [ ] | See WiFi Manager section above |
| OTA Updates | P1 | [ ] | No ArduinoOTA or ElegantOTA implementation |
| Web Configuration | P1 | [ ] | Depends on WiFi Manager |
| NVS Storage | P2 | [ ] | No Preferences library usage |

### Version 1.2 Features

| Feature | Priority | Status | Notes |
|---------|----------|--------|-------|
| Deep Sleep | P2 | [ ] | Not implemented |
| Multiple Layouts | P2 | [ ] | Fixed layout only |
| Delay Alerts (>15min) | P2 | [ ] | Not implemented |
| Time Display | P3 | [ ] | Not implemented |

---

## Code Quality Issues

### Technical Debt

1. **Duplicate WiFi credentials** - Credentials in both `main.cpp` (hardcoded) and `config.h` (`WIFI_CREDENTIALS` unused)

2. **Dead code in `displayDataFreshness()`** - Function body contains only commented-out LCD code (lines 405-419 in `display.cpp`)

3. **Unused library dependency** - `LiquidCrystal_I2C` in `platformio.ini` and `misc.h` - appears to be from earlier LCD version

4. **Missing header guard placement** - `#pragma once` after content in `misc.h`

5. **TODO comment** - Line 144 in `main.cpp`: `// @TODO fix data freshness`

6. **Exposed API keys** - Real API keys visible in `config.h` (should be in `.gitignore`)

7. **Icon size mismatch** - `weathericons.h` has 50x50 mini icons but display code uses 100x100 dimensions (lines 99-100 in `display.cpp`)

8. **RER banner missing fill** - Line 130 in `display.cpp` is commented out, so RER C banner has no black background

### Unused/Legacy Code

- `src/2_7screen.cpp` - Commented out code for older 2.7" display
- `registerSpecialChars()` in `misc.cpp` - LCD-specific, not used for E-Ink
- `SevSeg` library in `platformio.ini` - Not used

---

## Priority Implementation Order

### P0 - M5Paper S3 Migration (ACTIVE PRIORITY)

**This is the current active work.** See "M5Paper S3 Migration" section above for detailed tracking.

1. [ ] **Phase 1**: Environment setup (platformio.ini, dependencies)
2. [ ] **Phase 2**: Display driver migration (GxEPD2 -> M5Unified)
3. [ ] **Phase 3**: Drawing code refactor
4. [ ] **Phase 4**: UI layout redesign for 960x540
5. [ ] **Phase 5**: Button handling migration
6. [ ] **Phase 6**: Power management integration
7. [ ] **Phase 7**: Testing & verification

### P1 - Critical Bugs (Post-Migration or if staying on current hardware)

1. [ ] **Fix RER C banner** - Uncomment line 130 in `display.cpp` to fill banner background black
2. [ ] **Fix weather icon size** - Icons are 50x50 but code specifies 100x100

### P1 - Button Short Press (Quick Win)

3. [ ] **Implement short press action** - Trigger immediate data refresh when button released
   - Set `lastCallEpoch = 0` to force refresh on next loop iteration

### P2 - Button Long Press

4. [ ] **Add press duration tracking** - Track press start time with `millis()`
5. [ ] **Implement long press detection** - 5+ seconds triggers config mode placeholder

### P3 - WiFi Manager (Major Feature)

6. [ ] **Add WiFiManager library** - `tzapu/WiFiManager` to `platformio.ini`
7. [ ] **Implement AP mode on boot failure** - Fallback when WiFi connection fails
8. [ ] **Create captive portal** - Web form for SSID/password entry
9. [ ] **Store credentials in NVS** - Use Preferences library for persistence
10. [ ] **Long button press triggers AP mode** - Integrate with button handler

### P4 - StopPoint Configuration

11. [ ] **Extend web interface** - Add station configuration page
12. [ ] **Store station config in NVS** - Persist station settings
13. [ ] **Create station presets** - Pre-defined common stations
14. [ ] **API validation before save** - Test API call on config submit

### P5 - Code Cleanup

15. [ ] **Remove LCD dependencies** - Delete `LiquidCrystal_I2C` from `platformio.ini`, clean up `misc.cpp/h`
16. [ ] **Use WIFI_CREDENTIALS map** - Loop through config map instead of hardcoded credentials
17. [ ] **Fix `displayDataFreshness()`** - Either implement E-Ink version or remove
18. [ ] **Remove SevSeg dependency** - Not used
19. [ ] **Move API keys out of repo** - Ensure `config.h` is in `.gitignore`

### P6 - Future Enhancements

20. [ ] **OTA Updates** - Add ArduinoOTA or ElegantOTA support
21. [ ] **Deep Sleep** - Implement power saving between refreshes
22. [ ] **Time Display** - Show current time on screen
23. [ ] **Delay Alerts** - Visual indicator for delays >15min

---

## File Inventory

### Source Files (`src/`)

| File | Purpose | Status |
|------|---------|--------|
| `main.cpp` | Application entry, WiFi, main loop | Active |
| `transport_api.cpp` | PRIM API fetching | Active |
| `weather_api.cpp` | OpenWeatherMap API | Active |
| `display.cpp` | E-Ink rendering | Active |
| `misc.cpp` | Date/time utilities | Active (has legacy LCD code) |
| `2_7screen.cpp` | Old 2.7" display code | **Legacy - can be removed** |

### Header Files (`include/`)

| File | Purpose | Status |
|------|---------|--------|
| `config.h` | API keys, credentials, stations | Active (contains secrets) |
| `config.h.example` | Template for config | Active |
| `types.h` | Data structures | Active |
| `display.h` | Display function declarations | Active |
| `transport_api.h` | Transport API declarations | Active |
| `weather_api.h` | Weather API declarations | Active |
| `misc.h` | Utility declarations | Active (has legacy LCD include) |
| `weathericons.h` | Weather icon bitmaps | Active |
| `WeatherStationFonts.h` | Additional fonts | Active |

### Specification Files (`specs/`)

| File | Implemented |
|------|-------------|
| `transport-api.md` | Yes |
| `weather-api.md` | Yes |
| `display.md` | Mostly |
| `button-interaction.md` | Partial |
| `wifi-manager.md` | No |
| `stoppoint-config.md` | No |

---

## Notes

- The project has a solid core implementation for transit and weather display
- Main gaps are in user configuration features (WiFi Manager, StopPoint config)
- Button functionality needs completion for better UX
- Several cleanup opportunities exist (dead code, unused libraries)
- API keys are currently exposed in `config.h` - security concern

---

## Changelog

- **2026-01-11**: Firmware uploaded to M5Paper S3 device
  - Upload successful via esptool to `/dev/cu.usbmodem11101`
  - Device MAC: d0:cf:13:16:07:40
  - Hardware testing phase in progress
- **2026-01-11**: Added M5Paper S3 migration plan as P0 priority
  - Hardware comparison and compatibility analysis
  - 7-phase migration plan with detailed task tracking
  - API mapping from GxEPD2 to M5GFX
  - Resolution scaling guide (400x300 -> 960x540)
  - Success criteria and risk mitigations
- **2026-01-10**: Initial plan created from codebase analysis
