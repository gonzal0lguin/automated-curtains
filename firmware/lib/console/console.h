#pragma once
// Line-based serial command console (115200 baud). Type "help".
#include <functional>

namespace console {

void poll();  // call from a loop; reads Serial without blocking

// Extra commands registered by other modules (e.g. the UI's "screenshot").
using Handler = std::function<void(const char* arg)>;
void addCommand(const char* name, const char* help, Handler fn);

}  // namespace console
