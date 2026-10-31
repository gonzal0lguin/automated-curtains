#include "screens.h"

#include <math.h>

#include "blinds.h"
#include "connectivity.h"
#include "scheduler.h"
#include "services.h"
#include "settings.h"
#include "sun.h"

// Position gauge: 270° arc with the gap at the bottom. 0% (closed) at the
// lower-left end, 100% (open) at the lower-right end.
static constexpr float GAUGE_START = 225, GAUGE_SPAN = 270;

static float gaugeAngle(float pct) { return GAUGE_START + GAUGE_SPAN * pct / 100.0f; }

static void drawGauge(Canvas& c, int r, int thick, float pct, uint32_t color) {
    c.arc(120, 120, r, thick, GAUGE_START, GAUGE_SPAN, theme::TRACK);
    if (pct > 0.3f) c.arc(120, 120, r, thick, GAUGE_START, GAUGE_SPAN * pct / 100.0f, color);
}

static void dot(Canvas& c, float r, float angle, int size, uint32_t color) {
    c.fillCircle((int)lroundf(Canvas::px(120, r, angle)), (int)lroundf(Canvas::py(120, r, angle)), size, color);
}

static String hm(int minutes) {
    char b[16];
    snprintf(b, sizeof(b), "%02d:%02d", minutes / 60, minutes % 60);
    return b;
}

// ================================================================= Home

class HomeScreen : public Screen {
public:
    bool isHome() override { return true; }
    uint32_t refreshMs() override {
        BlindState s = blinds.status().state;
        return (s == BlindState::Moving || s == BlindState::MeasuringRatio) ? 40 : 1000;
    }

    void onTurn(int d, int accel) override {
        if (!blinds.status().calibrated) {
            ui::toast("Calibrate first", theme::ACCENT);
            return;
        }
        ui::push(createDialScreen(d, accel));
    }

    void onClick() override {
        if (blinds.status().state == BlindState::Error) {
            blinds.clearError();
            ui::toast("Error cleared", theme::OK);
            return;
        }
        ui::push(createMainMenu());
    }

    void onLongPress() override {
        BlindStatus st = blinds.status();
        if (st.state == BlindState::Moving) {
            blinds.stop(CmdSource::Knob);
            ui::toast("Stopped");
        } else if (!blinds.toggle(CmdSource::Knob)) {
            ui::toast("Calibrate first", theme::ACCENT);
        } else {
            ui::toast(st.percent >= 50 ? "Closing" : "Opening", theme::ACCENT);
        }
    }

