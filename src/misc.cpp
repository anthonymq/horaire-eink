#include "misc.h"
#include <string>
#include <ctime>

// Function to parse a date or time string.
time_t parseDateTime(const char *datetimeString, const char *format)
{
    struct tm tmStruct;
    strptime(datetimeString, format, &tmStruct);
    return mktime(&tmStruct);
}

// Function to format a time_t value into a date or time string.
char* toDateTime(time_t time, const char *format)
{
    static char buffer[90];
    struct tm *timeinfo = localtime(&time);
    strftime(buffer, sizeof(buffer), format, timeinfo);
    return buffer;
}
