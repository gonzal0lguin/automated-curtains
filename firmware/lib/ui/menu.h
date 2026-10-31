#pragma once
#include <functional>
#include <vector>

#include "ui.h"

struct MenuItem {
    const char* icon = nullptr;
    String label;
    std::function<String()> labelFn; // optional dynamic label (overrides label)
    std::function<String()> value;   // optional live value shown under the label
    std::function<void()> action;    // click
    uint32_t color = theme::TEXT;
};

// Vertical "wheel" list: the selected item is shown big in the middle with
// its icon; neighbours are hinted above and below.
class MenuScreen : public Screen {
public:
    using Builder = std::function<void(std::vector<MenuItem>&)>;
    MenuScreen(const char* title, Builder build, bool backItem = true);

    void enter() override;
    void onTurn(int d, int accel) override;
    void onClick() override;
    void draw(Canvas& c) override;
    uint32_t refreshMs() override;

private:
    String _title;
    Builder _build;
    bool _back;
    std::vector<MenuItem> _items;
    static String labelOf(const MenuItem& it) { return it.labelFn ? it.labelFn() : it.label; }
    int _sel = 0;
    float _from = 0;          // animation start offset (in items)
    uint32_t _animStart = 0;
    float offset() const;
};

// Edits one integer with an arc gauge. Click saves, long press cancels.
struct EditSpec {
    String title;
    int value = 0, min = 0, max = 100, step = 1;
    bool wrap = false;
    std::function<String(int)> format;   // default: plain number
    std::function<void(int)> preview;    // live while turning (optional)
    std::function<void(int)> save;
    std::function<void()> cancel;        // optional (undo a preview)
};

class EditScreen : public Screen {
public:
    explicit EditScreen(EditSpec spec) : _s(std::move(spec)) {}
    void onTurn(int d, int accel) override;
    void onClick() override;
    void onLongPress() override;
    void draw(Canvas& c) override;
    bool keepAwake() override { return true; }

private:
    EditSpec _s;
};

// Helpers to build common item types
namespace menu {
MenuItem item(const char* icon, const char* label, std::function<void()> action,
              std::function<String()> value = nullptr);
MenuItem sub(const char* icon, const char* label, std::function<Screen*()> make,
             std::function<String()> value = nullptr);
MenuItem toggle(const char* icon, const char* label, std::function<bool()> get, std::function<void(bool)> set);
MenuItem edit(const char* icon, const char* label, std::function<EditSpec()> spec,
              std::function<String()> value);
MenuItem confirm(const char* icon, const char* label, const char* question, std::function<void()> yes,
                 uint32_t color = theme::BAD);
String onOff(bool v);
String seconds(int s);    // "45 s", "5 min", "1 h 30"
}  // namespace menu
