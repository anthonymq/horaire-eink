#ifndef TRANSPORT_API_H
#define TRANSPORT_API_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>
#include <string>
#include "misc.h"
#include "types.h" // Include the new header
#include <config.h>

// Main function with parameters
// Update the function declaration to match exactly with the implementation
// Make sure the declaration matches the implementation
std::vector<departureType> fetchDepartures(const std::string &url, const std::string &type, const std::string &filter);
#endif