#include "misc.h"

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
    return String(asciiFoldStd(utf8).c_str());
}
