#include "display.h"
#include "weathericons.h"
#define USE_SERIAL Serial

// M5Paper S3: 960x540 resolution (landscape)
// Layout: Main content (560px) | Weather column (400px)

// ============================================================
// LAYOUT CONSTANTS
// ============================================================
static const int SCREEN_WIDTH = 960;
static const int SCREEN_HEIGHT = 540;
static const int WEATHER_COL_WIDTH = 400;
static const int MAIN_WIDTH = SCREEN_WIDTH - WEATHER_COL_WIDTH;  // 560px

static const int TOP_MARGIN = 0;        // No margin - banner extends to screen edge
static const int BANNER_HEIGHT = 42;    // Banner height for section titles
static const int STATUS_BAR_HEIGHT = 42; // Status bar matches banner height
static const int PADDING = 12;
static const int RER_ROWS = 5;
static const int BUS_ROWS = 3;

// ============================================================
// PARTIAL REFRESH TRACKING
// ============================================================
static bool firstDraw = true;
static unsigned long lastFullRefreshMs = 0;
static const unsigned long FULL_REFRESH_INTERVAL_MS = 1800000;  // 30 minutes

// Previous data for change detection
static String prevRerSchedules[RER_ROWS];
static String prevRerDelays[RER_ROWS];
static String prevBus1Schedules[BUS_ROWS];
static String prevBus1Delays[BUS_ROWS];
static String prevBus2Schedules[BUS_ROWS];
static String prevBus2Delays[BUS_ROWS];
static float prevTemperature = -999;
static int prevHumidity = -1;

// ============================================================
// HELPER: Draw 2-bit PROGMEM icon with full 4-level grayscale
// ============================================================
void draw2BitIcon(int x, int y, const char* iconData, int width, int height, uint16_t color) {
    (void)color;  // Unused in grayscale mode
    const int HEADER_SIZE = 6;
    int paddedWidth = ((width + 3) / 4) * 4;
    int bytesPerRow = paddedWidth / 4;
    
    // 4-level grayscale palette (0=white, 1=light gray, 2=dark gray, 3=black)
    // M5GFX uses RGB565, these are grayscale equivalents
    const uint16_t grayscale[4] = {
        0xFFFF,  // 0: White (background)
        0xC618,  // 1: Light gray (~75% white)
        0x7BEF,  // 2: Dark gray (~50% gray)
        0x0000   // 3: Black
    };
    
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            int byteIndex = HEADER_SIZE + (row * bytesPerRow) + (col / 4);
            int pixelInByte = 3 - (col % 4);
            int shift = pixelInByte * 2;
            
            uint8_t byteVal = pgm_read_byte(iconData + byteIndex);
            uint8_t pixelVal = (byteVal >> shift) & 0x03;
            
            // Draw all non-white pixels with appropriate grayscale
            if (pixelVal > 0) {
                M5.Display.drawPixel(x + col, y + row, grayscale[pixelVal]);
            }
        }
    }
}

// ============================================================
// HELPER: Clear a specific region
// ============================================================
static void clearRegion(int x, int y, int w, int h) {
    M5.Display.fillRect(x, y, w, h, TFT_WHITE);
}

// ============================================================
// HELPER: Get text Y for vertical centering in a box
// For GFX fonts: cursor Y = baseline position
// Position baseline near bottom of box, leaving small margin
// ============================================================
static int getCenteredTextY(int boxY, int boxH, int fontH) {
    // Position baseline at box bottom minus small margin
    // Text renders UPWARD from baseline, so this keeps text inside box
    (void)fontH;  // Not used in simplified formula
    return boxY + boxH - 8;  // 8px margin from bottom
}

// ============================================================
// HELPER: Draw degree symbol manually (FreeSans fonts lack char 176)
// Draws a small circle at the current cursor position
// ============================================================
static void drawDegreeSymbol(int x, int y, int fontSize) {
    // Scale circle size based on font - larger fonts get larger circles
    int radius = (fontSize >= 12) ? 3 : 2;
    // Position circle at top-right of the baseline (superscript position)
    // For GFX fonts, y is baseline, so move up by ~70% of font height
    int cx = x + radius + 1;
    int cy = y - (fontSize * 7 / 10) + radius;
    M5.Display.drawCircle(cx, cy, radius, TFT_BLACK);
}

