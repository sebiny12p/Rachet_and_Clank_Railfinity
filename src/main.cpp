#include "RailGame.hpp"
#include "raylib.h"

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
#if defined(PLATFORM_ANDROID)
    InitWindow(0, 0, "Ratchet & Clank: Railfinity | Sebiny Labs");
#else
    InitWindow(Railfinity::VIRTUAL_WIDTH, Railfinity::VIRTUAL_HEIGHT,
               "Ratchet & Clank: Railfinity | Sebiny Labs");
#endif
    SetWindowMinSize(640, 360);
    SetExitKey(KEY_NULL); // Let ESC navigate menus & Pause screen
    SetTargetFPS(120);

    Image appIcon = LoadImage("assets/icon.png");
    if (appIcon.data == nullptr) {
        appIcon = LoadImage("build/assets/icon.png");
    }
    if (appIcon.data != nullptr) {
        SetWindowIcon(appIcon);
        UnloadImage(appIcon);
    }

    Railfinity::RailGame game;
    game.Init();

    while (!WindowShouldClose()) {
        game.RunFrame();
    }

    game.Shutdown();
    CloseWindow();
    return 0;
}
