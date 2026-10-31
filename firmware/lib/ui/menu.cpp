#include "menu.h"

#include <math.h>

static constexpr uint32_t ANIM_MS = 160;

// =============================================================== MenuScreen

MenuScreen::MenuScreen(const char* title, Builder build, bool backItem)
    : _title(title), _build(std::move(build)), _back(backItem) {}

void MenuScreen::enter() {
    _items.clear();
    _build(_items);
    if (_back) _items.push_back(menu::item(ICON_LEFT, "Back", []() { ui::pop(); }));
    if (_sel >= (int)_items.size()) _sel = _items.size() - 1;
    if (_sel < 0) _sel = 0;
}

float MenuScreen::offset() const {
    uint32_t t = millis() - _animStart;
    if (t >= ANIM_MS) return 0;
    float k = 1.0f - (float)t / ANIM_MS;
    return _from * k * k;  // ease-out
}

void MenuScreen::onTurn(int d, int) {
    int n = _items.size();
    if (!n) return;
    int next = ((_sel + d) % n + n) % n;
    _from = offset() + (next - _sel);  // keep motion continuous on fast turns
    if (fabsf(_from) > 2) _from = _from > 0 ? 2 : -2;
    _sel = next;
    _animStart = millis();
}

void MenuScreen::onClick() {
    if (_sel < (int)_items.size() && _items[_sel].action) _items[_sel].action();
}

uint32_t MenuScreen::refreshMs() { return millis() - _animStart < ANIM_MS + 20 ? 16 : 400; }

void MenuScreen::draw(Canvas& c) {
    c.clear(theme::BG);
    int n = _items.size();
    float off = offset();
    const float SP = 74;  // px between item slots

    c.text(_title.c_str(), 120, 24, Font::Small, theme::MUTED);

    // neighbours (labels only)
    for (int k = -2; k <= 2; k++) {
        // skip slots that would show the same item twice in short lists
        if (!k || !(2 * abs(k) < n || (k > 0 && 2 * k == n))) continue;
        int idx = ((_sel + k) % n + n) % n;
        float y = 120 + (k + off) * SP;
        if (y < 30 || y > 225) continue;
        float fade = 1.0f - fminf(fabsf(y - 120) / 140.0f, 1.0f);
        c.textFit(labelOf(_items[idx]).c_str(), 120, (int)y, 170, Font::Body,
                  Canvas::mix(theme::BG, theme::MUTED, fade * 1.3f));
    }

    // selected item, sliding in
    if (n) {
        const MenuItem& it = _items[_sel];
        int y = (int)lroundf(120 + off * SP);
        uint32_t col = Canvas::mix(theme::BG, it.color, 1.0f - fminf(fabsf(off), 1.0f) * 0.6f);
        String val = it.value ? it.value() : String();
        int labelY = it.icon ? y + 24 : y;
        if (it.icon) c.text(it.icon, 120, y - 26, Font::IconBig, Canvas::mix(theme::BG, theme::ACCENT, 1.0f - fminf(fabsf(off), 1.0f) * 0.6f));
        c.textFit(labelOf(it).c_str(), 120, labelY, 200, Font::Title, col);
        if (val.length()) c.textFit(val.c_str(), 120, labelY + 27, 180, Font::Small, theme::ACCENT);
    }

    // scroll position dots on the right edge
    if (n > 1) {
        float span = fminf(70.0f, n * 9.0f);
        for (int i = 0; i < n; i++) {
            float a = 90 - span / 2 + span * i / (n - 1);
            int x = (int)lroundf(Canvas::px(120, 111, a)), yy = (int)lroundf(Canvas::py(120, 111, a));
            if (i == _sel) c.fillCircle(x, yy, 4, theme::ACCENT);
            else c.fillCircle(x, yy, 2, theme::DIM);
        }
    }
}

// =============================================================== EditScreen