// ============================================================
// HELPER: Draw banner with black background - uses textdatum for centering
// ============================================================
static void drawBanner(int x, int y, int w, int h, const char* text) {
    M5.Display.fillRect(x, y, w, h, TFT_BLACK);
    M5.Display.setFont(&FreeSansBold12pt7b);
    M5.Display.setTextColor(TFT_WHITE);
    
    // Use M5GFX text datum for automatic centering
    M5.Display.setTextDatum(textdatum_t::middle_center);
    M5.Display.drawString(text, x + w / 2, y + h / 2);
    
    // Reset to default datum
    M5.Display.setTextDatum(textdatum_t::top_left);
}

// ============================================================
// HELPER: Draw battery icon with level indicator
// ============================================================
static void drawBatteryIcon(int x, int y, int level, bool charging) {
    // Battery body: 30x16 pixels (larger for better visibility)
    const int bw = 30;
    const int bh = 16;
    const int nipple = 4;  // Battery tip width
    
    bool lowBattery = (level < 20);
    
    // Draw battery outline (thicker for low battery warning)
    M5.Display.drawRect(x, y, bw, bh, TFT_BLACK);
    if (lowBattery) {
        // Double outline for emphasis
        M5.Display.drawRect(x - 1, y - 1, bw + 2, bh + 2, TFT_BLACK);
    }
    // Draw battery tip (positive terminal)
    M5.Display.fillRect(x + bw, y + 4, nipple, bh - 8, TFT_BLACK);
    
    // Calculate fill level (inner area is bw-4 x bh-4)
    int innerW = bw - 4;
    int innerH = bh - 4;
    int fillW = (innerW * level) / 100;
    
    // Fill color based on level (grayscale for e-ink)
    uint16_t fillColor = TFT_BLACK;
    if (lowBattery) {
        // Low battery - use pattern fill for visibility
        fillColor = 0x7BEF;  // Dark gray
    }
    
    // Draw fill level
    if (fillW > 0) {
        M5.Display.fillRect(x + 2, y + 2, fillW, innerH, fillColor);
    }
    
    // Low battery: draw exclamation mark to the left
    if (lowBattery && !charging) {
        int exX = x - 10;
        int exY = y + 2;
        M5.Display.fillRect(exX, exY, 2, 5, TFT_BLACK);      // Line
        M5.Display.fillRect(exX, exY + 6, 2, 2, TFT_BLACK);  // Dot
    }
    
    // If charging, draw a lightning bolt
    if (charging) {
        // Simple lightning bolt in center of battery
        int cx = x + bw/2;
        int cy = y + bh/2;
        M5.Display.drawLine(cx + 2, cy - 4, cx - 1, cy, TFT_WHITE);
        M5.Display.drawLine(cx - 1, cy, cx + 2, cy, TFT_WHITE);
        M5.Display.drawLine(cx + 2, cy, cx - 1, cy + 4, TFT_WHITE);
    }
}

// ============================================================
// HELPER: Draw WiFi signal strength icon (4-bar indicator)
// ============================================================
static void drawWifiIcon(int x, int y, int rssi) {
    // RSSI ranges: >-50 (excellent), -50 to -60 (good), -60 to -70 (fair), <-70 (weak)
    int bars = 0;
    if (rssi > -50) bars = 4;
    else if (rssi > -60) bars = 3;
    else if (rssi > -70) bars = 2;
    else if (rssi > -80) bars = 1;
    
    // Draw 4 bars, filled based on signal strength
    const int barWidth = 3;
    const int barGap = 2;
    const int maxHeight = 12;
    
    for (int i = 0; i < 4; i++) {
        int barHeight = (i + 1) * 3;  // Heights: 3, 6, 9, 12
        int barX = x + i * (barWidth + barGap);
        int barY = y + (maxHeight - barHeight);
        
        if (i < bars) {
            M5.Display.fillRect(barX, barY, barWidth, barHeight, TFT_BLACK);
        } else {
            M5.Display.drawRect(barX, barY, barWidth, barHeight, 0xC618);  // Light gray outline
        }
    }
}

