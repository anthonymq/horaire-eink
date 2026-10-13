#include "display.h"
#include "weathericons.h"
#include "weather_api.h"
#include <config.h>
#include <math.h>
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
// What each row currently shows on the panel; a row is redrawn only when its
// rendered text changes, so M5GFX only refreshes the dirty area.
static String prevRerRows[RER_ROWS];
static String prevBusRows[2][BUS_ROWS];
static weatherType prevWeather;
static bool weatherDrawn = false;

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
    if (dataAgeSecs < 0) {
        snprintf(freshStr, sizeof(freshStr), "--");
    } else if (dataAgeSecs < 60) {
        snprintf(freshStr, sizeof(freshStr), "%ds", dataAgeSecs);
    } else if (dataAgeSecs < 3600) {
        snprintf(freshStr, sizeof(freshStr), "%dm", dataAgeSecs / 60);
    } else {
        snprintf(freshStr, sizeof(freshStr), "%dh", dataAgeSecs / 3600);
    }
    
    // Show warning if data is stale (missed more than one fetch)
    if (dataAgeSecs < 0 || dataAgeSecs > 300) {
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
    
    bool changed = forceRedraw || !weatherDrawn ||
                   fabsf(weather.temperature - prevWeather.temperature) > 0.05f ||
                   fabsf(weather.feels_like - prevWeather.feels_like) > 0.05f ||
                   fabsf(weather.wind_speed - prevWeather.wind_speed) > 0.05f ||
                   weather.humidity != prevWeather.humidity ||
                   weather.icon != prevWeather.icon ||
                   weather.description != prevWeather.description;
    
    if (!changed) {
        // Weather unchanged: only the status bar needs updating
        drawStatusBar(x, y, w, STATUS_BAR_HEIGHT, timeStr, batteryLevel, isCharging, dataAgeSecs, wifiRssi);
        return;
    }
    
    prevWeather = weather;
    weatherDrawn = true;
    
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
    const char* city = getWeatherLocation().name;
    int cityW = M5.Display.textWidth(city);
    int cityY = h - 20;
    M5.Display.setCursor(x + (w - cityW) / 2, cityY);
    M5.Display.print(city);
}

// ============================================================
// DRAW: Single departure row - uses textdatum for vertical centering
// ============================================================
static void drawDepartureRow(int x, int y, int w, int h, 
                              const char* schedule, const char* name, 
                              const char* delay) {
    // Clear row interior (keep grid lines)
    M5.Display.fillRect(x + 1, y + 1, w - 2, h - 2, TFT_WHITE);
    
    M5.Display.setFont(&FreeSansBold12pt7b);
    M5.Display.setTextColor(TFT_BLACK);
    
    int centerY = y + h / 2;
    
    M5.Display.setTextDatum(textdatum_t::middle_left);
    M5.Display.drawString(schedule, x + PADDING, centerY);
    
    if (name != nullptr && name[0] != '\0') {
        M5.Display.setTextDatum(textdatum_t::middle_center);
        M5.Display.drawString(name, x + w / 2, centerY);
    }
    
    if (delay != nullptr && delay[0] != '\0') {
        M5.Display.setTextDatum(textdatum_t::middle_right);
        M5.Display.drawString(delay, x + w - PADDING, centerY);
    }
    
    M5.Display.setTextDatum(textdatum_t::top_left);
}

// ============================================================
// DRAW: Departure board (banner + grid + rows), redrawing only changed rows
// ============================================================
static void drawBoard(int x, int y, int w, int h, int rows, const BoardView& board,
                      bool showNames, const char* emptyText, time_t now,
                      String* prevRows, bool forceRedraw) {
    int boardY = y + BANNER_HEIGHT;
    int boardH = h - BANNER_HEIGHT;
    int rowH = boardH / rows;
    
    if (forceRedraw) {
        clearRegion(x, y, w, h);
        drawBanner(x, y, w, BANNER_HEIGHT, board.title);
        M5.Display.drawRect(x, boardY, w, boardH, TFT_BLACK);
        for (int i = 1; i < rows; i++) {
            M5.Display.drawFastHLine(x, boardY + i * rowH, w, TFT_BLACK);
        }
        for (int i = 0; i < rows; i++) prevRows[i] = "";
    }
    
    const std::vector<departureType>& deps = *board.departures;
    for (int i = 0; i < rows; i++) {
        char schedule[8] = "";
        char delay[16] = "";
        const char* name = nullptr;
        
        if ((size_t)i < deps.size()) {
            struct tm local;
            localtime_r(&deps[i].expected, &local);
            snprintf(schedule, sizeof(schedule), "%02d:%02d", local.tm_hour, local.tm_min);
            long mins = (long)(deps[i].expected - now) / 60;
            if (mins < 0) mins = 0;
            snprintf(delay, sizeof(delay), "%ld min", mins);
            if (showNames) name = deps[i].name.c_str();
        } else if (i == 0 && deps.empty()) {
            snprintf(schedule, sizeof(schedule), "%s", "--:--");
            name = showNames ? emptyText : nullptr;
        }
        
        String key = String(schedule) + '|' + (name ? name : "") + '|' + delay;
        if (key == prevRows[i]) continue;
        prevRows[i] = key;
        
        int rowY = boardY + i * rowH;
        drawDepartureRow(x, rowY, w, rowH, schedule, name, delay);
        if (i > 0) {
            M5.Display.drawFastHLine(x, rowY, w, TFT_BLACK);
        }
    }
}

