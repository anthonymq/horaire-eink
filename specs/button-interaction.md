# Button Interaction Specification

## Overview
The physical button provides user interaction with the device for manual refresh and configuration access.

## Hardware
- Momentary push button connected to GPIO 12
- Uses internal pull-up resistor
- Active LOW (pressed = LOW, released = HIGH)

## Functional Requirements

### FR-B1: Button Debouncing
- Track previous button state
- Detect state transitions (LOW → HIGH = release)
- Ignore bouncing/noise

### FR-B2: Short Press Action
- Trigger immediate data refresh
- Bypass normal 30-second refresh timer
- Visual feedback (optional: brief message on display)

### FR-B3: Long Press Action (5+ seconds)
- Trigger WiFi configuration mode
- Display "Configuration..." message
- Start AP mode for WiFi Manager

### FR-B4: Press Detection
```cpp
// Current implementation
int lastButtonState = HIGH;
int buttonState = digitalRead(BUTTON_PIN);
if (lastButtonState == LOW && buttonState == HIGH) {
    // Button was released - trigger action
}
lastButtonState = buttonState;
```

## State Machine

```
        ┌─────────────────────────────────────────┐
        │                                         │
        ▼                                         │
   ┌─────────┐    press    ┌──────────┐          │
   │  IDLE   │────────────>│ PRESSED  │          │
   └─────────┘             └──────────┘          │
        ▲                       │                │
        │                       │                │
        │    release < 5s       │ held > 5s      │
        │    (short press)      │ (long press)   │
        │                       ▼                │
        │              ┌──────────────┐          │
        └──────────────│ LONG_PRESSED │──────────┘
                       └──────────────┘
                              │
                              ▼
                       [Enter Config Mode]
```

## Current Implementation Status
- Basic button reading: Implemented
- State tracking: Implemented
- Short press action: Not implemented (detected but no action)
- Long press detection: Not implemented
- Visual feedback: Not implemented

## Acceptance Criteria

- [ ] Button debouncing prevents false triggers
- [ ] Short press triggers immediate data refresh
- [ ] Display updates after manual refresh
- [ ] Long press (5s) enters configuration mode
- [ ] Configuration mode is clearly indicated on display
- [ ] Button works reliably during normal operation

## Priority
P2 - Enhancement (partially implemented)

## Notes
Current code detects button release but doesn't trigger any action. The log message "The state changed from LOW to HIGH" is printed but no refresh is triggered.