// ============================================================
// DRAW: Status bar (time + battery + data freshness) in weather column
// ============================================================
static void drawStatusBar(int x, int y, int w, int h, const char* timeStr, int batteryLevel, bool isCharging, int dataAgeSecs, int wifiRssi) {
    // Clear status bar area
    clearRegion(x, y, w, h);
    
    // Draw bottom border
    M5.Display.drawFastHLine(x, y + h - 1, w, TFT_BLACK);
    
    int centerY = y + h / 2;
    
    // Time display (left side) - use larger font
    M5.Display.setFont(&FreeSansBold12pt7b);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setTextDatum(textdatum_t::middle_left);
    M5.Display.drawString(timeStr, x + PADDING, centerY);
    
    // WiFi icon (after time)
    int timeW = M5.Display.textWidth(timeStr);
    int wifiX = x + PADDING + timeW + 8;
    int wifiY = y + (h - 12) / 2;  // 12px icon height
    drawWifiIcon(wifiX, wifiY, wifiRssi);
    
    // Battery percentage text + icon (right side)
    const int batteryW = 30;   // Larger battery icon
    const int batteryH = 16;
    const int nippleW = 4;
    const int textIconGap = 6;
    
    // Format battery percentage text
    char battText[8];
    snprintf(battText, sizeof(battText), "%d%%", batteryLevel);
    
    M5.Display.setFont(&FreeSansBold12pt7b);  // Same size as clock
    int battTextW = M5.Display.textWidth(battText);
    
    // Calculate positions from right edge
    int battIconX = x + w - batteryW - nippleW - PADDING;
    int battTextX = battIconX - textIconGap - battTextW;
    int battY = y + (h - batteryH) / 2;
    
    // Draw battery percentage text (right-aligned to its position)
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setTextDatum(textdatum_t::middle_right);
    M5.Display.drawString(battText, battIconX - textIconGap, centerY);
    
    // Draw battery icon
    drawBatteryIcon(battIconX, battY, batteryLevel, isCharging);
    
    // Data freshness between WiFi and battery text
    M5.Display.setFont(&FreeSansBold12pt7b);  // Same size as clock
    char freshStr[16];
    if (dataAgeSecs < 60) {
        snprintf(freshStr, sizeof(freshStr), "%ds", dataAgeSecs);
    } else {
        snprintf(freshStr, sizeof(freshStr), "%dm", dataAgeSecs / 60);
    }
    
    // Show warning if data is stale (>2 min)
    if (dataAgeSecs > 120) {
        M5.Display.setTextColor(0x7BEF);  // Gray for stale
    } else {
        M5.Display.setTextColor(TFT_BLACK);
    }
    
    // Position between WiFi icon end and battery text start
    int wifiEnd = wifiX + 20;  // WiFi icon is ~20px wide
    int freshX = wifiEnd + (battTextX - wifiEnd) / 2;
    
    M5.Display.setTextDatum(textdatum_t::middle_center);
    M5.Display.drawString(freshStr, freshX, centerY);
    
    // Reset
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setTextDatum(textdatum_t::top_left);
}

