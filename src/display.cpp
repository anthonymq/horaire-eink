#include "display.h"
#include "weathericons.h"
#include "weather_api.h"
#include "fonts/InterBoldDigits60.h"
#include "fonts/InterBoldDigits36.h"
#include <config.h>
#include <math.h>
#define USE_SERIAL Serial

// M5Paper S3: 960x540 (landscape). The countdowns are what matters from across
// the room, so they get the big type; everything else is secondary.
//
//  +----------------------------------------------------------------------+
//  | 14:32  lundi 13 octobre                  MAJ il y a 2 min  .ill [##] |  header
//  |======================================================================|
//  | RER title                                  | Bus title               |
//  | ------------------------------------------ | ----------------------- |
//  |  4 min   Destination              14:36    |  3 min           14:35  |
//  | 12 min   Destination              14:44    |  ...                    |
//  | 19 min   Destination              14:51    | Bus title               |
//  | 25 min   Destination              14:57    |  ...                    |
//  | 31 min   Destination              15:03    | City                    |
//  |                                            | [icon] 12°  Ressenti... |
//  +----------------------------------------------------------------------+

// ============================================================
// LAYOUT CONSTANTS
// ============================================================
static const int SCREEN_WIDTH = 960;
static const int SCREEN_HEIGHT = 540;
static const int MARGIN = 20;

static const int HEADER_H = 44;
static const int BODY_Y = HEADER_H + 12;
static const int BODY_BOTTOM = SCREEN_HEIGHT - 12;

static const int TITLE_H = 32;          // Card title + rule
static const int CARD_GAP = 14;

// Left: RER board, full height
static const int RER_X = MARGIN;
static const int RER_W = 604;
static const int RER_ROWS = 5;
static const int RER_H = BODY_BOTTOM - BODY_Y;

// Right: two bus boards, then the weather
static const int DIVIDER_X = RER_X + RER_W + 12;
static const int SIDE_X = DIVIDER_X + 12;
static const int SIDE_W = SCREEN_WIDTH - MARGIN - SIDE_X;
static const int BUS_ROWS = 3;
static const int BUS_H = TITLE_H + BUS_ROWS * 46;
static const int BUS1_Y = BODY_Y;
static const int BUS2_Y = BUS1_Y + BUS_H + CARD_GAP;
static const int WEATHER_Y = BUS2_Y + BUS_H + CARD_GAP;
static const int WEATHER_H = BODY_BOTTOM - WEATHER_Y;

// E-ink palette: secondary text stays dark enough to survive the text waveform
static const uint16_t INK = TFT_BLACK;
static const uint16_t INK_SOFT = 0x52AA;   // Dark gray (secondary text)
static const uint16_t INK_FAINT = 0xBDF7;  // Light gray (separators)
static const uint16_t PAPER = TFT_WHITE;

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
// TEXT HELPERS
// ============================================================
// Height of a digit above the baseline: used to center text optically
// (GFX fontHeight() is the line advance, much taller than the glyphs).
static int capHeight(const lgfx::GFXfont* font) {
    return font->glyph['0' - font->first].height;
}

// Baseline that vertically centers `font` digits/capitals in [y, y + h)
static int centeredBaseline(int y, int h, const lgfx::GFXfont* font) {
    return y + (h + capHeight(font)) / 2;
}

static void drawText(const char* text, int x, int baseline, const lgfx::GFXfont* font,
                     uint16_t color, textdatum_t datum) {
    M5.Display.setFont(font);
    M5.Display.setTextColor(color);
    M5.Display.setTextDatum(datum);
    M5.Display.drawString(text, x, baseline);
}

// Copies `text` into `out`, cut with "..." so it fits in maxW with the current font
static void fitText(const char* text, int maxW, char* out, size_t outSize) {
    snprintf(out, outSize, "%s", text);
    if (M5.Display.textWidth(out) <= maxW) return;
    size_t len = strlen(out);
    while (len > 0) {
        len--;
        while (len > 0 && out[len - 1] == ' ') len--;
        if (len + 4 > outSize) continue;
        memcpy(out + len, "...", 4);
        if (M5.Display.textWidth(out) <= maxW) return;
    }
}

