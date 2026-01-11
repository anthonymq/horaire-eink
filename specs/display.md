# E-Ink Display Specification

## Overview
The display module manages rendering of transit and weather information on a 4.2" E-Ink display using the GxEPD2 library.

## Hardware

| Property | Value |
|----------|-------|
| Display Model | GDEY042T81 |
| Resolution | 400 x 300 pixels |
| Color Depth | 1-bit (Black & White) |
| Driver IC | SSD1683 |
| Interface | SPI (HSPI) |
| Orientation | Landscape (rotation 3) |

## Pin Configuration

| E-Ink Pin | ESP32 GPIO |
|-----------|------------|
| DIN/MOSI | 14 |
| CLK/SCLK | 13 |
| CS | 15 |
| DC | 27 |
| RST | 26 |
| BUSY | 25 |

## Functional Requirements

### FR-D1: Display Layout
The screen is divided into sections:

```
┌──────────────────────────────────────────────────────────────┐
│ Weather bar (22px): Icon, temp, humidity                     │
├──────────────────────────────────────────────────────────────┤
│ RER C banner (centered, white on black)                      │
├──────────────────────────────────────────────────────────────┤
│ RER C departures (5 rows): Time | Destination | Delay        │
├─────────────────────────────┬────────────────────────────────┤
│ Bus 323 Molière banner      │ Bus 323 Jules F banner         │
├─────────────────────────────┼────────────────────────────────┤
│ Bus departures (5 rows)     │ Bus departures (5 rows)        │
└─────────────────────────────┴────────────────────────────────┘
```

### FR-D2: Weather Display
- Show weather icon (bitmap from PROGMEM)
- Show temperature with °C symbol
- Show humidity percentage
- Position at top of screen (22px height)

### FR-D3: Departure Boards
For each station:
- Banner with station name (white text on black background)
- Grid of departures with:
  - Schedule time (left-aligned, HH:MM)
  - Destination name (center, for RER only)
  - Delay in minutes (right-aligned, "Xmin")

### FR-D4: Refresh Strategy
- **Partial Refresh**: Every 30 seconds for data updates (fast, minimal flicker)
- **Full Refresh**: Every 5 minutes to clear E-Ink ghosting artifacts

### FR-D5: Loading State
- Display "Horaire..." message during initialization
- Display "Connexion au WIFI..." during WiFi connection

### FR-D6: Error States
- Display "Aucun train" when no departures available
- Center error message in the departure board area

## Typography

| Element | Font | Size |
|---------|------|------|
| Weather bar | FreeSansBold | 9pt |
| Station banners | FreeSansBold | 12pt |
| Departure times | FreeSansBold | 9pt |
| Destinations | FreeSansBold | 9pt |
| Delay countdown | FreeSansBold | 9pt |

## Weather Icons
- Icons stored in `weathericons.h` as PROGMEM bitmaps
- Icon size: 50x50 pixels (mini meteocons)
- Function: `getMiniMeteoconIconFromProgmem(iconName)`

## Acceptance Criteria

- [ ] Display initializes correctly with HSPI
- [ ] Weather bar shows icon, temperature, humidity
- [ ] RER C board shows up to 5 departures with destinations
- [ ] Bus boards show up to 5 departures (no destinations)
- [ ] Partial refresh updates content without full flash
- [ ] Full refresh clears ghosting every 5 minutes
- [ ] Loading messages display during initialization
- [ ] Error messages display when no data available
- [ ] Text is readable from 2+ meters distance