// ============================================================
// DRAW: Weather column (right side) with status bar
// ============================================================
static void drawWeatherColumn(weatherType weather, bool forceRedraw, const char* timeStr, int batteryLevel, bool isCharging, int dataAgeSecs, int wifiRssi) {
    int x = MAIN_WIDTH;
    int y = TOP_MARGIN;  // Start below top margin to match RER board
    int w = WEATHER_COL_WIDTH;
    int h = SCREEN_HEIGHT - TOP_MARGIN;
    
    // Check if weather changed
    bool changed = forceRedraw || 
                   (abs(weather.temperature - prevTemperature) > 0.1) ||
                   (weather.humidity != prevHumidity);
    
    if (!changed) return;
    
    // Update cache
    prevTemperature = weather.temperature;
    prevHumidity = weather.humidity;
    
    // Clear region
    clearRegion(x, y, w, h);
    
    // Draw border
    M5.Display.drawRect(x, y, w, h, TFT_BLACK);
    
    // Draw status bar at top (time + battery + data freshness + wifi)
    drawStatusBar(x, y, w, STATUS_BAR_HEIGHT, timeStr, batteryLevel, isCharging, dataAgeSecs, wifiRssi);
    
    // Adjust content Y to start below status bar
    int contentY = y + STATUS_BAR_HEIGHT;
    int contentH = h - STATUS_BAR_HEIGHT;
    
    // Weather icon (centered, 100x100 icon with 4-level grayscale)
    // Use 100x100 icons for better visibility on 960x540 screen
    const char* iconData = getMeteoconIconFromProgmem(weather.icon);
    int iconSize = 100;
    
    // Fall back to mini (50x50) if 100x100 not found
    if (iconData == nullptr) {
        iconData = getMiniMeteoconIconFromProgmem(weather.icon);
        iconSize = 50;
    }
    
    int iconY = contentY + 15;
    if (iconData != nullptr) {
        int iconX = x + (w - iconSize) / 2;
        draw2BitIcon(iconX, iconY, iconData, iconSize, iconSize, TFT_BLACK);
    }
    
    // Calculate vertical spacing for text elements
    // Available space: from after icon to bottom (minus city name area)
    int textStartY = iconY + iconSize + 25;
    int textEndY = h - 50;  // Reserve space for city name
    int textAreaH = textEndY - textStartY;
    int numTextLines = 5;  // temp, feels, humidity, description, wind
    int lineSpacing = textAreaH / numTextLines;
    
    // Temperature (large - use 18pt for better visibility)
    M5.Display.setFont(&FreeSansBold18pt7b);
    M5.Display.setTextColor(TFT_BLACK);
    
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.1fC", weather.temperature);  // No degree char - draw manually
    // Calculate width including space for degree symbol (circle ~10px wide for 18pt)
    int tempTextW = M5.Display.textWidth(tempStr);
    int degreeW = 10;  // Space for degree circle
    int tempTotalW = tempTextW + degreeW;
    int tempY = textStartY;
    int tempX = x + (w - tempTotalW) / 2;
    M5.Display.setCursor(tempX, tempY);
    // Print number only
    char numStr[16];
    snprintf(numStr, sizeof(numStr), "%.1f", weather.temperature);
    M5.Display.print(numStr);
    // Draw degree symbol after the number
    int afterNumX = tempX + M5.Display.textWidth(numStr);
    drawDegreeSymbol(afterNumX, tempY, 18);
    // Print "C" after the degree symbol
    M5.Display.setCursor(afterNumX + degreeW, tempY);
    M5.Display.print("C");
    
    // Feels like (use 12pt for better readability)
    M5.Display.setFont(&FreeSansBold12pt7b);
    char feelsNumStr[16];
    snprintf(feelsNumStr, sizeof(feelsNumStr), "Ressenti %.1f", weather.feels_like);
    int feelsY = textStartY + lineSpacing;
    // Calculate total width: "Ressenti X.X" + degree + "C"
    int feelsTextW = M5.Display.textWidth(feelsNumStr);
    int feelsDegreeW = 8;  // For 12pt font
    int feelsCW = M5.Display.textWidth("C");
    int feelsTotalW = feelsTextW + feelsDegreeW + feelsCW;
    int feelsX = x + (w - feelsTotalW) / 2;
    M5.Display.setCursor(feelsX, feelsY);
    M5.Display.print(feelsNumStr);
    // Draw degree symbol
    int afterFeelsX = feelsX + feelsTextW;
    drawDegreeSymbol(afterFeelsX, feelsY, 12);
    // Print "C"
    M5.Display.setCursor(afterFeelsX + feelsDegreeW, feelsY);
    M5.Display.print("C");
    
    // Humidity (use 12pt)
    char humStr[16];
    snprintf(humStr, sizeof(humStr), "Humidite %d%%", weather.humidity);
    int humW = M5.Display.textWidth(humStr);
    int humY = textStartY + lineSpacing * 2;
    M5.Display.setCursor(x + (w - humW) / 2, humY);
    M5.Display.print(humStr);
    
    // Description (use 12pt)
    int descY = textStartY + lineSpacing * 3;
    if (weather.description.length() > 0) {
        int descW = M5.Display.textWidth(weather.description.c_str());
        int descX = x + (w - descW) / 2;
        if (descX < x + 5) descX = x + 5;
        M5.Display.setCursor(descX, descY);
        M5.Display.print(weather.description);
    }
    
    // Wind (use 12pt)
    char windStr[24];
    snprintf(windStr, sizeof(windStr), "Vent %.1f m/s", weather.wind_speed);
    int windW = M5.Display.textWidth(windStr);
    int windY = textStartY + lineSpacing * 4;
    M5.Display.setCursor(x + (w - windW) / 2, windY);
    M5.Display.print(windStr);
    
    // City name at bottom (use 12pt)
    M5.Display.setFont(&FreeSansBold12pt7b);
    const char* city = "Ivry-sur-Seine";
    int cityW = M5.Display.textWidth(city);
    int cityY = h - 20;
    M5.Display.setCursor(x + (w - cityW) / 2, cityY);
    M5.Display.print(city);
}

