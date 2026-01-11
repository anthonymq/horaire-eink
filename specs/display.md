# E-Ink Display Specification

## Overview
The display module manages rendering of transit and weather information on the M5Paper S3's built-in E-Ink display using the M5Unified/M5GFX library.

## Hardware

| Property | Value |
|----------|-------|
| Device | M5Paper S3 |
| Display Model | ED047TC1 (4.7" parallel EPD) |
| Resolution | 960 x 540 pixels |
| Color Depth | 4-bit grayscale (16 levels) |
| Interface | Parallel 8-bit (handled internally by M5Unified) |
| Orientation | Landscape (rotation 1) |

## Display Access

The display is accessed through M5Unified's `M5.Display` object. No manual SPI/pin configuration is needed.

```cpp
#include <M5Unified.h>

// In setup()
auto cfg = M5.config();
M5.begin(cfg);
M5.Display.setRotation(1);  // Landscape

// Drawing
M5.Display.fillScreen(TFT_WHITE);
M5.Display.setTextColor(TFT_BLACK);
M5.Display.drawString("Hello", 100, 100);
```

## Functional Requirements

### FR-D1: Display Layout
The 960x540 screen is divided into sections:

```
+----------------------------------+------------+
|         RER C - Ivry             | 17:25 [WiFi] 12s 85%[bat] |
|  (760px width, 270px height)     |------------------------------|
|   Schedule    Name      Delay    |      [Weather Icon]          |
|   10:15     Paris       2 min    |         15.2°C               |
|   10:22     Versailles  9 min    |      Ressenti 14°C           |
|   ...                            |         63% hum              |
+----------------+-----------------+         nuageux              |
| 323 Moliere    | 323 Jules F     |        2.1 m/s               |
| (380px)        | (380px)         |                              |
| 10:18   3min   | 10:20   5min    |     Ivry-sur-Seine           |
+----------------+-----------------+------------------------------+
```

Layout dimensions:
- Status bar: 32px height (in weather column)
- Station banners: 36px height
- Weather column: 200px width (right side)
- Left area: 760px for departures

### FR-D2: Weather Display
- Show weather icon (2-bit PROGMEM, rendered as 4-level grayscale)
- Show temperature with degree symbol (°C)
- Show "feels like" temperature
- Show humidity percentage
- Show wind speed
- Show city name
- Position in 200px right column

### FR-D3: Departure Boards
For each station:
- Banner with station name (white text on black background)
- Grid of departures with:
  - Schedule time (left-aligned, HH:MM)
  - Destination name (center, for RER only)
  - Delay in minutes (right-aligned, "Xmin")

### FR-D4: Refresh Strategy
- **Partial Refresh**: Every 30 seconds for data updates (fast, minimal flicker)
- **Full Refresh**: Every 30 minutes to clear E-Ink ghosting artifacts

### FR-D5: Loading State
- Display "Horaire..." message during initialization
- Display "Connection au WIFI..." during WiFi connection

### FR-D6: Error States
- Display "Aucun train" or "Aucun bus" when no departures available
- Center error message in the departure board area

### FR-D7: Status Bar
- Current time (HH:MM, NTP-synced)
- WiFi signal strength (4-bar indicator)
- Data freshness (seconds/minutes since last update)
- Battery percentage and icon
- Low battery warning (<20%)
- Charging indicator

## Typography

| Element | Font | Notes |
|---------|------|-------|
| Status bar | FreeSansBold9pt7b | Time, indicators |
| Station banners | FreeSansBold12pt7b | White on black |
| Departure times | FreeSansBold9pt7b | Schedule column |
| Destinations | FreeSansBold9pt7b | Center column (RER only) |
| Weather text | FreeSansBold9pt7b | Temperature, humidity |
| Weather city | FreeSans9pt7b | Location name |

## Weather Icons
- Icons stored in `weathericons.h` as 2-bit PROGMEM bitmaps
- Icon size: 100x100 pixels (preferred), falls back to 50x50 mini icons
- Rendered via `draw2BitIcon()` with 4-level grayscale mapping
- Function: `getMeteoconIconFromProgmem(iconName)` for 100x100
- Function: `getMiniMeteoconIconFromProgmem(iconName)` for 50x50 fallback

## Colors
M5GFX uses TFT color constants:
- `TFT_BLACK` for text and icons
- `TFT_WHITE` for backgrounds
- `TFT_LIGHTGREY` for subtle elements
- `TFT_DARKGREY` for secondary text

## Acceptance Criteria

- [x] Display initializes correctly via M5Unified
- [x] Weather bar shows icon, temperature, humidity
- [x] RER C board shows up to 5 departures with destinations
- [x] Bus boards show up to 5 departures (no destinations)
- [x] Partial refresh updates content without full flash
- [x] Full refresh clears ghosting every 30 minutes
- [x] Loading messages display during initialization
- [x] Error messages display when no data available
- [x] Status bar shows time, WiFi, data age, battery
- [ ] Text is readable from 2+ meters distance

## Priority
P0 - Core feature (implemented)

## Migration Notes

| Old (GxEPD2) | New (M5GFX) |
|--------------|-------------|
| `display.init()` | `M5.begin(cfg)` |
| `display.setRotation(3)` | `M5.Display.setRotation(1)` |
| `display.getTextBounds()` | `M5.Display.textWidth()` + `fontHeight()` |
| `GxEPD_BLACK/WHITE` | `TFT_BLACK/WHITE` |
| `display.firstPage()/nextPage()` | Direct draw + `display()` |
