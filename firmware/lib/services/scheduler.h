#pragma once
// Runs the user schedules (fixed time / sunrise / sunset + offset, per weekday).
#include <time.h>

#include "settings.h"

namespace scheduler {

void tick();  // call about once per second (needs valid NTP time)

struct Next {
    bool valid = false;
    int index = -1;
    time_t when = 0;
    uint8_t position = 0;
};
Next next();  // next upcoming trigger within 7 days

// Trigger time for schedule `sc` on the local day containing `day`, in
// minutes after midnight, or -1 if it does not run that day.
int triggerMinute(const Schedule& sc, time_t day, float lat, float lon);
// e.g. "07:30", "Sunrise +15", "Sunset -10"
void describe(const Schedule& sc, char* out, size_t cap);
void daysString(uint8_t days, char* out, size_t cap);  // "Mon-Fri", "Every day", "Sat Sun"

}  // namespace scheduler
