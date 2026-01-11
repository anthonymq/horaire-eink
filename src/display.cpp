#include "display.h"
#include "weathericons.h"
#define USE_SERIAL Serial

// M5Paper S3: 960x540 resolution (landscape)
// Layout: Main content (760px) | Weather column (200px)

// ============================================================
// LAYOUT CONSTANTS
// ============================================================
static const int SCREEN_WIDTH = 960;
static const int SCREEN_HEIGHT = 540;
static const int WEATHER_COL_WIDTH = 200;
static const int MAIN_WIDTH = SCREEN_WIDTH - WEATHER_COL_WIDTH;  // 760px

static const int BANNER_HEIGHT = 36;   // Fits 12pt font nicely
static const int STATUS_BAR_HEIGHT = 32; // Status bar for time/battery
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
// Formula: boxY + (boxH + ascent) / 2
// Ascent is approximately 70-75% of fontHeight for FreeSans fonts
// ============================================================
static int getCenteredTextY(int boxY, int boxH, int fontH) {
    // For GFX fonts, ascent (height above baseline) is ~70% of total height
    int ascent = (fontH * 70) / 100;
    return boxY + (boxH + ascent) / 2;
}

// ============================================================
// HELPER: Draw banner with black background - FIXED CENTERING
// ============================================================
static void drawBanner(int x, int y, int w, int h, const char* text) {
    M5.Display.fillRect(x, y, w, h, TFT_BLACK);
    M5.Display.setFont(&FreeSansBold12pt7b);
    M5.Display.setTextColor(TFT_WHITE);
    
    int textW = M5.Display.textWidth(text);
    int fontH = M5.Display.fontHeight();
    
    int tx = x + (w - textW) / 2;
    int ty = getCenteredTextY(y, h, fontH);
    
    M5.Display.setCursor(tx, ty);
    M5.Display.print(text);
}

// ============================================================
// HELPER: Draw battery icon with level indicator
// ============================================================
static void drawBatteryIcon(int x, int y, int level, bool charging) {
    // Battery body: 24x12 pixels
    const int bw = 24;
    const int bh = 12;
    const int nipple = 3;  // Battery tip width
    
    // Draw battery outline
    M5.Display.drawRect(x, y, bw, bh, TFT_BLACK);
    // Draw battery tip (positive terminal)
    M5.Display.fillRect(x + bw, y + 3, nipple, bh - 6, TFT_BLACK);
    
    // Calculate fill level (inner area is bw-4 x bh-4)
    int innerW = bw - 4;
    int innerH = bh - 4;
    int fillW = (innerW * level) / 100;
    
    // Fill color based on level (grayscale for e-ink)
    uint16_t fillColor = TFT_BLACK;
    if (level < 20) {
        // Low battery - use lighter gray to indicate warning
        fillColor = 0x7BEF;  // Dark gray
    }
    
    // Draw fill level
    if (fillW > 0) {
        M5.Display.fillRect(x + 2, y + 2, fillW, innerH, fillColor);
    }
    
    // If charging, draw a small lightning bolt indicator
    if (charging) {
        // Simple charging indicator: small dot or mark
        M5.Display.fillCircle(x + bw/2, y + bh/2, 2, 0xC618);  // Light gray circle
    }
}

// ============================================================
// DRAW: Status bar (time + battery) in weather column
// ============================================================
static void drawStatusBar(int x, int y, int w, int h, const char* timeStr, int batteryLevel, bool isCharging) {
    // Clear status bar area
    clearRegion(x, y, w, h);
    
    // Draw bottom border
    M5.Display.drawFastHLine(x, y + h - 1, w, TFT_BLACK);
    
    // Time display (left side) - use larger font
    M5.Display.setFont(&FreeSansBold12pt7b);
    M5.Display.setTextColor(TFT_BLACK);
    
    int fontH = M5.Display.fontHeight();
    int timeY = getCenteredTextY(y, h, fontH);
    
    M5.Display.setCursor(x + PADDING, timeY);
    M5.Display.print(timeStr);
    
    // Battery icon (right side) - battery is 24x12 + 3px nipple
    const int batteryW = 24;
    const int batteryH = 12;
    const int nippleW = 3;
    int battX = x + w - batteryW - nippleW - PADDING;
    int battY = y + (h - batteryH) / 2;
    
    drawBatteryIcon(battX, battY, batteryLevel, isCharging);
    
    // Battery percentage (next to icon) - use smaller font
    M5.Display.setFont(&FreeSans9pt7b);
    int smallFontH = M5.Display.fontHeight();
    int battTextY = getCenteredTextY(y, h, smallFontH);
    
    char battStr[8];
    snprintf(battStr, sizeof(battStr), "%d%%", batteryLevel);
    int battStrW = M5.Display.textWidth(battStr);
    M5.Display.setCursor(battX - battStrW - 6, battTextY);
    M5.Display.print(battStr);
}