    void draw(Canvas& c) override {
        c.clear(theme::BG);
        if (net::state() == net::State::Portal) {
            drawPortal(c);
            return;
        }
        BlindStatus st = blinds.status();
        bool err = st.state == BlindState::Error;

        // ---- outer position ring
        if (err) {
            c.arc(120, 120, 118, 7, GAUGE_START, GAUGE_SPAN, Canvas::mix(theme::TRACK, theme::BAD, 0.45f));
        } else if (st.calibrated) {
            drawGauge(c, 118, 7, st.percent, theme::ACCENT);
            if (st.state == BlindState::Moving && st.targetPercent >= 0)
                dot(c, 114.5f, gaugeAngle(st.targetPercent), 5, theme::SKY);
        } else {
            c.arc(120, 120, 118, 7, GAUGE_START, GAUGE_SPAN, theme::TRACK);
        }

        // ---- date + clock
        char buf[40], ampm[4];
        time_t now = time(nullptr);
        struct tm t;
        localtime_r(&now, &t);
        if (net::timeValid()) {
            strftime(buf, sizeof(buf), "%a %d %b", &t);
            c.text(buf, 120, 56, Font::Small, theme::MUTED);
        } else if (net::state() != net::State::Connected) {
            c.text(ICON_WIFI, 120, 52, Font::Icon, theme::BAD);
        }
        ui::formatClock(buf, sizeof(buf), ampm, sizeof(ampm));
        int w = c.text(buf, 120, 108, Font::Big, theme::TEXT);
        if (ampm[0]) c.text(ampm, 120 + w / 2 + 4, 126, Font::Small, theme::MUTED, lgfx::middle_left);

        // ---- middle row: error, or weather + next schedule
        if (err) {
            c.text(ICON_WARNING, 120, 160, Font::Icon, theme::BAD);
            c.textFit(Blinds::errorName(st.error), 120, 185, 180, Font::Body, theme::BAD);
            c.text("click to clear", 120, 208, Font::Small, theme::MUTED);
        } else {
            WeatherData wd = services::weather();
            if (wd.valid) {
                char temp[16], range[24];
                snprintf(temp, sizeof(temp), "%.0f°", wd.temp);
                snprintf(range, sizeof(range), "%.0f° / %.0f°", wd.tMax, wd.tMin);
                int wt = c.textWidth(temp, Font::Body), wr = c.textWidth(range, Font::Small);
                int total = 26 + 6 + wt + 10 + wr;
                int x = 120 - total / 2;
                ui::drawWeatherIcon(c, x + 13, 156, wd.code, wd.isDay, false);
                c.text(temp, x + 32, 157, Font::Body, theme::TEXT, lgfx::middle_left);
                c.text(range, x + 32 + wt + 10, 158, Font::Small, theme::MUTED, lgfx::middle_left);
            }
            scheduler::Next n = scheduler::next();
            if (n.valid) {
                struct tm nt;
                localtime_r(&n.when, &nt);
                bool today = nt.tm_yday == t.tm_yday;
                char when[24];
                if (today) strftime(when, sizeof(when), "%H:%M", &nt);
                else strftime(when, sizeof(when), "%a %H:%M", &nt);
                snprintf(buf, sizeof(buf), "%s  → %d%%", when, n.position);
                // "→" is not in the font: draw arrow with an icon instead
                snprintf(buf, sizeof(buf), "%s", when);
                char pos[8];
                snprintf(pos, sizeof(pos), "%d%%", n.position);
                int w1 = c.textWidth(buf, Font::Small), w2 = c.textWidth(pos, Font::Small);
                int total = 24 + w1 + 28 + w2;
                int x = 120 - total / 2;
                c.text(ICON_BELL, x + 9, 186, Font::Icon, theme::DIM);
                c.text(buf, x + 24, 187, Font::Small, theme::MUTED, lgfx::middle_left);
                c.text(n.position >= 50 ? ICON_ARROW_UP : ICON_ARROW_DOWN, x + 24 + w1 + 14, 186, Font::Icon,
                       theme::DIM);
                c.text(pos, x + 24 + w1 + 28, 187, Font::Small, theme::MUTED, lgfx::middle_left);
            }
        }

        // ---- position readout in the gauge gap (hidden while showing an error)
        if (err) return;
        if (st.calibrated) {
            snprintf(buf, sizeof(buf), "%d%%", (int)lroundf(st.percent));
            if (st.state == BlindState::Moving) {
                int tw = c.textWidth(buf, Font::Body);
                c.text(st.direction > 0 ? ICON_ARROW_UP : ICON_ARROW_DOWN, 120 - tw / 2 - 10, 220, Font::Icon,
                       theme::ACCENT);
                c.text(buf, 120 + 8, 221, Font::Body, theme::TEXT);
            } else {
                c.text(buf, 120, 221, Font::Body, theme::TEXT);
            }
        } else {
            c.text("not calibrated", 120, 221, Font::Small, theme::ACCENT);
        }
    }

private:
    void drawPortal(Canvas& c) {
        c.ring(120, 120, 118, 4, theme::SKY);
        c.text(ICON_WIFI, 120, 62, Font::IconBig, theme::SKY);
        c.text("WiFi setup", 120, 112, Font::Title, theme::TEXT);
        c.text("Join the network", 120, 142, Font::Small, theme::MUTED);
        c.text(net::portalName(), 120, 166, Font::Body, theme::ACCENT);
        c.text("then open 192.168.4.1", 120, 192, Font::Small, theme::MUTED);
    }
};

Screen* createHomeScreen() { return new HomeScreen(); }

// ================================================================= Dial

class DialScreen : public Screen {
public:
    DialScreen(int d, int accel) : _initD(d), _initAccel(accel) {}

    void enter() override {
        if (_entered) return;
        _entered = true;
        BlindStatus st = blinds.status();
        _target = st.targetPercent >= 0 ? st.targetPercent : lroundf(st.percent);
        _lastTurn = millis();
        if (_initD) onTurn(_initD, _initAccel);
    }

