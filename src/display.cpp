#include "display.h"
#include "weathericons.h"
#include "weather_api.h"
#include "fonts/InterBoldDigits60.h"
#include "fonts/InterBoldDigits36.h"
#include <config.h>
#include <math.h>
#define USE_SERIAL Serial

// M5Paper S3: 960x540 (landscape). The countdowns are what matters from across
// the room, so they get the big type; everything else is secondary. Where the
// boards go is decided by computeLayout() (lib/transit_core/board_layout.h);
// the default (one large board, two compact ones) looks like this:
//
//  +----------------------------------------------------------------------+
//  | 14:32  lundi 13 octobre                  MAJ il y a 2 min  .ill [##] |  header
//  |======================================================================|
//  | RER title                                  | Bus title               |
//  | ------------------------------------------ | ----------------------- |
//  |  4 min   ROMI                     14:36    |  3 min           14:35  |
//  | 12 min   ROMI                +3   14:44    |  ...                    |
//  | Supprime ----------------------  -14:51-   | Bus title               |
//  | 25 min   ROMI                     14:57    |  ...                    |
//  | 31 min   ROMI                     15:03    | City                    |
//  |                                            | [icon] 12°  Ressenti... |
//  +----------------------------------------------------------------------+
//
//  A board whose line has an active incident gets an inverted title bar and
//  gives its first row to the incident text (filled sign = blocking, outline =
//  delays):
//
//  |#### RER C ############################ /!\ #|
//  | /!\ 6 gares non desservies     9h40 - 16h00 |  one line per incident
//  |  4 min   ROMI                     14:36     |
//
//  A tall weather card (no compact board in the right column) adds the 3-hour
//  forecast: hour, icon, temperature, chance of rain.

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
static String prevRows[LAYOUT_MAX_BOARDS][MAX_BOARD_ROWS];
static String prevIncidents[LAYOUT_MAX_BOARDS];  // Incidents shown per board: a change redraws the whole board
static String prevWeatherKey;
static ScreenLayout prevLayout;
static bool layoutDrawn = false;

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

// Warning sign: triangle with an exclamation mark, `size` wide, bottom at `bottom`.
// Filled draws a solid triangle in `color` with the mark in `bg`; otherwise an outline.
static void drawWarningIcon(int x, int bottom, int size, bool filled, uint16_t color, uint16_t bg) {
    const int h = size * 7 / 8;
    const int top = bottom - h, cx = x + size / 2;
    M5.Display.fillTriangle(cx, top, x, bottom, x + size, bottom, color);
    if (!filled) {
        // Hollow: inset triangle in the background color leaves a ~3px border
        M5.Display.fillTriangle(cx, top + 6, x + 4, bottom - 2, x + size - 4, bottom - 2, bg);
    }
    uint16_t mark = filled ? bg : color;
    const int markW = max(2, size / 8);
    M5.Display.fillRect(cx - markW / 2, top + h * 3 / 8, markW, h * 3 / 8, mark);
    M5.Display.fillRect(cx - markW / 2, bottom - h / 8 - markW, markW, markW, mark);
}

