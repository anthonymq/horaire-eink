#ifndef SETUP_PORTAL_H
#define SETUP_PORTAL_H

#include <WiFiMulti.h>

/**
 * Long-press setup (blocking until done, cancelled or timed out).
 *
 * - online == false: WiFiManager access point + captive portal, with a QR code to
 *   join it. Once a network is saved it is added to `wifiMulti` and setup goes on
 *   to the station step below.
 * - online: web portal on the home network (http://<ip>/stops, QR on screen) with
 *   stop search through PRIM/Navitia and weather location search through
 *   OpenWeatherMap geocoding. Tap the screen or "Terminer" to leave.
 *
 * Station and weather location changes are saved to NVS and applied to the runtime config right away.
 * WiFi is left in an undefined state: the caller turns it off.
 */
void runSetupPortal(WiFiMulti& wifiMulti, bool online);

#endif
