#pragma once
// Background data: weather (every 15 min) and bus arrivals (every 30 s while
// someone is looking at them). Runs in its own low-priority task so slow
// HTTPS requests never block the UI or the web server.
#include "bus.h"
#include "weather.h"

namespace services {

void begin();
WeatherData weather();
BusData bus();
void requestBus();       // keep bus data fresh for the next ~2 minutes
void refreshWeather();   // fetch as soon as possible
void refreshBus();

// For UI tests without network: inject fake data.
void setWeather(const WeatherData& w);
void setBus(const BusData& b);

}  // namespace services
