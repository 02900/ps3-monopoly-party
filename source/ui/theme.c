// Token theme names + image-asset registry. Compiled as C (gnu99).
#include <ya2d/ya2d.h>
#include "clay.h"
#include "clay_renderer.h"
#include "ui.h"
#include "theme.h"

static const char *THEME_NAMES[UI_THEME_COUNT] = {
    "Classic", "Fantasy", "Sci-Fi", "Ancient", "Prehistoric",
};

// 6 tokens per theme, per the Monopoly Party FAQ. Order MUST match the data/ file
// slugs and the load table below (so token index -> the right sprite).
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

// ---- embedded PNGs (bin2o: data/foo.png -> foo_png / foo_png_size) ----------
#define PNG(sym) extern const unsigned char sym##_png[]; extern const unsigned int sym##_png_size;
PNG(logo) PNG(menu_bg)
PNG(tok_classic_car) PNG(tok_classic_boot) PNG(tok_classic_hat)
PNG(tok_classic_iron) PNG(tok_classic_battleship) PNG(tok_classic_dog)
PNG(tok_fantasy_dwarf) PNG(tok_fantasy_wizard) PNG(tok_fantasy_troll)
PNG(tok_fantasy_knight) PNG(tok_fantasy_orc) PNG(tok_fantasy_elf)
PNG(tok_scifi_robot) PNG(tok_scifi_rover) PNG(tok_scifi_fighter)
PNG(tok_scifi_alien) PNG(tok_scifi_astronaut) PNG(tok_scifi_ufo)
PNG(tok_ancient_boat) PNG(tok_ancient_warship) PNG(tok_ancient_urn)
PNG(tok_ancient_sandal) PNG(tok_ancient_chariot) PNG(tok_ancient_camel)
PNG(tok_prehistoric_caveman) PNG(tok_prehistoric_triceratops) PNG(tok_prehistoric_archaeopteryx)
PNG(tok_prehistoric_mammoth) PNG(tok_prehistoric_sabertooth) PNG(tok_prehistoric_cavewoman)

// One clay_image_t per asset id; .tex stays NULL until loaded (Clay then skips the
// image and shows the element background), so this degrades gracefully.
static clay_image_t g_images[UI_IMG_COUNT];

static void load_slot(int id, const unsigned char *buf, unsigned int size) {
    if (id < 0 || id >= UI_IMG_COUNT) return;
    ya2d_Texture *t = ya2d_loadPNGfromBuffer((void *) buf, size);
    if (!t) return;
    g_images[id].tex   = t;
    g_images[id].src_w = (float) t->imageWidth;
    g_images[id].src_h = (float) t->imageHeight;
}
#define LOAD(id, sym) load_slot((id), sym##_png, sym##_png_size)

void ui_images_load(void) {
    LOAD(UI_IMG_LOGO, logo);
    LOAD(UI_IMG_BG,   menu_bg);

    LOAD(ui_img_token(UI_THEME_CLASSIC, 0), tok_classic_car);
    LOAD(ui_img_token(UI_THEME_CLASSIC, 1), tok_classic_boot);
    LOAD(ui_img_token(UI_THEME_CLASSIC, 2), tok_classic_hat);
    LOAD(ui_img_token(UI_THEME_CLASSIC, 3), tok_classic_iron);
    LOAD(ui_img_token(UI_THEME_CLASSIC, 4), tok_classic_battleship);
    LOAD(ui_img_token(UI_THEME_CLASSIC, 5), tok_classic_dog);

    LOAD(ui_img_token(UI_THEME_FANTASY, 0), tok_fantasy_dwarf);
    LOAD(ui_img_token(UI_THEME_FANTASY, 1), tok_fantasy_wizard);
    LOAD(ui_img_token(UI_THEME_FANTASY, 2), tok_fantasy_troll);
    LOAD(ui_img_token(UI_THEME_FANTASY, 3), tok_fantasy_knight);
    LOAD(ui_img_token(UI_THEME_FANTASY, 4), tok_fantasy_orc);
    LOAD(ui_img_token(UI_THEME_FANTASY, 5), tok_fantasy_elf);

    LOAD(ui_img_token(UI_THEME_SCIFI, 0), tok_scifi_robot);
    LOAD(ui_img_token(UI_THEME_SCIFI, 1), tok_scifi_rover);
    LOAD(ui_img_token(UI_THEME_SCIFI, 2), tok_scifi_fighter);
    LOAD(ui_img_token(UI_THEME_SCIFI, 3), tok_scifi_alien);
    LOAD(ui_img_token(UI_THEME_SCIFI, 4), tok_scifi_astronaut);
    LOAD(ui_img_token(UI_THEME_SCIFI, 5), tok_scifi_ufo);

    LOAD(ui_img_token(UI_THEME_ANCIENT, 0), tok_ancient_boat);
    LOAD(ui_img_token(UI_THEME_ANCIENT, 1), tok_ancient_warship);
    LOAD(ui_img_token(UI_THEME_ANCIENT, 2), tok_ancient_urn);
    LOAD(ui_img_token(UI_THEME_ANCIENT, 3), tok_ancient_sandal);
    LOAD(ui_img_token(UI_THEME_ANCIENT, 4), tok_ancient_chariot);
    LOAD(ui_img_token(UI_THEME_ANCIENT, 5), tok_ancient_camel);

    LOAD(ui_img_token(UI_THEME_PREHISTORIC, 0), tok_prehistoric_caveman);
    LOAD(ui_img_token(UI_THEME_PREHISTORIC, 1), tok_prehistoric_triceratops);
    LOAD(ui_img_token(UI_THEME_PREHISTORIC, 2), tok_prehistoric_archaeopteryx);
    LOAD(ui_img_token(UI_THEME_PREHISTORIC, 3), tok_prehistoric_mammoth);
    LOAD(ui_img_token(UI_THEME_PREHISTORIC, 4), tok_prehistoric_sabertooth);
    LOAD(ui_img_token(UI_THEME_PREHISTORIC, 5), tok_prehistoric_cavewoman);
}

void *ui_image(int id) {
    if (id < 0 || id >= UI_IMG_COUNT) return 0;
    return g_images[id].tex ? &g_images[id] : 0;   // NULL when not loaded -> Clay skips it
}

// Draw a token sprite with ya2d (for the raw board tokens). Returns 0 if the sprite
// isn't loaded so the caller can fall back to a coloured square. Must be called
// inside the tiny3d 2D frame (after Project2D, with blending on).
int ui_draw_token(int theme, int token, int x, int y, int size) {
    int id = ui_img_token(theme, token);
    if (id < 0 || !g_images[id].tex) return 0;
    float scale = (float) size / g_images[id].src_w;
    ya2d_drawTextureZ((ya2d_Texture *) g_images[id].tex, x, y, 0, scale);
    return 1;
}
