#include "web.h"

#include <ArduinoJson.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

#include "app_log.h"
#include "blinds.h"
#include "connectivity.h"
#include "scheduler.h"
#include "services.h"
#include "settings.h"
#include "web_page.h"

namespace web {

static WebServer* server = nullptr;
static bool isRunning = false;
static bool rebootAfterResponse = false;
static bool uploadRejected = false;

// ------------------------------------------------------------------ helpers

static void sendJson(JsonDocument& doc, int code = 200) {
    String out;
    serializeJson(doc, out);
    server->sendHeader("Access-Control-Allow-Origin", "*");
    server->sendHeader("Cache-Control", "no-store");
    server->send(code, "application/json", out);
}

static void sendResult(bool ok, const char* msg) {
    JsonDocument doc;
    doc["ok"] = ok;
    doc["message"] = msg;
    BlindStatus st = blinds.status();
    doc["position"] = roundf(st.percent * 10) / 10;
    doc["state"] = Blinds::stateName(st.state);
    sendJson(doc, ok ? 200 : 409);
}

static bool authorized() {
    Settings s = settings::get();
    if (!s.apiToken[0]) return true;
    String tok = server->arg("token");
    if (!tok.length()) tok = server->header("X-Token");
    if (!tok.length()) {
        String a = server->header("Authorization");
        if (a.startsWith("Bearer ")) tok = a.substring(7);
    }
    if (tok == s.apiToken) return true;
    JsonDocument doc;
    doc["ok"] = false;
    doc["message"] = "invalid or missing token";
    sendJson(doc, 401);
    return false;
}

static uint16_t durationArg() {
    if (server->hasArg("duration")) return (uint16_t)constrain(server->arg("duration").toInt(), 0, 3600);
    if (server->hasArg("gentle") && server->arg("gentle") != "0") return settings::get().gentleSeconds;
    return 0;
}

static bool bodyJson(JsonDocument& doc) {
    if (!server->hasArg("plain")) return false;
    return deserializeJson(doc, server->arg("plain")) == DeserializationError::Ok;
}

static void fmtTime(time_t t, const char* fmt, char* out, size_t cap) {
    struct tm lt;
    localtime_r(&t, &lt);
    strftime(out, cap, fmt, &lt);
}

// ----------------------------------------------------------------- handlers

static void handleStatus() {
    JsonDocument doc;
    BlindStatus st = blinds.status();
    doc["position"] = roundf(st.percent * 10) / 10;
    if (st.targetPercent >= 0) doc["target"] = st.targetPercent;
    else doc["target"] = nullptr;
    doc["state"] = Blinds::stateName(st.state);
    doc["error"] = Blinds::errorName(st.error);
    doc["direction"] = st.direction;
    doc["calibrated"] = st.calibrated;
    doc["ticks"] = st.ticks;
    doc["targetTicks"] = st.targetTicks;
    doc["rawAngle"] = st.rawAngle;
    doc["steps"] = st.motorSteps;
    doc["stepsPerTick"] = st.stepsPerTick;
    doc["encoderOk"] = st.encoderOk;
    doc["tmcUart"] = st.tmcUart;
    doc["simulated"] = st.simulated;

    Settings s = settings::get();
    doc["hostname"] = s.hostname;
    doc["version"] = FW_VERSION;
    doc["uptime"] = millis() / 1000;
    doc["heap"] = ESP.getFreeHeap();
    doc["schedulesPaused"] = s.schedulesPaused;
    JsonObject w = doc["wifi"].to<JsonObject>();
    w["ssid"] = net::ssid();
    w["rssi"] = net::rssi();
    w["ip"] = net::ip();

    time_t now = time(nullptr);
    if (net::timeValid()) {
        char buf[32];
        fmtTime(now, "%Y-%m-%d %H:%M:%S", buf, sizeof(buf));
        doc["time"] = buf;
        doc["epoch"] = (uint32_t)now;
        scheduler::Next n = scheduler::next();
        if (n.valid) {
            JsonObject o = doc["next"].to<JsonObject>();
            o["index"] = n.index;
            o["position"] = n.position;
            o["epoch"] = (uint32_t)n.when;
            fmtTime(n.when, "%a %H:%M", buf, sizeof(buf));
            o["text"] = buf;
        }
    }
    WeatherData wd = services::weather();
    if (wd.valid) {
        JsonObject o = doc["weather"].to<JsonObject>();
        o["temp"] = wd.temp;
        o["max"] = wd.tMax;
        o["min"] = wd.tMin;
        o["code"] = wd.code;
        o["text"] = weather::describe(wd.code);
        o["humidity"] = wd.humidity;
        o["wind"] = wd.wind;
        o["precip"] = wd.precipProb;
        o["isDay"] = wd.isDay;
    }
    sendJson(doc);
}

static void handleMove(int which) {  // 0 open, 1 close, 2 toggle
    if (!authorized()) return;
    uint16_t d = durationArg();
    bool ok = which == 0   ? blinds.open(d, CmdSource::Web)
              : which == 1 ? blinds.close(d, CmdSource::Web)
                           : blinds.toggle(CmdSource::Web);
    sendResult(ok, ok ? (which == 0 ? "opening" : which == 1 ? "closing" : "toggled") : "not calibrated");
}

static void handlePosition() {
    if (!authorized()) return;
    String v = server->hasArg("value") ? server->arg("value") : server->arg("percent");
    if (!v.length()) {
        sendResult(false, "missing ?value=0..100");
        return;
    }
    bool ok = blinds.moveToPercent(constrain(v.toFloat(), 0.0f, 100.0f), durationArg(), CmdSource::Web);
    sendResult(ok, ok ? "moving" : "not calibrated");
}

static void handleStop() {
    if (!authorized()) return;
    blinds.stop(CmdSource::Web);
    sendResult(true, "stopping");
}

static void handleJog() {
    if (!authorized()) return;
    blinds.jog(server->arg("ticks").toInt());
    sendResult(true, "jogging");
}

static void handleCalibrate() {
    if (!authorized()) return;
    String a = server->arg("action");
    if (a == "open") blinds.setOpenHere();
    else if (a == "closed") blinds.setClosedHere();
    else if (a == "ratio") blinds.measureRatio();
    else if (a == "reset") blinds.resetCalibration();
    else {
        sendResult(false, "action must be open|closed|ratio|reset");
        return;
    }
    sendResult(true, a.c_str());
}

static void handleClear() {
    if (!authorized()) return;
    blinds.clearError();
    sendResult(true, "cleared");
}

static void handleGetSettings() {
    JsonDocument doc;
    settings::toJson(settings::get(), doc.to<JsonObject>(), false);
    sendJson(doc);
}

static void handlePostSettings() {
    if (!authorized()) return;
    JsonDocument in;
    if (!bodyJson(in)) {
        sendResult(false, "invalid JSON body");
        return;
    }
    Settings before = settings::get();
    settings::update([&](Settings& s) { settings::fromJson(s, in.as<JsonObjectConst>()); });
    Settings after = settings::get();
    if (strcmp(before.timezone, after.timezone)) net::applyTimezone(after.timezone);
    if (after.latitude != before.latitude || after.longitude != before.longitude) services::refreshWeather();
    if (strcmp(before.busStop, after.busStop) || strcmp(before.busServices, after.busServices)) services::refreshBus();
    LOGI("web", "settings updated");
    handleGetSettings();
}

static void handleGetSchedules() {
    JsonDocument doc;
    settings::schedulesToJson(settings::get(), doc.to<JsonArray>());
    sendJson(doc);
}

static void handlePostSchedules() {
    if (!authorized()) return;
    JsonDocument in;
    if (!bodyJson(in) || !in.is<JsonArray>()) {
        sendResult(false, "expected a JSON array");
        return;
    }
    settings::update([&](Settings& s) { settings::schedulesFromJson(s, in.as<JsonArrayConst>()); });
    LOGI("web", "schedules updated");
    handleGetSchedules();
}

static void handleBus() {
    services::requestBus();
    BusData b = services::bus();
    JsonDocument doc;
    doc["valid"] = b.valid;
    doc["stop"] = b.stop;
    doc["name"] = b.stopName;
    doc["message"] = b.message;
    doc["updated"] = (uint32_t)b.updated;
    JsonArray arr = doc["arrivals"].to<JsonArray>();
    for (int i = 0; i < b.count; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["service"] = b.items[i].service;
        o["min"] = b.items[i].minMin;
        o["max"] = b.items[i].maxMin;
        o["meters"] = b.items[i].meters;
    }
    sendJson(doc);
}

static void handleSystem() {
    if (!authorized()) return;
    String a = server->arg("action");
    if (a == "reboot") {
        sendResult(true, "rebooting");
        rebootAfterResponse = true;
    } else if (a == "forget-wifi") {
        sendResult(true, "forgetting WiFi, rebooting into setup portal");
        delay(300);
        net::forgetWifi();
    } else if (a == "factory-reset") {
        sendResult(true, "factory reset, rebooting");
        settings::factoryReset();
        rebootAfterResponse = true;
    } else if (a == "pause-schedules" || a == "resume-schedules") {
        bool pause = a == "pause-schedules";
        settings::update([pause](Settings& s) { s.schedulesPaused = pause; });
        sendResult(true, pause ? "schedules paused" : "schedules resumed");
    } else {
        sendResult(false, "unknown action");
    }
}

static void handleUpdateDone() {
    if (uploadRejected) {
        server->send(401, "text/plain", "invalid or missing token");
        return;
    }
    bool ok = !Update.hasError() && Update.isFinished();
    server->sendHeader("Connection", "close");
    server->send(ok ? 200 : 500, "text/plain", ok ? "Update OK, rebooting..." : Update.errorString());
    if (ok) rebootAfterResponse = true;
}

static void handleUpdateUpload() {
    HTTPUpload& up = server->upload();
    if (up.status == UPLOAD_FILE_START) {
        Settings s = settings::get();
        uploadRejected = s.apiToken[0] && server->arg("token") != s.apiToken;
        if (uploadRejected) {
            LOGW("web", "firmware upload rejected: bad token");
            return;
        }
        blinds.stop(CmdSource::System);
        LOGW("web", "firmware upload: %s", up.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) LOGE("web", "update begin failed: %s", Update.errorString());
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (Update.isRunning() && Update.write(up.buf, up.currentSize) != up.currentSize)
            LOGE("web", "update write failed: %s", Update.errorString());
    } else if (up.status == UPLOAD_FILE_END) {
        if (Update.isRunning() && Update.end(true)) LOGI("web", "update OK: %u bytes", up.totalSize);
        else LOGE("web", "update failed: %s", Update.errorString());
    }
}

// --------------------------------------------------------------------- api

void begin() {
    if (isRunning) return;
    if (!server) {
        server = new WebServer(80);
        const char* headers[] = {"X-Token", "Authorization"};
        server->collectHeaders(headers, 2);

        server->on("/", HTTP_GET, []() {
            server->sendHeader("Cache-Control", "no-cache");
            server->send_P(200, "text/html", WEB_PAGE);
        });
        server->on("/api/status", HTTP_GET, handleStatus);
        for (const char* p : {"/api/open", "/open"}) server->on(p, HTTP_ANY, []() { handleMove(0); });
        for (const char* p : {"/api/close", "/close"}) server->on(p, HTTP_ANY, []() { handleMove(1); });
        for (const char* p : {"/api/toggle", "/toggle"}) server->on(p, HTTP_ANY, []() { handleMove(2); });
        for (const char* p : {"/api/stop", "/stop"}) server->on(p, HTTP_ANY, handleStop);
        server->on("/api/position", HTTP_ANY, handlePosition);
        server->on("/api/jog", HTTP_ANY, handleJog);
        server->on("/api/calibrate", HTTP_ANY, handleCalibrate);
        server->on("/api/clear", HTTP_ANY, handleClear);
        server->on("/api/settings", HTTP_GET, handleGetSettings);
        server->on("/api/settings", HTTP_POST, handlePostSettings);
        server->on("/api/schedules", HTTP_GET, handleGetSchedules);
        server->on("/api/schedules", HTTP_POST, handlePostSchedules);
        server->on("/api/bus", HTTP_GET, handleBus);
        server->on("/api/system", HTTP_ANY, handleSystem);
        server->on("/api/log", HTTP_GET, []() { server->send(200, "text/plain", applog::recent()); });
        server->on("/api/driver", HTTP_GET, []() { server->send(200, "text/plain", blinds.driverReport()); });
        server->on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
        server->onNotFound([]() {
            if (server->method() == HTTP_OPTIONS) {
                server->sendHeader("Access-Control-Allow-Origin", "*");
                server->sendHeader("Access-Control-Allow-Headers", "Content-Type, X-Token, Authorization");
                server->send(204);
                return;
            }
            server->send(404, "text/plain", "not found");
        });
    }
    server->begin();
    isRunning = true;
    LOGI("web", "HTTP server on port 80");
}

void stop() {
    if (server && isRunning) server->stop();
    isRunning = false;
}

bool running() { return isRunning; }

void loop() {
    if (!isRunning) return;
    server->handleClient();
    if (rebootAfterResponse) {
        delay(500);
        ESP.restart();
    }
}

}  // namespace web
