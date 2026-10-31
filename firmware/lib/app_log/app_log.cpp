#include "app_log.h"

#include <stdarg.h>

namespace applog {

static constexpr size_t RING_SIZE = 4096;
static char ring[RING_SIZE];
static size_t head = 0;      // next write index
static bool wrapped = false;
static SemaphoreHandle_t mtx = nullptr;

void begin(uint32_t baud) {
    if (!mtx) mtx = xSemaphoreCreateMutex();
    Serial.begin(baud);
}

static void ringPut(const char* s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        ring[head++] = s[i];
        if (head == RING_SIZE) {
            head = 0;
            wrapped = true;
        }
    }
}

void write(char level, const char* tag, const char* fmt, ...) {
    char line[200];
    uint32_t ms = millis();
    int n = snprintf(line, sizeof(line), "[%5lu.%03lu] %c %-7s ", (unsigned long)(ms / 1000),
                     (unsigned long)(ms % 1000), level, tag);
    va_list ap;
    va_start(ap, fmt);
    n += vsnprintf(line + n, sizeof(line) - n - 1, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof(line) - 2) n = sizeof(line) - 2;
    line[n++] = '\n';
    line[n] = 0;

    if (!mtx) mtx = xSemaphoreCreateMutex();
    xSemaphoreTake(mtx, portMAX_DELAY);
    Serial.write(line, n);
    ringPut(line, n);
    xSemaphoreGive(mtx);
}

String recent() {
    String out;
    if (!mtx) return out;
    xSemaphoreTake(mtx, portMAX_DELAY);
    out.reserve(wrapped ? RING_SIZE : head);
    if (wrapped) {
        // skip the partial first line
        size_t i = head;
        while (i < RING_SIZE && ring[i] != '\n') i++;
        if (i < RING_SIZE) out.concat(ring + i + 1, RING_SIZE - i - 1);
    }
    out.concat(ring, head);
    xSemaphoreGive(mtx);
    return out;
}

}  // namespace applog