void EditScreen::onTurn(int d, int accel) {
    int range = _s.max - _s.min;
    int stepMul = (accel > 1 && range / _s.step > 40) ? accel : 1;
    int v = _s.value + d * _s.step * stepMul;
    if (_s.wrap) {
        int span = range + _s.step;
        v = ((v - _s.min) % span + span) % span + _s.min;
    } else {
        v = constrain(v, _s.min, _s.max);
    }
    if (v != _s.value) {
        _s.value = v;
        if (_s.preview) _s.preview(v);
    }
}

void EditScreen::onClick() {
    if (_s.save) _s.save(_s.value);
    ui::toast("Saved", theme::OK, 900);
    ui::pop();
}

void EditScreen::onLongPress() {
    if (_s.cancel) _s.cancel();
    ui::pop();
}

static bool bigFontCapable(const String& s) {
    for (size_t i = 0; i < s.length(); i++) {
        if (!strchr("0123456789:%-.+ APM", s[i]) && (uint8_t)s[i] != 0xC2 && (uint8_t)s[i] != 0xB0) return false;
    }
    return s.length() <= 6;
}

void EditScreen::draw(Canvas& c) {
    c.clear(theme::BG);
    float frac = _s.max > _s.min ? (float)(_s.value - _s.min) / (_s.max - _s.min) : 0;
    c.arc(120, 120, 116, 9, 225, 270, theme::TRACK);
    if (frac > 0.002f) c.arc(120, 120, 116, 9, 225, 270 * frac, theme::ACCENT);
    float a = 225 + 270 * frac;
    c.fillCircle((int)lroundf(Canvas::px(120, 111.5f, a)), (int)lroundf(Canvas::py(120, 111.5f, a)), 8, theme::TEXT);

    c.textFit(_s.title.c_str(), 120, 62, 170, Font::Body, theme::MUTED);
    String v = _s.format ? _s.format(_s.value) : String(_s.value);
    if (bigFontCapable(v)) c.text(v.c_str(), 120, 120, Font::Big, theme::TEXT);
    else c.textFit(v.c_str(), 120, 120, 190, Font::Title, theme::TEXT);
    c.text("click to save", 120, 176, Font::Small, theme::DIM);
}

// ===================================================================== menu

namespace menu {

MenuItem item(const char* icon, const char* label, std::function<void()> action, std::function<String()> value) {
    MenuItem m;
    m.icon = icon;
    m.label = label;
    m.action = std::move(action);
    m.value = std::move(value);
    return m;
}

MenuItem sub(const char* icon, const char* label, std::function<Screen*()> make, std::function<String()> value) {
    return item(icon, label, [make]() { ui::push(make()); }, std::move(value));
}

MenuItem toggle(const char* icon, const char* label, std::function<bool()> get, std::function<void(bool)> set) {
    return item(icon, label, [get, set]() { set(!get()); ui::invalidate(); }, [get]() { return onOff(get()); });
}

MenuItem edit(const char* icon, const char* label, std::function<EditSpec()> spec, std::function<String()> value) {
    return item(icon, label, [spec]() { ui::push(new EditScreen(spec())); }, std::move(value));
}

MenuItem confirm(const char* icon, const char* label, const char* question, std::function<void()> yes,
                 uint32_t color) {
    String q = question;
    MenuItem m = item(icon, label, [q, yes, color]() {
        ui::push(new MenuScreen(q.c_str(), [yes, color](std::vector<MenuItem>& v) {
            v.push_back(item(ICON_TIMES, "No", []() { ui::pop(); }));
            MenuItem y = item(ICON_CHECK, "Yes", [yes]() {
                ui::pop();
                yes();
            });
            y.color = color;
            v.push_back(y);
        }, false));
    });
    m.color = color;
    return m;
}

String onOff(bool v) { return v ? "On" : "Off"; }

String seconds(int s) {
    char b[24];
    if (s <= 0) return "Never";
    if (s < 60) snprintf(b, sizeof(b), "%d s", s);
    else if (s < 3600) {
        if (s % 60) snprintf(b, sizeof(b), "%d min %d s", s / 60, s % 60);
        else snprintf(b, sizeof(b), "%d min", s / 60);
    } else snprintf(b, sizeof(b), "%d h %02d", s / 3600, (s % 3600) / 60);
    return b;
}

}  // namespace menu
