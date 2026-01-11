# Prochain Départ - Operational Guide

## Build & Upload

```bash
# Build firmware
pio run

# Upload to ESP32
pio run --target upload

# Build and upload in one command
pio run --target upload --environment HoraireEINK
```

## Monitor Serial Output

```bash
# Monitor at 115200 baud (configured in platformio.ini)
pio device monitor

# Or with specific baud rate
pio device monitor -b 115200
```

## Clean Build

```bash
# Clean build artifacts
pio run --target clean

# Full clean (removes .pio directory)
rm -rf .pio && pio run
```

## Project Structure

```
src/           - Application source code (.cpp files)
include/       - Header files (.h files)
  config.h     - API keys, WiFi credentials, station config (SENSITIVE - not in git)
  config.h.example - Template for config.h
lib/           - Project-specific libraries
docs/          - Documentation and reference files
specs/         - Feature specifications for Ralph loop
```

## Configuration

Before building, copy `include/config.h.example` to `include/config.h` and fill in:
- `API_TOKEN` - PRIM API key from https://prim.iledefrance-mobilites.fr
- `WEATHER_API_KEY` - OpenWeatherMap API key
- `WIFI_CREDENTIALS` - Your WiFi SSID and password

## Hardware Connections

| E-Ink Pin | ESP32 GPIO |
|-----------|------------|
| DIN/MOSI  | 14         |
| CLK/SCLK  | 13         |
| CS        | 15         |
| DC        | 27         |
| RST       | 26         |
| BUSY      | 25         |
| Button    | 12         |

## Codebase Patterns

- Use `USE_SERIAL` macro for Serial output (defined as `Serial`)
- E-Ink display uses GxEPD2 library with HSPI
- API responses are parsed with ArduinoJson v7
- Time management via ESP32Time library
- Weather icons stored in PROGMEM (weathericons.h)