// Alert card header: the title in white on a black bar, warning sign on the
// right (with the number of incidents when there are several)
static void drawAlertCardTitle(int x, int y, int w, const char* title, int incidentCount) {
    const int barH = TITLE_H - 2;
    M5.Display.fillRect(x, y, w, barH, INK);
    
    const int iconSize = 24;
    int right = x + w - 8;
    drawWarningIcon(right - iconSize, y + barH - 4, iconSize, true, PAPER, INK);
    right -= iconSize + 8;
    
    const lgfx::GFXfont* font = &FreeSansBold12pt7b;
    const int baseline = centeredBaseline(y, barH, font);
    if (incidentCount > 1) {
        char count[8];
        snprintf(count, sizeof(count), "%d", incidentCount);
        drawText(count, right, baseline, font, PAPER, textdatum_t::baseline_right);
        right -= M5.Display.textWidth(count) + 8;
    }
    
    M5.Display.setFont(font);
    char fitted[48];
    fitText(title, right - (x + 8) - 8, fitted, sizeof(fitted));
    drawText(fitted, x + 8, baseline, font, PAPER, textdatum_t::baseline_left);
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
    const lgfx::GFXfont* bold = &FreeSansBold12pt7b;
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
    const int leftEnd = x + M5.Display.textWidth(dateStr);

    // Right side, laid out from the right edge: battery %, battery, WiFi, freshness.
    // Battery saving mode shows the percentage in bold.
    int right = SCREEN_WIDTH - MARGIN;
    char battText[8];
    snprintf(battText, sizeof(battText), "%d%%", status.batteryLevel);
    drawText(battText, right, baseline, status.batteryLow ? bold : text,
             status.batteryLow ? INK : INK_SOFT, textdatum_t::baseline_right);
    right -= M5.Display.textWidth(battText) + 8 + 33;
    drawBatteryIcon(right, baseline - 16, status.batteryLevel, status.isCharging);
    right -= 18 + 25;
    drawWifiIcon(right, baseline, status.wifiRssi);
    right -= 18;

    // Fresh data stays quiet (gray); stale data (missed a fetch) is shown in bold
    // black, with the reason of the last failure
    char ago[24] = "";
    int age = status.dataAgeSecs;
    bool stale = age < 0 || age > 300;
    if (age >= 3600) {
        snprintf(ago, sizeof(ago), "il y a %d h", age / 3600);
    } else if (age >= 60) {
        snprintf(ago, sizeof(ago), "il y a %d min", age / 60);
    }
    char freshStr[80];
    bool hasError = status.error && status.error[0];
    if (stale && hasError) {
        snprintf(freshStr, sizeof(freshStr), age < 0 ? "%s" : "%s - MAJ %s", status.error, ago);
    } else if (age < 0) {
        snprintf(freshStr, sizeof(freshStr), "Hors ligne");
    } else if (age < 60) {
        snprintf(freshStr, sizeof(freshStr), "A jour");
    } else {
        snprintf(freshStr, sizeof(freshStr), "MAJ %s", ago);
    }
    M5.Display.setFont(stale ? bold : text);
    char fitted[80];
    fitText(freshStr, right - leftEnd - 24, fitted, sizeof(fitted));
    drawText(fitted, right, baseline, stale ? bold : text, stale ? INK : INK_SOFT, textdatum_t::baseline_right);

    M5.Display.fillRect(MARGIN, HEADER_H - 2, SCREEN_WIDTH - 2 * MARGIN, 2, INK);
}

// ============================================================
// WEATHER: city, icon, temperature, details, then the forecast if there is room
// ============================================================
static const int WEATHER_MAIN_H = 72;      // Icon + temperature block
static const int FORECAST_ROW_H = 118;     // Hour, icon, temperature, rain chance
static const int FORECAST_COLUMNS = 4;

static String weatherKey(const LayoutRect& r, const weatherType& w, bool valid) {
    String key = String(r.x) + ',' + r.y + ',' + r.w + ',' + r.h + '|';
    if (!valid) return key;
    key += String((int)lroundf(w.temperature)) + '|' + (int)lroundf(w.feels_like) + '|' +
           (int)lroundf(w.wind_speed * 3.6f) + '|' + w.icon;
    for (int i = 0; i < w.forecastCount; i++) {
        const ForecastSlot& s = w.forecast[i];
        key += String('|') + (long)s.time + ',' + (int)lroundf(s.temp) + ',' + s.pop + ',' + s.icon + ',' +
               (isRainy(s) ? 'r' : '-');
    }
    return key;
}

static void drawTemperature(int x, int baseline, float temp, const lgfx::GFXfont* font, int ring, int thick) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", (int)lroundf(temp));
    drawText(buf, x, baseline, font, INK, textdatum_t::baseline_center);
    drawDegree(x + M5.Display.textWidth(buf) / 2 + 2, baseline - capHeight(font), ring, thick, INK);
}

