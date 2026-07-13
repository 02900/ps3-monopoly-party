#ifndef MONOPOLY_UI_THEME_H
#define MONOPOLY_UI_THEME_H
// Token themes (names) + the image-asset registry for the Clay UI. Art is added in
// a later pass (see docs/assets.md); until then every image slot is a placeholder
// (NULL texture) and the Clay renderer simply skips it, showing the element's
// background colour instead. C only.
#include "ui.h"

const char *ui_theme_name(int theme);            // UI_THEME_* -> "Classic"...
const char *ui_token_name(int theme, int token); // token 0..5 -> "Car"...

// Image registry. Each slot wraps a ya2d_Texture once loaded; NULL until then.
// ui_image(id) returns a clay_image_t* (or NULL) to hand to a CLAY_IMAGE element.
enum {
    UI_IMG_LOGO = 0,                     // title logo
    UI_IMG_BG,                           // shared/fallback menu background
    UI_IMG_BG_TITLE,                     // per-screen menu backgrounds
    UI_IMG_BG_MENU,
    UI_IMG_BG_HOWTO,
    UI_IMG_BOARD,                        // board centre art
    UI_IMG_DICE,                         // + face 0..5  (dice_1..6)
    UI_IMG_ICON  = UI_IMG_DICE + 6,      // + 0 jail / 1 tax / 2 rr / 3 util
    UI_IMG_TOKEN = UI_IMG_ICON + 4,      // + theme*6 + token  (30 token sprites)
    UI_IMG_COUNT = UI_IMG_TOKEN + UI_THEME_COUNT * 6
};
void  ui_images_load(void);          // load the embedded PNGs into the registry
void *ui_image(int id);              // clay_image_t* for CLAY_IMAGE .imageData, or NULL
int   ui_img_token(int theme, int token);   // -> image id for a theme/token sprite
int   ui_img_dice(int face);                 // face 1..6 -> dice image id (-1 if bad)
int   ui_space_icon_id(int space_index);     // board space 0..39 -> icon image id (-1 if none)
int   ui_draw_cover(int id);                 // draw image id full-screen (cover fit); 0 if not loaded

#endif // MONOPOLY_UI_THEME_H
