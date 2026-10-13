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
