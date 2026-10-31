#include "canvas.h"
#include "font_data.h"

static lgfx::VLWfont s_fonts[(int)Font::COUNT];
static lgfx::PointerWrapper s_data[(int)Font::COUNT];
static bool s_loaded = false;

bool fontsLoad() {
    if (s_loaded) return true;
    const uint8_t* arrays[(int)Font::COUNT] = {font_small, font_body, font_title, font_big, font_icon, font_icon_big};
    bool ok = true;
    for (int i = 0; i < (int)Font::COUNT; i++) {
        s_data[i].set(arrays[i]);
        ok &= s_fonts[i].loadFont(&s_data[i]);
    }
    s_loaded = ok;
    return ok;
}

const lgfx::IFont* fontPtr(Font f) {
    if (!s_loaded) fontsLoad();
    return &s_fonts[(int)f];
}
