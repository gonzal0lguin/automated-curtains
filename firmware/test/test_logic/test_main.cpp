// On-device unit tests for the pure logic (no wiring needed).
//   pio test -e unit
#include <Arduino.h>
#include <unity.h>

#include "magnetic_encoder.h"
#include "scheduler.h"
#include "settings.h"
#include "sun.h"

static void setTz(const char* tz) {
    setenv("TZ", tz, 1);
    tzset();
}

static time_t localTime(int y, int mo, int d, int h, int mi) {
    struct tm t = {};
    t.tm_year = y - 1900;
    t.tm_mon = mo - 1;
    t.tm_mday = d;
    t.tm_hour = h;
    t.tm_min = mi;
    t.tm_isdst = -1;
    return mktime(&t);
}

// ------------------------------------------------------------------- sun

static void test_sun_santiago() {
    setTz("<-04>4<-03>,M9.1.6/24,M4.1.6/24");
    int rise, set;
    TEST_ASSERT_TRUE(sun::times(localTime(2026, 10, 19, 12, 0), -33.45f, -70.66f, rise, set));
    TEST_ASSERT_INT_WITHIN(3, 6 * 60 + 56, rise);   // Open-Meteo: 06:56
    TEST_ASSERT_INT_WITHIN(3, 19 * 60 + 58, set);   // Open-Meteo: 19:58
}

static void test_sun_utc_day_wrap() {
    // Los Angeles: sunset is after midnight UTC, must stay on the same local day
    setTz("PST8PDT,M3.2.0,M11.1.0");
    int rise, set;
    TEST_ASSERT_TRUE(sun::times(localTime(2026, 6, 21, 12, 0), 34.05f, -118.24f, rise, set));
    TEST_ASSERT_INT_WITHIN(4, 5 * 60 + 42, rise);
    TEST_ASSERT_INT_WITHIN(4, 20 * 60 + 8, set);
}

static void test_sun_polar_night() {
    setTz("UTC0");
    int rise, set;
    TEST_ASSERT_FALSE(sun::times(localTime(2026, 12, 21, 12, 0), 78.2f, 15.6f, rise, set));  // Svalbard
}

// ------------------------------------------------------------- scheduler

static void test_trigger_days() {
    setTz("<-04>4<-03>,M9.1.6/24,M4.1.6/24");
    Schedule sc;
    sc.enabled = true;
    sc.days = 0x1F;  // weekdays
    sc.trigger = ScheduleTrigger::Time;
    sc.minutes = 7 * 60;
    time_t monday = localTime(2026, 10, 19, 3, 0);    // 2026-10-19 is a Monday
    time_t saturday = localTime(2026, 10, 24, 3, 0);
    TEST_ASSERT_EQUAL(420, scheduler::triggerMinute(sc, monday, -33.45f, -70.66f));
    TEST_ASSERT_EQUAL(-1, scheduler::triggerMinute(sc, saturday, -33.45f, -70.66f));
}

static void test_trigger_sunset_offset() {
    setTz("<-04>4<-03>,M9.1.6/24,M4.1.6/24");
    Schedule sc;
    sc.days = 0x7F;
    sc.trigger = ScheduleTrigger::Sunset;
    sc.minutes = 15;
    time_t day = localTime(2026, 10, 19, 3, 0);
    int rise, set;
    sun::times(day, -33.45f, -70.66f, rise, set);
    TEST_ASSERT_EQUAL(set + 15, scheduler::triggerMinute(sc, day, -33.45f, -70.66f));
}

