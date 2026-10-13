#include "misc.h"
#include <cstdio>

// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's algorithm).
// Avoids mktime(), which interprets its input in the local timezone.
static long daysFromCivil(int y, int m, int d)
{
    y -= m <= 2;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long)doe - 719468;
}

time_t parseIsoUtc(const char *datetimeString)
{
    if (datetimeString == nullptr) return 0;
    int y, mo, d, h, mi, s;
    if (sscanf(datetimeString, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6) return 0;
    return (time_t)daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + s;
}

String urlEncode(const char *s)
{
    static const char hex[] = "0123456789ABCDEF";
    String out;
    for (; *s; s++) {
        uint8_t c = (uint8_t)*s;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == ':') {
            out += (char)c;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

String asciiFold(const char *utf8)
{
    // U+00C0..U+00FF
    static const char latin1[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
    String out;
    const uint8_t *p = (const uint8_t *)utf8;
    while (*p) {
        uint8_t c = *p;
        if (c < 0x80) {
            out += (char)c;
            p++;
            continue;
        }
        int len = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        for (int i = 1; i < len; i++) {
            if ((p[i] & 0xC0) != 0x80) { len = i; break; }  // Truncated sequence
        }
        uint32_t cp = len == 2 ? ((c & 0x1F) << 6) | (p[1] & 0x3F)
                    : len == 3 ? ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F)
                    : 0;
        p += len;
        if (cp >= 0xC0 && cp <= 0xFF) {
            out += latin1[cp - 0xC0];
        } else if (cp == 0x152) {
            out += "OE";
        } else if (cp == 0x153) {
            out += "oe";
        } else if (cp == 0xA0 || (cp >= 0x2000 && cp <= 0x200A) || cp == 0x202F) {
            out += ' ';
        } else if (cp >= 0x2010 && cp <= 0x2015) {
            out += '-';
        } else if (cp == 0x2018 || cp == 0x2019) {
            out += '\'';
        } else if (cp == 0xAB || cp == 0xBB || cp == 0x201C || cp == 0x201D) {
            out += '"';
        } else if (cp == 0x2022) {
            out += '-';
        } else if (cp == 0x2026) {
            out += "...";
        }
    }
    return out;
}
