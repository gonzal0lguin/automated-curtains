// Menu tree of the knob UI.
#include "blinds.h"
#include "connectivity.h"
#include "scheduler.h"
#include "screens.h"
#include "settings.h"

using namespace menu;

static void set(const std::function<void(Settings&)>& fn) { settings::update(fn); }
static Settings cfg() { return settings::get(); }

static String pct(int v) { return String(v) + "%"; }

// =============================================================== Blinds

static Screen* blindsMenu() {
    return new MenuScreen("Blinds", [](std::vector<MenuItem>& v) {
        auto moveTo = [](float p, bool gentle) {
            uint16_t d = gentle ? cfg().gentleSeconds : 0;
            if (blinds.moveToPercent(p, d, CmdSource::Knob)) ui::home();
            else ui::toast("Calibrate first", theme::ACCENT);
        };
        v.push_back(item(ICON_ARROW_UP, "Open", [moveTo]() { moveTo(100, false); }));
        v.push_back(item(ICON_ARROW_DOWN, "Close", [moveTo]() { moveTo(0, false); }));
        v.push_back(item(ICON_STOP, "Stop", []() {
            blinds.stop(CmdSource::Knob);
            ui::toast("Stopped");
        }));
        v.push_back(sub(ICON_SLIDERS, "Set position", []() { return createDialScreen(); }));
        v.push_back(item(ICON_COFFEE, "Gentle open", [moveTo]() { moveTo(100, true); },
                         []() { return seconds(cfg().gentleSeconds); }));
        v.push_back(item(ICON_BED, "Gentle close", [moveTo]() { moveTo(0, true); },
                         []() { return seconds(cfg().gentleSeconds); }));
    });
}

// ============================================================ Schedules