// Degree sign as a ring (the GFX fonts are 7-bit ASCII). (x, top) is its top-left.
static void drawDegree(int x, int top, int r, int thickness, uint16_t color) {
    for (int i = 0; i < thickness; i++) {
        M5.Display.drawCircle(x + r, top + r, r - i, color);
    }
}

static void clearRegion(int x, int y, int w, int h) {
    M5.Display.fillRect(x, y, w, h, PAPER);
}

// Card header: bold title over a 2px rule
static void drawCardTitle(int x, int y, int w, const char* title) {
    M5.Display.setFont(&FreeSansBold12pt7b);
    char fitted[48];
    fitText(title, w, fitted, sizeof(fitted));
    drawText(fitted, x, y + 22, &FreeSansBold12pt7b, INK, textdatum_t::baseline_left);
    M5.Display.fillRect(x, y + TITLE_H - 4, w, 2, INK);
}

// ============================================================
// HEADER: clock, date, data freshness, WiFi, battery
// ============================================================
static void drawBatteryIcon(int x, int y, int level, bool charging) {
    const int bw = 30, bh = 16, tip = 3;
    M5.Display.drawRect(x, y, bw, bh, INK);
    M5.Display.drawRect(x + 1, y + 1, bw - 2, bh - 2, INK);
    M5.Display.fillRect(x + bw, y + 4, tip, bh - 8, INK);
    
    int innerW = bw - 6;
    int fillW = innerW * constrain(level, 0, 100) / 100;
    if (fillW > 0) M5.Display.fillRect(x + 3, y + 3, fillW, bh - 6, level < 20 ? INK_SOFT : INK);
    
    if (charging) {
        // Lightning bolt, outlined so it shows on both the fill and the empty part
        int cx = x + bw / 2, cy = y + bh / 2;
        M5.Display.fillTriangle(cx + 3, cy - 6, cx - 4, cy + 1, cx, cy + 1, PAPER);
        M5.Display.fillTriangle(cx - 3, cy + 6, cx + 4, cy - 1, cx, cy - 1, PAPER);
        M5.Display.drawLine(cx + 2, cy - 4, cx - 2, cy, INK);
        M5.Display.drawLine(cx - 2, cy, cx + 2, cy, INK);
        M5.Display.drawLine(cx + 2, cy, cx - 2, cy + 4, INK);
    }
}

// 4 ascending bars, unfilled bars as light outlines
static void drawWifiIcon(int x, int bottom, int rssi) {
    int bars = rssi > -50 ? 4 : rssi > -60 ? 3 : rssi > -70 ? 2 : rssi > -80 ? 1 : 0;
    const int barW = 4, gap = 3;
    for (int i = 0; i < 4; i++) {
        int barH = 5 + i * 4;
        int bx = x + i * (barW + gap);
        if (i < bars) {
            M5.Display.fillRect(bx, bottom - barH, barW, barH, INK);
        } else {
            M5.Display.drawRect(bx, bottom - barH, barW, barH, INK_FAINT);
        }
    }
}

