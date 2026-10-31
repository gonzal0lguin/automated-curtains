#include "bus.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <algorithm>

#include "app_log.h"

namespace bus {

static bool serviceAllowed(const char* filter, const char* id) {
    if (!filter || !filter[0]) return true;
    size_t n = strlen(id);
    const char* p = filter;
    while (*p) {
        while (*p == ' ' || *p == ',') p++;
        const char* end = p;
        while (*end && *end != ',') end++;
        const char* e = end;
        while (e > p && e[-1] == ' ') e--;
        if ((size_t)(e - p) == n && strncasecmp(p, id, n) == 0) return true;
        p = end;
    }
    return false;
}

bool fetch(const char* stop, const char* filter, BusData& out) {
    out.valid = false;
    out.count = 0;
    strlcpy(out.stop, stop, sizeof(out.stop));
    if (!stop || !stop[0]) {
        strlcpy(out.message, "No bus stop configured", sizeof(out.message));
        return false;
    }

    char url[96];
    snprintf(url, sizeof(url), "https://api.xor.cl/red/bus-stop/%s", stop);
    WiFiClientSecure client;
    client.setInsecure();  // public, read-only data: skip CA validation
    HTTPClient http;
    http.useHTTP10(true);  // no chunked encoding -> we can stream-parse
    http.setTimeout(10000);
    if (!http.begin(client, url)) return false;
    int code = http.GET();
    if (code != 200) {
        snprintf(out.message, sizeof(out.message), "HTTP error %d", code);
        LOGW("bus", "%s", out.message);
        http.end();
        return false;
    }

    JsonDocument filterDoc;
    filterDoc["name"] = true;
    filterDoc["status_code"] = true;
    filterDoc["status_description"] = true;
    JsonObject svc = filterDoc["services"].add<JsonObject>();
    svc["id"] = true;
    svc["valid"] = true;
    JsonObject b = svc["buses"].add<JsonObject>();
    b["min_arrival_time"] = true;
    b["max_arrival_time"] = true;
    b["meters_distance"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filterDoc));
    http.end();
    if (err) {
        snprintf(out.message, sizeof(out.message), "JSON: %s", err.c_str());
        LOGW("bus", "%s", out.message);
        return false;
    }

    strlcpy(out.stopName, doc["name"] | "", sizeof(out.stopName));
    strlcpy(out.message, doc["status_description"] | "", sizeof(out.message));
    if ((doc["status_code"] | -1) != 0) {
        LOGW("bus", "stop %s: %s", stop, out.message);
        return false;
    }

    // gather every predicted bus, then keep the soonest ones
    BusArrival all[24];
    int n = 0;
    JsonArray services = doc["services"];
    for (JsonObject s : services) {
        const char* id = s["id"] | "";
        if (!(s["valid"] | false) || !serviceAllowed(filter, id)) continue;
        JsonArray buses = s["buses"];
        for (JsonObject bb : buses) {
            if (n >= 24) break;
            BusArrival& a = all[n++];
            strlcpy(a.service, id, sizeof(a.service));
            a.minMin = bb["min_arrival_time"] | 0;
            a.maxMin = bb["max_arrival_time"] | a.minMin;
            a.meters = bb["meters_distance"] | 0;
        }
    }
    std::sort(all, all + n, [](const BusArrival& x, const BusArrival& y) {
        return x.minMin != y.minMin ? x.minMin < y.minMin : x.meters < y.meters;
    });
    out.count = std::min(n, (int)BusData::MAX_ITEMS);
    memcpy(out.items, all, sizeof(BusArrival) * out.count);
    out.updated = time(nullptr);
    out.valid = true;
    if (!n) strlcpy(out.message, "No buses approaching", sizeof(out.message));
    LOGI("bus", "%s: %d buses (%s)", stop, n, out.stopName);
    return true;
}

}  // namespace bus
