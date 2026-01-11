# Button Interaction Specification

## Overview
The physical button provides user interaction with the device for manual refresh and configuration access.

## Hardware

M5Paper S3 has a built-in button handled by M5Unified:
- Access via `M5.BtnA`
- Internal debouncing handled by library
- No manual GPIO configuration required

## Functional Requirements

### FR-B1: Button Debouncing
- Handled automatically by M5Unified
- Call `M5.update()` in each loop iteration to process button state

### FR-B2: Short Press Action
- Trigger immediate data refresh
- Bypass normal 30-second refresh timer
- Sets `lastCallEpoch = 0` to force refresh

### FR-B3: Long Press Action (5+ seconds)
- Detect using `M5.BtnA.pressedFor(5000)`
- Display "Configuration..." message
- Trigger WiFi configuration mode (FR-WM5)
- Reset state on button release

### FR-B4: Press Detection
```cpp
// M5Unified button API
M5.update();  // Must call every loop iteration

// Short press
if (M5.BtnA.wasPressed()) {
    lastCallEpoch = 0;  // Force refresh
}

// Long press (5 seconds)
if (M5.BtnA.pressedFor(5000)) {
    displayLoader("Configuration...");
    // Enter config mode
}

// Button release
if (M5.BtnA.wasReleased()) {
    // Handle release after long press
}
```

## State Machine

```
        +---------------------------------------------+
        |                                             |
        v                                             |
   +---------+    press    +----------+               |
   |  IDLE   |------------>| PRESSED  |               |
   +---------+             +----------+               |
        ^                       |                     |
        |                       |                     |
        |    release < 5s       | held > 5s           |
        |    (short press)      | (long press)        |
        |                       v                     |
        |              +--------------+               |
        +--------------| LONG_PRESSED |---------------+
                       +--------------+
                              |
                              v
                       [Enter Config Mode]
```

## Implementation Status
- [x] Button debouncing via M5Unified
- [x] Short press action: Forces immediate refresh
- [x] Long press detection (5+ seconds)
- [x] Visual feedback: "Configuration..." message
- [x] WiFi Manager AP mode on long press (FR-WM5)

## Acceptance Criteria

- [x] Button debouncing prevents false triggers
- [x] Short press triggers immediate data refresh
- [x] Display updates after manual refresh
- [x] Long press (5s) shows configuration message
- [x] Configuration mode starts WiFi Manager AP

## Priority
P2 - Enhancement (COMPLETE)

## Notes
Long press triggers WiFi Manager AP mode. User can connect to "ProchainDepart-Setup" network and configure WiFi via captive portal.
