#pragma once

#include <ctime>
#include <ESP32Time.h>

time_t parseDateTime(const char *datetimeString, const char *format);
char *toDateTime(time_t time, const char *format);
