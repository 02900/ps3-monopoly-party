// Clay UI — shared init. Screens live in screens.c; in-game HUD/overlays in
// hud.c/overlays.c (later phases). Compiled as C (gnu99).
#include "clay.h"
#include "clay_renderer.h"
#include "ui.h"

void ui_init(int screen_w, int screen_h) {
    clay_backend_init(screen_w, screen_h);
}
