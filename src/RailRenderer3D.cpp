#include "RailGame.hpp"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace Railfinity {

namespace {
void DrawTextWrapped(const char* text, int x, int y, int maxWidth, int fontSize, int lineHeight, Color color) {
    if (!text || !text[0]) return;
    std::string input(text);
    std::string currentLine;
    std::string word;
    int curY = y;

    auto flushLine = [&]() {
        if (!currentLine.empty()) {
            DrawText(currentLine.c_str(), x, curY, fontSize, color);
            curY += lineHeight;
            currentLine.clear();
        }
    };

    for (size_t i = 0; i <= input.size(); ++i) {
        char c = (i < input.size()) ? input[i] : ' ';
        if (c == ' ' || c == '\n') {
            if (!word.empty()) {
                std::string candidate = currentLine.empty() ? word : (currentLine + " " + word);
                if (MeasureText(candidate.c_str(), fontSize) > maxWidth && !currentLine.empty()) {
                    flushLine();
                    currentLine = word;
                } else {
                    currentLine = candidate;
                }
                word.clear();
            }
            if (c == '\n') {
                flushLine();
            }
        } else {
            word.push_back(c);
        }
    }
    flushLine();
}

// Smooth portal emergence scale so objects exiting the distant reactor tunnel at Z = 146 never pop in!
float GetPortalEmergenceScale(float z) {
    if (z <= 116.0f) return 1.0f;
    if (z >= 146.0f) return 0.0f;
    float t = (146.0f - z) / 30.0f;
    return t * t * (3.0f - 2.0f * t);
}
} // namespace

void RailGame::RenderFrame() {
    BeginTextureMode(renderTarget_);

    GalaxySector sec = (mode_ == ScreenMode::PLAYING || mode_ == ScreenMode::PAUSED || mode_ == ScreenMode::GAME_OVER)
                       ? GetCurrentSector() : GalaxySector::METROPOLIS_KERWAN;

    // Industrial Ratchet & Clank facility atmosphere
    Color skyTop = { 16, 28, 48, 255 };
    Color skyBot = { 54, 72, 68, 255 };
    if (sec == GalaxySector::MEGACORP_FACILITY) {
        skyTop = { 12, 32, 54, 255 };
        skyBot = { 38, 68, 78, 255 };
    } else if (sec == GalaxySector::ZORDOOM_PRISON) {
        skyTop = { 24, 16, 36, 255 };
        skyBot = { 62, 42, 54, 255 };
    } else if (sec == GalaxySector::DIMENSIONATOR_RIFT) {
        skyTop = { 18, 12, 42, 255 };
        skyBot = { 74, 38, 96, 255 };
    }

    // Crucial: ClearBackground clears both the color buffer AND the 3D Depth Buffer of renderTarget_!
    ClearBackground(skyTop);

    DrawRectangleGradientV(0, 0, VIRTUAL_WIDTH, VIRTUAL_HEIGHT, skyTop, skyBot);

    RenderWorld3D();

    // Screen Flash
    if (screenFlashAlpha_ > 0.0f) {
        Color fc = screenFlashColor_;
        fc.a = static_cast<unsigned char>(std::clamp(screenFlashAlpha_ * 255.0f, 0.0f, 255.0f));
        DrawRectangle(0, 0, VIRTUAL_WIDTH, VIRTUAL_HEIGHT, fc);
    }

    // 2D Overlays & Menus
    if (mode_ == ScreenMode::PLAYING || mode_ == ScreenMode::PAUSED || mode_ == ScreenMode::GAME_OVER) {
        RenderHud2D();
    }

    if (mode_ == ScreenMode::TITLE_FRONT_PAGE) {
        RenderTitleFrontPage2D();
    } else if (mode_ == ScreenMode::MAIN_MENU) {
        RenderMainMenu2D();
    } else if (mode_ == ScreenMode::GADGETRON_VENDOR) {
        RenderGadgetronVendor2D();
    } else if (mode_ == ScreenMode::RARITANIUM_LAB) {
        RenderRaritaniumLab2D();
    } else if (mode_ == ScreenMode::SKILL_POINTS_VIEW) {
        RenderSkillPoints2D();
    } else if (mode_ == ScreenMode::PAUSED) {
        RenderPauseOverlay2D();
    } else if (mode_ == ScreenMode::GAME_OVER) {
        RenderGameOver2D();
    }

    RenderSkillToast2D();

    EndTextureMode();

    // Blit virtual 1280x720 canvas to window
    BeginDrawing();
    ClearBackground(BLACK);
    Rectangle src = { 0.0f, 0.0f, static_cast<float>(renderTarget_.texture.width),
                      -static_cast<float>(renderTarget_.texture.height) };
    Rectangle dst = { 0.0f, 0.0f, static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()) };
    DrawTexturePro(renderTarget_.texture, src, dst, { 0.0f, 0.0f }, 0.0f, WHITE);
    EndDrawing();
}

void RailGame::RenderWorld3D() {
    Camera3D cam = camera_;
    if (mode_ == ScreenMode::MAIN_MENU || mode_ == ScreenMode::GADGETRON_VENDOR ||
        mode_ == ScreenMode::RARITANIUM_LAB || mode_ == ScreenMode::SKILL_POINTS_VIEW) {
        cam.position = { std::sin(globalTime_ * 0.5f) * 1.4f, 3.05f, -6.2f };
        cam.target = { GetTrackCurveX(28.0f) * 0.35f, 2.1f, 28.0f };
        cam.fovy = 50.0f;
    } else if (cameraShake_ > 0.0f) {
        float mag = cameraShake_ * cameraShake_ * 0.65f;
        cam.position.x += std::sin(globalTime_ * 73.0f) * mag;
        cam.position.y += std::cos(globalTime_ * 61.0f) * mag;
    }

    BeginMode3D(cam);

    RenderSkyline3D();
    RenderGrindRails3D();

    if (mode_ == ScreenMode::PLAYING || mode_ == ScreenMode::PAUSED || mode_ == ScreenMode::GAME_OVER) {
        RenderHazards3D();
        RenderBolts3D();
        RenderProjectiles3D();
        RenderBossDropship3D();
        RenderParticles3D();

        bool blinkHide = (player_.invulnTimer > 0.0f && std::fmod(globalTime_, 0.12f) < 0.06f);
        if (!blinkHide) {
            RenderLombaxHero3D({ player_.x, player_.y, 0.0f }, player_.laneTiltDeg,
                               player_.slideTimer > 0.0f, player_.heliGliding,
                               player_.wrenchSwingTimer, save_.equippedWeapon);
        }
    } else {
        // Menu showcase: Sample Hex Bolts and a glowing Red-Orange Rail Ring Collar ahead of Ratchet!
        for (int i = 0; i < 4; ++i) {
            Vector3 bp = ApplyTrackCurve(0.0f, 0.72f, 5.5f + i * 3.0f);
            RenderSingleHexBolt3D(bp, globalTime_ * 180.0f + i * 45.0f, 1.0f, i == 2);
        }
        Vector3 ringPos = ApplyTrackCurve(GetLaneX(0), 0.15f, 13.5f);
        for (int s = 0; s < 10; ++s) {
            float a1 = (s * 36.0f + globalTime_ * 55.0f) * DEG2RAD;
            float a2 = ((s + 1) * 36.0f + globalTime_ * 55.0f) * DEG2RAD;
            Vector3 p1 = { ringPos.x + std::cos(a1) * 1.15f, ringPos.y + std::sin(a1) * 1.15f, ringPos.z };
            Vector3 p2 = { ringPos.x + std::cos(a2) * 1.15f, ringPos.y + std::sin(a2) * 1.15f, ringPos.z };
            DrawCylinderEx(p1, p2, 0.22f, 0.22f, 6, (s % 2 == 0) ? Color{ 255, 75, 25, 255 } : Color{ 255, 165, 35, 255 });
        }
        RenderLombaxHero3D({ 0.0f, 0.0f, 0.0f }, std::sin(globalTime_ * 2.0f) * 6.0f,
                           false, false, 0.0f, save_.equippedWeapon);
    }

    EndMode3D();
}

void RailGame::RenderSkyline3D() const {
    constexpr float PORTAL_Z = 148.0f;

    // 1. Continuous Industrial Canyon Floor & Unbroken Side Facility Walls (Zero Pop-In!)
    DrawCube({ 0.0f, -18.0f, 70.0f }, 96.0f, 2.0f, 190.0f, { 46, 56, 52, 255 });

    // Continuous segmented side walls that follow the track curve from Z = -12 all the way into the back wall at Z = 148!
    constexpr float wallStep = 10.0f;
    for (float z = -12.0f; z < PORTAL_Z; z += wallStep) {
        float midZ = z + wallStep * 0.5f;
        float cx = GetTrackCurveX(std::max(0.0f, midZ));
        float cy = GetTrackCurveY(std::max(0.0f, midZ));

        for (int side = -1; side <= 1; side += 2) {
            float wx = cx + side * 17.5f;
            // Continuous Cream/Tan Upper Industrial Concrete Wall
            DrawCube({ wx, cy + 5.0f, midZ }, 5.0f, 30.0f, wallStep + 0.4f, { 166, 160, 134, 255 });
            // Continuous Dark Olive/Gray Lower Plinth
            DrawCube({ wx - side * 1.8f, cy - 6.8f, midZ }, 5.8f, 9.5f, wallStep + 0.4f, { 76, 84, 74, 255 });
            // Horizontal dark-steel cornice ledge
            DrawCube({ wx - side * 2.5f, cy - 1.9f, midZ }, 0.8f, 0.7f, wallStep + 0.4f, { 42, 48, 58, 255 });
        }
    }

    // Scrolling vertical steel buttress ribs & glowing cyan light panels (smoothly scaled out of the back wall so they NEVER pop!)
    float baySpacing = 24.0f;
    float bayOffset = std::fmod(distanceMeters_ * 1.8f, baySpacing);

    for (float z = -10.0f - bayOffset; z < PORTAL_Z - 2.0f; z += baySpacing) {
        float emerge = GetPortalEmergenceScale(z);
        if (emerge <= 0.01f) continue;

        float cx = GetTrackCurveX(std::max(0.0f, z));
        float cy = GetTrackCurveY(std::max(0.0f, z));

        for (int side = -1; side <= 1; side += 2) {
            float wx = cx + side * 14.6f;
            // Vertical dark steel buttress column
            DrawCube({ wx, cy + 4.0f, z }, 0.9f * emerge, 26.0f, 1.8f * emerge, { 45, 52, 64, 255 });
        }

        // Bright Glowing Cyan/White Billboard Light Panel on Left Wall (+X)
        Vector3 panelPos = { cx + 14.2f, cy + 2.2f, z + 8.0f };
        float pEmerge = GetPortalEmergenceScale(z + 8.0f);
        if (pEmerge > 0.02f) {
            DrawCube(panelPos, 0.45f, 4.4f * pEmerge, 9.2f * pEmerge, { 35, 52, 68, 255 });
            DrawCube({ panelPos.x - 0.18f, panelPos.y, panelPos.z },
                     0.20f, 3.8f * pEmerge, 8.5f * pEmerge, { 185, 248, 255, 255 });
        }
    }

    // 2. Distant Giant Circular Reactor Tunnel Bulkhead & Twin Cooling Towers (anchored at Z = 148)
    Vector3 portalCenter = ApplyTrackCurve(0.0f, 0.5f, PORTAL_Z);

    // Deep blue tiled facility back-wall behind the portal
    DrawCube({ portalCenter.x, portalCenter.y + 8.0f, PORTAL_Z + 10.0f }, 68.0f, 44.0f, 4.0f, { 24, 58, 92, 255 });
    for (int tx = -3; tx <= 3; ++tx) {
        DrawCube({ portalCenter.x + tx * 7.2f, portalCenter.y + 4.8f, PORTAL_Z + 7.5f },
                 6.4f, 14.0f, 1.0f, { 52, 136, 196, 255 });
    }

    // Twin Cream/Tan Cylindrical Cooling Towers with Glowing Blue Reactor Vents on Top!
    for (int side = -1; side <= 1; side += 2) {
        float towerX = portalCenter.x + side * 12.6f;
        float towerZ = PORTAL_Z + 3.0f;
        DrawCylinderEx({ towerX, portalCenter.y - 10.0f, towerZ },
                       { towerX, portalCenter.y + 12.5f, towerZ }, 3.8f, 3.8f, 12, { 182, 174, 148, 255 });
        DrawCylinderEx({ towerX, portalCenter.y + 12.5f, towerZ },
                       { towerX, portalCenter.y + 15.5f, towerZ }, 4.6f, 3.1f, 12, { 196, 188, 162, 255 });
        DrawCylinderEx({ towerX, portalCenter.y + 15.5f, towerZ },
                       { towerX, portalCenter.y + 19.2f, towerZ }, 2.5f, 2.5f, 10, { 35, 115, 255, 255 });
        for (int r = -1; r <= 1; ++r) {
            DrawCube({ towerX + r * 1.0f, portalCenter.y + 17.5f, towerZ - 2.0f },
                     0.38f, 3.4f, 0.38f, { 120, 210, 255, 255 });
        }
    }

    // Giant Bolted Octagonal/Circular Dark-Steel Tunnel Archway Ring around the Reactor!
    constexpr int ARCH_SEGMENTS = 16;
    float archRadius = 9.6f;
    for (int i = 0; i < ARCH_SEGMENTS; ++i) {
        float a1 = (static_cast<float>(i) / ARCH_SEGMENTS) * 6.283185f;
        float a2 = (static_cast<float>(i + 1) / ARCH_SEGMENTS) * 6.283185f;
        Vector3 p1 = { portalCenter.x + std::cos(a1) * archRadius, portalCenter.y + std::sin(a1) * archRadius, PORTAL_Z };
        Vector3 p2 = { portalCenter.x + std::cos(a2) * archRadius, portalCenter.y + std::sin(a2) * archRadius, PORTAL_Z };
        DrawCylinderEx(p1, p2, 1.75f, 1.75f, 8, { 54, 62, 76, 255 });
        DrawSphere({ p1.x, p1.y, PORTAL_Z - 1.5f }, 0.48f, { 28, 32, 42, 255 });
    }

    // Large Trapezoidal Keystone Wedge at the 12-o'clock top of the Circular Arch
    DrawCylinderEx({ portalCenter.x, portalCenter.y + 7.4f, PORTAL_Z - 1.2f },
                   { portalCenter.x, portalCenter.y + 14.8f, PORTAL_Z - 1.2f }, 1.8f, 3.8f, 4, { 96, 106, 122, 255 });

    // Central Dark-Teal Spherical Reactor Core inside the Archway
    DrawSphere(portalCenter, 5.4f, { 26, 78, 82, 255 });
    DrawSphereWires(portalCenter, 5.45f, 8, 8, { 42, 112, 118, 255 });
    DrawSphere({ portalCenter.x - 1.5f, portalCenter.y + 2.4f, portalCenter.z - 4.6f }, 0.40f, { 210, 250, 255, 255 });
    DrawSphere({ portalCenter.x + 1.3f, portalCenter.y + 2.6f, portalCenter.z - 4.6f }, 0.40f, { 210, 250, 255, 255 });

    // Twin Glowing Red-Orange Intake Ports where the Grindrails connect directly into the Reactor Sphere!
    Vector3 leftPort  = { portalCenter.x + 1.85f, portalCenter.y - 0.35f, portalCenter.z - 4.9f };
    Vector3 rightPort = { portalCenter.x - 1.85f, portalCenter.y - 0.35f, portalCenter.z - 4.9f };
    DrawSphere(leftPort,  0.95f, { 255, 65, 35, 255 });
    DrawSphere(rightPort, 0.95f, { 255, 65, 35, 255 });
    DrawSphere(leftPort,  0.50f, YELLOW);
    DrawSphere(rightPort, 0.50f, YELLOW);
}

