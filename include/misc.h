#include <string>
#include "LiquidCrystal_I2C.h"
#include "Wire.h"
#include <ESP32Time.h>


time_t parseDateTime(const char *datetimeString, const char *format);
char *toDateTime(time_t time, const char *format);
#pragma once