// ============================================================
// DRAW: Single departure row - uses textdatum for vertical centering
// fontSize: 12 for RER (wide), 9 for Bus (narrow columns)
// ============================================================
static void drawDepartureRow(int x, int y, int w, int h, 
                              const char* schedule, const char* name, 
                              const char* delay, bool showName, bool useSmallFont = false) {
    // Clear row interior (keep grid lines)
    M5.Display.fillRect(x + 1, y + 1, w - 2, h - 2, TFT_WHITE);
    
    // Use smaller font for narrow bus columns
    if (useSmallFont) {
        M5.Display.setFont(&FreeSansBold9pt7b);
    } else {
        M5.Display.setFont(&FreeSansBold12pt7b);
    }
    M5.Display.setTextColor(TFT_BLACK);
    
    int centerY = y + h / 2;
    int pad = useSmallFont ? 8 : PADDING;
    
    // Schedule (left aligned, vertically centered)
    M5.Display.setTextDatum(textdatum_t::middle_left);
    M5.Display.drawString(schedule, x + pad, centerY);
    
    // Name (center) - only for RER
    if (showName && name != nullptr && strlen(name) > 0) {
        M5.Display.setTextDatum(textdatum_t::middle_center);
        M5.Display.drawString(name, x + w / 2, centerY);
    }
    
    // Delay (right aligned, vertically centered)
    M5.Display.setTextDatum(textdatum_t::middle_right);
    M5.Display.drawString(delay, x + w - pad, centerY);
    
    // Reset datum
    M5.Display.setTextDatum(textdatum_t::top_left);
}

// ============================================================
// DRAW: RER C board (top section)
// ============================================================
static void drawRerBoard(std::vector<departureType>& departures, bool forceRedraw) {
    int x = 0;
    int y = TOP_MARGIN;  // Start below top margin to avoid bezel cutoff
    int w = MAIN_WIDTH;
    int h = (SCREEN_HEIGHT / 2) - TOP_MARGIN;
    
    int boardY = y + BANNER_HEIGHT;
    int boardH = h - BANNER_HEIGHT;
    int rowH = boardH / RER_ROWS;
    
    if (forceRedraw) {
        clearRegion(x, y, w, h);
        drawBanner(x, y, w, BANNER_HEIGHT, "RER C - Ivry");
        M5.Display.drawRect(x, boardY, w, boardH, TFT_BLACK);
        
        // Draw grid lines
        for (int i = 1; i < RER_ROWS; i++) {
            M5.Display.drawFastHLine(x, boardY + i * rowH, w, TFT_BLACK);
        }
    }
    
    // Draw departures
    if (!departures.empty()) {
        for (size_t i = 0; i < departures.size() && i < RER_ROWS; i++) {
            char delayStr[16];
            snprintf(delayStr, sizeof(delayStr), "%d min", departures[i].delay);
            
            // Check if this row changed
            bool rowChanged = forceRedraw ||
                              (prevRerSchedules[i] != departures[i].schedule) ||
                              (prevRerDelays[i] != String(departures[i].delay));
            
            if (rowChanged) {
                prevRerSchedules[i] = departures[i].schedule;
                prevRerDelays[i] = String(departures[i].delay);
                
                drawDepartureRow(x, boardY + i * rowH, w, rowH,
                                 departures[i].schedule.c_str(),
                                 departures[i].name.c_str(),
                                 delayStr, true);
                
                // Redraw grid line after clearing row
                if (i > 0) {
                    M5.Display.drawFastHLine(x, boardY + i * rowH, w, TFT_BLACK);
                }
            }
        }
    } else if (forceRedraw) {
        M5.Display.setFont(&FreeSansBold12pt7b);
        M5.Display.setTextColor(TFT_BLACK);
        const char* msg = "Aucun train";
        int msgW = M5.Display.textWidth(msg);
        int fontH = M5.Display.fontHeight();
        M5.Display.setCursor(x + (w - msgW) / 2, getCenteredTextY(boardY, boardH, fontH));
        M5.Display.print(msg);
    }
}

