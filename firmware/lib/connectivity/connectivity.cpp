#include "connectivity.h"

#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>

#include "app_log.h"
#include "blinds.h"
#include "settings.h"

namespace net {

static WiFiManager wm;
static State st = State::Off;
static bool servicesStarted = false;
static bool restartPending = false;
static uint32_t restartAt = 0;
static uint32_t lastReconnect = 0, disconnectedSince = 0;
static bool everConnected = false, autoPortal = false;
static uint32_t connectingSince = 0;
static constexpr uint32_t AUTO_PORTAL_AFTER_MS = 3 * 60 * 1000UL;  // saved WiFi unreachable since boot
static constexpr uint32_t AUTO_PORTAL_TIMEOUT_S = 5 * 60;           // then go back to retrying it
static volatile int otaPct = -1;
static std::function<void()> portalCb, connectedCb;

const char* portalName() { return WIFI_SETUP_AP; }
void onPortalStart(std::function<void()> cb) { portalCb = cb; }
void onConnected(std::function<void()> cb) { connectedCb = cb; }
int otaProgress() { return otaPct; }

const char* stateName(State s) {
    switch (s) {
        case State::Off: return "off";
        case State::Connecting: return "connecting";
        case State::Connected: return "connected";
        case State::Disconnected: return "disconnected";
        case State::Portal: return "setup portal";
    }
    return "?";
}

void applyTimezone(const char* tz) {
    setenv("TZ", tz, 1);
    tzset();
}

static void startServices() {
    Settings s = settings::get();
    if (!MDNS.begin(s.hostname)) LOGW("net", "mDNS failed");
    else MDNS.addService("http", "tcp", 80);

    ArduinoOTA.setHostname(s.hostname);
    if (s.otaPassword[0]) ArduinoOTA.setPassword(s.otaPassword);
    ArduinoOTA.setMdnsEnabled(false);  // we already run mDNS
    ArduinoOTA.onStart([]() {
        otaPct = 0;
        blinds.stop(CmdSource::System);
        LOGW("net", "OTA update started");
    });
    ArduinoOTA.onProgress([](unsigned int done, unsigned int total) { otaPct = total ? done * 100 / total : 0; });
    ArduinoOTA.onEnd([]() {
        otaPct = 100;
        LOGI("net", "OTA done, rebooting");
    });
    ArduinoOTA.onError([](ota_error_t e) {
        otaPct = -1;
        LOGE("net", "OTA error %d", (int)e);
    });
    ArduinoOTA.begin();
    servicesStarted = true;
}

void begin() {
    Settings s = settings::get();
    applyTimezone(s.timezone);

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(s.hostname);
    WiFi.setSleep(false);  // lower latency for the web UI / shortcuts
    WiFi.setAutoReconnect(true);

    wm.setConfigPortalBlocking(false);
    wm.setConfigPortalTimeout(0);
    wm.setTitle("Smart Blinds");
    wm.setDarkMode(true);
    wm.setHostname(s.hostname);
    wm.setConnectTimeout(20);
    std::vector<const char*> menu = {"wifi", "info", "restart"};
    wm.setMenu(menu);

    if (wm.getWiFiIsSaved()) {
        LOGI("net", "connecting to '%s'...", wm.getWiFiSSID().c_str());
        WiFi.begin();
        st = State::Connecting;
        connectingSince = millis();
    } else {
        LOGW("net", "no WiFi saved: starting setup portal '%s'", WIFI_SETUP_AP);
        startPortal();
    }
    configTzTime(s.timezone, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
}

static void openPortal(uint32_t timeoutS) {
    if (st == State::Portal) return;
    if (portalCb) portalCb();
    st = State::Portal;
    wm.setConfigPortalTimeout(timeoutS);
    wm.setSaveConfigCallback([]() {
        LOGI("net", "WiFi credentials saved, rebooting");
        restartPending = true;
        restartAt = millis() + 1500;
    });
    wm.startConfigPortal(WIFI_SETUP_AP);
}

void startPortal() {
    autoPortal = false;
    openPortal(0);
}

void forgetWifi() {
    LOGW("net", "forgetting WiFi credentials");
    wm.resetSettings();
    delay(200);
    ESP.restart();
}

void loop() {
    if (restartPending && (int32_t)(millis() - restartAt) >= 0) ESP.restart();

    if (st == State::Portal) {
        wm.process();
        if (autoPortal && !wm.getConfigPortalActive()) {
            // nobody configured anything: go back to trying the saved network
            LOGI("net", "setup portal timed out, retrying saved WiFi");
            autoPortal = false;
            WiFi.mode(WIFI_STA);
            WiFi.begin();
            st = State::Connecting;
            connectingSince = millis();
        }
        return;
    }
    if (!everConnected && st == State::Connecting && millis() - connectingSince > AUTO_PORTAL_AFTER_MS) {
        LOGW("net", "saved WiFi unreachable: opening setup portal '%s' for %lu min", WIFI_SETUP_AP,
             (unsigned long)(AUTO_PORTAL_TIMEOUT_S / 60));
        autoPortal = true;
        openPortal(AUTO_PORTAL_TIMEOUT_S);
        return;
    }

    bool up = WiFi.status() == WL_CONNECTED;
    if (up && st != State::Connected) {
        st = State::Connected;
        everConnected = true;
        disconnectedSince = 0;
        LOGI("net", "WiFi connected: %s  IP %s  RSSI %d", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(),
             WiFi.RSSI());
        if (!servicesStarted) startServices();
        if (connectedCb) connectedCb();
    } else if (!up && st == State::Connected) {
        st = State::Disconnected;
        disconnectedSince = millis();
        LOGW("net", "WiFi lost, reconnecting...");
    } else if (!up && (st == State::Disconnected || st == State::Connecting)) {
        if (!disconnectedSince) disconnectedSince = millis();
        if (millis() - lastReconnect > 30000 && millis() - disconnectedSince > 15000) {
            lastReconnect = millis();
            WiFi.reconnect();
        }
    }
    if (servicesStarted && up) ArduinoOTA.handle();
}

State state() { return st; }
bool connected() { return st == State::Connected; }
String ip() { return st == State::Portal ? WiFi.softAPIP().toString() : WiFi.localIP().toString(); }
String ssid() { return st == State::Portal ? String(WIFI_SETUP_AP) : WiFi.SSID(); }
int rssi() { return connected() ? WiFi.RSSI() : 0; }
bool timeValid() { return time(nullptr) > 1700000000; }

}  // namespace net