static void drawHeader(const StatusInfo& status, time_t now) {
    static const char* const DAYS[] = {"dimanche", "lundi", "mardi", "mercredi",
                                       "jeudi", "vendredi", "samedi"};
    static const char* const MONTHS[] = {"janvier", "fevrier", "mars", "avril", "mai", "juin",
                                         "juillet", "aout", "septembre", "octobre",
                                         "novembre", "decembre"};
    clearRegion(0, 0, SCREEN_WIDTH, HEADER_H);
    
    const lgfx::GFXfont* text = &FreeSans12pt7b;
    const int baseline = centeredBaseline(0, HEADER_H - 2, &FreeSansBold18pt7b);
    
    // Clock + date
    drawText(status.timeStr, MARGIN, baseline, &FreeSansBold18pt7b, INK, textdatum_t::baseline_left);
    int x = MARGIN + M5.Display.textWidth(status.timeStr) + 14;
    struct tm local;
    localtime_r(&now, &local);
    char dateStr[32];
    snprintf(dateStr, sizeof(dateStr), "%s %d %s", DAYS[local.tm_wday], local.tm_mday,
             MONTHS[local.tm_mon]);
    drawText(dateStr, x, baseline, text, INK_SOFT, textdatum_t::baseline_left);
    
    // Right side, laid out from the right edge: battery %, battery, WiFi, freshness
    int right = SCREEN_WIDTH - MARGIN;
    char battText[8];
    snprintf(battText, sizeof(battText), "%d%%", status.batteryLevel);
    drawText(battText, right, baseline, text, INK_SOFT, textdatum_t::baseline_right);
    right -= M5.Display.textWidth(battText) + 8 + 33;
    drawBatteryIcon(right, baseline - 16, status.batteryLevel, status.isCharging);
    right -= 18 + 25;
    drawWifiIcon(right, baseline, status.wifiRssi);
    right -= 18;
    
    // Fresh data stays quiet (gray); stale data (missed a fetch) is shown in bold black
    char freshStr[24];
    int age = status.dataAgeSecs;
    bool stale = age < 0 || age > 300;
    if (age < 0) {
        snprintf(freshStr, sizeof(freshStr), "Hors ligne");
    } else if (age < 60) {
        snprintf(freshStr, sizeof(freshStr), "A jour");
    } else if (age < 3600) {
        snprintf(freshStr, sizeof(freshStr), "MAJ il y a %d min", age / 60);
    } else {
        snprintf(freshStr, sizeof(freshStr), "MAJ il y a %d h", age / 3600);
    }
    drawText(freshStr, right, baseline, stale ? &FreeSansBold12pt7b : text,
             stale ? INK : INK_SOFT, textdatum_t::baseline_right);
    
    M5.Display.fillRect(MARGIN, HEADER_H - 2, SCREEN_WIDTH - 2 * MARGIN, 2, INK);
}

// ============================================================
// WEATHER (bottom right): city, icon, temperature, details
// ============================================================
static void drawWeather(const weatherType& weather, bool forceRedraw) {
    bool changed = forceRedraw || !weatherDrawn ||
                   fabsf(weather.temperature - prevWeather.temperature) > 0.05f ||
                   fabsf(weather.feels_like - prevWeather.feels_like) > 0.05f ||
                   fabsf(weather.wind_speed - prevWeather.wind_speed) > 0.05f ||
                   weather.humidity != prevWeather.humidity ||
                   weather.icon != prevWeather.icon ||
                   weather.description != prevWeather.description;
    if (!changed) return;
    prevWeather = weather;
    weatherDrawn = true;
    
    clearRegion(SIDE_X, WEATHER_Y, SIDE_W, WEATHER_H);
    drawCardTitle(SIDE_X, WEATHER_Y, SIDE_W, getWeatherLocation().name);
    
    const int y = WEATHER_Y + TITLE_H;
    const int h = WEATHER_H - TITLE_H;
    const char* iconData = getMiniMeteoconIconFromProgmem(weather.icon);
    if (iconData != nullptr) {
        draw2BitIcon(SIDE_X, y + (h - 50) / 2, iconData, 50, 50, INK);
    }
    
    // Temperature
    const lgfx::GFXfont* big = &InterBoldDigits36;
    const int baseline = centeredBaseline(y, h, big);
    char buf[24];
    snprintf(buf, sizeof(buf), "%d", (int)lroundf(weather.temperature));
    int x = SIDE_X + 60;
    drawText(buf, x, baseline, big, INK, textdatum_t::baseline_left);
    x += M5.Display.textWidth(buf) + 3;
    drawDegree(x, baseline - capHeight(big), 5, 3, INK);
    x += 24;
    
    // Feels-like over humidity + wind
    const lgfx::GFXfont* small = &FreeSans9pt7b;
    const int line1 = baseline - capHeight(big) + capHeight(small);
    snprintf(buf, sizeof(buf), "Ressenti %d", (int)lroundf(weather.feels_like));
    drawText(buf, x, line1, small, INK_SOFT, textdatum_t::baseline_left);
    drawDegree(x + M5.Display.textWidth(buf) + 2, line1 - capHeight(small), 2, 1, INK_SOFT);
    snprintf(buf, sizeof(buf), "%d%%  -  %d km/h", weather.humidity,
             (int)lroundf(weather.wind_speed * 3.6f));
    drawText(buf, x, baseline, small, INK_SOFT, textdatum_t::baseline_left);
}

