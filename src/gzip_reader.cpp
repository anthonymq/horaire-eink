#include "gzip_reader.h"

size_t PsramBody::write(const uint8_t *buf, size_t n)
{
    if (len + n > cap) {
        size_t newCap = max(cap * 2, len + n + 8192);
        if (newCap > maxSize) return 0;
        uint8_t *p = (uint8_t *)ps_realloc(data, newCap);
        if (!p) return 0;
        data = p;
        cap = newCap;
    }
    memcpy(data + len, buf, n);
    len += n;
    return n;
}

GzipReader::~GzipReader()
{
    free(dict);
    free(inflator);
}

bool GzipReader::begin(const uint8_t *gz, size_t n)
{
    // RFC 1952 header: magic, deflate, flags, then optional fields
    if (n < 18 || gz[0] != 0x1F || gz[1] != 0x8B || gz[2] != 8) return false;
    uint8_t flags = gz[3];
    size_t pos = 10;
    if (flags & 0x04) pos += 2 + (gz[pos] | (gz[pos + 1] << 8));  // FEXTRA
    if (flags & 0x08) while (pos < n && gz[pos++]) {}             // FNAME
    if (flags & 0x10) while (pos < n && gz[pos++]) {}             // FCOMMENT
    if (flags & 0x02) pos += 2;                                   // FHCRC
    if (pos >= n) return false;
    in = gz + pos;
    inLeft = n - pos;

    dict = (uint8_t *)ps_malloc(TINFL_LZ_DICT_SIZE);
    inflator = (tinfl_decompressor *)ps_malloc(sizeof(tinfl_decompressor));
    if (!dict || !inflator) return false;
    tinfl_init(inflator);
    return true;
}

int GzipReader::read()
{
    if (pushedBack >= 0) {
        int c = pushedBack;
        pushedBack = -1;
        return c;
    }
    if (outPos == outEnd && !refill()) return -1;
    return dict[outPos++];
}

size_t GzipReader::readBytes(char *buf, size_t n)
{
    size_t got = 0;
    while (got < n) {
        int c = read();
        if (c < 0) break;
        buf[got++] = (char)c;
    }
    return got;
}

int GzipReader::readNonSpace()
{
    int c;
    do c = read(); while (c == ' ' || c == '\n' || c == '\r' || c == '\t');
    return c;
}

bool GzipReader::skipPast(const char *pattern)
{
    size_t matched = 0, len = strlen(pattern);
    int c;
    while ((c = read()) >= 0) {
        if (c == pattern[matched]) {
            if (++matched == len) return true;
        } else {
            matched = c == pattern[0] ? 1 : 0;
        }
    }
    return false;
}

bool GzipReader::refill()
{
    while (!finished) {
        dictOfs = (dictOfs + produced) & (TINFL_LZ_DICT_SIZE - 1);
        size_t inSize = inLeft, outSize = TINFL_LZ_DICT_SIZE - dictOfs;
        tinfl_status status = tinfl_decompress(inflator, in, &inSize, dict, dict + dictOfs, &outSize, 0);
        in += inSize;
        inLeft -= inSize;
        produced = outSize;
        if (status != TINFL_STATUS_HAS_MORE_OUTPUT) {
            finished = true;
            error = status != TINFL_STATUS_DONE;
        }
        outPos = dictOfs;
        outEnd = dictOfs + outSize;
        if (outSize) return true;
    }
    return false;
}