// ============================================================
// DRAW: Weather column (right side) with status bar
// ============================================================
static void drawWeatherColumn(weatherType weather, bool forceRedraw, const char* timeStr, int batteryLevel, bool isCharging) {
    int x = MAIN_WIDTH;
    int y = 0;
    int w = WEATHER_COL_WIDTH;
    int h = SCREEN_HEIGHT;
    
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
    
    // Draw status bar at top (time + battery)
    drawStatusBar(x, y, w, STATUS_BAR_HEIGHT, timeStr, batteryLevel, isCharging);
    
    // Adjust content Y to start below status bar
    int contentY = y + STATUS_BAR_HEIGHT;
    int contentH = h - STATUS_BAR_HEIGHT;
    
    // Weather icon (centered, larger 100x100 icon with grayscale)
    // Use smaller icon (50x50) to fit better in 200px column
    const char* iconData = getMiniMeteoconIconFromProgmem(weather.icon);
    int iconSize = 50;
    
    // If mini icon not found, try large icon
    if (iconData == nullptr) {
        iconData = getMeteoconIconFromProgmem(weather.icon);
        iconSize = 100;
    }
    
    int iconY = contentY + 15;
    if (iconData != nullptr) {
        int iconX = x + (w - iconSize) / 2;
        draw2BitIcon(iconX, iconY, iconData, iconSize, iconSize, TFT_BLACK);
    }
    
    // Calculate vertical spacing for text elements
    // Available space: from after icon to bottom (minus city name area)
    int textStartY = iconY + iconSize + 20;
    int textEndY = h - 40;  // Reserve space for city name
    int textAreaH = textEndY - textStartY;
    int numTextLines = 5;  // temp, feels, humidity, description, wind
    int lineSpacing = textAreaH / numTextLines;
    
    // Temperature (large)
    M5.Display.setFont(&FreeSansBold12pt7b);
    M5.Display.setTextColor(TFT_BLACK);
    
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.1f C", weather.temperature);
    int tempW = M5.Display.textWidth(tempStr);
    int tempY = textStartY;
    M5.Display.setCursor(x + (w - tempW) / 2, tempY);
    M5.Display.print(tempStr);
    
    // Feels like
    M5.Display.setFont(&FreeSans9pt7b);
    char feelsStr[24];
    snprintf(feelsStr, sizeof(feelsStr), "Ressenti %.1fC", weather.feels_like);
    int feelsW = M5.Display.textWidth(feelsStr);
    int feelsY = textStartY + lineSpacing;
    M5.Display.setCursor(x + (w - feelsW) / 2, feelsY);
    M5.Display.print(feelsStr);
    
    // Humidity
    char humStr[16];
    snprintf(humStr, sizeof(humStr), "%d%% hum", weather.humidity);
    int humW = M5.Display.textWidth(humStr);
    int humY = textStartY + lineSpacing * 2;
    M5.Display.setCursor(x + (w - humW) / 2, humY);
    M5.Display.print(humStr);
    
    // Description
    int descY = textStartY + lineSpacing * 3;
    if (weather.description.length() > 0) {
        int descW = M5.Display.textWidth(weather.description.c_str());
        int descX = x + (w - descW) / 2;
        if (descX < x + 5) descX = x + 5;
        M5.Display.setCursor(descX, descY);
        M5.Display.print(weather.description);
    }
    
    // Wind
    char windStr[16];
    snprintf(windStr, sizeof(windStr), "%.1f m/s", weather.wind_speed);
    int windW = M5.Display.textWidth(windStr);
    int windY = textStartY + lineSpacing * 4;
    M5.Display.setCursor(x + (w - windW) / 2, windY);
    M5.Display.print(windStr);
    
    // City name at bottom
    M5.Display.setFont(&FreeSansBold9pt7b);
    const char* city = "Ivry-sur-Seine";
    int cityW = M5.Display.textWidth(city);
    int cityY = h - 15;
    M5.Display.setCursor(x + (w - cityW) / 2, cityY);
    M5.Display.print(city);
}