void RailGame::RenderGrindRails3D() const {
    // 1. Draw 3 Curving Tubular Metallic Gold-and-Steel Grindrails all the way into the Reactor Ports at Z = 146!
    constexpr float segLen = 3.0f;
    for (int l = 0; l < NUM_LANES; ++l) {
        float lx = GetLaneX(l);
        for (float z = -10.0f; z < 145.0f; z += segLen) {
            float midZ = z + segLen * 0.5f;
            if (IsRailGapAt(l, midZ)) {
                continue; // Physical gap in the tubular rail!
            }
            Vector3 pA = ApplyTrackCurve(lx, -0.10f, z);
            Vector3 pB = ApplyTrackCurve(lx, -0.10f, z + segLen + 0.06f);

            // Central dark-steel structural spine underneath
            DrawCylinderEx({ pA.x, pA.y - 0.14f, pA.z }, { pB.x, pB.y - 0.14f, pB.z },
                           0.22f, 0.22f, 6, { 34, 42, 48, 255 });

            // Bright Silver/Chrome Center Grind Pipe
            DrawCylinderEx(pA, pB, 0.16f, 0.16f, 8, { 205, 218, 228, 255 });

            // Twin Warm Gold/Brass Side Guide Tubes
            DrawCylinderEx({ pA.x - 0.24f, pA.y - 0.03f, pA.z }, { pB.x - 0.24f, pB.y - 0.03f, pB.z },
                           0.09f, 0.09f, 6, { 222, 188, 86, 255 });
            DrawCylinderEx({ pA.x + 0.24f, pA.y - 0.03f, pA.z }, { pB.x + 0.24f, pB.y - 0.03f, pB.z },
                           0.09f, 0.09f, 6, { 222, 188, 86, 255 });
        }
    }

    // 2. Green-Gray Steel H-Frame Scaffolding Trusses & Black/Yellow Hazard Posts (smoothly scaled at far portal!)
    float trussSpacing = 16.0f;
    float trussOffset = std::fmod(distanceMeters_ * 2.2f, trussSpacing);
    Color trussCol = { 52, 88, 80, 255 };

    for (float z = 4.0f - trussOffset; z < 142.0f; z += trussSpacing) {
        if (z < -4.0f) continue;
        float emerge = GetPortalEmergenceScale(z);
        if (emerge <= 0.02f) continue;

        Vector3 cCenter = ApplyTrackCurve(0.0f, -0.35f, z);
        Vector3 cLeft   = ApplyTrackCurve((GetLaneX(0) + 0.8f) * emerge, -0.35f, z);
        Vector3 cRight  = ApplyTrackCurve((GetLaneX(2) - 0.8f) * emerge, -0.35f, z);

        DrawCylinderEx(cLeft, cRight, 0.18f * emerge, 0.18f * emerge, 6, trussCol);
        DrawCylinderEx({ cLeft.x * 0.7f, cCenter.y - 2.6f * emerge, z },
                       { cRight.x * 0.7f, cCenter.y - 2.6f * emerge, z }, 0.16f * emerge, 0.16f * emerge, 6, trussCol);
        DrawCylinderEx({ cLeft.x * 0.65f, cCenter.y, z },
                       { cLeft.x * 0.65f, cCenter.y - 12.0f * emerge, z }, 0.22f * emerge, 0.22f * emerge, 6, trussCol);
        DrawCylinderEx({ cRight.x * 0.65f, cCenter.y, z },
                       { cRight.x * 0.65f, cCenter.y - 12.0f * emerge, z }, 0.22f * emerge, 0.22f * emerge, 6, trussCol);

        // Black & Yellow Striped Warning Track Posts along the outer rail supports!
        for (int s = 0; s < 4; ++s) {
            Color bandCol = (s % 2 == 0) ? Color{ 245, 205, 35, 255 } : Color{ 25, 28, 32, 255 };
            DrawCylinderEx({ cLeft.x + 0.4f * emerge, cLeft.y + s * 0.32f * emerge, z },
                           { cLeft.x + 0.4f * emerge, cLeft.y + (s + 1) * 0.32f * emerge, z },
                           0.12f * emerge, 0.12f * emerge, 6, bandCol);
        }
    }
}

void RailGame::RenderSingleHexBolt3D(Vector3 pos, float spinDeg, float scale, bool titanium) const {
    if (scale <= 0.02f) return;
    float rad = spinDeg * DEG2RAD;
    float bob = std::sin(rad * 1.5f) * 0.08f * scale;
    Vector3 c = { pos.x, pos.y + bob, pos.z };

    Vector3 dir = { std::cos(rad) * 0.88f, 0.32f, std::sin(rad) * 0.88f };
    dir = Vector3Normalize(dir);

    Color headCol   = titanium ? Color{ 225, 240, 255, 255 } : Color{ 255, 205, 45, 255 };
    Color rimCol    = titanium ? Color{ 110, 210, 255, 255 } : Color{ 215, 135, 15, 255 };
    Color shaftCol  = titanium ? Color{ 180, 200, 225, 255 } : Color{ 215, 165, 40, 255 };
    Color threadCol = titanium ? Color{ 95, 130, 165, 255 }  : Color{ 140, 92, 18, 255 };
    Color boreCol   = { 35, 28, 18, 255 };

    // 1. Hexagonal Nut/Bolt Head (sides = 6)
    Vector3 headStart = Vector3Subtract(c, Vector3Scale(dir, 0.04f * scale));
    Vector3 headEnd   = Vector3Add(c, Vector3Scale(dir, 0.24f * scale));
    DrawCylinderEx(headStart, headEnd, 0.42f * scale, 0.42f * scale, 6, headCol);
    DrawCylinderWiresEx(headStart, headEnd, 0.44f * scale, 0.44f * scale, 6, rimCol);

    // 2. Dark recessed hexagonal socket bore
    Vector3 boreStart = Vector3Add(headEnd, Vector3Scale(dir, -0.02f * scale));
    Vector3 boreEnd   = Vector3Add(headEnd, Vector3Scale(dir, 0.03f * scale));
    DrawCylinderEx(boreStart, boreEnd, 0.20f * scale, 0.20f * scale, 6, boreCol);

    // 3. Threaded cylindrical screw shaft
    Vector3 shaftEnd = Vector3Subtract(c, Vector3Scale(dir, 0.46f * scale));
    DrawCylinderEx(headStart, shaftEnd, 0.19f * scale, 0.16f * scale, 8, shaftCol);

    // 4. Screw thread rings
    for (int t = 1; t <= 2; ++t) {
        float frac = static_cast<float>(t) * 0.34f;
        Vector3 rA = Vector3Lerp(headStart, shaftEnd, frac);
        Vector3 rB = Vector3Lerp(headStart, shaftEnd, frac + 0.12f);
        DrawCylinderEx(rA, rB, 0.23f * scale, 0.23f * scale, 6, threadCol);
    }

    // 5. Glowing golden/cyan halo ring on the rail beneath the bolt
    DrawCircle3D({ pos.x, pos.y - 0.55f * scale, pos.z }, 0.44f * scale, { 1.0f, 0.0f, 0.0f }, 90.0f,
                 titanium ? SKYBLUE : GOLD);
}

void RailGame::RenderRaritaniumCrystal3D(Vector3 pos, float spinDeg, float scale) const {
    if (scale <= 0.02f) return;
    float rad = spinDeg * DEG2RAD;
    float bob = std::sin(rad * 2.0f) * 0.12f * scale;
    Vector3 base = { pos.x, pos.y - 0.35f * scale + bob, pos.z };

    Vector3 tipMain = { pos.x, base.y + 1.05f * scale, pos.z };
    DrawCylinderEx(base, tipMain, 0.34f * scale, 0.0f, 4, { 45, 225, 255, 240 });
    DrawCylinderWiresEx(base, tipMain, 0.36f * scale, 0.0f, 4, WHITE);

    for (int s = -1; s <= 1; s += 2) {
        float sx = std::cos(rad + s * 1.1f) * 0.45f * scale;
        float sz = std::sin(rad + s * 1.1f) * 0.45f * scale;
        Vector3 sideTip = { base.x + sx, base.y + 0.72f * scale, base.z + sz };
        DrawCylinderEx(base, sideTip, 0.24f * scale, 0.0f, 4, { 110, 245, 255, 230 });
        DrawCylinderWiresEx(base, sideTip, 0.25f * scale, 0.0f, 4, SKYBLUE);
    }

    DrawCircle3D({ pos.x, pos.y - 0.5f * scale, pos.z }, 0.55f * scale, { 1.0f, 0.0f, 0.0f }, 90.0f, SKYBLUE);
}