// ============================================================
// DRAW: Bus board (bottom left or right)
// ============================================================
static void drawBusBoard(std::vector<departureType>& departures, 
                         const char* title, int boardIndex, bool forceRedraw,
                         String* prevSchedules, String* prevDelays) {
    int halfW = MAIN_WIDTH / 2;
    int x = (boardIndex == 0) ? 0 : halfW;
    int y = SCREEN_HEIGHT / 2;
    int w = halfW;
    int h = SCREEN_HEIGHT / 2;
    
    int boardY = y + BANNER_HEIGHT;
    int boardH = h - BANNER_HEIGHT;
    int rowH = boardH / BUS_ROWS;
    
    if (forceRedraw) {
        clearRegion(x, y, w, h);
        drawBanner(x, y, w, BANNER_HEIGHT, title);
        M5.Display.drawRect(x, boardY, w, boardH, TFT_BLACK);
        
        for (int i = 1; i < BUS_ROWS; i++) {
            M5.Display.drawFastHLine(x, boardY + i * rowH, w, TFT_BLACK);
        }
    }
    
    if (!departures.empty()) {
        for (size_t i = 0; i < departures.size() && i < BUS_ROWS; i++) {
            char delayStr[16];
            snprintf(delayStr, sizeof(delayStr), "%d min", departures[i].delay);
            
            bool rowChanged = forceRedraw ||
                              (prevSchedules[i] != departures[i].schedule) ||
                              (prevDelays[i] != String(departures[i].delay));
            
            if (rowChanged) {
                prevSchedules[i] = departures[i].schedule;
                prevDelays[i] = String(departures[i].delay);
                
                drawDepartureRow(x, boardY + i * rowH, w, rowH,
                                 departures[i].schedule.c_str(),
                                 nullptr, delayStr, false);  // Same font as RER
                
                if (i > 0) {
                    M5.Display.drawFastHLine(x, boardY + i * rowH, w, TFT_BLACK);
                }
            }
        }
    } else if (forceRedraw) {
        // Draw placeholder rows when no data
        M5.Display.setFont(&FreeSansBold12pt7b);  // Same font as departures
        M5.Display.setTextColor(TFT_BLACK);
        M5.Display.setTextDatum(textdatum_t::middle_left);
        for (int i = 0; i < BUS_ROWS; i++) {
            int rowCenterY = boardY + i * rowH + rowH / 2;
            M5.Display.drawString("--:--", x + PADDING, rowCenterY);
        }
        M5.Display.setTextDatum(textdatum_t::top_left);
    }
}

