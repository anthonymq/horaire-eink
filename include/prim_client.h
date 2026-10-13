#ifndef PRIM_CLIENT_H
#define PRIM_CLIENT_H

#include <Arduino.h>
#include "power_policy.h"

class PsramBody;

// GETs on PRIM with API_TOKEN over one shared keep-alive TLS connection: a TLS 1.3
// handshake costs far more CPU/radio time than a request, so it is paid once per
// fetch cycle. Call primEnd() once all requests are done.

// Plain (identity-encoded) body into `payload`
bool primGet(const char *url, String &payload);

// Body into `body`, gzip-compressed when the server agrees (`gzipped` says which)
bool primGetGzip(const char *url, PsramBody &body, bool &gzipped);

// Why the last failed request failed (stage + HTTP code)
const FetchError &primLastError();
void primSetParseError();

// Close the shared connection.
void primEnd();

#endif