// ============================================================
// DRAW: Single departure row - FIXED VERTICAL CENTERING
// ============================================================
static void drawDepartureRow(int x, int y, int w, int h, 
                              const char* schedule, const char* name, 
                              const char* delay, bool showName) {
    // Clear row interior (keep grid lines)
    M5.Display.fillRect(x + 1, y + 1, w - 2, h - 2, TFT_WHITE);
    
    M5.Display.setFont(&FreeSansBold12pt7b);
    M5.Display.setTextColor(TFT_BLACK);
    
    int fontH = M5.Display.fontHeight();
    int textY = getCenteredTextY(y, h, fontH);
    
    // Schedule (left aligned)
    M5.Display.setCursor(x + PADDING, textY);
    M5.Display.print(schedule);
    
    // Name (center) - only for RER
    if (showName && name != nullptr && strlen(name) > 0) {
        int nameW = M5.Display.textWidth(name);
        M5.Display.setCursor(x + (w - nameW) / 2, textY);
        M5.Display.print(name);
    }
    
    // Delay (right aligned)
    int delayW = M5.Display.textWidth(delay);
    M5.Display.setCursor(x + w - delayW - PADDING, textY);
    M5.Display.print(delay);
}

// ============================================================
// DRAW: RER C board (top section)
// ============================================================
static void drawRerBoard(std::vector<departureType>& departures, bool forceRedraw) {
    int x = 0;
    int y = 0;
    int w = MAIN_WIDTH;
    int h = SCREEN_HEIGHT / 2;
    
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
                                 nullptr, delayStr, false);
                
                if (i > 0) {
                    M5.Display.drawFastHLine(x, boardY + i * rowH, w, TFT_BLACK);
                }
            }
        }
    } else if (forceRedraw) {
        M5.Display.setFont(&FreeSansBold12pt7b);
        M5.Display.setTextColor(TFT_BLACK);
        const char* msg = "Aucun bus";
        int msgW = M5.Display.textWidth(msg);
        int fontH = M5.Display.fontHeight();
        M5.Display.setCursor(x + (w - msgW) / 2, getCenteredTextY(boardY, boardH, fontH));
        M5.Display.print(msg);
    }
}

// ============================================================
// MAIN: Draw screen with partial refresh support
// ============================================================
void drawScreen(std::vector<std::vector<departureType>> allDepartures, 
                weatherType weather, bool fullRefresh,
                const char* timeStr, int batteryLevel, bool isCharging)
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
    drawWeatherColumn(weather, needsFullRefresh, timeStr, batteryLevel, isCharging);
    
    // Draw separators
    M5.Display.drawFastVLine(MAIN_WIDTH, 0, SCREEN_HEIGHT, TFT_BLACK);
    M5.Display.drawFastHLine(0, SCREEN_HEIGHT / 2, MAIN_WIDTH, TFT_BLACK);
    
    M5.Display.endWrite();
    
    // Update display
    M5.Display.display();
    USE_SERIAL.println(needsFullRefresh ? "Full refresh" : "Partial refresh");
}

// ============================================================
// LOADER: Display loading message
// ============================================================
void displayLoader(String message)
{
    M5.Display.setRotation(1);
    M5.Display.fillScreen(TFT_WHITE);
    
    M5.Display.setFont(&FreeSansBold12pt7b);
    M5.Display.setTextColor(TFT_BLACK);
    
    int textW = M5.Display.textWidth(message.c_str());
    int fontH = M5.Display.fontHeight();
    
    int x = (SCREEN_WIDTH - textW) / 2;
    int y = getCenteredTextY(0, SCREEN_HEIGHT, fontH);
    
    M5.Display.startWrite();
    M5.Display.setCursor(x, y);
    M5.Display.print(message);
    M5.Display.endWrite();
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
    drawWeatherColumn(weather, true, "--:--", 0, false);
}