// ============================================================
// MAIN: Draw screen with partial refresh support
// ============================================================
void drawScreen(std::vector<std::vector<departureType>> allDepartures, 
                weatherType weather, bool fullRefresh,
                const char* timeStr, int batteryLevel, bool isCharging, int dataAgeSecs, int wifiRssi)
{
    M5.Display.setRotation(1);  // Landscape
    
    unsigned long now = millis();
    
    // Determine if we need a full refresh
    bool needsFullRefresh = fullRefresh || 
                            firstDraw || 
                            (now - lastFullRefreshMs > FULL_REFRESH_INTERVAL_MS);
    
    if (needsFullRefresh) {
        // Full refresh: clear ghosting
        M5.Display.fillScreen(TFT_WHITE);
        lastFullRefreshMs = now;
        firstDraw = false;
        
        // Reset all previous data to force redraw
        for (int i = 0; i < RER_ROWS; i++) {
            prevRerSchedules[i] = "";
            prevRerDelays[i] = "";
        }
        for (int i = 0; i < BUS_ROWS; i++) {
            prevBus1Schedules[i] = "";
            prevBus1Delays[i] = "";
            prevBus2Schedules[i] = "";
            prevBus2Delays[i] = "";
        }
        prevTemperature = -999;
        prevHumidity = -1;
    }
    
    M5.Display.startWrite();
    
    // Draw RER C (top half of main area)
    std::vector<departureType> rerDepartures;
    if (!allDepartures.empty() && !allDepartures[0].empty()) {
        rerDepartures = allDepartures[0];
    }
    drawRerBoard(rerDepartures, needsFullRefresh);
    
    // Draw Bus 323 Molière (bottom left)
    std::vector<departureType> bus1Departures;
    if (allDepartures.size() > 1 && !allDepartures[1].empty()) {
        bus1Departures = allDepartures[1];
    }
    drawBusBoard(bus1Departures, "323 Moliere", 0, needsFullRefresh,
                 prevBus1Schedules, prevBus1Delays);
    
    // Draw Bus 323 Jules Ferry (bottom right)
    std::vector<departureType> bus2Departures;
    if (allDepartures.size() > 2 && !allDepartures[2].empty()) {
        bus2Departures = allDepartures[2];
    }
    drawBusBoard(bus2Departures, "323 Jules F", 1, needsFullRefresh,
                 prevBus2Schedules, prevBus2Delays);
    
    // Draw weather column with status bar (right side)
    drawWeatherColumn(weather, needsFullRefresh, timeStr, batteryLevel, isCharging, dataAgeSecs, wifiRssi);
    
    // Draw separators
    M5.Display.drawFastVLine(MAIN_WIDTH, TOP_MARGIN, SCREEN_HEIGHT - TOP_MARGIN, TFT_BLACK);
    M5.Display.drawFastHLine(0, SCREEN_HEIGHT / 2, MAIN_WIDTH, TFT_BLACK);
    
    M5.Display.endWrite();
    
    // Update display
    M5.Display.display();
    USE_SERIAL.println(needsFullRefresh ? "Full refresh" : "Partial refresh");
}

// ============================================================
// LOADER: Display loading message (centered on screen)
// ============================================================
void displayLoader(String message)
{
    M5.Display.setRotation(1);
    M5.Display.fillScreen(TFT_WHITE);
    
    M5.Display.setFont(&FreeSansBold12pt7b);
    M5.Display.setTextColor(TFT_BLACK);
    
    // Use textdatum for true center positioning
    M5.Display.setTextDatum(textdatum_t::middle_center);
    M5.Display.drawString(message.c_str(), SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
    M5.Display.setTextDatum(textdatum_t::top_left);  // Reset
    
    M5.Display.display();
}

// ============================================================
// DATA FRESHNESS: Visual indicator (stub for now)
// ============================================================
void displayDataFreshness(bool success, long lastSuccess, long currentEpoch)
{
    USE_SERIAL.print("Data age: ");
    USE_SERIAL.print(currentEpoch - lastSuccess);
    USE_SERIAL.println("s");
}

// ============================================================
// STATUS BAR ONLY: Update just the status bar (time, battery, freshness)
// Used for frequent updates without redrawing departure data
// ============================================================
void updateStatusBarOnly(const char* timeStr, int batteryLevel, bool isCharging, int dataAgeSecs, int wifiRssi) {
    int x = MAIN_WIDTH;
    int y = TOP_MARGIN;
    int w = WEATHER_COL_WIDTH;
    
    M5.Display.startWrite();
    drawStatusBar(x, y, w, STATUS_BAR_HEIGHT, timeStr, batteryLevel, isCharging, dataAgeSecs, wifiRssi);
    M5.Display.endWrite();
    M5.Display.display();
    
    USE_SERIAL.printf("Status bar updated: %s, %d%%, %ds\n", timeStr, batteryLevel, dataAgeSecs);
}

// ============================================================
// BANNER: Standalone banner drawing
// ============================================================
void displayBanner(int posx, int posy, int height, int width, String text)
{
    drawBanner(posx, posy, width, height, text.c_str());
}

// ============================================================
// WEATHER: Standalone weather display (legacy compat)
// ============================================================
void displayWeather(int posx, int posy, int width, int height, weatherType weather) {
    (void)posx; (void)posy; (void)width; (void)height;  // Unused in new layout
    // Legacy function - use default time/battery for standalone calls
    drawWeatherColumn(weather, true, "--:--", 0, false, 0, -100);  // -100 = no WiFi
}
