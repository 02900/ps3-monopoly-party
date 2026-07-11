// Clay UI — shared init + frame setup. Screens live in screens.c; in-game
// HUD/overlays in game_ui.c. Compiled as C (gnu99).
#include <tiny3d.h>
#include "clay.h"
#include "clay_renderer.h"
#include "ui.h"

void ui_init(int screen_w, int screen_h) {
    clay_backend_init(screen_w, screen_h);
}

// Clear + enable alpha blending + 2D projection. REQUIRED before clay_render: with
// blending off, the TTF glyph quads' transparent pixels draw as opaque black boxes.
void ui_begin_frame(unsigned int clear_argb) {
    tiny3d_Clear(clear_argb, TINY3D_CLEAR_ALL);
    tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);
    tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
                        TINY3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_DST_ALPHA_ZERO,
                        TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);
    tiny3d_Project2D();
}