void RailGame::RenderHazards3D() const {
    for (const auto& h : hazards_) {
        if (!h.active || h.z < -8.0f || h.z > 145.0f) continue;
        float s = GetPortalEmergenceScale(h.z);
        if (s <= 0.02f) continue;

        float rawLx = GetLaneX(h.lane);
        Vector3 basePos = ApplyTrackCurve(rawLx, 0.0f, h.z);
        float lx = basePos.x;
        float ry = basePos.y;

        switch (h.kind) {
            case HazardKind::BOLT_CRATE: {
                Vector3 cPos = { lx, ry + 0.85f * s, h.z };
                DrawCube(cPos, 1.42f * s, 1.42f * s, 1.42f * s, { 178, 102, 34, 255 });
                DrawCube(cPos, 1.48f * s, 0.22f * s, 1.48f * s, { 115, 62, 18, 255 });
                DrawCube(cPos, 0.22f * s, 1.48f * s, 1.48f * s, { 115, 62, 18, 255 });
                for (int dx = -1; dx <= 1; dx += 2) {
                    for (int dz = -1; dz <= 1; dz += 2) {
                        DrawCube({ lx + dx * 0.68f * s, ry + 0.85f * s, h.z + dz * 0.68f * s },
                                 0.20f * s, 1.54f * s, 0.20f * s, { 225, 185, 65, 255 });
                    }
                }
                DrawCubeWires(cPos, 1.56f * s, 1.56f * s, 1.56f * s, GOLD);
                RenderSingleHexBolt3D({ lx, ry + 2.05f * s, h.z }, h.animPhase * 90.0f, 0.62f * s, false);
                break;
            }
            case HazardKind::NANOTECH_CRATE: {
                Vector3 cPos = { lx, ry + 0.85f * s, h.z };
                DrawCube(cPos, 1.42f * s, 1.42f * s, 1.42f * s, { 25, 145, 215, 110 });
                DrawCubeWires(cPos, 1.54f * s, 1.54f * s, 1.54f * s, SKYBLUE);
                for (int dx = -1; dx <= 1; dx += 2) {
                    for (int dz = -1; dz <= 1; dz += 2) {
                        DrawCube({ lx + dx * 0.68f * s, ry + 0.85f * s, h.z + dz * 0.68f * s },
                                 0.18f * s, 1.54f * s, 0.18f * s, { 190, 225, 255, 255 });
                    }
                }
                float bob = std::sin(h.animPhase * 2.0f) * 0.10f * s;
                DrawSphere({ lx, ry + 0.88f * s + bob, h.z }, 0.40f * s, { 65, 235, 255, 255 });
                DrawCube({ lx, ry + 2.02f * s + bob, h.z }, 0.60f * s, 0.18f * s, 0.18f * s, GREEN);
                DrawCube({ lx, ry + 2.02f * s + bob, h.z }, 0.18f * s, 0.60f * s, 0.18f * s, GREEN);
                break;
            }
            case HazardKind::RARITANIUM_CACHE: {
                DrawCylinderEx({ lx, ry, h.z }, { lx, ry + 0.42f * s, h.z }, 0.95f * s, 0.78f * s, 7, { 52, 60, 78, 255 });
                RenderRaritaniumCrystal3D({ lx, ry + 0.82f * s, h.z }, h.animPhase * 45.0f, 1.28f * s);
                break;
            }
            case HazardKind::TNT_CRATE: {
                float pulse = (1.0f + std::sin(h.animPhase * 4.0f) * 0.06f) * s;
                Vector3 bot = { lx, ry, h.z };
                Vector3 top = { lx, ry + 1.65f * pulse, h.z };
                DrawCylinderEx(bot, top, 0.76f * pulse, 0.76f * pulse, 12, { 215, 32, 32, 255 });
                DrawCylinderWiresEx(bot, top, 0.79f * pulse, 0.79f * pulse, 12, MAROON);
                DrawCylinderEx({ lx, ry + 0.60f * s, h.z }, { lx, ry + 1.05f * s, h.z },
                               0.80f * pulse, 0.80f * pulse, 12, { 255, 215, 25, 255 });
                Vector3 fuseTip = { lx, ry + 1.95f * s, h.z };
                DrawCylinderEx(top, fuseTip, 0.08f * s, 0.04f * s, 6, DARKGRAY);
                DrawSphere(fuseTip, (0.20f + std::sin(h.animPhase * 8.0f) * 0.07f) * s,
                           (std::fmod(h.animPhase, 0.5f) < 0.25f) ? YELLOW : ORANGE);
                break;
            }
            case HazardKind::LOW_LASER_BARRIER: {
                // ICONIC RATCHET & CLANK RED-ORANGE CIRCULAR SPIKED RAIL COLLAR MINE
                Vector3 ringCenter = { lx, ry + 0.12f * s, h.z };
                DrawCylinderEx({ lx, ry - 0.08f * s, h.z - 0.45f * s }, { lx, ry - 0.08f * s, h.z + 0.45f * s },
                               0.42f * s, 0.42f * s, 8, { 42, 48, 58, 255 });

                constexpr int RING_SEGS = 10;
                float ringR = 1.18f * s;
                for (int seg = 0; seg < RING_SEGS; ++seg) {
                    float a1 = (seg * (360.0f / RING_SEGS)) * DEG2RAD + h.animPhase * 0.8f;
                    float a2 = ((seg + 1) * (360.0f / RING_SEGS)) * DEG2RAD + h.animPhase * 0.8f;
                    Vector3 p1 = { ringCenter.x + std::cos(a1) * ringR, ringCenter.y + std::sin(a1) * ringR, ringCenter.z };
                    Vector3 p2 = { ringCenter.x + std::cos(a2) * ringR, ringCenter.y + std::sin(a2) * ringR, ringCenter.z };

                    Color segCol = (seg % 2 == 0) ? Color{ 255, 65, 20, 255 } : Color{ 255, 165, 30, 255 };
                    DrawCylinderEx(p1, p2, 0.22f * s, 0.22f * s, 6, segCol);

                    if (seg % 2 == 0) {
                        Vector3 spikeOut = { ringCenter.x + std::cos(a1) * (ringR + 0.32f * s),
                                             ringCenter.y + std::sin(a1) * (ringR + 0.32f * s), ringCenter.z };
                        DrawCylinderEx(p1, spikeOut, 0.16f * s, 0.04f * s, 5, { 185, 35, 18, 255 });
                    }
                }
                DrawCircle3D(ringCenter, 0.92f * s, { 0.0f, 0.0f, 1.0f }, 0.0f, { 255, 200, 60, 255 });
                DrawCircle3D(ringCenter, 1.35f * s, { 0.0f, 0.0f, 1.0f }, 0.0f, RED);
                break;
            }
            case HazardKind::HIGH_LASER_GATE: {
                DrawCube({ lx - 1.38f * s, ry + 1.95f * s, h.z }, 0.34f * s, 4.0f * s, 0.40f * s, { 52, 60, 74, 255 });
                DrawCube({ lx + 1.38f * s, ry + 1.95f * s, h.z }, 0.34f * s, 4.0f * s, 0.40f * s, { 52, 60, 74, 255 });
                DrawCube({ lx, ry + 3.45f * s, h.z }, 3.15f * s, 0.62f * s, 0.62f * s, { 75, 86, 106, 255 });
                DrawCubeWires({ lx, ry + 3.45f * s, h.z }, 3.18f * s, 0.65f * s, 0.65f * s, ORANGE);
                DrawCube({ lx, ry + 2.38f * s, h.z }, 2.65f * s, 1.45f * s, 0.16f * s, { 255, 115, 25, 210 });
                for (int bar = 0; bar < 3; ++bar) {
                    float by = ry + (1.72f + bar * 0.58f) * s;
                    DrawCylinderEx({ lx - 1.28f * s, by, h.z }, { lx + 1.28f * s, by, h.z },
                                   0.07f * s, 0.07f * s, 6, YELLOW);
                }
                break;
            }
            case HazardKind::ELECTRIFIED_RAIL: {
                float startZ = h.z - h.lengthZ * 0.5f;
                float endZ   = h.z + h.lengthZ * 0.5f;
                for (float pz = startZ + 0.8f; pz <= endZ - 0.4f; pz += 2.2f) {
                    Vector3 cp = ApplyTrackCurve(rawLx, 0.05f, pz);
                    DrawCylinderEx({ cp.x, cp.y, pz - 0.8f * s }, { cp.x, cp.y, pz + 0.8f * s },
                                   0.34f * s, 0.34f * s, 8, { 255, 95, 25, 220 });
                    DrawCircle3D(cp, 0.68f * s, { 0.0f, 0.0f, 1.0f }, 0.0f, YELLOW);
                }
                break;
            }
            case HazardKind::BROKEN_RAIL_GAP: {
                float startZ = h.z - h.lengthZ * 0.5f;
                float endZ   = h.z + h.lengthZ * 0.5f;
                Vector3 pStart = ApplyTrackCurve(rawLx, -0.10f, startZ);
                Vector3 pEnd   = ApplyTrackCurve(rawLx, -0.10f, endZ);
                DrawCube(pStart, 1.05f * s, 0.42f * s, 0.42f * s, RED);
                DrawCubeWires(pStart, 1.08f * s, 0.45f * s, 0.45f * s, YELLOW);
                DrawCube(pEnd, 1.05f * s, 0.42f * s, 0.42f * s, ORANGE);
                break;
            }
            case HazardKind::HYPER_BOOST_PAD: {
                for (int c = -1; c <= 1; ++c) {
                    float cz = h.z + c * 1.15f;
                    Vector3 tip   = ApplyTrackCurve(rawLx, 0.14f, cz + 0.45f);
                    Vector3 lWing = ApplyTrackCurve(rawLx + 0.55f * s, 0.14f, cz - 0.25f);
                    Vector3 rWing = ApplyTrackCurve(rawLx - 0.55f * s, 0.14f, cz - 0.25f);
                    DrawCylinderEx(lWing, tip, 0.10f * s, 0.10f * s, 6, GREEN);
                    DrawCylinderEx(rWing, tip, 0.10f * s, 0.10f * s, 6, GREEN);
                }
                break;
            }
            case HazardKind::THUG_DRONE: {
                Vector3 dPos = { lx, ry + (1.50f + std::sin(h.animPhase) * 0.28f) * s, h.z };
                DrawSphere(dPos, 0.82f * s, { 62, 72, 88, 255 });
                DrawCube({ dPos.x, dPos.y + 0.10f * s, dPos.z }, 1.52f * s, 0.38f * s, 0.80f * s, { 90, 102, 120, 255 });
                DrawSphere({ dPos.x, dPos.y + 0.08f * s, dPos.z - 0.68f * s }, 0.34f * s, RED);
                DrawSphere({ dPos.x, dPos.y + 0.08f * s, dPos.z - 0.82f * s }, 0.16f * s, YELLOW);
                DrawCylinderEx({ dPos.x - 0.50f * s, dPos.y - 0.32f * s, dPos.z },
                               { dPos.x - 0.50f * s, dPos.y - 0.32f * s, dPos.z - 1.05f * s }, 0.11f * s, 0.09f * s, 8, DARKGRAY);
                DrawCylinderEx({ dPos.x + 0.50f * s, dPos.y - 0.32f * s, dPos.z },
                               { dPos.x + 0.50f * s, dPos.y - 0.32f * s, dPos.z - 1.05f * s }, 0.11f * s, 0.09f * s, 8, DARKGRAY);
                float rAngle = h.animPhase * 6.0f;
                Vector3 hub = { dPos.x, dPos.y + 0.92f * s, dPos.z };
                DrawCylinderEx({ dPos.x, dPos.y + 0.45f * s, dPos.z }, hub, 0.10f * s, 0.10f * s, 6, LIGHTGRAY);
                Vector3 b1 = { hub.x + std::cos(rAngle) * 1.35f * s, hub.y, hub.z + std::sin(rAngle) * 1.35f * s };
                Vector3 b2 = { hub.x - std::cos(rAngle) * 1.35f * s, hub.y, hub.z - std::sin(rAngle) * 1.35f * s };
                DrawCylinderEx(b1, b2, 0.08f * s, 0.08f * s, 4, ORANGE);
                break;
            }
            case HazardKind::SHIELDED_ENFORCER: {
                DrawCube({ lx - 0.48f * s, ry + 0.52f * s, h.z }, 0.44f * s, 1.05f * s, 0.54f * s, { 55, 62, 82, 255 });
                DrawCube({ lx + 0.48f * s, ry + 0.52f * s, h.z }, 0.44f * s, 1.05f * s, 0.54f * s, { 55, 62, 82, 255 });
                Vector3 torso = { lx, ry + 1.58f * s, h.z };
                DrawCube(torso, 1.52f * s, 1.28f * s, 1.02f * s, { 75, 52, 105, 255 });
                DrawCube({ lx - 0.96f * s, ry + 1.85f * s, h.z }, 0.52f * s, 0.62f * s, 0.80f * s, { 110, 75, 150, 255 });
                DrawCube({ lx + 0.96f * s, ry + 1.85f * s, h.z }, 0.52f * s, 0.62f * s, 0.80f * s, { 110, 75, 150, 255 });
                DrawCube({ lx, ry + 2.42f * s, h.z }, 0.72f * s, 0.48f * s, 0.62f * s, { 90, 98, 118, 255 });
                DrawCube({ lx, ry + 2.42f * s, h.z - 0.30f * s }, 0.58f * s, 0.18f * s, 0.12f * s, RED);
                Vector3 shPos = { lx, ry + 1.38f * s, h.z - 0.82f * s };
                DrawCube(shPos, 1.95f * s, 2.10f * s, 0.14f * s, { 65, 215, 255, 135 });
                DrawCubeWires(shPos, 1.98f * s, 2.14f * s, 0.16f * s, SKYBLUE);
                break;
            }
        }
    }
}

void RailGame::RenderBolts3D() const {
    for (const auto& b : bolts_) {
        if (!b.active || b.pos.z < -8.0f || b.pos.z > 145.0f) continue;
        float s = GetPortalEmergenceScale(b.pos.z);
        if (s <= 0.02f) continue;

        Vector3 curvedPos = ApplyTrackCurve(b.pos.x, b.pos.y, b.pos.z);
        if (b.isRaritanium) {
            RenderRaritaniumCrystal3D(curvedPos, b.spin, 0.92f * s);
        } else {
            RenderSingleHexBolt3D(curvedPos, b.spin, (b.isTitanium ? 1.25f : 0.90f) * s, b.isTitanium);
        }
    }
}

void RailGame::RenderProjectiles3D() const {
    for (const auto& p : projectiles_) {
        if (!p.active || p.pos.z > 145.0f) continue;
        Vector3 cp = ApplyTrackCurve(p.pos.x, p.pos.y, p.pos.z);
        if (p.sourceWeapon == WeaponId::PLASMA_STRIKER && p.friendly) {
            Vector3 tail = ApplyTrackCurve(p.pos.x, p.pos.y, p.pos.z - 6.0f);
            DrawCylinderEx(tail, cp, 0.24f, 0.24f, 8, p.color);
            DrawCylinderEx(tail, cp, 0.11f, 0.11f, 6, WHITE);
        } else if (p.sourceWeapon == WeaponId::BUZZ_BLADES && p.friendly) {
            Vector3 bot = { cp.x, cp.y - 0.06f, cp.z };
            Vector3 top = { cp.x, cp.y + 0.06f, cp.z };
            DrawCylinderEx(bot, top, 0.58f, 0.58f, 8, p.color);
            DrawCylinderWiresEx(bot, top, 0.62f, 0.62f, 8, WHITE);
        } else {
            DrawSphere(cp, p.radius * 0.55f, p.color);
            DrawSphereWires(cp, p.radius * 0.75f, 6, 6, WHITE);
        }
    }
}

void RailGame::RenderParticles3D() const {
    for (const auto& pt : particles_) {
        if (!pt.active) continue;
        float r = std::clamp(pt.life / pt.maxLife, 0.0f, 1.0f);
        Vector3 cp = ApplyTrackCurve(pt.pos.x, pt.pos.y, pt.pos.z);
        DrawCubeV(cp, { pt.size * r, pt.size * r, pt.size * r }, pt.color);
    }
}

void RailGame::RenderBossDropship3D() const {
    if (!boss_.active) return;

    Vector3 hullPos = ApplyTrackCurve(boss_.hoverX, boss_.hoverY, boss_.z);
    DrawCube(hullPos, 9.5f, 2.2f, 4.2f, { 48, 54, 70, 255 });
    DrawCubeWires(hullPos, 9.6f, 2.3f, 4.3f, ORANGE);
    DrawCube({ hullPos.x - 5.2f, hullPos.y + 0.4f, hullPos.z + 0.5f }, 2.0f, 1.1f, 4.8f, { 68, 76, 96, 255 });
    DrawCube({ hullPos.x + 5.2f, hullPos.y + 0.4f, hullPos.z + 0.5f }, 2.0f, 1.1f, 4.8f, { 68, 76, 96, 255 });
    DrawCube({ hullPos.x, hullPos.y + 1.35f, hullPos.z - 0.8f }, 3.4f, 0.88f, 2.2f, { 255, 85, 45, 220 });

    for (int l = 0; l < NUM_LANES; ++l) {
        Vector3 tPos = ApplyTrackCurve(GetLaneX(l), boss_.hoverY - 1.15f, boss_.z - 1.8f);
        if (boss_.turretHp[l] > 0.0f) {
            DrawSphere(tPos, 0.95f, RED);
            DrawSphereWires(tPos, 1.1f, 6, 6, GOLD);
            DrawCylinderEx(tPos, { tPos.x, tPos.y - 0.2f, tPos.z - 1.3f }, 0.22f, 0.16f, 8, DARKGRAY);
        } else {
            DrawSphere(tPos, 0.60f, DARKGRAY);
        }
    }
}