// One row of forecast columns: "15h", icon, "14°", "70%"
static void drawForecastRow(int x, int y, int w, const ForecastSlot* slots, int count) {
    const int colW = w / FORECAST_COLUMNS;
    for (int i = 0; i < count; i++) {
        const ForecastSlot& s = slots[i];
        const int cx = x + i * colW + colW / 2;
        struct tm local;
        localtime_r(&s.time, &local);
        char buf[8];
        snprintf(buf, sizeof(buf), "%dh", local.tm_hour);
        drawText(buf, cx, y + 16, &FreeSans9pt7b, INK_SOFT, textdatum_t::baseline_center);
        const char* icon = getMiniMeteoconIconFromProgmem(s.icon);
        if (icon != nullptr) draw2BitIcon(cx - 25, y + 22, icon, 50, 50, INK);
        drawTemperature(cx, y + 96, s.temp, &FreeSansBold12pt7b, 3, 2);
        if (s.pop >= 20) {
            snprintf(buf, sizeof(buf), "%d%%", s.pop);
            drawText(buf, cx, y + 114, isRainy(s) ? &FreeSansBold9pt7b : &FreeSans9pt7b,
                     isRainy(s) ? INK : INK_SOFT, textdatum_t::baseline_center);
        }
    }
}

static void drawWeather(const LayoutRect& r, const weatherType& weather, bool valid, bool forceRedraw) {
    String key = weatherKey(r, weather, valid);
    if (!forceRedraw && key == prevWeatherKey) return;
    prevWeatherKey = key;

    clearRegion(r.x, r.y, r.w, r.h);
    drawCardTitle(r.x, r.y, r.w, getWeatherLocation().name);
    if (!valid) return;

    const int y = r.y + TITLE_H;
    const int h = min(r.h - TITLE_H, WEATHER_MAIN_H);
    const char* iconData = getMiniMeteoconIconFromProgmem(weather.icon);
    if (iconData != nullptr) {
        draw2BitIcon(r.x, y + (h - 50) / 2, iconData, 50, 50, INK);
    }

    // Temperature
    const lgfx::GFXfont* big = &InterBoldDigits36;
    const int baseline = centeredBaseline(y, h, big);
    char buf[48];
    snprintf(buf, sizeof(buf), "%d", (int)lroundf(weather.temperature));
    int x = r.x + 60;
    drawText(buf, x, baseline, big, INK, textdatum_t::baseline_left);
    x += M5.Display.textWidth(buf) + 3;
    drawDegree(x, baseline - capHeight(big), 5, 3, INK);
    x += 24;

    // Rain outlook on top, the largest wording that fits (humidity when there is
    // no forecast), then feels-like + wind
    const int textW = r.x + r.w - x;
    const lgfx::GFXfont* small = &FreeSans9pt7b;
    const int line1 = baseline - capHeight(big) + capHeight(&FreeSans12pt7b);
    char outlook[48];
    const lgfx::GFXfont* outlookFont = nullptr;
    bool rain = false;
    if (weather.forecastCount > 0) {
        struct Option { bool brief; const lgfx::GFXfont* regular; const lgfx::GFXfont* bold; };
        static const Option OPTIONS[] = {
            {false, &FreeSans12pt7b, &FreeSansBold12pt7b},
            {true, &FreeSans12pt7b, &FreeSansBold12pt7b},
            {true, &FreeSans9pt7b, &FreeSansBold9pt7b},
        };
        for (const Option& o : OPTIONS) {
            rainSummary(weather.forecast, weather.forecastCount, 12, o.brief, outlook, sizeof(outlook));
            rain = strncmp(outlook, "Pluie", 5) == 0;
            outlookFont = rain ? o.bold : o.regular;
            M5.Display.setFont(outlookFont);
            if (M5.Display.textWidth(outlook) <= textW) break;
        }
    } else {
        snprintf(outlook, sizeof(outlook), "Humidite %d%%", weather.humidity);
        outlookFont = &FreeSans12pt7b;
    }
    M5.Display.setFont(outlookFont);
    char fitted[48];
    fitText(outlook, textW, fitted, sizeof(fitted));
    drawText(fitted, x, line1, outlookFont, INK, textdatum_t::baseline_left);
    
    snprintf(buf, sizeof(buf), "Ressenti %d", (int)lroundf(weather.feels_like));
    drawText(buf, x, baseline, small, INK_SOFT, textdatum_t::baseline_left);
    int degX = x + M5.Display.textWidth(buf) + 2;
    drawDegree(degX, baseline - capHeight(small), 2, 1, INK_SOFT);
    snprintf(buf, sizeof(buf), "%d km/h", (int)lroundf(weather.wind_speed * 3.6f));
    if (degX + 16 + M5.Display.textWidth(buf) <= r.x + r.w) {
        drawText(buf, r.x + r.w, baseline, small, INK_SOFT, textdatum_t::baseline_right);
    }
    
    // Forecast rows below, as many as fit
    int rowY = y + h + 8;
    for (int first = 0; first < weather.forecastCount && rowY + FORECAST_ROW_H <= r.y + r.h;
         first += FORECAST_COLUMNS) {
        M5.Display.drawFastHLine(r.x, rowY - 4, r.w, INK_FAINT);
        drawForecastRow(r.x, rowY, r.w, weather.forecast + first,
                        min(FORECAST_COLUMNS, weather.forecastCount - first));
        rowY += FORECAST_ROW_H + 8;
    }
}