// ============================================================
// DEPARTURE BOARDS
// ============================================================
// Countdown as a big number with a small "min" after it, right-aligned on numRight
// so the numbers line up.
static void drawCountdown(int numRight, int baseline, long mins, const lgfx::GFXfont* numFont,
                          const lgfx::GFXfont* unitFont) {
    char num[8];
    if (mins < 1) {
        snprintf(num, sizeof(num), "<1");
    } else {
        snprintf(num, sizeof(num), "%ld", mins);
    }
    drawText(num, numRight, baseline, numFont, INK, textdatum_t::baseline_right);
    drawText("min", numRight + 8, baseline, unitFont, INK_SOFT, textdatum_t::baseline_left);
}

static void drawClockTime(int right, int baseline, time_t t) {
    struct tm local;
    localtime_r(&t, &local);
    char hhmm[8];
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", local.tm_hour, local.tm_min);
    drawText(hhmm, right, baseline, &FreeSans12pt7b, INK_SOFT, textdatum_t::baseline_right);
}

static void drawEmptyRow(int x, int y, int h, const char* text) {
    if (text) {
        drawText(text, x, centeredBaseline(y, h, &FreeSans12pt7b), &FreeSans12pt7b,
                 INK_SOFT, textdatum_t::baseline_left);
    }
}

// RER row:  [ 12 min ]  Destination ................  14:44
static void drawTrainRow(int x, int y, int w, int h, const departureType* dep, time_t now,
                         const char* emptyText) {
    clearRegion(x, y + 1, w, h - 1);
    if (dep == nullptr) {
        drawEmptyRow(x, y, h, emptyText);
        return;
    }
    
    const int baseline = centeredBaseline(y, h, &InterBoldDigits60);
    drawCountdown(x + 118, baseline, (long)(dep->expected - now) / 60, &InterBoldDigits60,
                  &FreeSans12pt7b);
    
    // Destination: large if it fits, otherwise smaller, truncated as a last resort
    const int nameX = x + 186;
    const int nameW = w - (nameX - x) - 72 - 12;  // Leave room for the clock time
    const lgfx::GFXfont* nameFont = &FreeSansBold18pt7b;
    M5.Display.setFont(nameFont);
    if (M5.Display.textWidth(dep->name.c_str()) > nameW) {
        nameFont = &FreeSansBold12pt7b;
        M5.Display.setFont(nameFont);
    }
    char name[48];
    fitText(dep->name.c_str(), nameW, name, sizeof(name));
    drawText(name, nameX, baseline, nameFont, INK, textdatum_t::baseline_left);
    
    drawClockTime(x + w, baseline, dep->expected);
}

// Bus row:  [ 7 min ]  ............  14:39
static void drawBusRow(int x, int y, int w, int h, const departureType* dep, time_t now,
                       const char* emptyText) {
    clearRegion(x, y + 1, w, h - 1);
    if (dep == nullptr) {
        drawEmptyRow(x, y, h, emptyText);
        return;
    }
    
    const int baseline = centeredBaseline(y, h, &InterBoldDigits36);
    drawCountdown(x + 72, baseline, (long)(dep->expected - now) / 60, &InterBoldDigits36,
                  &FreeSans9pt7b);
    drawClockTime(x + w, baseline, dep->expected);
}

