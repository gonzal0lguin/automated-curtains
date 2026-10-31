#pragma once
// Red (Santiago, Chile) bus arrival predictions via the community API
// https://api.xor.cl/red/bus-stop/<STOP_CODE>   (e.g. PA433)
#include <Arduino.h>
#include <time.h>

struct BusArrival {
    char service[8];
    int16_t minMin;     // minutes, lower estimate
    int16_t maxMin;     // minutes, upper estimate
    int32_t meters;     // distance to the stop
};

struct BusData {
    static constexpr int MAX_ITEMS = 8;
    bool valid = false;
    char stop[12] = "";
    char stopName[64] = "";
    char message[64] = "";   // API status / error text
    uint8_t count = 0;
    BusArrival items[MAX_ITEMS];
    time_t updated = 0;
};

namespace bus {

// servicesFilter: comma separated list ("506,507"), empty for all services.
bool fetch(const char* stop, const char* servicesFilter, BusData& out);

}  // namespace bus