// ============================================================
// DEPARTURE BOARDS
// ============================================================
// Countdown as a big number with a small "min" after it, right-aligned on numRight
// so the numbers line up. A number too wide for its column (3 digits) uses
// smallFont: anything drawn left of the row would never be cleared.
static void drawCountdown(int numRight, int maxW, int baseline, long mins, const lgfx::GFXfont* numFont,
                          const lgfx::GFXfont* smallFont, const lgfx::GFXfont* unitFont) {
    char num[8];
    if (mins < 1) {
        snprintf(num, sizeof(num), "<1");
    } else {
        snprintf(num, sizeof(num), "%ld", mins);
    }
    M5.Display.setFont(numFont);
    if (M5.Display.textWidth(num) > maxW) numFont = smallFont;
    drawText(num, numRight, baseline, numFont, INK, textdatum_t::baseline_right);
    drawText("min", numRight + 8, baseline, unitFont, INK_SOFT, textdatum_t::baseline_left);
}

// Horizontal line through text drawn at (left..right, baseline) with `font`
static void strikeThrough(int left, int right, int baseline, const lgfx::GFXfont* font, uint16_t color) {
    int y = baseline - capHeight(font) / 2;
    M5.Display.fillRect(left, y - 1, right - left, 2, color);
}

// Clock time right-aligned on `right`; returns its left edge
static int drawClockTime(int right, int baseline, time_t t, bool struck) {
    struct tm local;
    localtime_r(&t, &local);
    char hhmm[8];
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", local.tm_hour, local.tm_min);
    const lgfx::GFXfont* font = &FreeSans12pt7b;
    drawText(hhmm, right, baseline, font, INK_SOFT, textdatum_t::baseline_right);
    int left = right - M5.Display.textWidth(hhmm);
    if (struck) strikeThrough(left - 2, right + 2, baseline, font, INK);
    return left;
}

static void drawEmptyRow(int x, int y, int h, const char* text) {
    if (text) {
        drawText(text, x, centeredBaseline(y, h, &FreeSans12pt7b), &FreeSans12pt7b,
                 INK_SOFT, textdatum_t::baseline_left);
    }
}

