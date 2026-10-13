# Power Optimization & Loop Logic

This document describes the power-efficient execution flow of the Prochain Départ transit display on M5Paper S3 hardware.

## Optimization Overview

To maximize battery life while maintaining real-time accuracy, several layers of power optimization have been implemented:

1.  **Light Sleep Cycles**: The ESP32-S3 enters light sleep for 30-second intervals between tasks. This preserves RAM and allows for near-instant wake-up while consuming significantly less power than active mode.
2.  **Hardware RTC Tracking**: Time is managed using the hardware RTC. The display only wakes for status bar updates (clock/battery) when a minute boundary is crossed, avoiding unnecessary E-Ink refreshes.
3.  **Strategic Data Fetching**: Polling interval has been increased to 60 seconds (from 30s). WiFi is only enabled during the fetch window and is explicitly disconnected immediately after data acquisition.
4.  **WiFi Power Management**:
    *   **TX Power**: Reduced to **8.5 dBm** to minimize current spikes during transmission.
    *   **Max Modem Sleep**: Enabled to allow the radio to sleep between beacon intervals when connected.
5.  **Smart Refresh**: E-Ink partial refreshes are used for the clock and battery status, with full refreshes only occurring every 30 minutes to prevent ghosting.

## Loop Flow Diagram

```text
       +---------------------------------------+
       |             START LOOP                |
       +---------------------------------------+
                    |
                    v
       +---------------------------------------+
       |         M5.update() (Buttons)         |
       +---------------------------------------+
                    |
                    v
       +---------------------------------------+
       |       Check Button State (A)          |
       |  - Long Press (5s) -> Config Portal   |
       |  - Short Press    -> Force Refresh    |
       +---------------------------------------+
                    |
                    v
       +---------------------------------------+
       |     Data Fetch Needed? (> 60s)        |----[ ❌ NO ]---+
       +---------------------------------------+                |
                    | [ ✅ YES ]                                |
                    v                                           |
       +---------------------------------------+                |
       |         Connect WiFi (Multi)          |                |
       |  (TX Power: 8.5 dBm, Modem Sleep Max) |                |
       +---------------------------------------+                |
                    |                                           |
                    v                                           |
       +---------------------------------------+                |
       |        NTP Sync (if needed)           |                |
       |      Fetch Transit & Weather          |                |
       +---------------------------------------+                |
                    |                                           |
                    v                                           |
       +---------------------------------------+                |
       |        Draw Full Screen (EPD)         |                |
       |        Disconnect WiFi (Power)        |                |
       +---------------------------------------+                |
                    |                                           |
                    +<------------------------------------------+
                    |
                    v
       +---------------------------------------+
       |    Minute Changed? (Hardware RTC)     |----[ ❌ NO ]---+
       +---------------------------------------+                |
                    | [ ✅ YES ]                                |
                    v                                           |
       +---------------------------------------+                |
       |     Update Status Bar (Clock/Bat)     |                |
       |        (Partial EPD Refresh)          |                |
       +---------------------------------------+                |
                    |                                           |
                    +<------------------------------------------+
                    |
                    v
       +---------------------------------------+
       |         Power Save Enabled?           |----[ ❌ NO ]---+
       +---------------------------------------+                |
                    | [ ✅ YES ]                                |
                    v                                           |
       +---------------------------------------+                |
       |     Enter Light Sleep (30s)           |                |
       |   (Wakes on Timer or Button)          |                |
       +---------------------------------------+                |
                    |                                           |
                    +<------------------------------------------+
                    |
                    v
       +---------------------------------------+
       |              END LOOP                 |
       +---------------------------------------+
```

## Key Timing & Thresholds

| Component | Value | Power State |
|-----------|-------|-------------|
| **Light Sleep** | 30s | Sleep (CPU Clock Gated) |
| **Data Polling** | 60s | Active (WiFi ON) |
| **Weather Fetch** | 30m | Active (WiFi ON) |
| **Full Refresh** | 30m | Active (EPD High Current) |
| **Status Update** | 1m | Active (EPD Low Current) |
| **WiFi TX Power**| 8.5 dBm | Capped |

## Implementation Details

### WiFi Optimization
WiFi is managed by `WiFiMulti`. The TX power is capped at 8.5 dBm using `WiFi.setTxPower(WIFI_POWER_8_5dBm)`. To prevent unnecessary drain, `WiFi.disconnect(true)` is called after successful data retrieval, and the mode is set back to `WIFI_OFF`.

### Sleep Management
`M5.Power.lightSleep()` is used to ensure the device remains responsive to the physical button. Sleep is automatically bypassed if the device is connected to USB power (charging) to ensure high responsiveness during development or fixed-power use.
