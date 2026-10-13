#pragma once

#include <Arduino.h>
#include <ctime>
#include "text_utils.h"  // parseIsoUtc(), stifToNavitia() (lib/transit_core)

// Percent-encode a query string value (keeps ':' so STIF refs stay readable).
String urlEncode(const char *s);

// Transliterate UTF-8 to the 7-bit ASCII the e-ink fonts can draw (see asciiFoldStd()).
String asciiFold(const char *utf8);