static void test_days_string() {
    char b[40];
    scheduler::daysString(0x7F, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("Every day", b);
    scheduler::daysString(0x1F, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("Weekdays", b);
    scheduler::daysString(0x41, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("Mon Sun", b);
}

static void test_describe() {
    Schedule sc;
    char b[24];
    sc.trigger = ScheduleTrigger::Time;
    sc.minutes = 6 * 60 + 5;
    scheduler::describe(sc, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("06:05", b);
    sc.trigger = ScheduleTrigger::Sunrise;
    sc.minutes = -10;
    scheduler::describe(sc, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("Sunrise -10", b);
}

// -------------------------------------------------------------- settings

static void test_settings_roundtrip() {
    Settings a;
    a.openTicks = 123456;
    a.closedTicks = -789;
    a.openSet = a.closedSet = true;
    a.maxSpeed = 4321;
    strlcpy(a.busStop, "PA433", sizeof(a.busStop));
    a.schedules[3].enabled = true;
    a.schedules[3].trigger = ScheduleTrigger::Sunset;
    a.schedules[3].minutes = -20;
    a.schedules[3].days = 0x22;

    JsonDocument doc;
    JsonObject o = doc.to<JsonObject>();
    settings::toJson(a, o, true);
    settings::schedulesToJson(a, o["schedules"].to<JsonArray>());

    Settings b;
    settings::fromJson(b, doc.as<JsonObjectConst>());
    TEST_ASSERT_EQUAL(123456, b.openTicks);
    TEST_ASSERT_EQUAL(-789, b.closedTicks);
    TEST_ASSERT_TRUE(b.calibrated());
    TEST_ASSERT_EQUAL(4321, b.maxSpeed);
    TEST_ASSERT_EQUAL_STRING("PA433", b.busStop);
    TEST_ASSERT_TRUE(b.schedules[3].enabled);
    TEST_ASSERT_EQUAL((int)ScheduleTrigger::Sunset, (int)b.schedules[3].trigger);
    TEST_ASSERT_EQUAL(-20, b.schedules[3].minutes);
    TEST_ASSERT_EQUAL(0x22, b.schedules[3].days);
}

static void test_settings_clamp_and_partial() {
    Settings s;
    JsonDocument doc;
    deserializeJson(doc, R"({"brightness":999,"latitude":-200,"busStop":"pa12","knobStepPct":0})");
    settings::fromJson(s, doc.as<JsonObjectConst>());
    TEST_ASSERT_EQUAL(255, s.brightness);
    TEST_ASSERT_EQUAL_FLOAT(-90, s.latitude);
    TEST_ASSERT_EQUAL_STRING("PA12", s.busStop);   // upper-cased
    TEST_ASSERT_EQUAL(1, s.knobStepPct);
    TEST_ASSERT_EQUAL(3000, s.maxSpeed);           // untouched default
}

// --------------------------------------------------- multi-turn encoder

class FakeEncoder : public MagneticEncoder {
public:
    uint16_t next = 0;
    bool begin(int, int, uint32_t) override {
        _connected = true;
        _lastRaw = transform(next);
        _pos = _lastRaw;
        return true;
    }

protected:
    bool readRaw(uint16_t& raw) override {
        raw = next;
        return true;
    }
};

static void test_encoder_wraps_forward_and_back() {
    FakeEncoder e;
    e.next = 4000;
    e.begin(0, 0, 0);
    for (int a : {4050, 4095, 10, 200}) {
        e.next = a;
        e.update();
    }
    TEST_ASSERT_EQUAL(4096 + 200, e.position());
    for (int a : {100, 4090, 3000}) {
        e.next = a;
        e.update();
    }
    TEST_ASSERT_EQUAL(3000, e.position());
}

static void test_encoder_restore() {
    FakeEncoder e;
    e.next = 4090;
    e.begin(0, 0, 0);
    // saved 5 turns + 100 ticks; shaft now reads 4090 (= 106 ticks back)
    e.restore(5 * 4096 + 100);
    TEST_ASSERT_EQUAL(5 * 4096 + 100 - 106, e.position());
    e.next = 50;
    e.begin(0, 0, 0);
    e.restore(-3 * 4096 + 4000);  // negative positions too
    TEST_ASSERT_EQUAL(-3 * 4096 + 4000 + 146, e.position());
}

static void test_encoder_reverse_keeps_restore_consistent() {
    FakeEncoder e;
    e.next = 1000;
    e.begin(0, 0, 0);
    e.next = 1500;
    e.update();
    TEST_ASSERT_EQUAL(1500, e.position());
    e.setReversed(true);
    TEST_ASSERT_EQUAL(-1500, e.position());
    e.next = 1400;  // physical shaft moves backwards -> reversed count goes up
    e.update();
    TEST_ASSERT_EQUAL(-1400, e.position());
    int32_t saved = e.position();
    FakeEncoder f;
    f.setReversed(true);
    f.next = 1400;
    f.begin(0, 0, 0);
    f.restore(saved);
    TEST_ASSERT_EQUAL(saved, f.position());
}

void setup() {
    delay(1500);  // give the serial monitor time to attach
    UNITY_BEGIN();
    RUN_TEST(test_sun_santiago);
    RUN_TEST(test_sun_utc_day_wrap);
    RUN_TEST(test_sun_polar_night);
    RUN_TEST(test_trigger_days);
    RUN_TEST(test_trigger_sunset_offset);
    RUN_TEST(test_days_string);
    RUN_TEST(test_describe);
    RUN_TEST(test_settings_roundtrip);
    RUN_TEST(test_settings_clamp_and_partial);
    RUN_TEST(test_encoder_wraps_forward_and_back);
    RUN_TEST(test_encoder_restore);
    RUN_TEST(test_encoder_reverse_keeps_restore_consistent);
    UNITY_END();
}

void loop() {}
