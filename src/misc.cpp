#include "misc.h"
#include <string>
#include <ctime>

// Function to parse a UTC date/time string and return UTC epoch.
// Input is assumed to be UTC (e.g., "2026-01-11T18:12:00.000Z")
time_t parseDateTime(const char *datetimeString, const char *format)
{
    struct tm tmStruct = {0};
    strptime(datetimeString, format, &tmStruct);
    tmStruct.tm_isdst = 0;
    
    // mktime() treats input as local time and converts to UTC epoch.
    // But our input IS UTC, so mktime wrongly subtracts the timezone offset.
    // We need to add it back.
    
    // First, get what mktime produces (wrong epoch)
    time_t wrong_epoch = mktime(&tmStruct);
    
    // Calculate the current timezone offset by comparing gmtime vs localtime
    time_t now = time(nullptr);
    struct tm gm_now, local_now;
    gmtime_r(&now, &gm_now);
    localtime_r(&now, &local_now);
    
    // Offset = local - UTC (in seconds)
    int offset_secs = (local_now.tm_hour - gm_now.tm_hour) * 3600 +
                      (local_now.tm_min - gm_now.tm_min) * 60;
    
    // Handle day boundary wrap
    if (local_now.tm_mday != gm_now.tm_mday) {
        if (local_now.tm_mday > gm_now.tm_mday || 
            (local_now.tm_mday == 1 && gm_now.tm_mday > 1)) {
            offset_secs += 24 * 3600;  // Local is next day
        } else {
            offset_secs -= 24 * 3600;  // Local is previous day
        }
    }
    
    // mktime subtracted this offset (thinking input was local), so add it back
    return wrong_epoch + offset_secs;
}

// Function to format a time_t value into a date or time string.
char* toDateTime(time_t time, const char *format)
{
    static char buffer[90];
    struct tm *timeinfo = localtime(&time);
    strftime(buffer, sizeof(buffer), format, timeinfo);
    return buffer;
}
