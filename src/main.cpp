#include "RailGame.hpp"
#include "raylib.h"

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(Railfinity::VIRTUAL_WIDTH, Railfinity::VIRTUAL_HEIGHT,
               "Ratchet & Clank: Railfinity | Sebiny Labs");
    SetWindowMinSize(640, 360);
    SetExitKey(KEY_NULL); // Let ESC navigate menus & Pause screen
    SetTargetFPS(120);

    Railfinity::RailGame game;
    game.Init();

    while (!WindowShouldClose()) {
        game.RunFrame();
    }

    game.Shutdown();
    CloseWindow();
    return 0;
}