static Screen* daysMenu(int i) {
    return new MenuScreen("Days", [i](std::vector<MenuItem>& v) {
        static const char* names[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
        for (int d = 0; d < 7; d++) {
            v.push_back(toggle(nullptr, names[d], [i, d]() { return (cfg().schedules[i].days >> d) & 1; },
                               [i, d](bool on) {
                                   set([&](Settings& s) {
                                       if (on) s.schedules[i].days |= 1 << d;
                                       else s.schedules[i].days &= ~(1 << d);
                                   });
                               }));
        }
        auto preset = [i](const char* label, uint8_t mask) {
            return item(nullptr, label, [i, mask]() {
                set([&](Settings& s) { s.schedules[i].days = mask; });
                ui::toast("Saved", theme::OK, 800);
            });
        };
        v.push_back(preset("Every day", 0x7F));
        v.push_back(preset("Weekdays", 0x1F));
        v.push_back(preset("Weekends", 0x60));
    });
}

static Screen* scheduleMenu(int i) {
    char title[24];
    snprintf(title, sizeof(title), "Schedule %d", i + 1);
    return new MenuScreen(title, [i](std::vector<MenuItem>& v) {
        v.push_back(toggle(ICON_POWER, "Enabled", [i]() { return cfg().schedules[i].enabled; },
                           [i](bool on) { set([&](Settings& s) { s.schedules[i].enabled = on; }); }));

        v.push_back(item(ICON_SUN, "Trigger", [i]() {
            set([&](Settings& s) {
                Schedule& sc = s.schedules[i];
                sc.trigger = (ScheduleTrigger)(((int)sc.trigger + 1) % 3);
                sc.minutes = sc.trigger == ScheduleTrigger::Time ? 7 * 60 : 0;
            });
            ui::invalidate();
        }, [i]() {
            switch (cfg().schedules[i].trigger) {
                case ScheduleTrigger::Sunrise: return String("At sunrise");
                case ScheduleTrigger::Sunset: return String("At sunset");
                default: return String("At a time");
            }
        }));

        MenuItem when = edit(ICON_CLOCK, "Time", [i]() {
            Schedule sc = cfg().schedules[i];
            EditSpec e;
            e.value = sc.minutes;
            e.save = [i](int m) { set([&](Settings& s) { s.schedules[i].minutes = m; }); };
            if (sc.trigger == ScheduleTrigger::Time) {
                e.title = "Time";
                e.min = 0;
                e.max = 24 * 60 - 5;
                e.step = 5;
                e.wrap = true;
                e.format = [](int m) {
                    char b[16];
                    snprintf(b, sizeof(b), "%02d:%02d", m / 60, m % 60);
                    return String(b);
                };
            } else {
                e.title = sc.trigger == ScheduleTrigger::Sunrise ? "Minutes after sunrise" : "Minutes after sunset";
                e.min = -180;
                e.max = 180;
                e.step = 5;
                e.format = [](int m) { return String(m > 0 ? "+" : "") + String(m); };
            }
            return e;
        }, [i]() {
            char b[24];
            scheduler::describe(cfg().schedules[i], b, sizeof(b));
            return String(b);
        });
        when.labelFn = [i]() { return String(cfg().schedules[i].trigger == ScheduleTrigger::Time ? "Time" : "Offset"); };
        v.push_back(when);

        v.push_back(edit(ICON_SLIDERS, "Position", [i]() {
            EditSpec e;
            e.title = "Move blind to";
            e.value = cfg().schedules[i].position;
            e.min = 0;
            e.max = 100;
            e.step = 5;
            e.format = pct;
            e.save = [i](int p) { set([&](Settings& s) { s.schedules[i].position = p; }); };
            return e;
        }, [i]() { return pct(cfg().schedules[i].position); }));

        v.push_back(sub(ICON_CALENDAR, "Days", [i]() { return daysMenu(i); }, [i]() {
            char b[32];
            scheduler::daysString(cfg().schedules[i].days, b, sizeof(b));
            return String(b);
        }));

        v.push_back(toggle(ICON_COFFEE, "Gentle", [i]() { return cfg().schedules[i].gentle; },
                           [i](bool on) { set([&](Settings& s) { s.schedules[i].gentle = on; }); }));
    });
}

static Screen* schedulesMenu() {
    return new MenuScreen("Schedules", [](std::vector<MenuItem>& v) {
        v.push_back(toggle(ICON_PAUSE, "Pause all", []() { return cfg().schedulesPaused; },
                           [](bool on) { set([&](Settings& s) { s.schedulesPaused = on; }); }));
        for (int i = 0; i < MAX_SCHEDULES; i++) {
            MenuItem m = sub(ICON_BELL, "", [i]() { return scheduleMenu(i); }, [i]() {
                Schedule sc = cfg().schedules[i];
                char days[24], b[48];
                scheduler::daysString(sc.days, days, sizeof(days));
                snprintf(b, sizeof(b), "%s · %d%% · %s", days, sc.position, sc.enabled ? "on" : "off");
                return String(b);
            });
            m.labelFn = [i]() {
                char b[24];
                scheduler::describe(cfg().schedules[i], b, sizeof(b));
                return String(b);
            };
            v.push_back(m);
        }
    });
}

// ============================================================ Calibrate

static Screen* calibrateMenu() {
    return new MenuScreen("Calibrate", [](std::vector<MenuItem>& v) {
        v.push_back(sub(ICON_PLAY, "Setup wizard", []() { return createRatioScreen(true); },
                        []() { return String(cfg().calibrated() ? "calibrated" : "not calibrated"); }));
        v.push_back(sub(ICON_ARROW_DOWN, "Closed position", []() { return createJogScreen(false, false); },
                        []() { return String(cfg().closedSet ? "set" : "not set"); }));
        v.push_back(sub(ICON_ARROW_UP, "Open position", []() { return createJogScreen(true, false); },
                        []() { return String(cfg().openSet ? "set" : "not set"); }));
        v.push_back(sub(ICON_GAUGE, "Motor ratio", []() { return createRatioScreen(false); }, []() {
            Settings s = cfg();
            char b[24];
            snprintf(b, sizeof(b), "%.4f%s", s.stepsPerTick, s.ratioMeasured ? "" : " (guess)");
            return String(b);
        }));
        v.push_back(toggle(ICON_REFRESH, "Reverse motor", []() { return cfg().motorReversed; },
                           [](bool on) { set([&](Settings& s) { s.motorReversed = on; }); }));
        v.push_back(confirm(ICON_UNDO, "Reset", "Forget end positions?", []() {
            blinds.resetCalibration();
            ui::toast("Calibration reset", theme::ACCENT);
        }));
    });
}

// ============================================================= Settings

static EditSpec numberSpec(const char* title, int value, int lo, int hi, int step, std::function<void(int)> save,
                           std::function<String(int)> fmt = nullptr) {
    EditSpec e;
    e.title = title;
    e.value = value;
    e.min = lo;
    e.max = hi;
    e.step = step;
    e.save = std::move(save);
    e.format = std::move(fmt);
    return e;
}

static Screen* displayMenu() {
    return new MenuScreen("Display", [](std::vector<MenuItem>& v) {
        v.push_back(edit(ICON_BULB, "Brightness", []() {
            int orig = cfg().brightness;
            EditSpec e = numberSpec("Brightness", orig, 5, 255, 10, [](int b) {
                set([&](Settings& s) { s.brightness = b; });
            }, [](int b) { return String(b * 100 / 255) + "%"; });
            // live preview without writing flash on every detent
            e.preview = [](int b) { settings::update([&](Settings& s) { s.brightness = b; }, false); };
            e.cancel = [orig]() { settings::update([&](Settings& s) { s.brightness = orig; }, false); };
            return e;
        }, []() { return String(cfg().brightness * 100 / 255) + "%"; }));
        v.push_back(edit(ICON_MOON, "Dim level", []() {
            return numberSpec("Dim level", cfg().dimBrightness, 0, 120, 5,
                              [](int b) { set([&](Settings& s) { s.dimBrightness = b; }); },
                              [](int b) { return String(b * 100 / 255) + "%"; });
        }, []() { return String(cfg().dimBrightness * 100 / 255) + "%"; }));
        v.push_back(edit(ICON_HOURGLASS, "Dim after", []() {
            return numberSpec("Dim after", cfg().dimAfterS, 0, 600, 5,
                              [](int t) { set([&](Settings& s) { s.dimAfterS = t; }); }, seconds);
        }, []() { return seconds(cfg().dimAfterS); }));
        v.push_back(edit(ICON_POWER, "Screen off after", []() {
            return numberSpec("Screen off after", cfg().offAfterS, 0, 3600, 30,
                              [](int t) { set([&](Settings& s) { s.offAfterS = t; }); }, seconds);
        }, []() { return seconds(cfg().offAfterS); }));
        v.push_back(item(ICON_REFRESH, "Rotation", []() {
            set([](Settings& s) { s.rotation = (s.rotation + 1) & 3; });
            ui::display().setRotation(cfg().rotation);
        }, []() { return String(cfg().rotation * 90) + "°"; }));
        v.push_back(toggle(ICON_CLOCK, "24 h clock", []() { return cfg().clock24h; },
                           [](bool on) { set([&](Settings& s) { s.clock24h = on; }); }));
    });
}

static Screen* knobMenu() {
    return new MenuScreen("Knob", [](std::vector<MenuItem>& v) {
        v.push_back(toggle(ICON_REFRESH, "Reverse knob", []() { return cfg().knobReversed; },
                           [](bool on) { set([&](Settings& s) { s.knobReversed = on; }); }));
        v.push_back(edit(ICON_SLIDERS, "Step per click", []() {
            return numberSpec("Step per click", cfg().knobStepPct, 1, 10, 1,
                              [](int p) { set([&](Settings& s) { s.knobStepPct = p; }); }, pct);
        }, []() { return pct(cfg().knobStepPct); }));
    });
}

static Screen* motionMenu() {
    return new MenuScreen("Motion", [](std::vector<MenuItem>& v) {
        v.push_back(edit(ICON_GAUGE, "Max speed", []() {
            return numberSpec("Max speed (steps/s)", cfg().maxSpeed, 200, 20000, 100,
                              [](int x) { set([&](Settings& s) { s.maxSpeed = x; }); });
        }, []() { return String(cfg().maxSpeed) + " steps/s"; }));
        v.push_back(edit(ICON_GAUGE, "Acceleration", []() {
            return numberSpec("Acceleration", cfg().acceleration, 100, 30000, 100,
                              [](int x) { set([&](Settings& s) { s.acceleration = x; }); });
        }, []() { return String(cfg().acceleration) + " steps/s²"; }));
        v.push_back(edit(ICON_COFFEE, "Gentle duration", []() {
            return numberSpec("Gentle move", cfg().gentleSeconds, 30, 1800, 30,
                              [](int x) { set([&](Settings& s) { s.gentleSeconds = x; }); }, seconds);
        }, []() { return seconds(cfg().gentleSeconds); }));
        v.push_back(toggle(ICON_MAGNET, "Hold when idle", []() { return cfg().holdWhenIdle; },
                           [](bool on) { set([&](Settings& s) { s.holdWhenIdle = on; }); }));
        if (blinds.status().tmcUart) {
            v.push_back(edit(ICON_BOLT, "Motor current", []() {
                return numberSpec("Current (mA RMS)", cfg().runCurrent, 200, 1500, 50,
                                  [](int x) { set([&](Settings& s) { s.runCurrent = x; }); },
                                  [](int x) { return String(x) + " mA"; });
            }, []() { return String(cfg().runCurrent) + " mA"; }));
            v.push_back(toggle(ICON_MOON, "Silent mode", []() { return cfg().stealthChop; },
                               [](bool on) { set([&](Settings& s) { s.stealthChop = on; }); }));
        }
    });
}

static Screen* wifiMenu() {
    return new MenuScreen("WiFi", [](std::vector<MenuItem>& v) {
        v.push_back(item(ICON_WIFI, "Status", []() {}, []() -> String {
            if (net::connected()) return net::ssid() + " · " + net::ip();
            return String(net::stateName(net::state()));
        }));
        v.push_back(confirm(ICON_GLOBE, "Setup portal", "Open WiFi setup?", []() {
            net::startPortal();
            ui::home();
        }, theme::ACCENT));
        v.push_back(confirm(ICON_TIMES, "Forget WiFi", "Forget saved WiFi?", []() { net::forgetWifi(); }));
    });
}

static Screen* systemMenu() {
    return new MenuScreen("System", [](std::vector<MenuItem>& v) {
        v.push_back(sub(ICON_INFO, "Info", createInfoScreen));
        v.push_back(confirm(ICON_REFRESH, "Reboot", "Reboot now?", []() {
            delay(200);
            ESP.restart();
        }, theme::ACCENT));
        v.push_back(confirm(ICON_WARNING, "Factory reset", "Erase everything?", []() {
            settings::factoryReset();
            delay(300);
            ESP.restart();
        }));
    });
}

static Screen* settingsMenu() {
    return new MenuScreen("Settings", [](std::vector<MenuItem>& v) {
        v.push_back(sub(ICON_BULB, "Display", displayMenu));
        v.push_back(sub(ICON_REFRESH, "Knob", knobMenu));
        v.push_back(sub(ICON_GAUGE, "Motion", motionMenu));
        v.push_back(sub(ICON_WIFI, "WiFi", wifiMenu, []() { return String(net::stateName(net::state())); }));
        v.push_back(sub(ICON_CHIP, "System", systemMenu, []() { return String("v" FW_VERSION); }));
    });
}

// ================================================================== Main

Screen* createMainMenu() {
    return new MenuScreen("Menu", [](std::vector<MenuItem>& v) {
        v.push_back(sub(ICON_UPDOWN, "Blinds", blindsMenu, []() -> String {
            BlindStatus st = blinds.status();
            if (!st.calibrated) return String("not calibrated");
            return String((int)lroundf(st.percent)) + "% open";
        }));
        v.push_back(sub(ICON_BUS, "Bus", createBusScreen, []() {
            Settings s = cfg();
            return String(s.busStop[0] ? s.busStop : "no stop set");
        }));
        v.push_back(sub(ICON_SUN, "Weather", createWeatherScreen));
        v.push_back(sub(ICON_CALENDAR, "Schedules", schedulesMenu, []() {
            if (cfg().schedulesPaused) return String("paused");
            scheduler::Next n = scheduler::next();
            if (!n.valid) return String("none upcoming");
            char b[24];
            struct tm t;
            localtime_r(&n.when, &t);
            strftime(b, sizeof(b), "next %a %H:%M", &t);
            return String(b);
        }));
        v.push_back(sub(ICON_CROSSHAIRS, "Calibrate", calibrateMenu));
        v.push_back(sub(ICON_COG, "Settings", settingsMenu));
    });
}