// ============================================================
// MAIN: Draw screen with partial refresh support
// ============================================================
void drawScreen(const BoardView boards[3], const weatherType& weather, const StatusInfo& status,
                time_t now, bool fullRefresh)
{
    // Full refreshes use the high-quality waveform to clear ghosting; the
    // per-minute updates use the shorter text waveform (less time driving the panel).
    M5.Display.setEpdMode(fullRefresh ? epd_mode_t::epd_quality : epd_mode_t::epd_text);
    
    if (fullRefresh) {
        M5.Display.fillScreen(TFT_WHITE);
    }
    
    M5.Display.startWrite();
    
    const int halfH = SCREEN_HEIGHT / 2;
    const int halfW = MAIN_WIDTH / 2;
    drawBoard(0, TOP_MARGIN, MAIN_WIDTH, halfH - TOP_MARGIN, RER_ROWS, boards[0], true,
              "Aucun train", now, prevRerRows, fullRefresh);
    drawBoard(0, halfH, halfW, halfH, BUS_ROWS, boards[1], false,
              nullptr, now, prevBusRows[0], fullRefresh);
    drawBoard(halfW, halfH, halfW, halfH, BUS_ROWS, boards[2], false,
              nullptr, now, prevBusRows[1], fullRefresh);
    
    drawWeatherColumn(weather, fullRefresh, status.timeStr, status.batteryLevel,
                      status.isCharging, status.dataAgeSecs, status.wifiRssi);
    
    if (fullRefresh) {
        M5.Display.drawFastVLine(MAIN_WIDTH, TOP_MARGIN, SCREEN_HEIGHT - TOP_MARGIN, TFT_BLACK);
        M5.Display.drawFastHLine(0, SCREEN_HEIGHT / 2, MAIN_WIDTH, TFT_BLACK);
    }
    
    M5.Display.endWrite();
    M5.Display.display();
    USE_SERIAL.println(fullRefresh ? "Full refresh" : "Partial refresh");
}

// ============================================================
// LOADER: Display a centered message (full screen)
// Forces the next drawScreen() to be a full refresh.
// ============================================================
void displayLoader(const char* message)
{
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    M5.Display.setRotation(1);
    M5.Display.fillScreen(TFT_WHITE);
    
    M5.Display.setFont(&FreeSansBold12pt7b);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setTextDatum(textdatum_t::middle_center);
    M5.Display.drawString(message, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
    M5.Display.setTextDatum(textdatum_t::top_left);
    
    M5.Display.display();
    weatherDrawn = false;
}

// ============================================================
// SETUP PORTAL SCREEN
// ============================================================
void drawSetupScreen(const char* title, const SetupQr* qrs, int qrCount,
                     const char* const* lines, int lineCount)
{
    const int titleH = 60;
    const int margin = 40;
    const int qrSize = qrCount > 1 ? 250 : 340;
    const int qrY = titleH + 50;
    
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    M5.Display.setRotation(1);
    M5.Display.fillScreen(TFT_WHITE);
    drawBanner(0, 0, SCREEN_WIDTH, titleH, title);
    M5.Display.setTextColor(TFT_BLACK);
    
    int x = margin;
    M5.Display.setFont(&FreeSansBold9pt7b);
    M5.Display.setTextDatum(textdatum_t::top_center);
    for (int i = 0; i < qrCount; i++) {
        M5.Display.qrcode(qrs[i].payload, x, qrY, qrSize);
        M5.Display.drawString(qrs[i].caption, x + qrSize / 2, qrY + qrSize + 12);
        x += qrSize + 30;
    }
    M5.Display.setTextDatum(textdatum_t::top_left);
    
    int textX = x + 10;
    int y = qrY;
    for (int i = 0; i < lineCount; i++) {
        const char* line = lines[i];
        if (line[0] == '\0') {
            y += 14;
            continue;
        }
        bool bold = line[0] == '*';
        M5.Display.setFont(bold ? &FreeSansBold12pt7b : &FreeSans9pt7b);
        M5.Display.drawString(bold ? line + 1 : line, textX, y);
        y += M5.Display.fontHeight() + 8;
    }
    
    M5.Display.display();
    weatherDrawn = false;
}
