#include <LiquidCrystal_I2C.h>
#include "misc.h"
#include <string>
void registerSpecialChars(LiquidCrystal_I2C i2c)
{
    uint8_t bell[8] = {0x4, 0xe, 0xe, 0xe, 0x1f, 0x0, 0x4};
    uint8_t note[8] = {0x2, 0x3, 0x2, 0xe, 0x1e, 0xc, 0x0};
    uint8_t clockChar[8] = {0x0, 0xe, 0x15, 0x17, 0x11, 0xe, 0x0};
    uint8_t heart[8] = {0x0, 0xa, 0x1f, 0x1f, 0xe, 0x4, 0x0};
    uint8_t duck[8] = {0x0, 0xc, 0x1d, 0xf, 0xf, 0x6, 0x0};
    uint8_t check[8] = {0x0, 0x1, 0x3, 0x16, 0x1c, 0x8, 0x0};
    uint8_t cross[8] = {0x0, 0x1b, 0xe, 0x4, 0xe, 0x1b, 0x0};
    uint8_t retarrow[8] = {0x1, 0x1, 0x5, 0x9, 0x1f, 0x8, 0x4};
    i2c.createChar(0, bell);
    i2c.createChar(1, note);
    i2c.createChar(2, clockChar);
    i2c.createChar(3, heart);
    i2c.createChar(4, duck);
    i2c.createChar(5, check);
    i2c.createChar(6, cross);
    i2c.createChar(7, retarrow);
}


// function to parse a date or time string.
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
// void DateTime(time_t time, const char *format)
// {
//     char buffer[90];
//     struct tm *timeinfo = localtime(&time);
//     strftime(buffer, sizeof(buffer), format, timeinfo);
//     return buffer;
// }