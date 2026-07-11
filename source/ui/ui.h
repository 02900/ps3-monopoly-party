#ifndef MONOPOLY_UI_H
#define MONOPOLY_UI_H

// Plain-C bridge between the C++ game (Main.cpp) and the Clay-based UI (source/ui/*.c).
// The C++ side never includes clay.h — it only calls these functions and fills the
// plain-C UiSnapshot struct. Clay's compound-literal macros stay in the C TUs.

#ifdef __cplusplus
extern "C" {
#endif

// Create Clay's arena + text-measure callback. Call once, after ya2d + ttf init.
void ui_init(int screen_w, int screen_h);

// P1 smoke: lay out and render a trivial Clay panel to prove the pipeline. Must be
// called each frame AFTER tiny3d_Project2D() and reset_ttf_frame().
void ui_render_smoke(void);

#ifdef __cplusplus
}
#endif

#endif // MONOPOLY_UI_H