void RailGame::RenderLombaxHero3D(Vector3 pos, float tiltDeg, bool sliding, bool gliding,
                                  float wrenchTimer, int weaponId) const {
    // Animation Blend Weights & Procedural Timers
    float aimBlend   = std::clamp(player_.gunAimTimer / 0.16f, 0.0f, 1.0f);
    float recoilFrac = std::clamp(player_.gunRecoilTimer / 0.20f, 0.0f, 1.0f);
    float recoilKick = std::sin(recoilFrac * 3.14159265f); // Snappy impulse curve 0 -> 1 -> 0
    float squash     = std::sin(std::clamp(player_.landingSquashTimer / 0.25f, 0.0f, 1.0f) * 3.14159265f);
    bool airborne    = !player_.grounded;

    // Rhythmic Grind Suspension Bob
    float grindBob = airborne ? 0.0f : (std::sin(globalTime_ * 18.0f) * 0.04f);
    float crouchDrop = (sliding ? -0.45f : 0.0f) - squash * 0.20f + grindBob;
    float leanX = tiltDeg * 0.014f;

    // Authentic Ratchet & Clank Palette
    Color furGold     = { 245, 186, 52, 255 };
    Color furInner    = { 215, 138, 68, 255 };
    Color furStripe   = { 128, 52, 16, 255 };
    Color armorOrange = { 216, 96, 26, 255 };
    Color armorDark   = { 52, 60, 72, 255 };
    Color pantsBlue   = { 58, 76, 102, 255 };
    Color leatherBrn  = { 92, 50, 22, 255 };
    Color gloveBrn    = { 72, 38, 16, 255 };
    Color chromeSilv  = { 196, 208, 224, 255 };

    // =========================================================================
    // 1. LEGS & MAGNETIC GRIND BOOTS (Dynamic Jump & Glide Animation)
    // =========================================================================
    float vy = player_.vy;
    float jumpTuck = 0.0f;
    float legSway = 0.0f;
    if (airborne) {
        if (player_.heliDeploy > 0.12f) {
            // Dangling in Clank's flight harness with rhythmic Lombax leg kick/sway
            legSway = std::sin(globalTime_ * 9.0f) * 0.12f * player_.heliDeploy;
            jumpTuck = 0.18f;
        } else if (vy > 0.0f) {
            // Ascending leap: snappy knee tuck tightens with upward momentum
            float t = std::clamp(vy / 18.5f, 0.0f, 1.0f);
            jumpTuck = 0.20f + t * 0.28f;
        } else {
            // Descending arc: legs extend downward to prepare for rail impact
            float t = std::clamp(-vy / 24.0f, 0.0f, 1.0f);
            jumpTuck = std::max(0.04f, 0.20f - t * 0.16f);
        }
    }

    Vector3 leftFoot  = { pos.x + 0.17f, pos.y + 0.14f + jumpTuck + legSway, pos.z + (sliding ? 0.42f : 0.24f) };
    Vector3 rightFoot = { pos.x - 0.17f, pos.y + 0.14f + jumpTuck * 0.82f - legSway, pos.z - (sliding ? 0.32f : 0.24f) };

    auto DrawGrindBoot = [&](Vector3 fPos) {
        DrawCylinderEx({ fPos.x, fPos.y - 0.08f, fPos.z - 0.20f },
                       { fPos.x, fPos.y - 0.08f, fPos.z + 0.26f }, 0.17f, 0.14f, 8, chromeSilv);
        DrawSphere({ fPos.x, fPos.y - 0.05f, fPos.z + 0.22f }, 0.15f, chromeSilv);
        DrawCylinderEx({ fPos.x, fPos.y - 0.02f, fPos.z - 0.04f },
                       { fPos.x, fPos.y + 0.15f, fPos.z - 0.02f }, 0.14f, 0.13f, 8, armorOrange);
        DrawCube({ fPos.x, fPos.y - 0.12f, fPos.z + 0.02f }, 0.32f, 0.07f, 0.52f, SKYBLUE);
    };
    DrawGrindBoot(leftFoot);
    DrawGrindBoot(rightFoot);

    float kneeForwardTuck = airborne ? (jumpTuck * 0.32f) : 0.0f;
    Vector3 pelvisPos = { pos.x + leanX * 0.6f, pos.y + 0.72f + crouchDrop, pos.z };
    Vector3 leftKnee  = { pos.x + 0.23f + leanX * 0.3f, pos.y + 0.44f + crouchDrop * 0.55f + jumpTuck * 0.55f, pos.z + 0.20f + kneeForwardTuck };
    Vector3 rightKnee = { pos.x - 0.23f + leanX * 0.3f, pos.y + 0.42f + crouchDrop * 0.55f + jumpTuck * 0.45f, pos.z - 0.10f + kneeForwardTuck };
    Vector3 leftHip   = { pelvisPos.x + 0.16f, pelvisPos.y, pelvisPos.z + 0.06f };
    Vector3 rightHip  = { pelvisPos.x - 0.16f, pelvisPos.y, pelvisPos.z - 0.06f };

    DrawCylinderEx({ leftFoot.x, leftFoot.y + 0.10f, leftFoot.z }, leftKnee, 0.11f, 0.13f, 8, pantsBlue);
    DrawCylinderEx({ rightFoot.x, rightFoot.y + 0.10f, rightFoot.z }, rightKnee, 0.11f, 0.13f, 8, pantsBlue);
    DrawSphere(leftKnee, 0.13f, armorDark);
    DrawSphere(rightKnee, 0.13f, armorDark);
    DrawCylinderEx(leftKnee, leftHip, 0.13f, 0.15f, 8, pantsBlue);
    DrawCylinderEx(rightKnee, rightHip, 0.13f, 0.15f, 8, pantsBlue);

    // =========================================================================
    // 2. PELVIS, BELT, TAIL & COMPACT ATHLETIC TORSO
    // =========================================================================
    DrawCylinderEx({ pelvisPos.x, pelvisPos.y - 0.05f, pelvisPos.z },
                   { pelvisPos.x, pelvisPos.y + 0.09f, pelvisPos.z }, 0.30f, 0.28f, 8, leatherBrn);

    // Whipping Striped Lombax Tail
    Vector3 t0 = { pelvisPos.x, pelvisPos.y - 0.02f, pelvisPos.z - 0.22f };
    for (int seg = 0; seg < 4; ++seg) {
        float f1 = static_cast<float>(seg + 1) * 0.25f;
        float wagX = std::sin(globalTime_ * 10.0f - seg * 0.85f) * (0.24f * f1) - leanX * 0.7f * f1;
        float wagY = std::cos(globalTime_ * 5.0f - seg * 0.6f) * 0.06f + f1 * 0.10f;
        Vector3 t1 = { pelvisPos.x + wagX, pelvisPos.y - 0.04f + wagY, pelvisPos.z - 0.22f - f1 * 1.05f };
        float r0 = 0.095f - seg * 0.012f;
        float r1 = (seg == 3) ? 0.13f : (0.085f - seg * 0.012f);
        DrawCylinderEx(t0, t1, r0, r1, 6, (seg % 2 == 0) ? furGold : furStripe);
        if (seg == 3) DrawSphere(t1, 0.12f, furStripe);
        t0 = t1;
    }

    // Compact Torso (height 0.44m so shoulders sit cleanly at y = 1.14, well below the head!)
    float recoilLeanZ = -recoilKick * 0.10f;
    Vector3 shoulderCenter = { pelvisPos.x + leanX * 0.4f, pelvisPos.y + 0.44f, pelvisPos.z + recoilLeanZ };
    DrawCylinderEx(pelvisPos, shoulderCenter, 0.26f, 0.34f, 8, armorOrange);
    // Leather harness belt across chest
    DrawCylinderEx({ pelvisPos.x, pelvisPos.y + 0.10f, pelvisPos.z },
                   { shoulderCenter.x, shoulderCenter.y - 0.06f, shoulderCenter.z }, 0.28f, 0.33f, 6, leatherBrn);
    // Neck
    Vector3 neckTop = { shoulderCenter.x, shoulderCenter.y + 0.14f, shoulderCenter.z + 0.02f };
    DrawCylinderEx(shoulderCenter, neckTop, 0.18f, 0.16f, 8, furGold);

    // =========================================================================
    // 3. AUTHENTIC CLANK CHARACTER MODEL (MATCHING IN-GAME REFERENCE EXACTLY!)
    // =========================================================================
    Vector3 clankPos = { shoulderCenter.x - 0.02f, shoulderCenter.y - 0.14f, shoulderCenter.z - 0.28f };
    Color chromeBright = { 230, 238, 248, 255 };

    // --- A. Iconic Beveled Silver Backpack Housing (Matching Image 2) ---
    // Main tapered backpack block
    DrawCube({ clankPos.x, clankPos.y, clankPos.z }, 0.46f, 0.52f, 0.22f, chromeSilv);
    DrawCubeWires({ clankPos.x, clankPos.y, clankPos.z }, 0.465f, 0.525f, 0.225f, armorDark);

    // Beveled outer edge frame around the backpack
    DrawCylinderEx({ clankPos.x - 0.21f, clankPos.y - 0.24f, clankPos.z - 0.09f },
                   { clankPos.x - 0.21f, clankPos.y + 0.24f, clankPos.z - 0.09f }, 0.028f, 0.028f, 6, chromeBright);
    DrawCylinderEx({ clankPos.x + 0.21f, clankPos.y - 0.24f, clankPos.z - 0.09f },
                   { clankPos.x + 0.21f, clankPos.y + 0.24f, clankPos.z - 0.09f }, 0.028f, 0.028f, 6, chromeBright);
    DrawCylinderEx({ clankPos.x - 0.21f, clankPos.y + 0.24f, clankPos.z - 0.09f },
                   { clankPos.x + 0.21f, clankPos.y + 0.24f, clankPos.z - 0.09f }, 0.028f, 0.028f, 6, chromeBright);
    DrawCylinderEx({ clankPos.x - 0.21f, clankPos.y - 0.24f, clankPos.z - 0.09f },
                   { clankPos.x + 0.21f, clankPos.y - 0.24f, clankPos.z - 0.09f }, 0.028f, 0.028f, 6, chromeBright);

    // --- B. Top Stepped Collar with 3 Iconic Metallic Rivets / Circular Ports (Image 2) ---
    Vector3 topArch = { clankPos.x, clankPos.y + 0.21f, clankPos.z - 0.11f };
    DrawCube({ topArch.x, topArch.y, topArch.z }, 0.42f, 0.09f, 0.05f, chromeBright);
    // 3 Embossed Circular Rivets / Sockets across the top band
    float rivetY = topArch.y + 0.005f;
    float rivetZ = topArch.z - 0.028f;
    // Left Rivet
    DrawCylinderEx({ clankPos.x - 0.125f, rivetY, rivetZ + 0.01f },
                   { clankPos.x - 0.125f, rivetY, rivetZ }, 0.034f, 0.034f, 8, armorDark);
    DrawSphere({ clankPos.x - 0.125f, rivetY, rivetZ - 0.005f }, 0.026f, chromeBright);
    // Center Rivet
    DrawCylinderEx({ clankPos.x, rivetY + 0.012f, rivetZ + 0.01f },
                   { clankPos.x, rivetY + 0.012f, rivetZ }, 0.038f, 0.038f, 8, armorDark);
    DrawSphere({ clankPos.x, rivetY + 0.012f, rivetZ - 0.005f }, 0.029f, chromeBright);
    // Right Rivet
    DrawCylinderEx({ clankPos.x + 0.125f, rivetY, rivetZ + 0.01f },
                   { clankPos.x + 0.125f, rivetY, rivetZ }, 0.034f, 0.034f, 8, armorDark);
    DrawSphere({ clankPos.x + 0.125f, rivetY, rivetZ - 0.005f }, 0.026f, chromeBright);

    // --- C. Large Recessed Dark Vent / Screen in Center of Back (Image 1 & 2) ---
    Vector3 ventPos = { clankPos.x, clankPos.y - 0.01f, clankPos.z - 0.115f };
    // Raised chrome frame border
    DrawCube({ ventPos.x, ventPos.y, ventPos.z }, 0.32f, 0.30f, 0.035f, chromeBright);
    DrawCubeWires({ ventPos.x, ventPos.y, ventPos.z }, 0.325f, 0.305f, 0.038f, armorDark);
    // Recessed dark metallic screen pane
    DrawCube({ ventPos.x, ventPos.y, ventPos.z - 0.012f }, 0.25f, 0.23f, 0.025f, { 22, 26, 34, 255 });
    // Horizontal dark grey vent slats (Image 1 & 2)
    for (int slat = -1; slat <= 1; ++slat) {
        float slatY = ventPos.y + slat * 0.06f;
        DrawCube({ ventPos.x, slatY, ventPos.z - 0.026f }, 0.23f, 0.018f, 0.012f, { 52, 60, 74, 255 });
    }

    // --- D. Clank's Rounded Metallic Feet Tucked at Bottom of Backpack (Image 1 & 2) ---
    Vector3 footL = { clankPos.x - 0.12f, clankPos.y - 0.27f, clankPos.z - 0.06f };
    Vector3 footR = { clankPos.x + 0.12f, clankPos.y - 0.27f, clankPos.z - 0.06f };
    DrawSphere(footL, 0.085f, chromeBright);
    DrawSphere(footR, 0.085f, chromeBright);
    // Bottom notched rim soles (Image 1 & 2)
    DrawCylinderEx({ footL.x, footL.y - 0.04f, footL.z }, { footL.x, footL.y + 0.01f, footL.z }, 0.088f, 0.082f, 8, armorDark);
    DrawCylinderEx({ footR.x, footR.y - 0.04f, footR.z }, { footR.x, footR.y + 0.01f, footR.z }, 0.088f, 0.082f, 8, armorDark);

    // --- E. Clank's Head & Face LOOKING DIRECTLY AT CAMERA (Image 2) ---
    float clankHeadNod = airborne ? std::clamp(-player_.vy * 0.007f, -0.06f, 0.06f) : 0.0f;
    Vector3 clankHead = { clankPos.x, clankPos.y + 0.38f + clankHeadNod, clankPos.z - 0.03f };

    // Metallic spherical skull dome
    DrawSphere(clankHead, 0.195f, chromeSilv);
    // Rounded lower chin / jaw line (Image 1 & 2)
    DrawSphere({ clankHead.x, clankHead.y - 0.065f, clankHead.z - 0.06f }, 0.145f, chromeBright);
    // Subtle dark neck joint ring
    DrawCylinderEx({ clankHead.x, clankHead.y - 0.14f, clankHead.z },
                   { clankHead.x, clankHead.y - 0.06f, clankHead.z }, 0.12f, 0.13f, 8, armorDark);

    // Side cylindrical ear dials (Image 1 & 2)
    Vector3 earL = { clankHead.x - 0.195f, clankHead.y + 0.015f, clankHead.z };
    Vector3 earR = { clankHead.x + 0.195f, clankHead.y + 0.015f, clankHead.z };
    DrawCylinderEx({ earL.x - 0.03f, earL.y, earL.z }, { earL.x + 0.02f, earL.y, earL.z }, 0.052f, 0.052f, 8, armorDark);
    DrawCylinderEx({ earR.x - 0.02f, earR.y, earR.z }, { earR.x + 0.03f, earR.y, earR.z }, 0.052f, 0.052f, 8, armorDark);
    DrawSphere({ earL.x - 0.03f, earL.y, earL.z }, 0.038f, chromeBright);
    DrawSphere({ earR.x + 0.03f, earR.y, earR.z }, 0.038f, chromeBright);

    // --- F. BIG GLOWING EMERALD-GREEN CAMERA EYES (Image 1 & 2) ---
    // Facing the player/camera in -Z direction!
    Vector3 eyeL = { clankHead.x - 0.082f, clankHead.y + 0.052f, clankHead.z - 0.142f };
    Vector3 eyeR = { clankHead.x + 0.082f, clankHead.y + 0.052f, clankHead.z - 0.142f };

    // Thick dark gunmetal socket rims
    DrawSphere(eyeL, 0.082f, BLACK);
    DrawSphere(eyeR, 0.082f, BLACK);

    // Brilliant luminous neon-lime / emerald glowing lenses (matching reference screenshot!)
    Color clankEyeGlow = { 65, 255, 95, 255 };
    DrawSphere(eyeL, 0.072f, clankEyeGlow);
    DrawSphere(eyeR, 0.072f, clankEyeGlow);

    // Bright inner lens highlights
    DrawSphere({ eyeL.x, eyeL.y, eyeL.z - 0.028f }, 0.045f, { 185, 255, 195, 255 });
    DrawSphere({ eyeR.x, eyeR.y, eyeR.z - 0.028f }, 0.045f, { 185, 255, 195, 255 });
    DrawSphere({ eyeL.x - 0.015f, eyeL.y + 0.015f, eyeL.z - 0.045f }, 0.022f, WHITE);
    DrawSphere({ eyeR.x - 0.015f, eyeR.y + 0.015f, eyeR.z - 0.045f }, 0.022f, WHITE);

    // --- G. Top Hatch Collar, Antenna & Organic Red Beacon Disappearance ---
    float deploy = player_.heliDeploy;
    Vector3 hatchPos = { clankHead.x, clankHead.y + 0.185f, clankHead.z };
    // Circular titanium hatch collar
    DrawCylinderEx(hatchPos, { hatchPos.x, hatchPos.y + 0.025f, hatchPos.z }, 0.082f, 0.075f, 10, armorDark);

    // Organic Antenna & Red Dot Transition:
    // When deploy == 0, antenna is fully upright and red beacon pulses brightly.
    // When deploy starts rising (0.0 -> 0.18), the antenna stem and red light smoothly sink into the hatch and fade away!
    float antFrac = std::clamp(deploy / 0.18f, 0.0f, 1.0f);
    float antVisible = 1.0f - antFrac;
    if (antVisible > 0.01f) {
        float stemH = 0.15f * antVisible;
        Vector3 stemTip = { hatchPos.x, hatchPos.y + 0.02f + stemH, hatchPos.z };
        DrawCylinderEx({ hatchPos.x, hatchPos.y + 0.02f, hatchPos.z }, stemTip,
                       0.016f * antVisible, 0.012f * antVisible, 6, chromeSilv);
        // Red glowing beacon bulb at antenna tip (Image 1 & 2)
        float redRadius = 0.045f * antVisible;
        Color redCol  = ColorAlpha({ 255, 35, 35, 255 }, antVisible);
        Color redCore = ColorAlpha(WHITE, antVisible * 0.85f);
        DrawSphere(stemTip, redRadius, redCol);
        DrawSphere(stemTip, redRadius * 0.45f, redCore);
    }

    // --- H. Articulated Segmented Robot Arms Wrapping Over Ratchet's Shoulders ---
    Vector3 clankLShoulder = { clankPos.x + 0.19f, clankPos.y + 0.14f, clankPos.z + 0.02f };
    Vector3 clankRShoulder = { clankPos.x - 0.19f, clankPos.y + 0.14f, clankPos.z + 0.02f };
    Vector3 clankLElbow    = { clankPos.x + 0.22f, clankPos.y + 0.08f, clankPos.z + 0.08f };
    Vector3 clankRElbow    = { clankPos.x - 0.22f, clankPos.y + 0.08f, clankPos.z + 0.08f };
    Vector3 clankLHand     = { shoulderCenter.x + 0.17f, shoulderCenter.y + 0.03f, shoulderCenter.z + 0.10f };
    Vector3 clankRHand     = { shoulderCenter.x - 0.17f, shoulderCenter.y + 0.03f, shoulderCenter.z + 0.10f };

    DrawCylinderEx(clankLShoulder, clankLElbow, 0.038f, 0.032f, 6, chromeSilv);
    DrawCylinderEx(clankRShoulder, clankRElbow, 0.038f, 0.032f, 6, chromeSilv);
    DrawSphere(clankLElbow, 0.042f, armorDark);
    DrawSphere(clankRElbow, 0.042f, armorDark);
    DrawCylinderEx(clankLElbow, clankLHand, 0.032f, 0.028f, 6, chromeSilv);
    DrawCylinderEx(clankRElbow, clankRHand, 0.032f, 0.028f, 6, chromeSilv);
    // Articulated hands resting on Ratchet's front shoulder harness straps
    DrawSphere(clankLHand, 0.042f, armorDark);
    DrawSphere(clankRHand, 0.042f, armorDark);

    // --- I. Procedural Clank Heli-Pack Deployment (Smooth mechanical unfolding - NO POP-IN!) ---
    if (deploy > 0.008f) {
        // 1. Telescoping chrome mast extending from Clank's head hatch (0.06m -> 0.85m)
        float mastHeight = Lerp(0.06f, 0.85f, deploy);
        Vector3 mastBase = { hatchPos.x, hatchPos.y + 0.02f, hatchPos.z };
        Vector3 rotorHub = { hatchPos.x, hatchPos.y + 0.02f + mastHeight, hatchPos.z };
        DrawCylinderEx(mastBase, rotorHub, 0.048f, 0.042f, 8, chromeSilv);
        DrawSphere(rotorHub, 0.075f * deploy, armorDark);

        // 2. Twin titanium blades unfolding from vertical stowage to horizontal flight plane
        float unfoldFrac = std::clamp((deploy - 0.10f) / 0.60f, 0.0f, 1.0f);
        // Smoothstep easing for organic unfolding
        float s = unfoldFrac * unfoldFrac * (3.0f - 2.0f * unfoldFrac);
        float bladeSpan  = Lerp(0.25f, 1.65f, s);
        // Blades start folded pointing down along the mast (bladeTipY < 0), then swing out flat
        float bladeTipY  = -0.38f * (1.0f - s);
        float angle = player_.heliRotorAngle;

        for (int b = 0; b < 2; ++b) {
            float a = angle + b * 1.570796f;
            Vector3 pA = { rotorHub.x + std::cos(a) * bladeSpan, rotorHub.y + bladeTipY, rotorHub.z + std::sin(a) * bladeSpan };
            Vector3 pB = { rotorHub.x - std::cos(a) * bladeSpan, rotorHub.y + bladeTipY, rotorHub.z - std::sin(a) * bladeSpan };
            DrawCylinderEx(pA, pB, 0.055f * deploy, 0.055f * deploy, 4, (b == 0) ? SKYBLUE : WHITE);
            DrawSphere(pA, 0.04f * deploy, SKYBLUE);
            DrawSphere(pB, 0.04f * deploy, SKYBLUE);
        }

        // 3. Hypersonic aerodynamic energy disc halo (fades in as RPM accelerates)
        if (deploy > 0.35f) {
            float discFrac = (deploy - 0.35f) / 0.65f;
            DrawCircle3D(rotorHub, bladeSpan, { 1.0f, 0.0f, 0.0f }, 90.0f, ColorAlpha(SKYBLUE, discFrac * 0.70f));
            DrawCircle3D(rotorHub, bladeSpan * 0.55f, { 1.0f, 0.0f, 0.0f }, 90.0f, ColorAlpha(WHITE, discFrac * 0.40f));
        }

        // 4. Twin thruster exhaust plumes under Clank's feet
        float thrustLen = 0.58f * deploy;
        DrawCylinderEx({ footL.x, footL.y - 0.04f, footL.z },
                       { footL.x, footL.y - 0.04f - thrustLen, footL.z },
                       0.075f * deploy, 0.015f, 8, ColorAlpha(SKYBLUE, deploy * 0.90f));
        DrawCylinderEx({ footR.x, footR.y - 0.04f, footR.z },
                       { footR.x, footR.y - 0.04f - thrustLen, footR.z },
                       0.075f * deploy, 0.015f, 8, ColorAlpha(SKYBLUE, deploy * 0.90f));
    }

    // =========================================================================
    // 4. RATCHET'S HEAD, AVIATOR CAP & TRUE POINTED TRIANGULAR LOMBAX EARS!
    // =========================================================================
    Vector3 headPos = { neckTop.x, neckTop.y + 0.22f, neckTop.z + 0.02f };
    // Golden fur head & snout
    DrawSphere(headPos, 0.34f, furGold);
    DrawCylinderEx(headPos, { headPos.x, headPos.y - 0.05f, headPos.z + 0.30f }, 0.22f, 0.14f, 8, furGold);
    // Brown Leather Pilot Helmet Cap covering crown & back of skull
    DrawSphere({ headPos.x, headPos.y + 0.04f, headPos.z - 0.03f }, 0.355f, leatherBrn);
    // Pilot Goggles on Forehead
    DrawCylinderEx({ headPos.x - 0.13f, headPos.y + 0.15f, headPos.z + 0.22f },
                   { headPos.x - 0.13f, headPos.y + 0.15f, headPos.z + 0.32f }, 0.10f, 0.10f, 8, SKYBLUE);
    DrawCylinderEx({ headPos.x + 0.13f, headPos.y + 0.15f, headPos.z + 0.22f },
                   { headPos.x + 0.13f, headPos.y + 0.15f, headPos.z + 0.32f }, 0.10f, 0.10f, 8, SKYBLUE);

    // True Pointed Fan-Shaped Lombax Ears (Broad vertical attachment at skull -> tapering to a sharp pointed tip!)
    // Built from 4 stacked horizontal spars that converge to a single outer tip so the ear is THIN front-to-back
    // and WIDE vertically near the head, tapering to a sharp point!
    float earBob = std::sin(globalTime_ * 20.0f) * 0.03f - std::clamp(player_.vy * 0.010f, -0.16f, 0.16f);
    for (int side = -1; side <= 1; side += 2) {
        float s = static_cast<float>(side);
        Vector3 earTip     = { headPos.x + s * 0.92f, headPos.y + 0.22f + earBob, headPos.z - 0.10f };
        Vector3 rootTop    = { headPos.x + s * 0.26f, headPos.y + 0.15f, headPos.z - 0.02f };
        Vector3 rootUpper  = { headPos.x + s * 0.28f, headPos.y + 0.06f, headPos.z - 0.03f };
        Vector3 rootMid    = { headPos.x + s * 0.28f, headPos.y - 0.03f, headPos.z - 0.04f };
        Vector3 rootBot    = { headPos.x + s * 0.24f, headPos.y - 0.12f, headPos.z - 0.05f };

        // 1. Thick dark-brown/auburn upper ear rim tapering to a sharp point at earTip
        DrawCylinderEx(rootTop, earTip, 0.085f, 0.025f, 6, furStripe);
        // 2. Golden fur upper-middle ear blade tapering into earTip
        DrawCylinderEx(rootUpper, earTip, 0.095f, 0.020f, 6, furGold);
        // 3. Golden fur lower-middle ear blade tapering into earTip
        DrawCylinderEx(rootMid, Vector3Lerp(rootMid, earTip, 0.92f), 0.090f, 0.025f, 6, furGold);
        // 4. Warm tan lower ear fold cartilage ridge tapering into the outer edge
        DrawCylinderEx(rootBot, Vector3Lerp(rootTop, earTip, 0.85f), 0.075f, 0.020f, 6, furInner);

        // 3 Iconic Dark-Brown Vertical Tiger Stripes along the back of the ear fan!
        for (int st = 1; st <= 3; ++st) {
            float frac = st * 0.23f;
            Vector3 stTop = Vector3Lerp(rootTop, earTip, frac);
            Vector3 stBot = Vector3Lerp(rootBot, earTip, frac * 0.9f);
            stTop.z -= 0.03f;
            stBot.z -= 0.03f;
            DrawCylinderEx(stTop, stBot, 0.035f, 0.025f, 5, furStripe);
        }
    }

    // =========================================================================
    // 5. CLEAN, NATURAL ARMS WITH SIDE-MOUNTED GUN AIMING & RECOIL!
    // =========================================================================
    Vector3 leftShoulder  = { shoulderCenter.x + 0.32f, shoulderCenter.y - 0.04f, shoulderCenter.z };
    Vector3 rightShoulder = { shoulderCenter.x - 0.32f, shoulderCenter.y - 0.04f, shoulderCenter.z };
    DrawSphere(leftShoulder,  0.13f, armorOrange);
    DrawSphere(rightShoulder, 0.13f, armorOrange);

    WeaponId wEnum = static_cast<WeaponId>(weaponId);
    bool wrenchSwinging = (wrenchTimer > 0.0f);
    // Progress of forward wrench strike: 0.0 (start) -> 1.0 (finish)
    float swingProg = wrenchSwinging ? (1.0f - std::clamp(wrenchTimer / 0.38f, 0.0f, 1.0f)) : 0.0f;

    // Smooth alternating skater balance arm swing (visible sideways & vertically from behind camera!)
    float swingL = std::sin(globalTime_ * 8.0f) * 0.06f;
    float swingR = std::cos(globalTime_ * 8.0f) * 0.06f;

    // --- Left Arm (+X, screen-left): Clean relaxed skater balance pose ---
    Vector3 leftElbow = {
        leftShoulder.x + 0.24f,
        leftShoulder.y - 0.22f + swingL * 0.5f,
        leftShoulder.z - 0.04f
    };
    Vector3 leftHand = {
        leftShoulder.x + 0.46f,
        leftShoulder.y - 0.30f + swingL,
        leftShoulder.z + 0.14f
    };

    // --- Right Arm (-X, screen-right): Skater Balance Pose, Side Gun Aiming, OR Forward Wrench Strike! ---
    Vector3 surfRightElbow = {
        rightShoulder.x - 0.24f,
        rightShoulder.y - 0.22f + swingR * 0.5f,
        rightShoulder.z - 0.04f
    };
    Vector3 surfRightHand = {
        rightShoulder.x - 0.44f,
        rightShoulder.y - 0.32f + swingR,
        rightShoulder.z + 0.12f
    };

    Vector3 aimRightElbow = {
        rightShoulder.x - 0.22f,
        rightShoulder.y - 0.14f - recoilKick * 0.04f,
        rightShoulder.z + 0.08f - recoilKick * 0.16f
    };
    Vector3 aimRightHand = {
        rightShoulder.x - 0.26f,
        rightShoulder.y + 0.02f + recoilKick * 0.12f,
        rightShoulder.z + 0.38f - recoilKick * 0.22f
    };

    Vector3 rightElbow = Vector3Lerp(surfRightElbow, aimRightElbow, aimBlend);
    Vector3 rightHand  = Vector3Lerp(surfRightHand,  aimRightHand,  aimBlend);

    // Dynamic Lombax jumping arm balance & Heli-Pack harness grip pose
    if (airborne && !wrenchSwinging && player_.gunAimTimer <= 0.0f) {
        if (deploy > 0.22f) {
            // Reaching both hands up to grip Clank's backpack shoulder straps
            float gripFrac = (deploy - 0.22f) / 0.78f;
            Vector3 gripLeftHand   = { leftShoulder.x - 0.10f, leftShoulder.y + 0.16f, leftShoulder.z + 0.06f };
            Vector3 gripRightHand  = { rightShoulder.x + 0.10f, rightShoulder.y + 0.16f, rightShoulder.z + 0.06f };
            Vector3 gripLeftElbow  = { leftShoulder.x + 0.18f, leftShoulder.y + 0.06f, leftShoulder.z };
            Vector3 gripRightElbow = { rightShoulder.x - 0.18f, rightShoulder.y + 0.06f, rightShoulder.z };
            leftHand   = Vector3Lerp(leftHand, gripLeftHand, gripFrac);
            rightHand  = Vector3Lerp(rightHand, gripRightHand, gripFrac);
            leftElbow  = Vector3Lerp(leftElbow, gripLeftElbow, gripFrac);
            rightElbow = Vector3Lerp(rightElbow, gripRightElbow, gripFrac);
        } else if (vy > 0.0f) {
            // Leaping upward: arms flare out and back dynamically for mid-air balance
            float jumpFrac = std::clamp(vy / 18.5f, 0.0f, 1.0f);
            leftElbow.y  += jumpFrac * 0.16f;
            leftElbow.z  -= jumpFrac * 0.12f;
            leftHand.y   += jumpFrac * 0.20f;
            leftHand.x   += jumpFrac * 0.14f;
            rightElbow.y += jumpFrac * 0.16f;
            rightElbow.z -= jumpFrac * 0.12f;
            rightHand.y  += jumpFrac * 0.20f;
            rightHand.x  -= jumpFrac * 0.14f;
        } else if (vy < 0.0f) {
            // Descending arc: arms sweep forward and slightly down to brace for landing
            float fallFrac = std::clamp(-vy / 24.0f, 0.0f, 1.0f);
            leftElbow.y  -= fallFrac * 0.12f;
            leftElbow.z  += fallFrac * 0.14f;
            leftHand.y   -= fallFrac * 0.16f;
            leftHand.z   += fallFrac * 0.22f;
            rightElbow.y -= fallFrac * 0.12f;
            rightElbow.z += fallFrac * 0.14f;
            rightHand.y  -= fallFrac * 0.16f;
            rightHand.z  += fallFrac * 0.22f;
        }
    }

    // Right-to-Left Horizontal Wrench Swing across Ratchet's lane (not a forward knife poke!)
    // Note: Camera looks from -Z toward +Z, so Screen-Right is -X and Screen-Left is +X.
    // Idle wrench points out to the right (-X) and slightly up so its full silhouette is clear.
    Vector3 wrenchForwardDir = Vector3Normalize({ -0.82f, 0.42f, 0.18f });
    Vector3 wrenchJawPerp    = { 0.0f, 1.0f, 0.0f };
    if (wrenchSwinging) {
        // Smoothstep 0.0 (far right) -> 0.5 (front center) -> 1.0 (far left)
        float s = swingProg * swingProg * (3.0f - 2.0f * swingProg);
        // Sweep angle in XZ plane: 0 rad = Screen-Right (-X), PI/2 = Front (+Z), PI = Screen-Left (+X)
        float sweepAngle = -0.15f + s * 3.30f;
        float dirX = -std::cos(sweepAngle); // Sweeps from -0.99 (Right) -> 0.0 (Center) -> +0.99 (Left)
        float dirZ =  std::sin(sweepAngle); // Arcs forward (+Z) across the front of Ratchet's body

        rightElbow = {
            shoulderCenter.x + dirX * 0.36f,
            shoulderCenter.y - 0.06f,
            shoulderCenter.z + std::max(0.06f, dirZ * 0.34f)
        };
        rightHand = {
            shoulderCenter.x + dirX * 0.64f,
            shoulderCenter.y - 0.04f,
            shoulderCenter.z + std::max(0.14f, dirZ * 0.58f)
        };
        // Wrench shaft extends radially along the horizontal swing so it sweeps right-to-left like a bat!
        wrenchForwardDir = Vector3Normalize({ dirX, 0.05f - s * 0.10f, dirZ });
        wrenchJawPerp    = { 0.0f, 1.0f, 0.0f };
    }

    auto DrawCleanArm = [&](Vector3 sh, Vector3 elb, Vector3 hnd) {
        // Golden fur upper arm
        DrawCylinderEx(sh, elb, 0.105f, 0.090f, 8, furGold);
        DrawSphere(elb, 0.092f, furGold);
        // Tapered brown leather glove forearm
        DrawCylinderEx(elb, hnd, 0.092f, 0.110f, 8, leatherBrn);
        // Compact gloved fist
        DrawSphere(hnd, 0.115f, gloveBrn);
    };

    DrawCleanArm(leftShoulder,  leftElbow,  leftHand);
    DrawCleanArm(rightShoulder, rightElbow, rightHand);

    // =========================================================================
    // 6. 6 CUSTOM 3D WEAPON MODELS (RIGHT-TO-LEFT OMNIWRENCH + 5 GADGETRON GUNS)
    // =========================================================================
    Color weapCol = GetWeaponCatalog()[weaponId].projectileColor;
    Vector3 gunDir = Vector3Normalize({
        (1.0f - aimBlend) * (-0.12f),
        recoilKick * 0.22f - (1.0f - aimBlend) * 0.12f,
        1.0f
    });
    Vector3 gunBreech = Vector3Subtract(rightHand, Vector3Scale(gunDir, 0.10f));
    Vector3 gunMuzzle = Vector3Add(rightHand, Vector3Scale(gunDir, 0.78f));

    if (wEnum == WeaponId::OMNIWRENCH_8000 || wrenchSwinging) {
        // Draw the 3D OmniWrench 8000 in Ratchet's right hand (ready pose or right-to-left horizontal swing!)
        Vector3 wDir = wrenchForwardDir;
        Vector3 wPerp = wrenchJawPerp;
        Vector3 wGripBot = Vector3Subtract(rightHand, Vector3Scale(wDir, 0.14f));
        Vector3 wNeck    = Vector3Add(rightHand, Vector3Scale(wDir, 0.56f));
        DrawCylinderEx(wGripBot, wNeck, 0.046f, 0.056f, 8, chromeSilv);

        // Wide Perpendicular Wrench Jaw Crossbar
        Vector3 jawBaseL = Vector3Add(wNeck, Vector3Scale(wPerp, -0.17f));
        Vector3 jawBaseR = Vector3Add(wNeck, Vector3Scale(wPerp, +0.17f));
        DrawCylinderEx(jawBaseL, jawBaseR, 0.062f, 0.062f, 6, armorDark);

        // Twin Open Parallel Wrench Prongs (U-shaped crescent wrench head)
        Vector3 jawTipL = Vector3Add(jawBaseL, Vector3Scale(wDir, 0.26f));
        Vector3 jawTipR = Vector3Add(jawBaseR, Vector3Scale(wDir, 0.26f));
        DrawCylinderEx(jawBaseL, jawTipL, 0.062f, 0.038f, 6, chromeSilv);
        DrawCylinderEx(jawBaseR, jawTipR, 0.062f, 0.038f, 6, chromeSilv);

        // Horizontal Right-to-Left Slash Arc Trail across Ratchet's lane during swing!
        if (wrenchSwinging) {
            float s = swingProg * swingProg * (3.0f - 2.0f * swingProg);
            float curAng = -0.15f + s * 3.30f;
            float startAng = std::max(-0.15f, curAng - 1.55f);
            constexpr int ARC_SEGS = 8;
            for (int seg = 0; seg < ARC_SEGS; ++seg) {
                float a0 = Lerp(startAng, curAng, static_cast<float>(seg) / static_cast<float>(ARC_SEGS));
                float a1 = Lerp(startAng, curAng, static_cast<float>(seg + 1) / static_cast<float>(ARC_SEGS));
                Vector3 p0 = {
                    shoulderCenter.x - std::cos(a0) * 1.32f,
                    shoulderCenter.y - 0.04f,
                    shoulderCenter.z + std::sin(a0) * 1.25f
                };
                Vector3 p1 = {
                    shoulderCenter.x - std::cos(a1) * 1.32f,
                    shoulderCenter.y - 0.04f,
                    shoulderCenter.z + std::sin(a1) * 1.25f
                };
                DrawCylinderEx(p0, p1, 0.065f, 0.045f, 6, SKYBLUE);
                Vector3 p0In = {
                    shoulderCenter.x - std::cos(a0) * 1.05f,
                    shoulderCenter.y - 0.04f,
                    shoulderCenter.z + std::sin(a0) * 1.00f
                };
                Vector3 p1In = {
                    shoulderCenter.x - std::cos(a1) * 1.05f,
                    shoulderCenter.y - 0.04f,
                    shoulderCenter.z + std::sin(a1) * 1.00f
                };
                DrawCylinderEx(p0In, p1In, 0.040f, 0.030f, 6, WHITE);
            }
        }
    } else if (wEnum == WeaponId::COMBUSTER) {
        gunMuzzle = Vector3Add(rightHand, Vector3Scale(gunDir, 0.68f));
        DrawCylinderEx(gunBreech, gunMuzzle, 0.11f, 0.085f, 8, armorOrange);
        DrawCylinderEx({ gunBreech.x, gunBreech.y + 0.07f, gunBreech.z + 0.08f },
                       { gunMuzzle.x, gunMuzzle.y + 0.07f, gunMuzzle.z }, 0.06f, 0.06f, 6, chromeSilv);
        DrawSphere(gunMuzzle, 0.095f, YELLOW);
    } else if (wEnum == WeaponId::SHOCK_BLASTER) {
        gunMuzzle = Vector3Add(rightHand, Vector3Scale(gunDir, 0.92f));
        DrawCylinderEx(gunBreech, gunMuzzle, 0.14f, 0.15f, 8, armorDark);
        DrawCylinderEx({ gunBreech.x - 0.08f, gunBreech.y + 0.03f, gunBreech.z + 0.12f },
                       { gunMuzzle.x - 0.08f, gunMuzzle.y + 0.03f, gunMuzzle.z }, 0.075f, 0.085f, 8, chromeSilv);
        DrawCylinderEx({ gunBreech.x + 0.08f, gunBreech.y + 0.03f, gunBreech.z + 0.12f },
                       { gunMuzzle.x + 0.08f, gunMuzzle.y + 0.03f, gunMuzzle.z }, 0.075f, 0.085f, 8, chromeSilv);
        DrawCylinderEx(Vector3Lerp(gunBreech, gunMuzzle, 0.35f),
                       Vector3Lerp(gunBreech, gunMuzzle, 0.68f), 0.12f, 0.12f, 6, SKYBLUE);
    } else if (wEnum == WeaponId::BUZZ_BLADES) {
        gunMuzzle = Vector3Add(rightHand, Vector3Scale(gunDir, 0.78f));
        DrawCylinderEx(gunBreech, gunMuzzle, 0.13f, 0.13f, 8, { 45, 85, 55, 255 });
        Vector3 discCenter = Vector3Lerp(gunBreech, gunMuzzle, 0.72f);
        DrawCylinderEx({ discCenter.x, discCenter.y + 0.07f, discCenter.z },
                       { discCenter.x, discCenter.y + 0.13f, discCenter.z }, 0.28f, 0.28f, 8, GREEN);
        DrawCylinderWiresEx({ discCenter.x, discCenter.y + 0.07f, discCenter.z },
                            { discCenter.x, discCenter.y + 0.13f, discCenter.z }, 0.30f, 0.30f, 8, WHITE);
    } else if (wEnum == WeaponId::PLASMA_STRIKER) {
        gunMuzzle = Vector3Add(rightHand, Vector3Scale(gunDir, 1.22f));
        DrawCylinderEx(gunBreech, gunMuzzle, 0.11f, 0.07f, 8, chromeSilv);
        DrawCylinderEx(Vector3Lerp(gunBreech, gunMuzzle, 0.25f),
                       Vector3Lerp(gunBreech, gunMuzzle, 0.75f), 0.13f, 0.10f, 6, { 185, 75, 255, 255 });
        Vector3 scopeA = Vector3Add(Vector3Lerp(gunBreech, gunMuzzle, 0.15f), { 0.0f, 0.14f, 0.0f });
        Vector3 scopeB = Vector3Add(Vector3Lerp(gunBreech, gunMuzzle, 0.52f), { 0.0f, 0.14f, 0.0f });
        DrawCylinderEx(scopeA, scopeB, 0.05f, 0.06f, 8, armorDark);
    } else if (wEnum == WeaponId::RYNO_INFINITE) {
        gunMuzzle = Vector3Add(rightHand, Vector3Scale(gunDir, 1.02f));
        DrawCylinderEx(gunBreech, Vector3Lerp(gunBreech, gunMuzzle, 0.55f), 0.22f, 0.22f, 10, { 185, 28, 35, 255 });
        DrawCylinderWiresEx(gunBreech, Vector3Lerp(gunBreech, gunMuzzle, 0.55f), 0.23f, 0.23f, 10, GOLD);
        for (int b = 0; b < 6; ++b) {
            float ba = (b * 60.0f + player_.barrelSpinAngle) * DEG2RAD;
            Vector3 offset = { std::cos(ba) * 0.13f, std::sin(ba) * 0.13f, 0.0f };
            DrawCylinderEx(Vector3Add(gunBreech, offset), Vector3Add(gunMuzzle, offset),
                           0.048f, 0.048f, 6, chromeSilv);
        }
    }

    if (recoilFrac > 0.15f && wEnum != WeaponId::OMNIWRENCH_8000) {
        float flashScale = recoilKick * 0.48f;
        DrawSphere(gunMuzzle, flashScale * 0.55f, WHITE);
        DrawSphere(gunMuzzle, flashScale * 0.85f, weapCol);
        for (int f = 0; f < 4; ++f) {
            float fa = (f * 90.0f + globalTime_ * 400.0f) * DEG2RAD;
            Vector3 spikeTip = {
                gunMuzzle.x + std::cos(fa) * flashScale * 1.5f,
                gunMuzzle.y + std::sin(fa) * flashScale * 1.5f,
                gunMuzzle.z + 0.22f
            };
            DrawCylinderEx(gunMuzzle, spikeTip, 0.08f * flashScale, 0.01f, 5, YELLOW);
        }
        DrawCircle3D(gunMuzzle, flashScale * 1.25f, { 0.0f, 0.0f, 1.0f }, 0.0f, weapCol);
    }
}