    void onTurn(int d, int accel) override {
        int step = settings::get().knobStepPct;
        _target = constrain(_target + d * step * accel, 0, 100);
        _lastTurn = millis();
        _committed = false;
    }

    void onClick() override {
        BlindStatus st = blinds.status();
        if (_committed && st.state == BlindState::Moving) {
            blinds.stop(CmdSource::Knob);
            _target = lroundf(st.percent);
            ui::toast("Stopped");
        } else {
            commit();
        }
    }

    void update() override {
        uint32_t now = millis();
        if (!_committed && now - _lastTurn > 650) commit();
        BlindStatus st = blinds.status();
        if (_committed && st.state != BlindState::Moving && now - _lastTurn > 5000) ui::pop();
    }

    uint32_t refreshMs() override { return blinds.status().state == BlindState::Moving ? 40 : 250; }
    bool keepAwake() override { return true; }

    void draw(Canvas& c) override {
        c.clear(theme::BG);
        BlindStatus st = blinds.status();
        drawGauge(c, 118, 10, _target, theme::ACCENT);
        // actual position on a thin inner track
        c.arc(120, 120, 101, 3, GAUGE_START, GAUGE_SPAN, theme::SURFACE);
        if (st.percent > 0.3f) c.arc(120, 120, 101, 3, GAUGE_START, GAUGE_SPAN * st.percent / 100, theme::MUTED);
        dot(c, 113, gaugeAngle(_target), 8, theme::TEXT);
        c.text(ICON_ARROW_DOWN, (int)Canvas::px(120, 90, 222), (int)Canvas::py(120, 90, 222), Font::Icon, theme::DIM);
        c.text(ICON_ARROW_UP, (int)Canvas::px(120, 90, 138), (int)Canvas::py(120, 90, 138), Font::Icon, theme::DIM);

        const char* label = !_committed ? "Move to"
                            : st.state == BlindState::Moving ? (st.direction > 0 ? "Opening" : "Closing")
                                                               : (_target >= 100 ? "Open" : _target <= 0 ? "Closed" : "Position");
        c.text(label, 120, 66, Font::Body, theme::MUTED);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", _target);
        int w = c.text(buf, 114, 118, Font::Big, theme::TEXT);
        c.text("%", 114 + w / 2 + 4, 104, Font::Title, theme::MUTED, lgfx::middle_left);
        snprintf(buf, sizeof(buf), "%d%%", (int)lroundf(st.percent));
        char now[20];
        snprintf(now, sizeof(now), "now %s", buf);
        c.text(now, 120, 166, Font::Small, theme::DIM);
        if (_committed && st.state == BlindState::Moving) c.text("click to stop", 120, 190, Font::Small, theme::DIM);
    }

private:
    void commit() {
        _committed = true;
        if (!blinds.moveToPercent(_target, 0, CmdSource::Knob)) ui::toast("Calibrate first", theme::ACCENT);
    }
    int _initD, _initAccel;
    bool _entered = false, _committed = true;
    int _target = 0;
    uint32_t _lastTurn = 0;
};

Screen* createDialScreen(int d, int accel) { return new DialScreen(d, accel); }

// ============================================================== Weather