// Title, then rows separated by light rules. Only rows whose content changed are redrawn.
static void drawBoard(int x, int y, int w, int h, int rows, const BoardView& board, bool isTrain,
                      time_t now, String* prevRows, bool forceRedraw) {
    const int rowsY = y + TITLE_H;
    const int rowH = (h - TITLE_H) / rows;
    const char* emptyText = isTrain ? "Aucun train annonce" : "Aucun passage annonce";
    
    if (forceRedraw) {
        clearRegion(x, y, w, h);
        drawCardTitle(x, y, w, board.title);
        for (int i = 1; i < rows; i++) {
            M5.Display.drawFastHLine(x, rowsY + i * rowH, w, INK_FAINT);
        }
        for (int i = 0; i < rows; i++) prevRows[i] = "";
    }
    
    const std::vector<departureType>& deps = *board.departures;
    for (int i = 0; i < rows; i++) {
        const departureType* dep = (size_t)i < deps.size() ? &deps[i] : nullptr;
        
        String key;
        if (dep) {
            long mins = (long)(dep->expected - now) / 60;
            key = String((long)dep->expected) + '|' + String(mins < 1 ? 0 : mins) + '|' + dep->name;
        } else if (i == 0) {
            key = "empty";
        }
        if (key == prevRows[i]) continue;
        prevRows[i] = key;
        
        const char* empty = i == 0 ? emptyText : nullptr;
        int rowY = rowsY + i * rowH;
        if (isTrain) {
            drawTrainRow(x, rowY, w, rowH, dep, now, empty);
        } else {
            drawBusRow(x, rowY, w, rowH, dep, now, empty);
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
        M5.Display.fillScreen(PAPER);
    }
    
    M5.Display.startWrite();
    
    drawHeader(status, now);
    
    drawBoard(RER_X, BODY_Y, RER_W, RER_H, RER_ROWS, boards[0], true,
              now, prevRerRows, fullRefresh);
    drawBoard(SIDE_X, BUS1_Y, SIDE_W, BUS_H, BUS_ROWS, boards[1], false,
              now, prevBusRows[0], fullRefresh);
    drawBoard(SIDE_X, BUS2_Y, SIDE_W, BUS_H, BUS_ROWS, boards[2], false,
              now, prevBusRows[1], fullRefresh);
    
    drawWeather(weather, fullRefresh);
    
    if (fullRefresh) {
        M5.Display.drawFastVLine(DIVIDER_X, BODY_Y, BODY_BOTTOM - BODY_Y, INK_FAINT);
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
    M5.Display.fillScreen(PAPER);

    M5.Display.setFont(&FreeSansBold12pt7b);
    M5.Display.setTextColor(INK);
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
    const int qrSize = qrCount > 1 ? 250 : 340;
    const int qrY = HEADER_H + 40;

    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    M5.Display.setRotation(1);
    M5.Display.fillScreen(PAPER);

    drawText(title, MARGIN, centeredBaseline(0, HEADER_H - 2, &FreeSansBold18pt7b),
             &FreeSansBold18pt7b, INK, textdatum_t::baseline_left);
    M5.Display.fillRect(MARGIN, HEADER_H - 2, SCREEN_WIDTH - 2 * MARGIN, 2, INK);

    int x = MARGIN + 20;
    M5.Display.setFont(&FreeSans9pt7b);
    M5.Display.setTextColor(INK_SOFT);
    M5.Display.setTextDatum(textdatum_t::top_center);
    for (int i = 0; i < qrCount; i++) {
        M5.Display.qrcode(qrs[i].payload, x, qrY, qrSize);
        M5.Display.drawString(qrs[i].caption, x + qrSize / 2, qrY + qrSize + 12);
        x += qrSize + 30;
    }

    int textX = x + 10;
    int y = qrY;
    M5.Display.setTextColor(INK);
    M5.Display.setTextDatum(textdatum_t::top_left);
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
