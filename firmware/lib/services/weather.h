#pragma once
#include <Arduino.h>
#include <time.h>

struct WeatherData {
    bool valid = false;
    float temp = 0;        // °C
    float tMax = 0, tMin = 0;
    float wind = 0;        // km/h
    int humidity = 0;      // %
    int precipProb = -1;   // % today, -1 unknown
    int code = 0;          // WMO weather code
    bool isDay = true;
    time_t updated = 0;
};

namespace weather {

// Blocking HTTP fetch from api.open-meteo.com (no API key needed).
bool fetch(float latitude, float longitude, WeatherData& out);
const char* describe(int wmoCode);

}  // namespace weather