// One departure:  [ 12 min ]  Name ..............  +3  14:44
//   cancelled:    Supprime    -Name-----------------  -14:51-
// Rows of BIG_ROW_MIN_H and more use the big digits; the name is only shown on
// boards with showNames.
static void drawDepartureRow(int x, int y, int w, int h, const departureType* dep, time_t now,
                             bool showNames, const char* emptyText) {
    clearRegion(x, y + 1, w, h - 1);
    if (dep == nullptr) {
        drawEmptyRow(x, y, h, emptyText);
        return;
    }

    const bool big = h >= BIG_ROW_MIN_H;
    const lgfx::GFXfont* numFont = big ? &InterBoldDigits60 : &InterBoldDigits36;
    const int numW = big ? 118 : 72;
    const int baseline = centeredBaseline(y, h, numFont);

    // Right edge: clock time, and the delay just before it
    int right = drawClockTime(x + w, baseline, dep->expected, dep->cancelled) - 10;
    long delay = dep->delayMinutes();
    if (delay > 0) {
        char buf[8];
        snprintf(buf, sizeof(buf), "+%ld", delay);
        drawText(buf, right, baseline, &FreeSansBold12pt7b, INK, textdatum_t::baseline_right);
        right -= M5.Display.textWidth(buf) + 10;
    }

    if (dep->cancelled) {
        drawText("Supprime", x, baseline, big ? &FreeSansBold12pt7b : &FreeSansBold9pt7b, INK,
                 textdatum_t::baseline_left);
    } else {
        drawCountdown(x + numW, numW, baseline, (long)(dep->expected - now) / 60, numFont,
                      big ? &InterBoldDigits36 : &FreeSansBold18pt7b,
                      big ? &FreeSans12pt7b : &FreeSans9pt7b);
    }
    if (!showNames || dep->name.empty()) return;

    // Name: large if it fits, otherwise smaller, truncated as a last resort
    const int nameX = x + numW + (big ? 68 : 50);
    const int nameW = right - nameX - 2;
    if (nameW < 30) return;
    const lgfx::GFXfont* nameFont = big ? &FreeSansBold18pt7b : &FreeSansBold12pt7b;
    M5.Display.setFont(nameFont);
    if (M5.Display.textWidth(dep->name.c_str()) > nameW) {
        nameFont = big ? &FreeSansBold12pt7b : &FreeSansBold9pt7b;
        M5.Display.setFont(nameFont);
    }
    char name[48];
    fitText(dep->name.c_str(), nameW, name, sizeof(name));
    drawText(name, nameX, baseline, nameFont, dep->cancelled ? INK_SOFT : INK, textdatum_t::baseline_left);
    if (dep->cancelled) strikeThrough(nameX - 2, nameX + M5.Display.textWidth(name) + 2, baseline, nameFont, INK);
}

// Incidents in the first row of a board, one line each: warning sign (filled =
// blocking, outline = delays), text cut to fit, time window right-aligned.
static void drawIncidents(int x, int y, int w, int h, const std::vector<incidentType>& incidents) {
    const bool roomy = h >= BIG_ROW_MIN_H;
    const lgfx::GFXfont* bold = roomy ? &FreeSansBold12pt7b : &FreeSansBold9pt7b;
    const lgfx::GFXfont* regular = roomy ? &FreeSans12pt7b : &FreeSans9pt7b;
    const int maxLines = constrain((h - 4) / (roomy ? 28 : 21), 1, 3);
    const int iconSize = roomy ? 22 : 16;
    const int textX = x + iconSize + 10;

    const int count = min((int)incidents.size(), maxLines);
    const int lineH = (h - 4) / maxLines;
    const int top = y + (h - lineH * count) / 2;
    clearRegion(x, y + 1, w, h - 1);
    for (int i = 0; i < count; i++) {
        const incidentType& inc = incidents[i];
        const int baseline = centeredBaseline(top + i * lineH, lineH, bold);
        drawWarningIcon(x + 2, baseline, iconSize, inc.blocking, INK, PAPER);

        int textRight = x + w;
        if (inc.when.length()) {
            drawText(inc.when.c_str(), x + w, baseline, regular, INK, textdatum_t::baseline_right);
            textRight -= M5.Display.textWidth(inc.when.c_str()) + 12;
        }
        M5.Display.setFont(bold);
        char fitted[96];
        fitText(inc.text.c_str(), textRight - textX, fitted, sizeof(fitted));
        drawText(fitted, textX, baseline, bold, INK, textdatum_t::baseline_left);
    }
}

