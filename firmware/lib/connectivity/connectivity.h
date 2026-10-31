#pragma once
// WiFi (with captive-portal setup), NTP time, mDNS and ArduinoOTA.
//
// First boot (no saved WiFi): an access point "Blinds-Setup" is opened; join
// it with a phone and pick your network. Afterwards the device reconnects on
// its own and is reachable at http://<hostname>.local
#include <Arduino.h>

#include <functional>

namespace net {

enum class State : uint8_t { Off, Connecting, Connected, Disconnected, Portal };

void begin();                 // uses settings (hostname, timezone, OTA password)
void loop();                  // call every few ms from the network task
State state();
const char* stateName(State s);
bool connected();
String ip();
String ssid();
int rssi();
bool timeValid();             // NTP time received
void applyTimezone(const char* posixTz);

void startPortal();           // opens the WiFi setup access point (reboots when done)
void forgetWifi();            // erases saved credentials and reboots into the portal
const char* portalName();

// Called right before the config portal grabs port 80 (to stop our server)
// and once the station is connected (to start it).
void onPortalStart(std::function<void()> cb);
void onConnected(std::function<void()> cb);

// OTA progress for the UI: -1 when no update is running, else 0..100.
int otaProgress();

}  // namespace net
