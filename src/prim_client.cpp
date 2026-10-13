#include "prim_client.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <config.h>
#include "gzip_reader.h"

#define USE_SERIAL Serial

static WiFiClientSecure client;
static HTTPClient http;
static bool clientReady = false;
static FetchError lastError = {FETCH_OK, 0};

static bool begin(const char *url, const char *encoding)
{
    if (!clientReady) {
        client.setInsecure();  // No CA pinned (same as before); see review notes
        http.setReuse(true);
        http.setConnectTimeout(5000);
        http.setTimeout(8000);
        clientReady = true;
    }
    if (!http.begin(client, url)) {
        USE_SERIAL.println("[PRIM] begin failed");
        lastError = {FETCH_HTTP, -1};
        return false;
    }
    http.addHeader("apiKey", API_TOKEN);
    http.setAcceptEncoding(encoding);
    return true;
}

static bool get()
{
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) return true;
    USE_SERIAL.printf("[PRIM] GET failed: %d %s\n", httpCode, httpCode < 0 ? http.errorToString(httpCode).c_str() : "");
    lastError = {FETCH_HTTP, httpCode};
    http.end();
    return false;
}

bool primGet(const char *url, String &payload)
{
    if (!begin(url, "identity") || !get()) return false;
    payload = http.getString();
    http.end();  // Keeps the socket open thanks to setReuse(true)
    return true;
}

bool primGetGzip(const char *url, PsramBody &body, bool &gzipped)
{
    if (!begin(url, "gzip")) return false;
    const char *wanted[] = {"Content-Encoding"};
    http.collectHeaders(wanted, 1);
    if (!get()) return false;
    gzipped = http.header("Content-Encoding").equalsIgnoreCase("gzip");
    int written = http.writeToStream(&body);
    http.end();
    if (written < 0) {
        USE_SERIAL.printf("[PRIM] body failed: %d\n", written);
        lastError = {FETCH_HTTP, written};
        return false;
    }
    return true;
}

const FetchError &primLastError()
{
    return lastError;
}

void primSetParseError()
{
    lastError = {FETCH_PARSE, 0};
}

void primEnd()
{
    http.end();
    client.stop();
}
