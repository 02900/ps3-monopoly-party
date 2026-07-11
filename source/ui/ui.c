// Clay UI — P1 smoke. Proves the clay-ps3 pipeline (layout -> clay_render over the
// board). The real screens/HUD/overlays land in later phases. Compiled as C (gnu99);
// the C++ game calls only the ui.h entry points.
#include "clay.h"
#include "clay_renderer.h"
#include "ui.h"

void ui_init(int screen_w, int screen_h) {
    clay_backend_init(screen_w, screen_h);
}

void ui_render_smoke(void) {
    const Clay_Color panel   = {  27,  42,  56, 235 };
    const Clay_Color border  = { 245, 240, 225, 255 };
    const Clay_Color title   = { 255, 224, 130, 255 };
    const Clay_Color sub     = { 200, 200, 200, 255 };

    Clay_SetLayoutDimensions((Clay_Dimensions){ 848, 512 });
    Clay_BeginLayout();

    CLAY(CLAY_ID("SmokeRoot"), {
        .layout = {
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER }
        }
    }) {
        CLAY(CLAY_ID("SmokePanel"), {
            .layout = {
                .padding = CLAY_PADDING_ALL(24),
                .childGap = 8,
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER }
            },
            .backgroundColor = panel,
            .border = { .color = border, .width = CLAY_BORDER_OUTSIDE(2) }
        }) {
            CLAY_TEXT(CLAY_STRING("CLAY IS ALIVE"),
                      CLAY_TEXT_CONFIG({ .textColor = title, .fontSize = 24 }));
            CLAY_TEXT(CLAY_STRING("P1 pipeline smoke"),
                      CLAY_TEXT_CONFIG({ .textColor = sub, .fontSize = 16 }));
        }
    }

    Clay_RenderCommandArray cmds = Clay_EndLayout(0.0f);
    clay_render(cmds);
}