void RailGame::RenderHud2D() {
    // 2D Tactical Action Callouts & Target Reticles projected over 3D Crates, Obstacles & Enemies!
    for (const auto& h : hazards_) {
        if (!h.active || h.z < 3.5f || h.z > 58.0f) continue;

        float rawLx = GetLaneX(h.lane);
        Vector3 worldTop = ApplyTrackCurve(rawLx, 2.25f, h.z);
        Vector2 scr = GetWorldToScreen(worldTop, camera_);
        int sx = static_cast<int>(scr.x);
        int sy = static_cast<int>(scr.y);

        // Enemy Lock-On Reticle + Floating HP Bar
        if (h.kind == HazardKind::THUG_DRONE || h.kind == HazardKind::SHIELDED_ENFORCER) {
            Vector3 bodyWorld = ApplyTrackCurve(rawLx, 1.40f, h.z);
            Vector2 bodyScr = GetWorldToScreen(bodyWorld, camera_);
            int bx = static_cast<int>(bodyScr.x);
            int by = static_cast<int>(bodyScr.y);
            if (h.lane == player_.currentLane) {
                DrawCircleLines(bx, by, 24.0f, RED);
                DrawCircleLines(bx, by, 16.0f, GOLD);
            }
            float hpRatio = std::clamp(h.hp / h.maxHp, 0.0f, 1.0f);
            DrawRectangle(sx - 28, sy - 10, 56, 8, { 25, 15, 20, 220 });
            DrawRectangle(sx - 28, sy - 10, static_cast<int>(56.0f * hpRatio), 8, RED);
            DrawRectangleLines(sx - 28, sy - 10, 56, 8, WHITE);
        } else if (h.z < 42.0f) {
            const char* tag = nullptr;
            Color tagCol = WHITE;
            if (h.kind == HazardKind::LOW_LASER_BARRIER && h.lane == player_.currentLane) {
                tag = "^ JUMP ^";
                tagCol = { 255, 95, 110, 255 };
            } else if (h.kind == HazardKind::HIGH_LASER_GATE && h.lane == player_.currentLane) {
                tag = "v SLIDE v";
                tagCol = { 255, 175, 45, 255 };
            } else if (h.kind == HazardKind::BROKEN_RAIL_GAP && h.lane == player_.currentLane) {
                tag = "! RAIL GAP: JUMP !";
                tagCol = RED;
            } else if (h.kind == HazardKind::TNT_CRATE) {
                tag = "! TNT !";
                tagCol = YELLOW;
            } else if (h.kind == HazardKind::NANOTECH_CRATE) {
                tag = "+NANOTECH";
                tagCol = SKYBLUE;
            } else if (h.kind == HazardKind::RARITANIUM_CACHE) {
                tag = "+RARITANIUM";
                tagCol = SKYBLUE;
            }

            if (tag != nullptr) {
                int tw = MeasureText(tag, 14);
                DrawRectangle(sx - tw / 2 - 6, sy - 8, tw + 12, 20, { 10, 14, 24, 210 });
                DrawRectangleLines(sx - tw / 2 - 6, sy - 8, tw + 12, 20, tagCol);
                DrawText(tag, sx - tw / 2, sy - 5, 14, tagCol);
            }
        }
    }

    // Top-Left Nanotech Health & Bolt Multiplier Box
    DrawRectangle(20, 20, 345, 96, { 12, 18, 32, 215 });
    DrawRectangleLinesEx({ 20.0f, 20.0f, 345.0f, 96.0f }, 2.0f, { 255, 165, 40, 190 });

    DrawText("NANOTECH HEALTH", 34, 28, 16, SKYBLUE);
    int maxPips = static_cast<int>(player_.maxNanotechHp);
    for (int i = 0; i < maxPips; ++i) {
        float fill = std::clamp(player_.nanotechHp - static_cast<float>(i), 0.0f, 1.0f);
        DrawRectangle(34 + i * 34, 48, 28, 18, { 35, 45, 60, 255 });
        if (fill > 0.0f) {
            Color pipCol = (fill < 0.99f) ? Color{ 255, 195, 55, 255 } : Color{ 65, 225, 255, 255 };
            DrawRectangle(34 + i * 34, 48, static_cast<int>(28.0f * fill), 18, pipCol);
        }
        DrawRectangleLines(34 + i * 34, 48, 28, 18, WHITE);
    }

    char multTxt[64];
    std::snprintf(multTxt, sizeof(multTxt), "BOLT MULTIPLIER: %dx", player_.multiplierTier);
    DrawText(multTxt, 34, 76, 18, (player_.multiplierTier >= 12) ? GOLD : ORANGE);

    DrawRectangle(245, 80, 104, 12, { 30, 38, 52, 255 });
    DrawRectangle(245, 80, static_cast<int>(104.0f * player_.multiplierProgress), 12, GOLD);

    // Top-Right Bolts, Raritanium & Distance Odometer
    DrawRectangle(VIRTUAL_WIDTH - 320, 20, 300, 96, { 12, 18, 32, 215 });
    DrawRectangleLinesEx({ VIRTUAL_WIDTH - 320.0f, 20.0f, 300.0f, 96.0f }, 2.0f, { 65, 215, 255, 190 });

    char distBuf[64], boltBuf[64], rariBuf[64];
    std::snprintf(distBuf, sizeof(distBuf), "DISTANCE: %d m", static_cast<int>(distanceMeters_));
    std::snprintf(boltBuf, sizeof(boltBuf), "BOLTS: %d (+%d)", save_.bolts, runBoltsEarned_);
    std::snprintf(rariBuf, sizeof(rariBuf), "RARITANIUM: %d (+%d)", save_.raritanium, runRaritaniumEarned_);

    DrawText(distBuf, VIRTUAL_WIDTH - 304, 28, 24, WHITE);
    DrawText(boltBuf, VIRTUAL_WIDTH - 304, 58, 18, GOLD);
    DrawText(rariBuf, VIRTUAL_WIDTH - 304, 82, 16, SKYBLUE);

    // Boss Health Bar
    if (boss_.active) {
        float bw = 520.0f;
        float bx = (VIRTUAL_WIDTH - bw) * 0.5f;
        DrawRectangle(static_cast<int>(bx - 6), 24, static_cast<int>(bw + 12), 54, { 18, 12, 24, 230 });
        DrawText(boss_.name.c_str(), static_cast<int>(bx + 8), 28, 18, GOLD);

        float totalHp = boss_.turretHp[0] + boss_.turretHp[1] + boss_.turretHp[2];
        float ratio = std::clamp(totalHp / (boss_.maxTurretHp * 3.0f), 0.0f, 1.0f);
        DrawRectangle(static_cast<int>(bx), 52, static_cast<int>(bw), 16, { 50, 20, 20, 255 });
        DrawRectangle(static_cast<int>(bx), 52, static_cast<int>(bw * ratio), 16, RED);
        DrawRectangleLines(static_cast<int>(bx), 52, static_cast<int>(bw), 16, WHITE);
    }

    // Bottom-Right Quick-Select Weapon Hotbar (1..6)
    const auto& weaps = GetWeaponCatalog();
    for (int w = 0; w < static_cast<int>(NUM_WEAPONS); ++w) {
        Rectangle sr = { VIRTUAL_WIDTH - 418.0f + w * 66.0f, VIRTUAL_HEIGHT - 84.0f, 60.0f, 64.0f };
        bool unlocked = save_.unlockedWeapons[w];
        bool equipped = (save_.equippedWeapon == w);

        DrawRectangleRec(sr, equipped ? Color{ 45, 68, 105, 240 } : Color{ 16, 22, 36, 210 });
        DrawRectangleLinesEx(sr, equipped ? 2.5f : 1.5f, equipped ? GOLD : (unlocked ? weaps[w].projectileColor : DARKGRAY));

        char keyTxt[16];
        std::snprintf(keyTxt, sizeof(keyTxt), "[%d]", w + 1);
        DrawText(keyTxt, static_cast<int>(sr.x + 6), static_cast<int>(sr.y + 6), 14, LIGHTGRAY);

        if (unlocked) {
            char lvlTxt[16];
            std::snprintf(lvlTxt, sizeof(lvlTxt), "V%d", save_.weaponLevels[w]);
            DrawText(lvlTxt, static_cast<int>(sr.x + 17), static_cast<int>(sr.y + 28), 20,
                     (save_.weaponLevels[w] >= 5) ? GOLD : weaps[w].projectileColor);
        } else {
            DrawText("LOCK", static_cast<int>(sr.x + 9), static_cast<int>(sr.y + 30), 15, GRAY);
        }
    }

    // Equipped Weapon Banner (Bottom-Left)
    int eq = save_.equippedWeapon;
    int eqLvl = save_.weaponLevels[eq];
    const char* wTitle = (eqLvl >= 5) ? weaps[eq].omegaName : weaps[eq].name;
    DrawRectangle(20, VIRTUAL_HEIGHT - 96, 480, 76, { 12, 18, 32, 220 });
    DrawRectangleLinesEx({ 20.0f, VIRTUAL_HEIGHT - 96.0f, 480.0f, 76.0f }, 1.5f, weaps[eq].projectileColor);
    DrawText(wTitle, 32, VIRTUAL_HEIGHT - 88, 18, WHITE);
    char xpLine[64];
    if (eqLvl >= 5) std::snprintf(xpLine, sizeof(xpLine), "OMEGA MASTERWORK V5");
    else std::snprintf(xpLine, sizeof(xpLine), "LEVEL V%d  |  WEAP XP: %d / %d", eqLvl, save_.weaponXp[eq], eqLvl * 120);
    DrawText(xpLine, 32, VIRTUAL_HEIGHT - 65, 14, GOLD);
    DrawText(weaps[eq].specialPerk, 32, VIRTUAL_HEIGHT - 44, 12, SKYBLUE);

    // Sector Entry Banner
    if (sectorBannerTimer_ > 0.0f) {
        const char* sName = GetSectorName(GetCurrentSector());
        int sw = MeasureText(sName, 20);
        DrawRectangle(VIRTUAL_WIDTH / 2 - sw / 2 - 20, 128, sw + 40, 38, { 12, 18, 32, 220 });
        DrawRectangleLines(VIRTUAL_WIDTH / 2 - sw / 2 - 20, 128, sw + 40, 38, ORANGE);
        DrawText(sName, VIRTUAL_WIDTH / 2 - sw / 2, 137, 20, GOLD);
    }
}