class WeatherScreen : public Screen {
public:
    uint32_t refreshMs() override { return 5000; }
    void onClick() override { ui::pop(); }
    void draw(Canvas& c) override {
        c.clear(theme::BG);
        Settings s = settings::get();
        WeatherData w = services::weather();
        int rise, set;
        if (net::timeValid() && sun::times(time(nullptr), s.latitude, s.longitude, rise, set)) {
            String a = hm(rise), b = hm(set);
            c.text(ICON_SUN, 76, 36, Font::Icon, theme::ACCENT);
            c.text(a.c_str(), 90, 37, Font::Small, theme::MUTED, lgfx::middle_left);
            c.text(ICON_MOON, 140, 36, Font::Icon, theme::CLOUD);
            c.text(b.c_str(), 154, 37, Font::Small, theme::MUTED, lgfx::middle_left);
        }
        if (!w.valid) {
            c.text(ICON_CLOUD, 120, 96, Font::IconBig, theme::DIM);
            c.text(s.weatherEnabled ? (net::connected() ? "Loading..." : "No WiFi") : "Weather is off", 120, 150,
                   Font::Body, theme::MUTED);
            return;
        }
        ui::drawWeatherIcon(c, 120, 72, w.code, w.isDay, true);
        char buf[40];
        snprintf(buf, sizeof(buf), "%.0f°", w.temp);
        c.text(buf, 126, 122, Font::Big, theme::TEXT);
        c.textFit(weather::describe(w.code), 120, 160, 200, Font::Body, theme::TEXT);
        snprintf(buf, sizeof(buf), "High %.0f°   Low %.0f°", w.tMax, w.tMin);
        c.text(buf, 120, 185, Font::Small, theme::MUTED);
        if (w.precipProb >= 0) snprintf(buf, sizeof(buf), "Rain %d%%  ·  %.0f km/h", w.precipProb, w.wind);
        else snprintf(buf, sizeof(buf), "Humidity %d%%  ·  %.0f km/h", w.humidity, w.wind);
        c.textFit(buf, 120, 206, 160, Font::Small, theme::DIM);
    }
};

Screen* createWeatherScreen() { return new WeatherScreen(); }

// ================================================================== Bus

class BusScreen : public Screen {
public:
    void enter() override { services::requestBus(); }
    void update() override {
        if (millis() - _lastReq > 10000) {
            _lastReq = millis();
            services::requestBus();
        }
    }
    uint32_t refreshMs() override { return 1000; }
    bool keepAwake() override { return true; }
    void onTurn(int d, int) override {
        BusData b = services::bus();
        _scroll = constrain(_scroll + d, 0, max(0, (int)b.count - 3));
    }
    void onClick() override {
        services::refreshBus();
        ui::toast("Refreshing...");
    }

    void draw(Canvas& c) override {
        c.clear(theme::BG);
        Settings s = settings::get();
        if (!s.busStop[0]) {
            c.text(ICON_BUS, 120, 84, Font::IconBig, theme::DIM);
            c.text("No bus stop set", 120, 138, Font::Body, theme::TEXT);
            c.text("add it in the web app", 120, 164, Font::Small, theme::MUTED);
            return;
        }
        BusData b = services::bus();
        int hw = c.textWidth(s.busStop, Font::Small);
        c.text(ICON_BUS, 120 - hw / 2 - 6, 36, Font::Icon, theme::ACCENT);
        c.text(s.busStop, 120 + 12, 37, Font::Small, theme::MUTED);
        if (b.stopName[0]) c.textFit(b.stopName, 120, 60, 170, Font::Small, theme::DIM);

        if (!b.valid || !b.count) {
            const char* msg = b.message[0] ? b.message : (net::connected() ? "Loading..." : "No WiFi");
            c.textFit(msg, 120, 124, 190, Font::Body, theme::MUTED);
            return;
        }
        for (int i = 0; i < 3 && _scroll + i < b.count; i++) {
            const BusArrival& a = b.items[_scroll + i];
            int y = 98 + i * 40;
            char mins[24], dist[16];
            if (a.minMin == a.maxMin) snprintf(mins, sizeof(mins), "%d min", a.minMin);
            else snprintf(mins, sizeof(mins), "%d-%d min", a.minMin, a.maxMin);
            snprintf(dist, sizeof(dist), "%.1f km", a.meters / 1000.0f);
            c.text(a.service, 38, y, Font::Title, theme::ACCENT, lgfx::middle_left);
            c.text(mins, 202, y - 6, Font::Body, a.minMin <= 3 ? theme::OK : theme::TEXT, lgfx::middle_right);
            c.text(dist, 202, y + 13, Font::Small, theme::DIM, lgfx::middle_right);
        }
        if (b.updated && net::timeValid()) {
            char ago[24];
            snprintf(ago, sizeof(ago), "updated %lds ago", (long)(time(nullptr) - b.updated));
            c.text(ago, 120, 218, Font::Small, theme::DIM);
        }
    }

private:
    int _scroll = 0;
    uint32_t _lastReq = 0;
};

Screen* createBusScreen() { return new BusScreen(); }

// ================================================================= Info

