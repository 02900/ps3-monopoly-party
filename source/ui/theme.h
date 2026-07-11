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
    UI_IMG_LOGO = 0,        // title logo
    UI_IMG_BG,             // menu background
    UI_IMG_TOKEN,          // + theme*6 + token  (30 token sprites)
    UI_IMG_COUNT = UI_IMG_TOKEN + UI_THEME_COUNT * 6
};
void  ui_images_load(void);          // load any embedded PNGs (stub until art lands)
void *ui_image(int id);              // clay_image_t* for CLAY_IMAGE .imageData, or NULL
int   ui_img_token(int theme, int token);   // -> image id for a theme/token sprite

#endif // MONOPOLY_UI_THEME_H
