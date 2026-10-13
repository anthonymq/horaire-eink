# Prochain Départ - Project Knowledge Base

**Generated:** 2026-01-11  
**Commit:** 97136e4  
**Branch:** migrate-to-paper-s3

## Overview

ESP32-S3 E-Ink dashboard displaying real-time transit (PRIM/IDFM API) and weather (OpenWeatherMap) for Île-de-France. Currently targeting **M5Paper S3** (960x540 parallel EPD) via M5Unified.

## Structure

```
horaire-eink/
├── src/                    # Application source
│   ├── main.cpp            # Entry point, WiFi, loop orchestration
│   ├── display.cpp         # M5GFX rendering (960x540 layout)
│   ├── transport_api.cpp   # PRIM API client, SIRI JSON parsing
│   ├── weather_api.cpp     # OpenWeatherMap client
│   └── misc.cpp            # Date/time utilities
├── include/                # Headers
│   ├── config.h            # SENSITIVE: API keys, WiFi, stations (not in git)
│   ├── config.h.example    # Template for config.h
│   ├── types.h             # departureType, weatherType structs
│   ├── weathericons.h      # 100x100 and 50x50 2-bit PROGMEM icons
│   └── WeatherStationFonts.h
├── specs/                  # Feature specifications (Ralph loop)
├── docs/                   # Reference documentation
└── platformio.ini          # Build config (M5PaperS3 env active)
```

## Where to Look

| Task | Location | Notes |
|------|----------|-------|
| Add new station | `include/config.h` | Add to `STATIONS[]` array |
| Change UI layout | `src/display.cpp` | `drawScreen()`, constants at top |
| Modify refresh timing | `src/main.cpp` | `FETCH_INTERVAL_S`, `WEATHER_INTERVAL_S`, quiet hours (see `docs/power_optimization.md`) |
| Add WiFi network | `include/config.h` | `WIFI_CREDENTIALS[]` array |
| Change hardware pins | N/A | M5Unified handles internally |
| API response parsing | `src/transport_api.cpp` | `fetchDepartures()` - complex SIRI structure |

## Commands

```bash
# Build
pio run

# Upload to M5Paper S3
pio run --target upload

# Serial monitor
pio device monitor -b 115200

# Clean build
rm -rf .pio && pio run
```

## Conventions

- **Serial output**: Use `USE_SERIAL` macro (defined as `Serial`)
- **Display API**: All drawing via `M5.Display.*` (M5GFX)
- **Colors**: `TFT_BLACK`, `TFT_WHITE` (not GxEPD constants)
- **JSON parsing**: ArduinoJson v7 with `JsonDocument` (not v6 `DynamicJsonDocument`)
- **Time**: `ESP32Time rtc` for epoch, POSIX `TZ` env var for timezone
- **Icons**: 2-bit format in PROGMEM, rendered via `draw2BitIcon()`

## Anti-Patterns (This Project)

| Pattern | Why Forbidden |
|---------|---------------|
| `GxEPD2` / SPI display code | **REMOVED** - M5Paper S3 uses parallel EPD |
| `display.firstPage()/nextPage()` | Use `M5.Display.startWrite()/endWrite()` |
| GPIO 12 for button | **CONFLICTS with boot** - M5Unified handles button |
| Light sleep without `M5.Display.waitDisplay()` | EPD refresh runs in a background task; sleeping mid-refresh corrupts the panel and keeps HV rails on |
| Global STL containers (`std::map`) | Causes boot crash - use C arrays in config.h |
| `as any`, `@ts-ignore` | N/A (C++ project) |

## Hardware

**Current**: M5Paper S3 (ESP32-S3, 960x540 EPD, GT911 touch, BM8563 RTC)

| Feature | API |
|---------|-----|
| Display | `M5.Display.*` |
| Button | None usable (`M5.BtnA` is a virtual touch button, disabled) |
| Touch | GT911, INT on GPIO 48: tap = refresh, hold 5 s = config portal, wakes from light sleep |
| RTC | BM8563 available (not implemented) |

## Data Flow

```
main.cpp::loop()
  ├─→ runFetchCycle() (every FETCH_INTERVAL_S): WiFi on → fetchDepartures() ×N → fetchWeather() (30min) → WiFi off
  ├─→ render() every minute: countdowns computed from departureType.expected → drawScreen()
  └─→ idleFor(): waitDisplay() → light sleep until next minute (timer or touch)
```

## Known Issues

1. **Fixed layout** - 3 boards (RER top, 2 bus bottom); titles come from station `displayName`

## Configuration Setup

```bash
cp include/config.h.example include/config.h
# Edit config.h with:
#   - API_TOKEN (from prim.iledefrance-mobilites.fr)
#   - WEATHER_API_KEY (from openweathermap.org)
#   - WIFI_CREDENTIALS
```

## Migration Notes (GxEPD2 → M5Unified)

| Old (GxEPD2) | New (M5GFX) |
|--------------|-------------|
| `display.init()` | `M5.begin(cfg)` |
| `display.setRotation(3)` | `M5.Display.setRotation(1)` |
| `display.getTextBounds()` | `M5.Display.textWidth()` + `fontHeight()` |
| `GxEPD_BLACK/WHITE` | `TFT_BLACK/WHITE` |
| `display.firstPage()/nextPage()` | Direct draw + `display()` |
| GPIO button handling | `M5.BtnA.wasPressed()` |

## References

- [M5Unified](https://github.com/m5stack/M5Unified)
- [M5Paper S3 Docs](https://docs.m5stack.com/en/core/M5PaperS3)
- [PRIM API](https://prim.iledefrance-mobilites.fr/fr/apis/idfm-ivtr-requete_unitaire)
- [ArduinoJson v7](https://arduinojson.org/v7/)