void RailGame::RenderBrandingFooter2D() const {
    DrawRectangle(0, VIRTUAL_HEIGHT - 56, VIRTUAL_WIDTH, 56, { 8, 12, 22, 240 });
    DrawLine(0, VIRTUAL_HEIGHT - 56, VIRTUAL_WIDTH, VIRTUAL_HEIGHT - 56, { 255, 165, 40, 160 });

    const char* studio = "SEBINY LABS";
    int sw = MeasureText(studio, 18);
    DrawText(studio, (VIRTUAL_WIDTH - sw) / 2, VIRTUAL_HEIGHT - 48, 18, GOLD);

    const char* ipOwner = "Non-commercial fan tribute. Ratchet & Clank and Before the Nexus are trademarks of Sony Interactive Entertainment LLC / Insomniac Games.";
    int iw = MeasureText(ipOwner, 14);
    DrawText(ipOwner, (VIRTUAL_WIDTH - iw) / 2, VIRTUAL_HEIGHT - 22, 14, { 185, 205, 225, 235 });
}

void RailGame::RenderTitleFrontPage2D() {
    // Insomniac / Rift Apart style Title Screen (Dimensional Rift glow, Lombax silhouette, Press Any Key)
    DrawRectangle(0, 0, VIRTUAL_WIDTH, VIRTUAL_HEIGHT, { 6, 10, 20, 160 });

    Vector2 center = { VIRTUAL_WIDTH * 0.5f, 260.0f };
    float pulse = std::sin(globalTime_ * 2.2f);

    // Dimensionator Rift Ring
    DrawCircleGradient(static_cast<int>(center.x), static_cast<int>(center.y), 230.0f,
                       { 255, 140, 40, static_cast<unsigned char>(40 + pulse * 15) }, BLANK);
    DrawPolyLinesEx(center, 8, 190.0f + pulse * 8.0f, globalTime_ * 24.0f, 3.0f, { 255, 160, 45, 180 });
    DrawPolyLinesEx(center, 6, 145.0f - pulse * 6.0f, -globalTime_ * 32.0f, 2.5f, { 70, 220, 255, 190 });

    const char* preTitle = "INSOMNIAC GAMES FAN TRIBUTE // NEXUS SECTOR";
    int ptw = MeasureText(preTitle, 16);
    DrawText(preTitle, (VIRTUAL_WIDTH - ptw) / 2, 380, 16, { 255, 185, 95, 230 });

    const char* title = "RATCHET & CLANK: RAILFINITY";
    int tw = MeasureText(title, 58);
    DrawText(title, (VIRTUAL_WIDTH - tw) / 2, 410, 58, GOLD);

    DrawLine(VIRTUAL_WIDTH / 2 - 320, 485, VIRTUAL_WIDTH / 2 + 320, 485, { 255, 165, 40, 120 });

    // Pulsing "Press Any Key / Tap Screen to Start" Call to Action
    float blink = std::sin(globalTime_ * 4.2f) * 0.5f + 0.5f;
    Color promptCol = { 255, 255, 255, static_cast<unsigned char>(140 + blink * 115) };
    const char* prompt = "PRESS ANY KEY OR TAP SCREEN TO START";
    int pw = MeasureText(prompt, 22);
    DrawText(prompt, (VIRTUAL_WIDTH - pw) / 2, 525, 22, promptCol);

    const char* ostNotice = "PLAYING: RATCHET & CLANK: RIFT APART OST - RIFT APART";
    int onw = MeasureText(ostNotice, 14);
    DrawText(ostNotice, (VIRTUAL_WIDTH - onw) / 2, 595, 14, { 140, 210, 255, 200 });

    RenderBrandingFooter2D();
}

