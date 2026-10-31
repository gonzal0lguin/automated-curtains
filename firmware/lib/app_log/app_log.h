#pragma once
// Tiny thread-safe logger: prints to Serial and keeps the last few KB in RAM
// so the web UI (/api/log) can show recent events without a USB cable.
#include <Arduino.h>

namespace applog {

void begin(uint32_t baud = 115200);
void write(char level, const char* tag, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
// Copies the buffered log (oldest first) into a String.
String recent();

}  // namespace applog

#define LOGI(tag, ...) applog::write('I', tag, __VA_ARGS__)
#define LOGW(tag, ...) applog::write('W', tag, __VA_ARGS__)
#define LOGE(tag, ...) applog::write('E', tag, __VA_ARGS__)
