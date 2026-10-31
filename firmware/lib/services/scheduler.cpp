#include "scheduler.h"

#include <Arduino.h>

#include "app_log.h"
#include "blinds.h"
#include "sun.h"

namespace scheduler {

static int lastFiredYday[MAX_SCHEDULES];
static bool initDone = false;

static int weekdayBit(const struct tm& t) { return (t.tm_wday + 6) % 7; }  // Monday = 0

int triggerMinute(const Schedule& sc, time_t day, float lat, float lon) {
    struct tm t;
    localtime_r(&day, &t);
    if (!(sc.days & (1 << weekdayBit(t)))) return -1;
    if (sc.trigger == ScheduleTrigger::Time) return sc.minutes;
    int rise, set;
    if (!sun::times(day, lat, lon, rise, set)) return -1;
    int m = (sc.trigger == ScheduleTrigger::Sunrise ? rise : set) + sc.minutes;
    return constrain(m, 0, 24 * 60 - 1);
}

void tick() {
    time_t now = time(nullptr);
    if (now < 1700000000) return;  // no NTP time yet
    struct tm t;
    localtime_r(&now, &t);
    int nowMin = t.tm_hour * 60 + t.tm_min;

    Settings s = settings::get();
    if (!initDone) {
        // don't fire schedules whose time already passed before boot
        for (int i = 0; i < MAX_SCHEDULES; i++) {
            int m = triggerMinute(s.schedules[i], now, s.latitude, s.longitude);
            lastFiredYday[i] = (m >= 0 && m < nowMin) ? t.tm_yday : -1;
        }
        initDone = true;
    }
    if (s.schedulesPaused) return;

    for (int i = 0; i < MAX_SCHEDULES; i++) {
        const Schedule& sc = s.schedules[i];
        if (!sc.enabled || lastFiredYday[i] == t.tm_yday) continue;
        int m = triggerMinute(sc, now, s.latitude, s.longitude);
        // fire within a 3 minute window so a busy minute is never missed
        if (m < 0 || nowMin < m || nowMin > m + 3) continue;
        lastFiredYday[i] = t.tm_yday;
        char desc[24];
        describe(sc, desc, sizeof(desc));
        LOGI("sched", "schedule #%d (%s) -> %d%%%s", i + 1, desc, sc.position, sc.gentle ? " gentle" : "");
        blinds.moveToPercent(sc.position, sc.gentle ? s.gentleSeconds : 0, CmdSource::Schedule);
    }
}

Next next() {
    Next n;
    time_t now = time(nullptr);
    if (now < 1700000000) return n;
    Settings s = settings::get();
    if (s.schedulesPaused) return n;
    struct tm t;
    localtime_r(&now, &t);
    for (int d = 0; d < 8 && !n.valid; d++) {
        time_t day = now + d * 86400L;
        struct tm dt;
        localtime_r(&day, &dt);
        dt.tm_hour = dt.tm_min = dt.tm_sec = 0;
        dt.tm_isdst = -1;
        time_t midnight = mktime(&dt);
        for (int i = 0; i < MAX_SCHEDULES; i++) {
            const Schedule& sc = s.schedules[i];
            if (!sc.enabled) continue;
            int m = triggerMinute(sc, day, s.latitude, s.longitude);
            if (m < 0) continue;
            time_t when = midnight + m * 60L;
            if (when <= now - 60) continue;
            if (!n.valid || when < n.when) {
                n.valid = true;
                n.index = i;
                n.when = when;
                n.position = sc.position;
            }
        }
    }
    return n;
}

void describe(const Schedule& sc, char* out, size_t cap) {
    if (sc.trigger == ScheduleTrigger::Time) {
        snprintf(out, cap, "%02d:%02d", sc.minutes / 60, sc.minutes % 60);
    } else {
        const char* n = sc.trigger == ScheduleTrigger::Sunrise ? "Sunrise" : "Sunset";
        if (sc.minutes == 0) snprintf(out, cap, "%s", n);
        else snprintf(out, cap, "%s %+d", n, sc.minutes);
    }
}

void daysString(uint8_t days, char* out, size_t cap) {
    static const char* names[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    days &= 0x7F;
    if (days == 0x7F) snprintf(out, cap, "Every day");
    else if (days == 0x1F) snprintf(out, cap, "Weekdays");
    else if (days == 0x60) snprintf(out, cap, "Weekends");
    else if (!days) snprintf(out, cap, "Never");
    else {
        out[0] = 0;
        for (int i = 0; i < 7; i++)
            if (days & (1 << i)) {
                if (out[0]) strlcat(out, " ", cap);
                strlcat(out, names[i], cap);
            }
    }
}

}  // namespace scheduler