void RailGame::RenderMainMenu2D() {
    DrawRectangle(0, 0, VIRTUAL_WIDTH, 165, { 10, 14, 26, 210 });
    DrawLine(0, 165, VIRTUAL_WIDTH, 165, ORANGE);

    const char* pre = "SOLANA & POLARIS INTERGALACTIC GRINDWAY NETWORK";
    int pw = MeasureText(pre, 16);
    DrawText(pre, (VIRTUAL_WIDTH - pw) / 2, 24, 16, SKYBLUE);

    const char* title = "RATCHET & CLANK: RAILFINITY";
    int tw = MeasureText(title, 52);
    DrawText(title, (VIRTUAL_WIDTH - tw) / 2, 50, 52, GOLD);

    const char* sub = "INFINITE 3D BEFORE-THE-NEXUS GRINDRAIL ACTION";
    int subw = MeasureText(sub, 18);
    DrawText(sub, (VIRTUAL_WIDTH - subw) / 2, 110, 18, WHITE);

    char stats[160];
    std::snprintf(stats, sizeof(stats),
                  "BOLTS: %d   |   RARITANIUM: %d   |   BEST RUN: %d m   |   DAILY BEST: %d m",
                  save_.bolts, save_.raritanium, save_.bestDistanceMeters, save_.bestDailyMeters);
    int stw = MeasureText(stats, 16);
    DrawText(stats, (VIRTUAL_WIDTH - stw) / 2, 140, 16, LIGHTGRAY);

    auto MenuButton = [&](Rectangle r, const char* label, const char* desc, Color col) -> bool {
        bool hov = IsRectHovered(r);
        DrawRectangleRec(r, hov ? Color{ 32, 48, 76, 240 } : Color{ 14, 22, 38, 225 });
        DrawRectangleLinesEx(r, hov ? 2.5f : 1.5f, hov ? col : GRAY);
        DrawRectangle(static_cast<int>(r.x), static_cast<int>(r.y), 7, static_cast<int>(r.height), col);

        int maxW = static_cast<int>(r.width) - 28;
        int labelFs = 20;
        while (labelFs > 12 && MeasureText(label, labelFs) > maxW) labelFs--;
        int descFs = 14;
        while (descFs > 10 && MeasureText(desc, descFs) > maxW) descFs--;

        DrawText(label, static_cast<int>(r.x + 18), static_cast<int>(r.y + 12), labelFs, WHITE);
        DrawText(desc, static_cast<int>(r.x + 18), static_cast<int>(r.y + 38), descFs, LIGHTGRAY);
        return IsButtonClicked(r);
    };

    Rectangle btnPlay   = { 50.0f, 215.0f, 400.0f, 66.0f };
    Rectangle btnDaily  = { 50.0f, 295.0f, 400.0f, 66.0f };
    Rectangle btnVendor = { 50.0f, 375.0f, 400.0f, 66.0f };
    Rectangle btnLab    = { 50.0f, 455.0f, 400.0f, 66.0f };
    Rectangle btnSkills = { VIRTUAL_WIDTH - 380.0f, 215.0f, 330.0f, 66.0f };

    if (MenuButton(btnPlay, "LAUNCH GRINDRAIL RUN", "Infinite procedural 3-lane action", ORANGE)) {
        StartNewRun(false);
    }

    std::string dailyDesc = std::string("Seeded Daily Track [") + PCG32::GetDailyDateString() + "]";
    if (MenuButton(btnDaily, "DAILY CHALLENGE SEED", dailyDesc.c_str(), GOLD)) {
        StartNewRun(true);
    }

    if (MenuButton(btnVendor, "GADGETRON WEAPON VENDOR", "Unlock & Upgrade 6 Weapons (V1-V5)", SKYBLUE)) {
        mode_ = ScreenMode::GADGETRON_VENDOR;
        audio_.PlayUiClick();
    }

    if (MenuButton(btnLab, "RARITANIUM MOD LAB", "Upgrade Armor, Magnet & Thrusters", GREEN)) {
        mode_ = ScreenMode::RARITANIUM_LAB;
        audio_.PlayUiClick();
    }

    int spDone = 0;
    for (bool b : save_.skillPoints) if (b) spDone++;
    char spTxt[64];
    std::snprintf(spTxt, sizeof(spTxt), "Unlocked: %d / %d Skill Points", spDone, static_cast<int>(NUM_SKILL_POINTS));
    if (MenuButton(btnSkills, "SKILL POINTS LOG", spTxt, MAGENTA)) {
        mode_ = ScreenMode::SKILL_POINTS_VIEW;
        audio_.PlayUiClick();
    }

    // Controls Reference Box on Right
    DrawRectangle(VIRTUAL_WIDTH - 380, 300, 330, 222, { 12, 18, 32, 225 });
    DrawRectangleLines(VIRTUAL_WIDTH - 380, 300, 330, 222, SKYBLUE);
    DrawText("CONTROLS (PC & TOUCH)", VIRTUAL_WIDTH - 362, 314, 18, GOLD);
    DrawText("> A / D or Swipe L/R: Switch Rail", VIRTUAL_WIDTH - 362, 344, 14, WHITE);
    DrawText("> SPACE / W or Swipe Up: Jump", VIRTUAL_WIDTH - 362, 368, 14, WHITE);
    DrawText("> Hold SPACE mid-air: Clank Glide", VIRTUAL_WIDTH - 362, 392, 14, WHITE);
    DrawText("> S / DOWN or Swipe Down: Slide", VIRTUAL_WIDTH - 362, 416, 14, WHITE);
    DrawText("> L-CLICK / F / Tap: Fire / Wrench", VIRTUAL_WIDTH - 362, 440, 14, WHITE);
    DrawText("> 1-6 / Q-E: Switch Weapon", VIRTUAL_WIDTH - 362, 464, 14, WHITE);
    DrawText("> M: Toggle Audio Mute", VIRTUAL_WIDTH - 362, 488, 14, LIGHTGRAY);

    RenderBrandingFooter2D();
}

