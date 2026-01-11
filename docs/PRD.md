# Product Requirements Document (PRD)
# Prochain Départ - Real-Time Transit Display

**Version**: 1.0  
**Date**: January 10, 2026  
**Status**: Active Development  
**Author**: Product Team

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Problem Statement](#2-problem-statement)
3. [Target Users / Personas](#3-target-users--personas)
4. [Product Vision & Goals](#4-product-vision--goals)
5. [Feature Requirements](#5-feature-requirements)
6. [Technical Architecture Overview](#6-technical-architecture-overview)
7. [Hardware Requirements](#7-hardware-requirements)
8. [Software Dependencies](#8-software-dependencies)
9. [API Integrations](#9-api-integrations)
10. [User Interface / Display Layout](#10-user-interface--display-layout)
11. [Data Flow](#11-data-flow)
12. [Configuration Requirements](#12-configuration-requirements)
13. [Performance Requirements](#13-performance-requirements)
14. [Success Metrics / KPIs](#14-success-metrics--kpis)
15. [Future Roadmap / Backlog](#15-future-roadmap--backlog)
16. [Risks & Mitigations](#16-risks--mitigations)
17. [Glossary](#17-glossary)

---

## 1. Executive Summary

**Prochain Départ** ("Next Departure" in French) is an ESP32-based IoT device that displays real-time public transportation schedules and weather information on a low-power E-Ink display. Designed specifically for residents of Ivry-sur-Seine in the Île-de-France region, the device provides at-a-glance departure information for RER C trains and Bus line 323, eliminating the need to check mobile apps or websites when leaving home.

The product combines:
- **Real-time transit data** from Île-de-France Mobilités (IDFM) PRIM API
- **Current weather conditions** from OpenWeatherMap API
- **E-Ink display technology** for excellent readability and minimal power consumption
- **WiFi connectivity** for automatic data updates

---

## 2. Problem Statement

### The Challenge

Residents of Île-de-France who rely on public transportation face a daily challenge: knowing the right time to leave home to catch their train or bus. Current solutions have significant drawbacks:

| Current Solution | Problems |
|------------------|----------|
| Mobile apps (RATP, Citymapper) | Requires unlocking phone, waiting for app load, navigating UI |
| Station displays | Only visible at the station—too late to adjust departure time |
| Static schedules | Don't account for delays, cancellations, or real-time changes |
| Smart home displays | Often too complex, require significant setup, expensive |

### Pain Points

1. **Time waste**: Arriving too early at stations means wasted time waiting
2. **Missed connections**: Leaving too late means missing trains and cascading delays
3. **Weather surprises**: Forgetting to check weather leads to inappropriate clothing choices
4. **Cognitive load**: Checking multiple sources (transit + weather) adds mental overhead

### Opportunity

A dedicated, always-on display near the door provides instant visibility into departure times, allowing users to optimize their departure timing with zero friction.

---

## 3. Target Users / Personas

### Primary Persona: Marie, 34 - Daily Commuter

| Attribute | Details |
|-----------|---------|
| **Location** | Ivry-sur-Seine, apartment near RER C station |
| **Commute** | RER C to work in Versailles, 45 minutes each way |
| **Tech comfort** | Moderate - uses smartphone apps but prefers simple solutions |
| **Pain points** | Often runs for trains, hates checking phone while rushing |
| **Goals** | Leave home at the optimal time, never miss a train |

**Quote**: *"I just want to glance at something on my way out the door and know if I need to hurry."*

### Secondary Persona: Jean-Pierre, 62 - Semi-Retired

| Attribute | Details |
|-----------|---------|
| **Location** | Ivry-sur-Seine, house with family |
| **Transit use** | Bus 323 for shopping and appointments, RER C occasionally |
| **Tech comfort** | Low - finds smartphone apps confusing |
| **Pain points** | Small text on phone, complex interfaces |
| **Goals** | Simple, readable display that "just works" |

**Quote**: *"Why do I need to learn an app? I just want to see when the next bus comes."*

### Secondary Persona: Thomas, 28 - Tech Enthusiast

| Attribute | Details |
|-----------|---------|
| **Location** | Ivry-sur-Seine, shared apartment |
| **Interests** | IoT, home automation, DIY electronics |
| **Tech comfort** | High - comfortable with Arduino, APIs, soldering |
| **Pain points** | Commercial solutions are expensive and locked-down |
| **Goals** | Build and customize a transit display for his specific needs |

**Quote**: *"I want to hack on this thing and maybe add my own features."*

---

## 4. Product Vision & Goals

### Vision Statement

> Prochain Départ transforms any wall into a smart transit dashboard, giving Île-de-France residents the information they need to leave home at exactly the right time.

### Product Goals

| Goal | Description | Success Criteria |
|------|-------------|------------------|
| **G1: Instant Information** | Users can see next departures in under 1 second | 100% of users report faster access than phone apps |
| **G2: Reliable Data** | Display shows accurate, real-time information | Data matches official sources within 30 seconds |
| **G3: Low Maintenance** | Device operates autonomously once installed | < 1 maintenance interaction per month |
| **G4: Energy Efficient** | Minimal power consumption suitable for continuous operation | < 1W average power draw |
| **G5: Extensible** | Platform allows customization and feature additions | Users can modify stations, add features |

### Non-Goals (Out of Scope for v1.0)

- Mobile app companion
- Cloud backend / remote management
- Multiple language support
- Support for regions outside Île-de-France
- Battery operation (AC power required)

---

## 5. Feature Requirements

### 5.1 Functional Requirements

#### FR-1: Real-Time Transit Display

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| FR-1.1 | Display next 5 RER C departures from configured platform | P0 | Implemented |
| FR-1.2 | Display next 5 Bus 323 departures from Molière stop | P0 | Implemented |
| FR-1.3 | Display next 5 Bus 323 departures from Jules Ferry stop | P0 | Implemented |
| FR-1.4 | Show destination name for each departure | P0 | Implemented |
| FR-1.5 | Show scheduled time in HH:MM format | P0 | Implemented |
| FR-1.6 | Show minutes until departure (countdown) | P0 | Implemented |
| FR-1.7 | Refresh departure data every 30 seconds | P0 | Implemented |
| FR-1.8 | Filter RER departures by platform (configurable) | P1 | Implemented |
| FR-1.9 | Handle API errors gracefully with "Aucun train" message | P1 | Implemented |

#### FR-2: Weather Display

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| FR-2.1 | Display current temperature in Celsius | P1 | Implemented |
| FR-2.2 | Display weather condition icon | P1 | Implemented |
| FR-2.3 | Display humidity percentage | P2 | Implemented |
| FR-2.4 | Refresh weather data every 30 minutes | P1 | Implemented |
| FR-2.5 | Map OpenWeatherMap icons to local bitmap icons | P1 | Implemented |

#### FR-3: Network Connectivity

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| FR-3.1 | Connect to WiFi network on startup | P0 | Implemented |
| FR-3.2 | Support multiple WiFi network credentials | P1 | Implemented |
| FR-3.3 | Display "Connecting..." message during WiFi setup | P1 | Implemented |
| FR-3.4 | Auto-reconnect on WiFi disconnection | P1 | Implemented |
| FR-3.5 | WiFi Manager for config without recompiling | P2 | Planned |

#### FR-4: User Interaction

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| FR-4.1 | Physical button for manual refresh | P2 | Partial |
| FR-4.2 | Button debouncing to prevent false triggers | P2 | Implemented |
| FR-4.3 | Visual feedback on button press | P3 | Planned |

### 5.2 Non-Functional Requirements

#### NFR-1: Performance

| ID | Requirement | Target |
|----|-------------|--------|
| NFR-1.1 | API response processing time | < 2 seconds |
| NFR-1.2 | Display update time (partial refresh) | < 1 second |
| NFR-1.3 | Display update time (full refresh) | < 3 seconds |
| NFR-1.4 | Boot to first display | < 15 seconds |
| NFR-1.5 | Memory usage | < 80% of available RAM |

#### NFR-2: Reliability

| ID | Requirement | Target |
|----|-------------|--------|
| NFR-2.1 | Uptime | > 99% (< 7.2 hours downtime/month) |
| NFR-2.2 | Graceful API failure handling | Display last known data |
| NFR-2.3 | Watchdog reset on hang | Within 30 seconds |

#### NFR-3: Usability

| ID | Requirement | Target |
|----|-------------|--------|
| NFR-3.1 | Readable from 2 meters distance | Text height > 3mm |
| NFR-3.2 | Readable in direct sunlight | E-Ink reflective display |
| NFR-3.3 | Readable in darkness | External light source (not backlit) |

#### NFR-4: Maintainability

| ID | Requirement | Target |
|----|-------------|--------|
| NFR-4.1 | OTA firmware updates | Planned for v1.1 |
| NFR-4.2 | Configuration via web interface | Planned for v1.1 |
| NFR-4.3 | Debug logging via Serial | 115200 baud |

---

## 6. Technical Architecture Overview

### System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         EXTERNAL SERVICES                        │
├──────────────────────────────┬──────────────────────────────────┤
│   PRIM API (IDFM)            │   OpenWeatherMap API             │
│   Real-time transit data     │   Weather conditions             │
│   SIRI/JSON format           │   JSON format                    │
└──────────────┬───────────────┴──────────────┬───────────────────┘
               │                              │
               │         HTTPS/WiFi           │
               ▼                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                         ESP32 DEVICE                             │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │
│  │  WiFi       │  │  HTTP       │  │  JSON       │              │
│  │  Manager    │  │  Client     │  │  Parser     │              │
│  │  (WiFiMulti)│  │  (HTTPClient│  │  (ArduinoJ) │              │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘              │
│         │                │                │                      │
│         └────────────────┼────────────────┘                      │
│                          ▼                                       │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                    MAIN LOOP                               │  │
│  │  • Poll APIs at configured intervals                       │  │
│  │  • Process responses into data structures                  │  │
│  │  • Trigger display updates                                 │  │
│  │  • Handle button input                                     │  │
│  └───────────────────────────────────────────────────────────┘  │
│                          │                                       │
│                          ▼                                       │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                  DISPLAY MANAGER                           │  │
│  │  • GxEPD2 library for E-Ink control                        │  │
│  │  • Partial/Full refresh strategy                           │  │
│  │  • Layout rendering (weather, departures, banners)         │  │
│  └───────────────────────────────────────────────────────────┘  │
│                          │                                       │
└──────────────────────────┼───────────────────────────────────────┘
                           │ SPI (HSPI)
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│                    4.2" E-INK DISPLAY                            │
│                    GDEY042T81 (400x300)                          │
└─────────────────────────────────────────────────────────────────┘
```

### Module Responsibilities

| Module | File(s) | Responsibility |
|--------|---------|----------------|
| **Main Controller** | `main.cpp` | Application lifecycle, WiFi management, timing, main loop |
| **Transport API** | `transport_api.cpp/.h` | Fetch and parse PRIM API data |
| **Weather API** | `weather_api.cpp/.h` | Fetch and parse OpenWeatherMap data |
| **Display Manager** | `display.cpp/.h` | E-Ink rendering, layout management |
| **Utilities** | `misc.cpp/.h` | Date/time parsing, helper functions |
| **Configuration** | `config.h` | API keys, WiFi credentials, station config |
| **Types** | `types.h` | Data structure definitions |
| **Weather Icons** | `weathericons.h` | Bitmap icon data in PROGMEM |

---

## 7. Hardware Requirements

### Bill of Materials (BOM)

| Component | Specification | Quantity | Notes |
|-----------|---------------|----------|-------|
| ESP32 Dev Board | ESP32-WROOM-32D | 1 | 4MB Flash, WiFi+BT |
| E-Ink Display | GDEY042T81 | 1 | 4.2", 400x300, B&W, SSD1683 driver |
| Push Button | Momentary SPST | 1 | Any tactile switch |
| Jumper Wires | Male-to-Female | 10+ | For prototyping |
| USB Cable | Micro-USB | 1 | Power and programming |
| Power Supply | 5V/1A USB | 1 | Wall adapter |
| Enclosure | 3D printed or project box | 1 | Optional |

### Wiring Diagram

```
ESP32 DevKit              E-Ink Display (GDEY042T81)
─────────────             ─────────────────────────
     GND ◄──────────────► GND
    3.3V ◄──────────────► VCC
 GPIO 14 ◄──────────────► SDA/DIN  (MOSI)
 GPIO 13 ◄──────────────► SCL/CLK  (SCLK)
 GPIO 15 ◄──────────────► CS       (Chip Select)
 GPIO 27 ◄──────────────► DC       (Data/Command)
 GPIO 26 ◄──────────────► RST      (Reset)
 GPIO 25 ◄──────────────► BUSY     (Busy signal)

ESP32 DevKit              Push Button
─────────────             ───────────
 GPIO 12 ◄──────────────► Terminal 1
     GND ◄──────────────► Terminal 2 (diagonal)
```

### Pin Configuration Summary

| Function | GPIO | SPI Signal | Direction |
|----------|------|------------|-----------|
| MOSI (DIN) | 14 | HSPI MOSI | Output |
| SCLK (CLK) | 13 | HSPI CLK | Output |
| CS | 15 | HSPI CS | Output |
| DC | 27 | - | Output |
| RST | 26 | - | Output |
| BUSY | 25 | - | Input |
| Button | 12 | - | Input (Pull-up) |

---

## 8. Software Dependencies

### Platform Configuration

```ini
[env:HoraireEINK]
platform = platformio/espressif32@^6.1.0
board = esp32dev
framework = arduino
```

### Library Dependencies

| Library | Version | Purpose | License |
|---------|---------|---------|---------|
| ArduinoJson | ^7.2.1 | JSON parsing for API responses | MIT |
| ESP32Time | ^2.0.6 | Real-time clock management | MIT |
| GxEPD2 | ^1.6.0 | E-Ink display driver | GPL-3.0 |
| Adafruit GFX | (dep) | Graphics primitives and fonts | BSD |
| Adafruit BusIO | (dep) | SPI/I2C abstraction | MIT |
| WiFi | (builtin) | ESP32 WiFi connectivity | Apache-2.0 |
| HTTPClient | (builtin) | HTTP request handling | Apache-2.0 |

### Build Requirements

- **PlatformIO Core** >= 6.0
- **Python** >= 3.8 (for PlatformIO)
- **USB Driver** for ESP32 (CP2102 or CH340)

---

## 9. API Integrations

### 9.1 PRIM API (Île-de-France Mobilités)

**Purpose**: Real-time departure information for public transit in Île-de-France.

#### Authentication

| Parameter | Value |
|-----------|-------|
| Method | API Key in HTTP Header |
| Header Name | `apiKey` |
| Key Acquisition | https://prim.iledefrance-mobilites.fr/fr/mes-jetons-authentification |

#### Endpoints Used

| Station | Type | MonitoringRef | Filter |
|---------|------|---------------|--------|
| RER C (Ivry) | RER | `STIF:StopArea:SP:46366:` | Platform "2B" |
| Bus 323 Molière | BUS | `STIF:StopPoint:Q:39570:` | Operator `MeC_Bus_PC:Operator::100:` |
| Bus 323 Jules Ferry | BUS | `STIF:StopPoint:Q:39571:` | Operator `MeC_Bus_PC:Operator::100:` |

#### Request Example

```bash
curl --request GET \
  --url 'https://prim.iledefrance-mobilites.fr/marketplace/stop-monitoring?MonitoringRef=STIF:StopArea:SP:46366:' \
  --header 'accept: application/json' \
  --header 'apiKey: YOUR_API_KEY'
```

#### Response Structure (SIRI Format)

```json
{
  "Siri": {
    "ServiceDelivery": {
      "ResponseTimestamp": "2026-01-10T12:00:00.000Z",
      "StopMonitoringDelivery": [{
        "MonitoredStopVisit": [{
          "MonitoredVehicleJourney": {
            "JourneyNote": [{"value": "Versailles Château"}],
            "OperatorRef": {"value": "..."},
            "MonitoredCall": {
              "ExpectedArrivalTime": "2026-01-10T12:05:00.000Z",
              "ArrivalPlatformName": {"value": "2B"}
            }
          }
        }]
      }]
    }
  }
}
```

#### Data Extracted

| Field | Path | Use |
|-------|------|-----|
| Destination | `MonitoredVehicleJourney.JourneyNote[0].value` | Displayed name |
| Arrival Time | `MonitoredCall.ExpectedArrivalTime` | Schedule calculation |
| Platform | `MonitoredCall.ArrivalPlatformName.value` | RER filtering |
| Operator | `OperatorRef.value` | Bus filtering |

### 9.2 OpenWeatherMap API

**Purpose**: Current weather conditions for display on the device.

#### Authentication

| Parameter | Value |
|-----------|-------|
| Method | API Key in URL query parameter |
| Parameter Name | `appid` |
| Key Acquisition | https://openweathermap.org/api |

#### Endpoint

```
GET https://api.openweathermap.org/data/2.5/weather
    ?q=Ivry-sur-Seine
    &appid=YOUR_API_KEY
    &units=metric
```

#### Response Structure

```json
{
  "main": {
    "temp": 15.2,
    "feels_like": 14.1,
    "humidity": 65
  },
  "weather": [{
    "description": "scattered clouds",
    "icon": "03d"
  }],
  "wind": {
    "speed": 3.5
  }
}
```

#### Icon Mapping

| OWM Code | Condition | Local Icon |
|----------|-----------|------------|
| 01d/01n | Clear sky | sunny |
| 02d/02n | Few clouds | partlycloudy |
| 03d/03n | Scattered clouds | mostlycloudy |
| 04d/04n | Broken clouds | cloudy |
| 09d/09n | Shower rain | chancerain |
| 10d/10n | Rain | rain |
| 11d/11n | Thunderstorm | tstorms |
| 13d/13n | Snow | snow |
| 50d/50n | Mist | fog |

---

## 10. User Interface / Display Layout

### Display Specifications

| Property | Value |
|----------|-------|
| Resolution | 400 x 300 pixels |
| Orientation | Landscape (rotated 270°) |
| Color Depth | 1-bit (Black & White) |
| Technology | E-Ink (electrophoretic) |

### Layout Diagram

```
┌──────────────────────────────────────────────────────────────┐
│ ☀️ 15.2°C, partlycloudy, 65% hum                             │ 22px
├──────────────────────────────────────────────────────────────┤
│                                                              │
│                          RER C                               │ 44px (banner)
│                                                              │
├──────────────────────────────────────────────────────────────┤
│ 12:05    Versailles-Château                           3min  │
├──────────────────────────────────────────────────────────────┤
│ 12:12    Pontoise                                    10min  │
├──────────────────────────────────────────────────────────────┤ ~84px
│ 12:18    St-Quentin-en-Yvelines                      16min  │ (5 rows)
├──────────────────────────────────────────────────────────────┤
│ 12:25    Versailles-Château                          23min  │
├──────────────────────────────────────────────────────────────┤
│ 12:32    Pontoise                                    30min  │
├─────────────────────────────┬────────────────────────────────┤
│       323 Molière           │        323 Jules F             │ 44px
├─────────────────────────────┼────────────────────────────────┤
│ 12:08                 2min  │ 12:15                    9min  │
├─────────────────────────────┼────────────────────────────────┤
│ 12:20                14min  │ 12:30                   24min  │
├─────────────────────────────┼────────────────────────────────┤ ~84px
│ 12:32                26min  │ 12:45                   39min  │
├─────────────────────────────┼────────────────────────────────┤
│ 12:44                38min  │ 13:00                   54min  │
├─────────────────────────────┼────────────────────────────────┤
│ 12:56                50min  │ 13:15                   69min  │
└─────────────────────────────┴────────────────────────────────┘
          200px                          200px
```

### Typography

| Element | Font | Size | Color |
|---------|------|------|-------|
| Weather bar | FreeSansBold | 9pt | Black on White |
| Station banners | FreeSansBold | 12pt | White on Black |
| Departure times | FreeSansBold | 9pt | Black on White |
| Destination names | FreeSansBold | 9pt | Black on White |
| Countdown (min) | FreeSansBold | 9pt | Black on White |

### Refresh Strategy

| Trigger | Refresh Type | Duration | Purpose |
|---------|--------------|----------|---------|
| Every 30s | Partial | ~0.5s | Update departure data |
| Every 5min | Full | ~2s | Clear E-Ink ghosting |
| On WiFi reconnect | Full | ~2s | Re-initialize display |

---

## 11. Data Flow

### Sequence Diagram

```
┌─────┐     ┌─────────┐     ┌─────────┐     ┌─────────┐     ┌─────────┐
│User │     │  ESP32  │     │WiFi/Net │     │PRIM API │     │ OWM API │
└──┬──┘     └────┬────┘     └────┬────┘     └────┬────┘     └────┬────┘
   │             │               │               │               │
   │ Power On    │               │               │               │
   │────────────>│               │               │               │
   │             │               │               │               │
   │             │ Connect WiFi  │               │               │
   │             │──────────────>│               │               │
   │             │               │               │               │
   │             │ WiFi Connected│               │               │
   │             │<──────────────│               │               │
   │             │               │               │               │
   │             │ GET /stop-monitoring (x3 stations)            │
   │             │──────────────────────────────>│               │
   │             │               │               │               │
   │             │ JSON Response (SIRI)          │               │
   │             │<──────────────────────────────│               │
   │             │               │               │               │
   │             │ GET /weather                  │               │
   │             │──────────────────────────────────────────────>│
   │             │               │               │               │
   │             │ JSON Response                 │               │
   │             │<──────────────────────────────────────────────│
   │             │               │               │               │
   │             │ Parse JSON    │               │               │
   │             │──────┐        │               │               │
   │             │      │        │               │               │
   │             │<─────┘        │               │               │
   │             │               │               │               │
   │             │ Render to E-Ink               │               │
   │             │──────┐        │               │               │
   │ Display     │      │        │               │               │
   │ Updated     │<─────┘        │               │               │
   │<────────────│               │               │               │
   │             │               │               │               │
   │             │ Wait 30s      │               │               │
   │             │──────┐        │               │               │
   │             │      │        │               │               │
   │             │<─────┘        │               │               │
   │             │               │               │               │
   │             │ [Loop: Repeat data fetch]     │               │
```

### State Transitions

```
                    ┌─────────────────┐
                    │                 │
                    ▼                 │
┌─────────┐    ┌─────────┐    ┌─────────────┐
│  BOOT   │───>│WIFI_CONN│───>│ DATA_FETCH  │
└─────────┘    └─────────┘    └─────────────┘
                    │                 │
                    │ Fail            │ Success
                    ▼                 ▼
              ┌─────────┐    ┌─────────────┐
              │  RETRY  │    │  DISPLAY    │
              └─────────┘    └─────────────┘
                    │                 │
                    │ Success         │ 30s timeout
                    └────────────────>│
                                      │
                                      ▼
                              ┌─────────────┐
                              │ DATA_FETCH  │ (loop)
                              └─────────────┘
```

---

## 12. Configuration Requirements

### config.h Structure

```cpp
#ifndef CONFIG_H
#define CONFIG_H

// ===========================================
// API CREDENTIALS (REQUIRED)
// ===========================================

// PRIM API (Île-de-France Mobilités)
// Get your key: https://prim.iledefrance-mobilites.fr/fr/mes-jetons-authentification
#define API_TOKEN "your_prim_api_key_here"

// OpenWeatherMap API
// Get your key: https://openweathermap.org/api
#define WEATHER_API_KEY "your_openweathermap_key_here"

// ===========================================
// WIFI CONFIGURATION (REQUIRED)
// ===========================================

const std::map<std::string, std::string> WIFI_CREDENTIALS = {
    {"Your_SSID", "Your_Password"},
    {"Backup_SSID", "Backup_Password"}  // Optional backup network
};

// ===========================================
// STATION CONFIGURATION
// ===========================================

// Station endpoints (MonitoringRef IDs)
const std::vector<std::pair<std::string, std::string>> STATION_ENDPOINTS = {
    {"RER C", "https://prim.iledefrance-mobilites.fr/marketplace/stop-monitoring?MonitoringRef=STIF:StopArea:SP:46366:"},
    {"323 Moliere", "https://prim.iledefrance-mobilites.fr/marketplace/stop-monitoring?MonitoringRef=STIF:StopPoint:Q:39570:"},
    {"323 Jules Ferry", "https://prim.iledefrance-mobilites.fr/marketplace/stop-monitoring?MonitoringRef=STIF:StopPoint:Q:39571:"}
};

// Station types for parsing logic
const std::map<std::string, std::string> STATION_TYPES = {
    {"RER C", "RER"},
    {"323 Moliere", "BUS"},
    {"323 Jules Ferry", "BUS"}
};

// Station filters (platform for RER, operator for BUS)
const std::map<std::string, std::string> STATION_FILTERS = {
    {"RER C", "2B"},
    {"323 Moliere", "MeC_Bus_PC:Operator::100:"},
    {"323 Jules Ferry", "MeC_Bus_PC:Operator::100:"}
};

// ===========================================
// WEATHER CONFIGURATION
// ===========================================

#define WEATHER_API_URL "https://api.openweathermap.org/data/2.5/weather"
#define WEATHER_CITY "Ivry-sur-Seine"
#define WEATHER_UNITS "metric"

#endif // CONFIG_H
```

### Finding Your StopPoint ID

1. Visit: https://prim.iledefrance-mobilites.fr/fr/jeux-de-donnees/arrets-lignes
2. Download the dataset
3. Search for your station name
4. Use the `stop_id` value as your MonitoringRef

---

## 13. Performance Requirements

### Timing Constraints

| Operation | Maximum Time | Typical Time |
|-----------|--------------|--------------|
| WiFi connection | 15 seconds | 3-5 seconds |
| API request (single) | 10 seconds | 1-2 seconds |
| JSON parsing | 2 seconds | 200-500ms |
| E-Ink partial refresh | 1 second | 0.5 seconds |
| E-Ink full refresh | 4 seconds | 2 seconds |
| Total boot to display | 30 seconds | 10-15 seconds |

### Memory Constraints

| Resource | Available | Target Usage |
|----------|-----------|--------------|
| Flash | 4 MB | < 1.5 MB |
| SRAM | 520 KB | < 400 KB |
| PSRAM | 0 (not equipped) | N/A |

### Network Requirements

| Parameter | Requirement |
|-----------|-------------|
| WiFi standard | 802.11 b/g/n (2.4 GHz) |
| Minimum bandwidth | 100 Kbps |
| Latency tolerance | < 5 seconds |
| Data usage | ~500 KB/day (estimated) |

---

## 14. Success Metrics / KPIs

### User Experience Metrics

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| Time to glance | < 1 second | User feedback |
| Information accuracy | > 99% | Compare to official app |
| User satisfaction | > 4.5/5 | Survey |
| Daily active usage | 100% of installed devices | Logging (future) |

### Technical Metrics

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| Uptime | > 99% | System logging |
| API success rate | > 95% | Error counting |
| Memory stability | No crashes | Watchdog resets |
| Display ghosting | Imperceptible | Visual inspection |

### Business Metrics (for open-source project)

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| GitHub stars | 100+ | GitHub API |
| Forks | 25+ | GitHub API |
| Community builds | 10+ | Issue/Discussion reports |

---

## 15. Future Roadmap / Backlog

### Version 1.1 (Next Release)

| Feature | Description | Priority |
|---------|-------------|----------|
| WiFi Manager | Web-based WiFi configuration | P1 |
| OTA Updates | Over-the-air firmware updates | P1 |
| Web Configuration | Configure stations via web interface | P1 |
| NVS Storage | Persist configuration in flash | P2 |

### Version 1.2

| Feature | Description | Priority |
|---------|-------------|----------|
| Deep Sleep | Sleep between refreshes for battery operation | P2 |
| Multiple Layouts | User-selectable display layouts | P2 |
| Alerts | Visual alert for delays > 15 min | P2 |
| Time Display | Show current time on display | P3 |

### Version 2.0

| Feature | Description | Priority |
|---------|-------------|----------|
| Multi-region Support | Support other SIRI-compatible transit systems | P3 |
| Color E-Ink | Support 3-color displays for alerts | P3 |
| Home Assistant Integration | MQTT publishing for smart home | P3 |
| Touch Interface | Touch screen support | P3 |

### Backlog (Unscheduled)

- [ ] Battery level indicator
- [ ] Brightness/contrast settings
- [ ] Multiple language support
- [ ] API key validation on boot
- [ ] Heartbeat LED
- [ ] Enclosure design files
- [ ] PCB design for production

---

## 16. Risks & Mitigations

### Technical Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| PRIM API changes/deprecation | Medium | High | Abstract API layer, monitor IDFM communications |
| WiFi instability | Medium | Medium | Implement exponential backoff, store last-known data |
| E-Ink burn-in/failure | Low | High | Full refresh every 5 min, component replacement guide |
| Memory overflow | Medium | High | Monitor heap usage, optimize JSON parsing |
| ESP32 obsolescence | Low | Medium | Document compatible alternatives |

### External Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| API rate limiting | Low | Medium | Respect 30s minimum interval, cache responses |
| API authentication changes | Low | High | Document key renewal process |
| WiFi security deprecation | Low | Low | Support WPA3 when available |
| Component supply issues | Medium | Low | Document alternative parts |

### User Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Incorrect wiring | Medium | Medium | Clear documentation, photos, test procedure |
| API key exposure | Medium | Medium | Config file in .gitignore, warnings in docs |
| Timezone confusion | Low | Low | Auto-detect or clear configuration |

---

## 17. Glossary

| Term | Definition |
|------|------------|
| **E-Ink** | Electrophoretic display technology that mimics paper, requiring no power to maintain an image |
| **ESP32** | A low-cost microcontroller with integrated WiFi and Bluetooth, commonly used in IoT projects |
| **GDEY042T81** | Specific E-Ink display model: 4.2 inches, 400x300 pixels, black and white |
| **GPIO** | General Purpose Input/Output - configurable digital pins on a microcontroller |
| **GxEPD2** | Arduino library for driving E-Paper displays from Good Display and Waveshare |
| **HSPI** | Hardware SPI interface on ESP32 (alternative to default VSPI) |
| **IDFM** | Île-de-France Mobilités - the transit authority for the Paris region |
| **Partial Refresh** | E-Ink update method that only changes modified pixels, faster but may cause ghosting |
| **Full Refresh** | E-Ink update method that flashes the entire screen, slower but clears artifacts |
| **PlatformIO** | Development platform for embedded systems, alternative to Arduino IDE |
| **PRIM API** | "Plateforme Régionale d'Information Multimodale" - IDFM's real-time data API |
| **RER** | "Réseau Express Régional" - Paris regional express rail network |
| **SIRI** | Standard Interface for Real-time Information - European transit data standard |
| **SPI** | Serial Peripheral Interface - synchronous communication protocol |
| **StopPoint** | Unique identifier for a transit stop in the IDFM system |

---

## Appendix A: Finding Your Station IDs

### Step-by-Step Guide

1. **Visit the PRIM Portal**: https://prim.iledefrance-mobilites.fr
2. **Navigate to Datasets**: Find "Arrêts et lignes" (Stops and Lines)
3. **Download the CSV**: Contains all stop IDs in the network
4. **Search for your station**: Use the station name to find your stop
5. **Note the ID format**:
   - StopArea: `STIF:StopArea:SP:XXXXX:` (general station area)
   - StopPoint: `STIF:StopPoint:Q:XXXXX:` (specific platform/stop)

### Example Query

```bash
# Test your StopPoint ID
curl -s "https://prim.iledefrance-mobilites.fr/marketplace/stop-monitoring?MonitoringRef=STIF:StopPoint:Q:YOUR_ID:" \
  -H "apiKey: YOUR_API_KEY" | jq '.Siri.ServiceDelivery.StopMonitoringDelivery[0].MonitoredStopVisit | length'
```

---

## Appendix B: Troubleshooting

### Common Issues

| Symptom | Possible Cause | Solution |
|---------|---------------|----------|
| "Aucun train" always displayed | Invalid StopPoint ID | Verify ID in PRIM dataset |
| Display stays white | SPI wiring incorrect | Check pin connections |
| WiFi won't connect | Wrong credentials | Verify SSID/password in config.h |
| "deserializeJson failed" | JSON too large | Increase ArduinoJson capacity |
| Weather shows 0°C | Invalid API key | Verify OpenWeatherMap key |
| Display has ghosting | Too many partial refreshes | Reduce partial refresh count |

### Debug Mode

Enable verbose logging by monitoring Serial at 115200 baud:

```bash
# PlatformIO
pio device monitor -b 115200

# Arduino IDE
# Tools > Serial Monitor > 115200 baud
```

---

*Document maintained by the Prochain Départ development team.*