// Title, then rows separated by light rules. Only rows whose content changed are
// redrawn, except when the incidents change: then the whole board is.
static void drawBoard(const BoardPlacement& place, const BoardView& board, time_t now,
                      String* rowKeys, String& prevIncidentKey, bool forceRedraw) {
    const LayoutRect& r = place.rect;
    const int rowsY = r.y + TITLE_H;
    const char* emptyText = board.isTrain ? "Aucun train annonce" : "Aucun passage annonce";

    static const std::vector<incidentType> none;
    const std::vector<incidentType>& incidents = board.incidents ? *board.incidents : none;
    String incidentKey;
    for (const auto& inc : incidents) {
        incidentKey += inc.blocking ? '!' : '~';
        incidentKey += inc.text;
        incidentKey += '|';
        incidentKey += inc.when;
        incidentKey += '\n';
    }
    if (incidentKey != prevIncidentKey) forceRedraw = true;
    prevIncidentKey = incidentKey;
    const int firstRow = incidents.empty() ? 0 : 1;  // Departures start after the incident row

    if (forceRedraw) {
        clearRegion(r.x, r.y, r.w, r.h);
        if (incidents.empty()) {
            drawCardTitle(r.x, r.y, r.w, board.title);
        } else {
            drawAlertCardTitle(r.x, r.y, r.w, board.title, incidents.size());
            drawIncidents(r.x, rowsY, r.w, place.rowH, incidents);
        }
        for (int i = 1; i < place.rows; i++) {
            M5.Display.drawFastHLine(r.x, rowsY + i * place.rowH, r.w, INK_FAINT);
        }
        for (int i = 0; i < place.rows; i++) rowKeys[i] = "";
    }

    const std::vector<departureType>& deps = *board.departures;
    for (int i = firstRow; i < place.rows; i++) {
        size_t depIndex = i - firstRow;
        const departureType* dep = depIndex < deps.size() ? &deps[depIndex] : nullptr;

        String key;
        if (dep) {
            long mins = (long)(dep->expected - now) / 60;
            key = String((long)dep->expected) + '|' + String(mins < 1 ? 0 : mins) + '|' + dep->name.c_str() +
                  '|' + (dep->cancelled ? 'x' : '-') + dep->delayMinutes();
        } else if (depIndex == 0) {
            key = "empty";
        }
        if (key == rowKeys[i]) continue;
        rowKeys[i] = key;

        drawDepartureRow(r.x, rowsY + i * place.rowH, r.w, place.rowH, dep, now, board.showNames,
                         depIndex == 0 ? emptyText : nullptr);
    }
}

// No station at all: say how to add one
static void drawNoStations() {
    const int y = BODY_Y + 120;
    drawText("Aucun arret configure", LEFT_X, y, &FreeSansBold18pt7b, INK, textdatum_t::baseline_left);
    drawText("Maintenez l'ecran appuye 5 s pour choisir vos arrets.", LEFT_X, y + 44,
             &FreeSans12pt7b, INK_SOFT, textdatum_t::baseline_left);
}

// ============================================================
// MAIN: Draw screen with partial refresh support
// ============================================================
void drawScreen(const ScreenLayout& layout, const BoardView* boards, const weatherType& weather,
                bool weatherValid, const StatusInfo& status, time_t now, bool fullRefresh)
{
    if (!layoutDrawn || layout != prevLayout) fullRefresh = true;

    // Full refreshes use the high-quality waveform to clear ghosting; the
    // per-minute updates use the shorter text waveform (less time driving the panel).
    M5.Display.setEpdMode(fullRefresh ? epd_mode_t::epd_quality : epd_mode_t::epd_text);

    if (fullRefresh) {
        M5.Display.fillScreen(PAPER);
    }

    M5.Display.startWrite();

    drawHeader(status, now);

    for (int i = 0; i < layout.count; i++) {
        const BoardPlacement& place = layout.boards[i];
        drawBoard(place, boards[place.station], now, prevRows[i], prevIncidents[i], fullRefresh);
    }
    if (layout.count == 0 && fullRefresh) drawNoStations();

    drawWeather(layout.weather, weather, weatherValid, fullRefresh);

    if (fullRefresh) {
        M5.Display.drawFastVLine(DIVIDER_X, BODY_Y, BODY_BOTTOM - BODY_Y, INK_FAINT);
    }

    M5.Display.endWrite();
    M5.Display.display();
    prevLayout = layout;
    layoutDrawn = true;
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
    layoutDrawn = false;  // Next drawScreen() redraws everything
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

    int x = qrCount > 0 ? MARGIN + 20 : MARGIN - 10;
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
    layoutDrawn = false;  // Next drawScreen() redraws everything
}