void RailGame::RenderGadgetronVendor2D() {
    DrawRectangle(0, 0, VIRTUAL_WIDTH, VIRTUAL_HEIGHT, { 8, 12, 24, 235 });
    DrawText("GADGETRON WEAPONS VENDOR // ARSENAL & OMEGA UPGRADES", 60, 26, 26, GOLD);

    char curr[96];
    std::snprintf(curr, sizeof(curr), "BOLTS: %d   |   RARITANIUM: %d", save_.bolts, save_.raritanium);
    DrawText(curr, VIRTUAL_WIDTH - 420, 30, 20, SKYBLUE);

    const auto& cat = GetWeaponCatalog();
    for (int i = 0; i < static_cast<int>(NUM_WEAPONS); ++i) {
        Rectangle r = { 60.0f, 68.0f + i * 82.0f, 1160.0f, 76.0f };
        bool unlocked = save_.unlockedWeapons[i];
        bool equipped = (save_.equippedWeapon == i);
        int lvl = save_.weaponLevels[i];

        DrawRectangleRec(r, equipped ? Color{ 28, 44, 70, 245 } : Color{ 16, 22, 36, 230 });
        DrawRectangleLinesEx(r, equipped ? 2.5f : 1.5f, equipped ? GOLD : cat[i].projectileColor);

        const char* title = (lvl >= 5) ? cat[i].omegaName : cat[i].name;
        DrawText(title, static_cast<int>(r.x + 18), static_cast<int>(r.y + 8), 19, cat[i].projectileColor);
        DrawText(cat[i].specialPerk, static_cast<int>(r.x + 18), static_cast<int>(r.y + 31), 14, GOLD);
        DrawText(cat[i].description, static_cast<int>(r.x + 18), static_cast<int>(r.y + 51), 13, LIGHTGRAY);

        char lvlStr[32];
        std::snprintf(lvlStr, sizeof(lvlStr), "LEVEL V%d", lvl);
        DrawText(lvlStr, static_cast<int>(r.x + 695), static_cast<int>(r.y + 28), 19, (lvl >= 5) ? GOLD : WHITE);

        Rectangle eqBtn = { r.x + 810.0f, r.y + 16.0f, 160.0f, 44.0f };
        if (unlocked) {
            DrawRectangleRec(eqBtn, equipped ? GOLD : Color{ 38, 65, 98, 255 });
            DrawText(equipped ? "EQUIPPED" : "EQUIP", static_cast<int>(eqBtn.x + 40), static_cast<int>(eqBtn.y + 14), 16,
                     equipped ? BLACK : WHITE);
            if (IsButtonClicked(eqBtn)) {
                save_.equippedWeapon = i;
                SaveProgress();
                audio_.PlayUiClick();
            }
        } else {
            bool canBuy = (save_.bolts >= cat[i].unlockBoltCost);
            DrawRectangleRec(eqBtn, canBuy ? Color{ 30, 88, 58, 255 } : Color{ 50, 30, 35, 255 });
            char costStr[48];
            std::snprintf(costStr, sizeof(costStr), "BUY (%d B)", cat[i].unlockBoltCost);
            DrawText(costStr, static_cast<int>(eqBtn.x + 20), static_cast<int>(eqBtn.y + 14), 16, canBuy ? GREEN : RED);
            if (IsButtonClicked(eqBtn) && canBuy) {
                save_.bolts -= cat[i].unlockBoltCost;
                save_.unlockedWeapons[i] = true;
                save_.equippedWeapon = i;
                SaveProgress();
                audio_.PlayLevelUp();

                int countW = 0;
                for (bool u : save_.unlockedWeapons) if (u) countW++;
                if (countW >= 3) AwardSkillPoint(9);
            }
        }

        if (unlocked) {
            Rectangle upBtn = { r.x + 985.0f, r.y + 16.0f, 155.0f, 44.0f };
            int rCost = cat[i].upgradeRaritaniumCost * lvl;
            bool maxed = (lvl >= 5);
            bool canUp = (!maxed && save_.raritanium >= rCost);
            DrawRectangleRec(upBtn, maxed ? DARKGRAY : (canUp ? Color{ 30, 75, 105, 255 } : Color{ 35, 35, 45, 255 }));
            char upStr[48];
            if (maxed) std::snprintf(upStr, sizeof(upStr), "V5 OMEGA MAX");
            else std::snprintf(upStr, sizeof(upStr), "+V%d (%d RARI)", lvl + 1, rCost);
            DrawText(upStr, static_cast<int>(upBtn.x + 14), static_cast<int>(upBtn.y + 14), 15,
                     maxed ? GOLD : (canUp ? SKYBLUE : GRAY));
            if (IsButtonClicked(upBtn) && canUp) {
                save_.raritanium -= rCost;
                save_.weaponLevels[i]++;
                SaveProgress();
                audio_.PlayLevelUp();
                if (save_.weaponLevels[i] >= 5) AwardSkillPoint(5);
            }
        }
    }

    Rectangle backBtn = { 60.0f, 568.0f, 220.0f, 46.0f };
    DrawRectangleRec(backBtn, IsRectHovered(backBtn) ? Color{ 45, 68, 100, 255 } : Color{ 24, 36, 56, 255 });
    DrawRectangleLinesEx(backBtn, 2.0f, ORANGE);
    DrawText("< BACK TO MENU", static_cast<int>(backBtn.x + 34), static_cast<int>(backBtn.y + 16), 16, WHITE);
    if (IsButtonClicked(backBtn)) {
        mode_ = ScreenMode::MAIN_MENU;
        audio_.PlayUiClick();
    }

    RenderBrandingFooter2D();
}

void RailGame::RenderRaritaniumLab2D() {
    DrawRectangle(0, 0, VIRTUAL_WIDTH, VIRTUAL_HEIGHT, { 8, 12, 24, 235 });
    DrawText("RARITANIUM ENGINEERING LAB // ARMOR & GADGET MODS", 60, 40, 28, SKYBLUE);

    char curr[96];
    std::snprintf(curr, sizeof(curr), "BOLTS: %d   |   RARITANIUM: %d", save_.bolts, save_.raritanium);
    DrawText(curr, VIRTUAL_WIDTH - 420, 46, 20, GOLD);

    auto DrawModCard = [&](int idx, const char* title, const char* desc, int& lvl, int maxLvl, int baseBoltCost) {
        Rectangle r = { 60.0f + (idx % 2) * 585.0f, 110.0f + (idx / 2) * 210.0f, 565.0f, 185.0f };
        int bCost = baseBoltCost * (lvl + 1);
        bool maxed = (lvl >= maxLvl);
        bool canBuy = (!maxed && save_.bolts >= bCost);

        DrawRectangleRec(r, { 16, 24, 40, 235 });
        DrawRectangleLinesEx(r, 2.0f, canBuy ? SKYBLUE : GRAY);
        DrawText(title, static_cast<int>(r.x + 22), static_cast<int>(r.y + 16), 20, GOLD);
        DrawTextWrapped(desc, static_cast<int>(r.x + 22), static_cast<int>(r.y + 46), 520, 15, 20, LIGHTGRAY);

        for (int p = 0; p < maxLvl; ++p) {
            DrawRectangle(static_cast<int>(r.x + 22 + p * 42), static_cast<int>(r.y + 92), 34, 20,
                          (p < lvl) ? SKYBLUE : Color{ 35, 45, 62, 255 });
        }

        Rectangle buyBtn = { r.x + 22.0f, r.y + 128.0f, 240.0f, 40.0f };
        DrawRectangleRec(buyBtn, maxed ? DARKGRAY : (canBuy ? Color{ 30, 88, 62, 255 } : Color{ 48, 32, 38, 255 }));
        char btxt[64];
        if (maxed) std::snprintf(btxt, sizeof(btxt), "MAX UPGRADE");
        else std::snprintf(btxt, sizeof(btxt), "UPGRADE (%d BOLTS)", bCost);
        DrawText(btxt, static_cast<int>(buyBtn.x + 24), static_cast<int>(buyBtn.y + 12), 16, maxed ? GOLD : WHITE);

        if (IsButtonClicked(buyBtn) && canBuy) {
            save_.bolts -= bCost;
            lvl++;
            SaveProgress();
            audio_.PlayLevelUp();
        }
    };

    DrawModCard(0, "CARBONOX NANOTECH ARMOR", "Increases maximum Nanotech health pips (+1 HP per tier).", save_.armorLevel, 4, 450);
    DrawModCard(1, "BOX BREAKER ATTRACTOR", "Expands magnetic pickup radius for Bolts & Raritanium.", save_.magnetLevel, 5, 300);
    DrawModCard(2, "CLANK HELI-PACK OVERDRIVE", "Extends mid-air Heli-Pack glide fuel & Hyper-Boost duration.", save_.bootThrusterLevel, 5, 350);
    DrawModCard(3, "MULTIPLIER AMPLIFIER", "Accelerates Bolt Multiplier climb toward 20x.", save_.multiplierModLevel, 5, 400);

    Rectangle backBtn = { 60.0f, 575.0f, 220.0f, 48.0f };
    DrawRectangleRec(backBtn, IsRectHovered(backBtn) ? Color{ 45, 68, 100, 255 } : Color{ 24, 36, 56, 255 });
    DrawRectangleLinesEx(backBtn, 2.0f, ORANGE);
    DrawText("< BACK TO MENU", static_cast<int>(backBtn.x + 34), static_cast<int>(backBtn.y + 16), 16, WHITE);
    if (IsButtonClicked(backBtn)) {
        mode_ = ScreenMode::MAIN_MENU;
        audio_.PlayUiClick();
    }

    RenderBrandingFooter2D();
}

void RailGame::RenderSkillPoints2D() {
    DrawRectangle(0, 0, VIRTUAL_WIDTH, VIRTUAL_HEIGHT, { 8, 12, 24, 235 });
    DrawText("INTERGALACTIC SKILL POINTS // CHALLENGES & REWARDS", 60, 38, 28, GOLD);

    const auto& cat = GetSkillPointCatalog();
    for (int i = 0; i < static_cast<int>(NUM_SKILL_POINTS); ++i) {
        int col = i % 2;
        int row = i / 2;
        Rectangle r = { 60.0f + col * 580.0f, 92.0f + row * 92.0f, 560.0f, 82.0f };
        bool done = save_.skillPoints[i];

        DrawRectangleRec(r, done ? Color{ 26, 46, 62, 240 } : Color{ 16, 20, 32, 225 });
        DrawRectangleLinesEx(r, 1.5f, done ? GOLD : GRAY);

        DrawText(cat[i].title, static_cast<int>(r.x + 20), static_cast<int>(r.y + 12), 18, done ? GOLD : WHITE);
        DrawTextWrapped(cat[i].requirement, static_cast<int>(r.x + 20), static_cast<int>(r.y + 38), 520, 14, 18, LIGHTGRAY);

        char rew[48];
        std::snprintf(rew, sizeof(rew), done ? "EARNED (+%d B)" : "+%d BOLTS", cat[i].boltReward);
        DrawText(rew, static_cast<int>(r.x + 410), static_cast<int>(r.y + 12), 15, done ? GREEN : SKYBLUE);
    }

    Rectangle backBtn = { 60.0f, 575.0f, 220.0f, 48.0f };
    DrawRectangleRec(backBtn, IsRectHovered(backBtn) ? Color{ 45, 68, 100, 255 } : Color{ 24, 36, 56, 255 });
    DrawRectangleLinesEx(backBtn, 2.0f, ORANGE);
    DrawText("< BACK TO MENU", static_cast<int>(backBtn.x + 34), static_cast<int>(backBtn.y + 16), 16, WHITE);
    if (IsButtonClicked(backBtn)) {
        mode_ = ScreenMode::MAIN_MENU;
        audio_.PlayUiClick();
    }

    RenderBrandingFooter2D();
}

void RailGame::RenderPauseOverlay2D() {
    DrawRectangle(0, 0, VIRTUAL_WIDTH, VIRTUAL_HEIGHT, { 8, 12, 22, 200 });
    const char* t = "HOLOCOMM PAUSED";
    int tw = MeasureText(t, 40);
    DrawText(t, (VIRTUAL_WIDTH - tw) / 2, 220, 40, GOLD);

    Rectangle resBtn = { VIRTUAL_WIDTH / 2.0f - 160.0f, 300.0f, 320.0f, 54.0f };
    Rectangle menuBtn= { VIRTUAL_WIDTH / 2.0f - 160.0f, 372.0f, 320.0f, 54.0f };

    DrawRectangleRec(resBtn, IsRectHovered(resBtn) ? Color{ 40, 75, 110, 255 } : Color{ 22, 38, 60, 255 });
    DrawRectangleLinesEx(resBtn, 2.0f, SKYBLUE);
    DrawText("RESUME GRIND [SPACE]", static_cast<int>(resBtn.x + 52), static_cast<int>(resBtn.y + 18), 18, WHITE);

    DrawRectangleRec(menuBtn, IsRectHovered(menuBtn) ? Color{ 85, 40, 40, 255 } : Color{ 45, 24, 30, 255 });
    DrawRectangleLinesEx(menuBtn, 2.0f, ORANGE);
    DrawText("MAIN MENU [ESC]", static_cast<int>(menuBtn.x + 76), static_cast<int>(menuBtn.y + 18), 18, WHITE);

    if (IsButtonClicked(resBtn)) {
        mode_ = ScreenMode::PLAYING;
        audio_.PlayUiClick();
    }
    if (IsButtonClicked(menuBtn)) {
        SaveProgress();
        mode_ = ScreenMode::MAIN_MENU;
        audio_.PlayUiClick();
    }
}

void RailGame::RenderGameOver2D() {
    DrawRectangle(0, 0, VIRTUAL_WIDTH, VIRTUAL_HEIGHT, { 12, 8, 18, 220 });

    const char* goTitle = "NANOTECH DEPLETED // WIPEOUT!";
    int gow = MeasureText(goTitle, 44);
    DrawText(goTitle, (VIRTUAL_WIDTH - gow) / 2, 165, 44, ORANGE);

    DrawRectangle(VIRTUAL_WIDTH / 2 - 250, 240, 500, 190, { 16, 22, 38, 240 });
    DrawRectangleLinesEx({ VIRTUAL_WIDTH / 2.0f - 250.0f, 240.0f, 500.0f, 190.0f }, 2.0f, GOLD);

    char l1[96], l2[96], l3[96], l4[96];
    std::snprintf(l1, sizeof(l1), "DISTANCE GRINDED: %d m   (BEST: %d m)",
                  static_cast<int>(distanceMeters_), isDailyRun_ ? save_.bestDailyMeters : save_.bestDistanceMeters);
    std::snprintf(l2, sizeof(l2), "TITANIUM BOLTS COLLECTED: +%d", runBoltsEarned_);
    std::snprintf(l3, sizeof(l3), "RARITANIUM MINED: +%d", runRaritaniumEarned_);
    std::snprintf(l4, sizeof(l4), "CRATES SMASHED: %d   |   DRONES BLASTED: %d", runCrates_, runKills_);

    DrawText(l1, VIRTUAL_WIDTH / 2 - 215, 264, 20, WHITE);
    DrawText(l2, VIRTUAL_WIDTH / 2 - 215, 304, 19, GOLD);
    DrawText(l3, VIRTUAL_WIDTH / 2 - 215, 342, 18, SKYBLUE);
    DrawText(l4, VIRTUAL_WIDTH / 2 - 215, 380, 17, LIGHTGRAY);

    Rectangle retryBtn = { VIRTUAL_WIDTH / 2.0f - 250.0f, 455.0f, 240.0f, 56.0f };
    Rectangle menuBtn  = { VIRTUAL_WIDTH / 2.0f + 10.0f,  455.0f, 240.0f, 56.0f };

    DrawRectangleRec(retryBtn, IsRectHovered(retryBtn) ? Color{ 40, 95, 65, 255 } : Color{ 24, 64, 46, 255 });
    DrawRectangleLinesEx(retryBtn, 2.0f, GREEN);
    DrawText("RETRY RUN [SPACE]", static_cast<int>(retryBtn.x + 32), static_cast<int>(retryBtn.y + 19), 18, WHITE);

    DrawRectangleRec(menuBtn, IsRectHovered(menuBtn) ? Color{ 45, 68, 100, 255 } : Color{ 24, 36, 56, 255 });
    DrawRectangleLinesEx(menuBtn, 2.0f, ORANGE);
    DrawText("MAIN MENU [ESC]", static_cast<int>(menuBtn.x + 42), static_cast<int>(menuBtn.y + 19), 18, WHITE);

    if (IsButtonClicked(retryBtn)) {
        StartNewRun(isDailyRun_);
    }
    if (IsButtonClicked(menuBtn)) {
        mode_ = ScreenMode::MAIN_MENU;
        audio_.PlayUiClick();
    }
}

void RailGame::RenderSkillToast2D() const {
    if (!toast_.active) return;

    float alpha = std::clamp(toast_.timer * 1.5f, 0.0f, 1.0f);
    Rectangle box = { VIRTUAL_WIDTH / 2.0f - 260.0f, 105.0f, 520.0f, 64.0f };
    DrawRectangleRec(box, { 16, 28, 48, static_cast<unsigned char>(235 * alpha) });
    DrawRectangleLinesEx(box, 2.0f, GOLD);

    DrawText(toast_.title.c_str(), static_cast<int>(box.x + 18), static_cast<int>(box.y + 12), 18, GOLD);
    DrawText(toast_.subtitle.c_str(), static_cast<int>(box.x + 18), static_cast<int>(box.y + 36), 14, WHITE);
}

} // namespace Railfinity