class InfoScreen : public Screen {
public:
    uint32_t refreshMs() override { return 500; }
    bool keepAwake() override { return true; }
    void onTurn(int d, int) override { _page = ((_page + d) % 3 + 3) % 3; }
    void onClick() override { ui::pop(); }

    void draw(Canvas& c) override {
        c.clear(theme::BG);
        static const char* titles[] = {"Network", "Blinds", "System"};
        c.text(titles[_page], 120, 44, Font::Body, theme::ACCENT);
        _row = 0;
        char b[48];
        if (_page == 0) {
            line(c, "State", net::stateName(net::state()));
            line(c, "SSID", net::ssid().c_str());
            line(c, "IP", net::ip().c_str());
            snprintf(b, sizeof(b), "%d dBm", net::rssi());
            line(c, "Signal", b);
            snprintf(b, sizeof(b), "%s.local", settings::get().hostname);
            line(c, "Name", b);
            line(c, "Time", net::timeValid() ? "synced" : "not synced");
        } else if (_page == 1) {
            BlindStatus st = blinds.status();
            line(c, "State", Blinds::stateName(st.state));
            snprintf(b, sizeof(b), "%ld", (long)st.ticks);
            line(c, "Ticks", b);
            snprintf(b, sizeof(b), "%u", st.rawAngle);
            line(c, "Raw", b);
            snprintf(b, sizeof(b), "%ld", (long)st.motorSteps);
            line(c, "Steps", b);
            snprintf(b, sizeof(b), "%.4f", st.stepsPerTick);
            line(c, "Ratio", b);
            line(c, "Encoder", st.encoderOk ? "OK" : "MISSING");
        } else {
            line(c, "Firmware", FW_VERSION);
            uint32_t up = millis() / 1000;
            snprintf(b, sizeof(b), "%lud %02lu:%02lu", (unsigned long)(up / 86400), (unsigned long)(up / 3600 % 24),
                     (unsigned long)(up / 60 % 60));
            line(c, "Uptime", b);
            snprintf(b, sizeof(b), "%lu KB", (unsigned long)(ESP.getFreeHeap() / 1024));
            line(c, "Free RAM", b);
            BlindStatus st = blinds.status();
            line(c, "Driver", st.tmcUart ? "UART" : "standalone");
            line(c, "Mode", st.simulated ? "SIMULATED" : "hardware");
            snprintf(b, sizeof(b), "%lu ms", (unsigned long)ui::display().lastFrameMs());
            line(c, "Frame", b);
        }
        for (int i = 0; i < 3; i++) c.fillCircle(108 + i * 12, 222, i == _page ? 4 : 2, i == _page ? theme::ACCENT : theme::DIM);
    }

private:
    void line(Canvas& c, const char* k, const char* v) {
        int y = 76 + _row++ * 24;
        c.text(k, 112, y, Font::Small, theme::MUTED, lgfx::middle_right);
        c.textFit(v, 122, y, 96, Font::Small, theme::TEXT, lgfx::middle_left);
    }
    int _page = 0, _row = 0;
};

Screen* createInfoScreen() { return new InfoScreen(); }

// ======================================================= Calibration: jog

class JogScreen : public Screen {
public:
    JogScreen(bool open, bool wizard) : _open(open), _wizard(wizard) {}
    void enter() override {
        if (!_started) _startTicks = blinds.status().ticks;
        _started = true;
    }
    bool keepAwake() override { return true; }
    uint32_t refreshMs() override { return blinds.status().state == BlindState::Moving ? 50 : 300; }

    void onTurn(int d, int accel) override { blinds.jog((int32_t)d * 256 * accel); }

    void onClick() override {
        blinds.stop(CmdSource::Knob);
        if (_open) blinds.setOpenHere();
        else blinds.setClosedHere();
        ui::toast(_open ? "Open position saved" : "Closed position saved", theme::OK);
        ui::pop();
        if (_wizard) {
            if (!_open) ui::push(createJogScreen(true, true));
            else {
                ui::home();
                ui::toast("Calibration done!", theme::OK, 2500);
            }
        }
    }

    void onLongPress() override {
        blinds.stop(CmdSource::Knob);
        ui::pop();
    }

