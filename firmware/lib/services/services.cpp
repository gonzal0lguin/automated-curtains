#include "services.h"

#include <WiFi.h>

#include "app_log.h"
#include "settings.h"

namespace services {

static SemaphoreHandle_t mtx;
static WeatherData wData;
static BusData bData;
static volatile uint32_t busWantedUntil = 0;
static volatile bool weatherNow = true, busNow = false;

WeatherData weather() {
    if (!mtx) return WeatherData();
    xSemaphoreTake(mtx, portMAX_DELAY);
    WeatherData w = wData;
    xSemaphoreGive(mtx);
    return w;
}

BusData bus() {
    if (!mtx) return BusData();
    xSemaphoreTake(mtx, portMAX_DELAY);
    BusData b = bData;
    xSemaphoreGive(mtx);
    return b;
}

void requestBus() {
    uint32_t until = millis() + 120000;
    if (busWantedUntil == 0 || (int32_t)(millis() - busWantedUntil) > 0) busNow = true;  // was idle: fetch now
    busWantedUntil = until;
}

void refreshWeather() { weatherNow = true; }

void setWeather(const WeatherData& w) {
    if (!mtx) mtx = xSemaphoreCreateMutex();
    xSemaphoreTake(mtx, portMAX_DELAY);
    wData = w;
    xSemaphoreGive(mtx);
}

void setBus(const BusData& b) {
    if (!mtx) mtx = xSemaphoreCreateMutex();
    xSemaphoreTake(mtx, portMAX_DELAY);
    bData = b;
    xSemaphoreGive(mtx);
}
void refreshBus() {
    busNow = true;
    requestBus();
}

static void task(void*) {
    uint32_t lastWeather = 0, lastBus = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(500));
        if (WiFi.status() != WL_CONNECTED) continue;
        Settings s = settings::get();
        uint32_t now = millis();

        if (s.weatherEnabled && (weatherNow || now - lastWeather > 15 * 60 * 1000UL || !lastWeather)) {
            weatherNow = false;
            WeatherData w;
            bool ok = weather::fetch(s.latitude, s.longitude, w);
            lastWeather = now;
            if (ok) {
                xSemaphoreTake(mtx, portMAX_DELAY);
                wData = w;
                xSemaphoreGive(mtx);
            } else {
                lastWeather = now - 13 * 60 * 1000UL;  // retry in ~2 min
            }
        }

        bool wanted = (int32_t)(busWantedUntil - now) > 0;
        if (s.busStop[0] && (busNow || (wanted && now - lastBus > 30000))) {
            busNow = false;
            BusData b;
            bus::fetch(s.busStop, s.busServices, b);
            lastBus = millis();
            xSemaphoreTake(mtx, portMAX_DELAY);
            bData = b;
            xSemaphoreGive(mtx);
        }
    }
}

void begin() {
    if (!mtx) mtx = xSemaphoreCreateMutex();
    // TLS needs a generous stack
    xTaskCreatePinnedToCore(task, "fetch", 10240, nullptr, 1, nullptr, 0);
}

}  // namespace services
