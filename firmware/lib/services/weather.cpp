#include "weather.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "app_log.h"

namespace weather {

const char* describe(int c) {
    switch (c) {
        case 0: return "Clear";
        case 1: return "Mostly clear";
        case 2: return "Partly cloudy";
        case 3: return "Overcast";
        case 45: case 48: return "Fog";
        case 51: case 53: case 55: return "Drizzle";
        case 56: case 57: return "Freezing drizzle";
        case 61: return "Light rain";
        case 63: return "Rain";
        case 65: return "Heavy rain";
        case 66: case 67: return "Freezing rain";
        case 71: case 73: case 75: case 77: return "Snow";
        case 80: case 81: case 82: return "Showers";
        case 85: case 86: return "Snow showers";
        case 95: return "Thunderstorm";
        case 96: case 99: return "Hail storm";
        default: return "Unknown";
    }
}

bool fetch(float lat, float lon, WeatherData& out) {
    char url[320];
    snprintf(url, sizeof(url),
             "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,relative_humidity_2m,weather_code,is_day,wind_speed_10m"
             "&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max"
             "&timezone=auto&forecast_days=1",
             lat, lon);
    WiFiClient client;
    HTTPClient http;
    http.setTimeout(8000);
    if (!http.begin(client, url)) return false;
    int code = http.GET();
    if (code != 200) {
        LOGW("weather", "HTTP %d", code);
        http.end();
        return false;
    }
    String body = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        LOGW("weather", "JSON error: %s", err.c_str());
        return false;
    }
    JsonObject cur = doc["current"];
    JsonObject daily = doc["daily"];
    if (cur.isNull()) return false;
    out.temp = cur["temperature_2m"] | 0.0f;
    out.humidity = cur["relative_humidity_2m"] | 0;
    out.code = cur["weather_code"] | 0;
    out.isDay = (cur["is_day"] | 1) != 0;
    out.wind = cur["wind_speed_10m"] | 0.0f;
    out.tMax = daily["temperature_2m_max"][0] | out.temp;
    out.tMin = daily["temperature_2m_min"][0] | out.temp;
    out.precipProb = daily["precipitation_probability_max"][0] | -1;
    out.updated = time(nullptr);
    out.valid = true;
    LOGI("weather", "%.1fC %s (max %.0f / min %.0f)", out.temp, describe(out.code), out.tMax, out.tMin);
    return true;
}

}  // namespace weather