    void draw(Canvas& c) override {
        c.clear(theme::BG);
        BlindStatus st = blinds.status();
        uint32_t col = _open ? theme::ACCENT : theme::SKY;
        c.ring(120, 120, 118, 3, theme::TRACK);
        // spinner showing the shaft angle
        float a = st.rawAngle * 360.0f / ENCODER_CPR;
        c.arc(120, 120, 118, 6, a - 20, 40, col);
        if (_wizard) c.text(_open ? "Step 3 of 3" : "Step 2 of 3", 120, 34, Font::Small, theme::DIM);
        c.text(_open ? ICON_ARROW_UP : ICON_ARROW_DOWN, 120, 62, Font::Icon, col);
        c.text(_open ? "Fully OPEN" : "Fully CLOSED", 120, 90, Font::Title, theme::TEXT);
        c.text("turn the knob to move", 120, 118, Font::Small, theme::MUTED);
        char b[24];
        snprintf(b, sizeof(b), "%+.2f turns", (st.ticks - _startTicks) / (float)ENCODER_CPR);
        c.text(b, 120, 150, Font::Body, theme::TEXT);
        c.text("click to save", 120, 182, Font::Small, theme::DIM);
        c.text("hold to cancel", 120, 202, Font::Small, theme::DIM);
    }

private:
    bool _open, _wizard, _started = false;
    int32_t _startTicks = 0;
};

Screen* createJogScreen(bool open, bool wizard) { return new JogScreen(open, wizard); }

// ====================================================== Calibration: ratio

class RatioScreen : public Screen {
public:
    explicit RatioScreen(bool wizard) : _wizard(wizard) {}
    bool keepAwake() override { return true; }
    uint32_t refreshMs() override { return _phase == 1 ? 50 : 500; }

    void onClick() override {
        if (_phase == 0) {
            blinds.measureRatio();
            _phase = 1;
            _since = millis();
        } else if (_phase == 2) {
            ui::pop();
            if (_wizard && _ok) ui::push(createJogScreen(false, true));
        }
    }

    void update() override {
        if (_phase != 1) return;
        BlindStatus st = blinds.status();
        if (st.state == BlindState::MeasuringRatio) _seen = true;
        if ((_seen || millis() - _since > 1500) && st.state != BlindState::MeasuringRatio) {
            _phase = 2;
            _ok = st.state != BlindState::Error;
            _ratio = st.stepsPerTick;
            ui::invalidate();
        }
    }

    void draw(Canvas& c) override {
        c.clear(theme::BG);
        if (_wizard) c.text("Step 1 of 3", 120, 34, Font::Small, theme::DIM);
        if (_phase == 0) {
            c.text(ICON_GAUGE, 120, 70, Font::IconBig, theme::ACCENT);
            c.text("Motor ratio", 120, 116, Font::Title, theme::TEXT);
            c.text("The motor turns about 2", 120, 146, Font::Small, theme::MUTED);
            c.text("revolutions and back.", 120, 166, Font::Small, theme::MUTED);
            c.text("click to start", 120, 196, Font::Small, theme::DIM);
        } else if (_phase == 1) {
            float a = (millis() % 1200) * 0.3f;
            c.ring(120, 120, 110, 8, theme::TRACK);
            c.arc(120, 120, 110, 8, a, 90, theme::ACCENT);
            c.text("Measuring...", 120, 120, Font::Title, theme::TEXT);
        } else {
            c.text(_ok ? ICON_CHECK : ICON_WARNING, 120, 72, Font::IconBig, _ok ? theme::OK : theme::BAD);
            if (_ok) {
                char b[32];
                snprintf(b, sizeof(b), "%.4f", _ratio);
                c.text(b, 120, 122, Font::Title, theme::TEXT);
                c.text("steps per tick", 120, 150, Font::Small, theme::MUTED);
            } else {
                c.text("Failed", 120, 122, Font::Title, theme::BAD);
                c.text("check magnet / motor", 120, 150, Font::Small, theme::MUTED);
            }
            c.text(_wizard && _ok ? "click to continue" : "click to close", 120, 190, Font::Small, theme::DIM);
        }
    }

private:
    bool _wizard, _seen = false, _ok = false;
    int _phase = 0;
    uint32_t _since = 0;
    float _ratio = 0;
};

Screen* createRatioScreen(bool wizard) { return new RatioScreen(wizard); }
