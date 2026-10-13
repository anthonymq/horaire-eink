#pragma once

#include <Arduino.h>
#include <miniz.h>  // ROM inflate (tinfl)

// Response body collected in PSRAM (up to `maxSize`); HTTPClient::writeToStream()
// also undoes chunked transfer encoding.
class PsramBody : public Stream {
public:
    explicit PsramBody(size_t maxSize = 1024 * 1024) : maxSize(maxSize) {}
    ~PsramBody() { free(data); }
    size_t write(uint8_t b) override { return write(&b, 1); }
    size_t write(const uint8_t *buf, size_t n) override;
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }

    uint8_t *data = nullptr;
    size_t len = 0;
    size_t cap = 0;

private:
    size_t maxSize;
};

// Inflates a gzip body on the fly for ArduinoJson (a reader needs read() and
// readBytes()), so a large JSON is never held in memory at once. The 32 KB
// inflate window doubles as the output buffer.
class GzipReader {
public:
    ~GzipReader();

    bool begin(const uint8_t *gz, size_t n);
    int read();
    size_t readBytes(char *buf, size_t n);
    void unread(int c) { pushedBack = c; }
    int readNonSpace();
    // Consume input up to and including `pattern`
    bool skipPast(const char *pattern);
    bool failed() const { return error; }

private:
    bool refill();

    const uint8_t *in = nullptr;
    size_t inLeft = 0;
    uint8_t *dict = nullptr;
    tinfl_decompressor *inflator = nullptr;
    size_t dictOfs = 0, produced = 0, outPos = 0, outEnd = 0;
    int pushedBack = -1;
    bool finished = false, error = false;
};
