# WiFi Manager Specification

## Overview
The WiFi Manager provides a web-based configuration interface for WiFi credentials, eliminating the need to recompile firmware for different networks.

## Current State
- WiFi credentials are hardcoded in `config.h`
- Requires recompilation to change networks
- Multiple networks supported via WiFiMulti

## Target State
- Captive portal for initial configuration
- Web interface for credential management
- Credentials stored in non-volatile storage (NVS/Preferences)
- Fallback to AP mode if connection fails

## Functional Requirements

### FR-WM1: Access Point Mode
- Create WiFi AP when no credentials stored or connection fails
- AP Name: "ProchainDepart-Setup" (or similar)
- No password (open network for easy setup)
- Captive portal redirects to configuration page

### FR-WM2: Configuration Web Interface
Simple HTML form with:
- WiFi SSID input (with network scan option)
- WiFi Password input
- Save button
- Status display

### FR-WM3: Credential Storage
- Store credentials in ESP32 Preferences/NVS
- Persist across reboots
- Support at least 3 saved networks

### FR-WM4: Connection Logic
1. On boot, check for stored credentials
2. If credentials exist, attempt connection
3. If connection fails after timeout (30s), start AP mode
4. If no credentials, start AP mode immediately

### FR-WM5: Runtime Configuration Access
- Button press (GPIO 12) for 5+ seconds triggers AP mode
- Allows reconfiguration without clearing flash

## Dependencies
- WiFiManager library (tzapu/WiFiManager) OR
- Custom implementation with ESPAsyncWebServer

## Implementation Options

### Option A: WiFiManager Library
```cpp
#include <WiFiManager.h>
WiFiManager wm;
wm.autoConnect("ProchainDepart-Setup");
```
Pros: Simple, well-tested
Cons: Large library, may conflict with existing code

### Option B: Custom Implementation
Using ESPAsyncWebServer + Preferences
Pros: Lightweight, full control
Cons: More code to write

## Acceptance Criteria

- [ ] Device creates AP when no WiFi configured
- [ ] Captive portal serves configuration page
- [ ] User can enter SSID and password
- [ ] Credentials are saved to NVS
- [ ] Device connects to saved network on reboot
- [ ] Fallback to AP mode on connection failure
- [ ] Long button press triggers AP mode

## Priority
P2 - Enhancement (listed in README TODOs)

## Notes
This feature was identified in the project README as a TODO item. Implementation should not break existing functionality.
