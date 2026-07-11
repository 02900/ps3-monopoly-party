// Token theme names + image-asset registry. Compiled as C (gnu99).
#include "clay.h"
#include "clay_renderer.h"
#include "ui.h"
#include "theme.h"

static const char *THEME_NAMES[UI_THEME_COUNT] = {
    "Classic", "Fantasy", "Sci-Fi", "Ancient", "Prehistoric",
};

// 6 tokens per theme, per the Monopoly Party FAQ.
static const char *TOKENS[UI_THEME_COUNT][6] = {
    { "Car", "Boot", "Hat", "Iron", "Battleship", "Dog" },
    { "Dwarf", "Wizard", "Troll", "Knight", "Orc", "Elf" },
    { "Robot", "Rover", "Fighter", "Alien", "Astronaut", "UFO" },
    { "Boat", "Warship", "Urn", "Sandal", "Chariot", "Camel" },
    { "Caveman", "Triceratops", "Archaeoptx", "Mammoth", "Sabertooth", "Cavewoman" },
};

const char *ui_theme_name(int t) {
    return (t >= 0 && t < UI_THEME_COUNT) ? THEME_NAMES[t] : "?";
}
const char *ui_token_name(int t, int k) {
    if (t < 0 || t >= UI_THEME_COUNT || k < 0 || k >= 6) return "?";
    return TOKENS[t][k];
}

int ui_img_token(int theme, int token) {
    if (theme < 0 || theme >= UI_THEME_COUNT || token < 0 || token >= 6) return -1;
    return UI_IMG_TOKEN + theme * 6 + token;
}

// One clay_image_t per asset id. All placeholders (tex == NULL) until the real art
// is generated (docs/assets.md) and embedded via data/*.png (bin2o). Once embedded,
// ui_images_load() will ya2d_loadPNGfromBuffer() each and fill .tex/.src_w/.src_h,
// and the CLAY_IMAGE elements already referencing these slots start drawing.
static clay_image_t g_images[UI_IMG_COUNT];

void ui_images_load(void) {
    // Stub: no embedded art yet. Left as placeholders so CLAY_IMAGE slots render as
    // their background colour. Wire real PNGs here in the asset-integration pass.
}

void *ui_image(int id) {
    if (id < 0 || id >= UI_IMG_COUNT) return 0;
    return g_images[id].tex ? &g_images[id] : 0;   // NULL when not loaded -> Clay skips it
}
