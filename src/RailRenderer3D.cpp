#include "RailGame.hpp"
#include "raymath.h"
#include "rlgl.h"
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

inline void DrawLowPolySphere(Vector3 center, float radius, Color color) {
    if (radius <= 0.08f) {
        DrawSphereEx(center, radius, 6, 6, color);
    } else if (radius <= 0.25f) {
        DrawSphereEx(center, radius, 8, 8, color);
    } else {
        DrawSphereEx(center, radius, 12, 12, color);
    }
}
#define DrawSphere(...) DrawLowPolySphere(__VA_ARGS__)

namespace {
struct SectorPalette {
    Color skyTop;
    Color skyBot;
    Color floorCol;
    Color wallUpper;
    Color wallLower;
    Color neonLight;
    Color portalBack;
    Color towerCol;
    Color archCol;
    bool enclosed;
};

Color LerpCol(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return {
        static_cast<unsigned char>(a.r + (b.r - a.r) * t),
        static_cast<unsigned char>(a.g + (b.g - a.g) * t),
        static_cast<unsigned char>(a.b + (b.b - a.b) * t),
        static_cast<unsigned char>(a.a + (b.a - a.a) * t)
    };
}

Color ScaleAlpha(Color c, float aScale) {
    c.a = static_cast<unsigned char>(std::clamp(c.a * aScale, 0.0f, 255.0f));
    return c;
}

SectorPalette GetSectorPalette(Railfinity::GalaxySector sec) {
    using GS = Railfinity::GalaxySector;
    switch (sec) {
        case GS::CORSON_V:
            return { { 20, 10, 36, 255 }, { 78, 22, 92, 255 }, { 20, 14, 34, 255 },
                     { 44, 32, 62, 255 }, { 24, 18, 40, 255 }, { 255, 55, 185, 255 },
                     { 36, 16, 62, 255 }, { 58, 42, 82, 255 }, { 42, 34, 64, 255 }, false };
        case GS::GASPAR_VOLCANIC:
            return { { 32, 10, 8, 255 }, { 96, 28, 14, 255 }, { 205, 58, 12, 255 },
                     { 54, 32, 28, 255 }, { 35, 18, 16, 255 }, { 255, 145, 25, 255 },
                     { 48, 18, 14, 255 }, { 66, 40, 34, 255 }, { 50, 34, 32, 255 }, false };
        case GS::POKITARU_RESORT:
            return { { 35, 115, 215, 255 }, { 110, 205, 235, 255 }, { 18, 132, 188, 255 },
                     { 215, 198, 162, 255 }, { 150, 135, 105, 255 }, { 65, 235, 245, 255 },
                     { 26, 98, 148, 255 }, { 225, 208, 172, 255 }, { 86, 128, 145, 255 }, false };
        case GS::TORREN_IV_SCRAP:
            return { { 50, 32, 20, 255 }, { 125, 78, 45, 255 }, { 118, 56, 30, 255 },
                     { 142, 76, 44, 255 }, { 85, 42, 24, 255 }, { 255, 185, 55, 255 },
                     { 76, 42, 28, 255 }, { 132, 78, 48, 255 }, { 82, 54, 38, 255 }, false };
        case GS::MEGACORP_FACILITY:
            return { { 12, 32, 54, 255 }, { 38, 68, 78, 255 }, { 36, 52, 58, 255 },
                     { 156, 154, 134, 255 }, { 68, 78, 74, 255 }, { 125, 235, 255, 255 },
                     { 24, 58, 92, 255 }, { 182, 174, 148, 255 }, { 54, 62, 76, 255 }, true };
        case GS::ZORDOOM_PRISON:
            return { { 24, 16, 36, 255 }, { 62, 42, 54, 255 }, { 24, 26, 34, 255 },
                     { 38, 42, 58, 255 }, { 22, 24, 35, 255 }, { 255, 45, 45, 255 },
                     { 40, 18, 24, 255 }, { 52, 56, 72, 255 }, { 54, 62, 76, 255 }, true };
        case GS::DIMENSIONATOR_RIFT:
            return { { 18, 12, 42, 255 }, { 74, 38, 96, 255 }, { 24, 12, 42, 255 },
                     { 55, 30, 75, 255 }, { 32, 18, 48, 255 }, { 210, 85, 255, 255 },
                     { 44, 20, 72, 255 }, { 76, 44, 108, 255 }, { 85, 48, 122, 255 }, false };
        default: // METROPOLIS_KERWAN
            return { { 14, 24, 44, 255 }, { 42, 62, 82, 255 }, { 22, 32, 48, 255 },
                     { 126, 128, 124, 255 }, { 52, 62, 70, 255 }, { 95, 175, 215, 210 },
                     { 24, 58, 92, 255 }, { 168, 162, 142, 255 }, { 54, 62, 76, 255 }, false };
    }
}

void DrawSectorSkyDecor(Railfinity::GalaxySector sec, float virtualWidth, float globalTime, float alpha) {
    if (alpha <= 0.01f) return;
    using GS = Railfinity::GalaxySector;
    float cx = virtualWidth * 0.5f;
    if (sec == GS::METROPOLIS_KERWAN) {
        DrawCircleGradient(static_cast<int>(cx + 260.0f), 145, 92.0f, ScaleAlpha({ 255, 225, 165, 160 }, alpha), { 16, 28, 48, 0 });
        DrawCircle(static_cast<int>(cx + 260.0f), 145, 48.0f, ScaleAlpha({ 215, 175, 115, 195 }, alpha));
        DrawEllipse(static_cast<int>(cx + 260.0f), 148, 88.0f, 16.0f, ScaleAlpha({ 245, 215, 155, 110 }, alpha));
    } else if (sec == GS::CORSON_V) {
        DrawCircleGradient(static_cast<int>(cx - 240.0f), 135, 95.0f, ScaleAlpha({ 255, 70, 200, 140 }, alpha), { 20, 10, 36, 0 });
        DrawCircle(static_cast<int>(cx - 240.0f), 135, 44.0f, ScaleAlpha({ 225, 110, 245, 190 }, alpha));
        for (int r = 0; r < 24; ++r) {
            float rx = std::fmod(r * 137.0f + globalTime * 95.0f, virtualWidth);
            float ry = std::fmod(r * 89.0f + globalTime * 420.0f, 420.0f);
            DrawLine(static_cast<int>(rx), static_cast<int>(ry), static_cast<int>(rx - 6.0f), static_cast<int>(ry + 22.0f),
                     ScaleAlpha({ 190, 140, 255, 75 }, alpha));
        }
    } else if (sec == GS::GASPAR_VOLCANIC) {
        DrawCircleGradient(static_cast<int>(cx + 180.0f), 170, 130.0f, ScaleAlpha({ 255, 110, 30, 175 }, alpha), { 32, 10, 8, 0 });
        DrawCircle(static_cast<int>(cx + 180.0f), 170, 56.0f, ScaleAlpha({ 255, 155, 55, 215 }, alpha));
    } else if (sec == GS::POKITARU_RESORT) {
        DrawCircleGradient(static_cast<int>(cx - 210.0f), 120, 115.0f, ScaleAlpha({ 255, 245, 170, 210 }, alpha), { 35, 115, 215, 0 });
        DrawCircle(static_cast<int>(cx - 210.0f), 120, 42.0f, ScaleAlpha({ 255, 250, 205, 245 }, alpha));
        for (int c = 0; c < 5; ++c) {
            float cloudX = std::fmod(c * 290.0f + globalTime * 18.0f, virtualWidth + 220.0f) - 110.0f;
            float cloudY = 85.0f + (c % 3) * 42.0f;
            DrawEllipse(static_cast<int>(cloudX), static_cast<int>(cloudY), 76.0f, 18.0f, ScaleAlpha({ 255, 255, 255, 95 }, alpha));
            DrawEllipse(static_cast<int>(cloudX + 28.0f), static_cast<int>(cloudY - 7.0f), 48.0f, 15.0f, ScaleAlpha({ 255, 255, 255, 110 }, alpha));
        }
    } else if (sec == GS::TORREN_IV_SCRAP) {
        DrawCircle(static_cast<int>(cx + 210.0f), 130, 52.0f, ScaleAlpha({ 225, 155, 95, 145 }, alpha));
        DrawCircle(static_cast<int>(cx - 290.0f), 165, 24.0f, ScaleAlpha({ 185, 140, 110, 130 }, alpha));
    } else if (sec == GS::DIMENSIONATOR_RIFT) {
        for (int s = 0; s < 42; ++s) {
            float sx = std::fmod(s * 173.0f, virtualWidth);
            float sy = std::fmod(s * 97.0f, 360.0f);
            DrawCircle(static_cast<int>(sx), static_cast<int>(sy), (s % 3 == 0) ? 2.0f : 1.2f, ScaleAlpha({ 230, 190, 255, 180 }, alpha));
        }
        DrawCircleLines(static_cast<int>(cx), 165, 68.0f + std::sin(globalTime * 2.5f) * 6.0f, ScaleAlpha({ 210, 110, 255, 150 }, alpha));
        DrawCircleLines(static_cast<int>(cx), 165, 98.0f + std::cos(globalTime * 1.9f) * 8.0f, ScaleAlpha({ 120, 220, 255, 110 }, alpha));
    }
}
} // namespace

void RailGame::RenderFrame() {
    UpdateVirtualResolution();

    BeginTextureMode(renderTarget_);

    bool inRun = (mode_ == ScreenMode::PLAYING || mode_ == ScreenMode::PAUSED || mode_ == ScreenMode::GAME_OVER);
    GalaxySector sec = inRun ? GetCurrentSector() : GalaxySector::METROPOLIS_KERWAN;
    GalaxySector nextSec = inRun ? GetSectorAtDistance(distanceMeters_ + 500.0f) : GalaxySector::METROPOLIS_KERWAN;
    float sectorRem = inRun ? std::fmod(distanceMeters_, 500.0f) : 0.0f;
    float blend = inRun ? std::clamp((sectorRem - 460.0f) / 40.0f, 0.0f, 1.0f) : 0.0f;

    SectorPalette palA = GetSectorPalette(sec);
    SectorPalette palB = GetSectorPalette(nextSec);
    Color skyTop = LerpCol(palA.skyTop, palB.skyTop, blend);
    Color skyBot = LerpCol(palA.skyBot, palB.skyBot, blend);

    // Crucial: ClearBackground clears both the color buffer AND the 3D Depth Buffer of renderTarget_!
    rlEnableDepthMask();
    ClearBackground(skyTop);

    // Disable Depth Mask during 2D background gradient & skybox art so it does NOT write into the 3D depth buffer!
    rlDisableDepthMask();
    DrawRectangleGradientV(0, 0, static_cast<int>(virtualWidth_), 720, skyTop, skyBot);
    DrawSectorSkyDecor(sec, virtualWidth_, globalTime_, 1.0f - blend);
    if (blend > 0.01f) {
        DrawSectorSkyDecor(nextSec, virtualWidth_, globalTime_, blend);
    }
    rlEnableDepthMask();
    RenderWorld3D();

    // Screen Flash
    if (screenFlashAlpha_ > 0.0f && save_.screenFlashes) {
        Color fc = screenFlashColor_;
        fc.a = static_cast<unsigned char>(std::clamp(screenFlashAlpha_ * 255.0f, 0.0f, 255.0f));
        DrawRectangle(0, 0, static_cast<int>(virtualWidth_), 720, fc);
    }

    // 2D Overlays & Menus
    if (mode_ == ScreenMode::PLAYING || mode_ == ScreenMode::PAUSED || mode_ == ScreenMode::GAME_OVER) {
        RenderHud2D();
    }

    if (mode_ == ScreenMode::SPLASH_INTRO) {
        RenderSplashIntro2D();
    } else if (mode_ == ScreenMode::TITLE_FRONT_PAGE) {
        RenderTitleFrontPage2D();
    } else if (mode_ == ScreenMode::MAIN_MENU) {
        RenderMainMenu2D();
    } else if (mode_ == ScreenMode::GADGETRON_VENDOR) {
        RenderGadgetronVendor2D();
    } else if (mode_ == ScreenMode::RARITANIUM_LAB) {
        RenderRaritaniumLab2D();
    } else if (mode_ == ScreenMode::ARMOR_WARDROBE) {
        RenderArmorWardrobe2D();
    } else if (mode_ == ScreenMode::SKILL_POINTS_VIEW) {
        RenderSkillPoints2D();
    } else if (mode_ == ScreenMode::SETTINGS_VIEW) {
        RenderSettings2D();
    } else if (mode_ == ScreenMode::STATS_VIEW) {
        RenderCareerStats2D();
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
    } else if (mode_ == ScreenMode::ARMOR_WARDROBE) {
        // Orbit camera around Ratchet & Clank cleanly framed in left viewport with generous headroom
        float rad = wardrobeRotAngle_ * DEG2RAD;
        float dist = 5.8f;
        Vector3 ratCenter = { 0.0f, 1.15f, 0.0f };

        // Base camera orbit position in horizontal plane
        Vector3 camOffset = { std::sin(rad) * dist, 0.75f, -std::cos(rad) * dist };
        Vector3 baseCamPos = Vector3Add(ratCenter, camOffset);

        // Direction from camera to character center
        Vector3 toChar = Vector3Normalize(Vector3Subtract(ratCenter, baseCamPos));
        Vector3 up     = { 0.0f, 1.0f, 0.0f };
        Vector3 right  = Vector3Normalize(Vector3CrossProduct(toChar, up));

        // Shift camera viewing axis to the right by 1.55m so Ratchet appears at X ≈ 305px (center of left panel)
        float shiftRight = 1.55f;
        cam.position = Vector3Add(baseCamPos, Vector3Scale(right, shiftRight));
        cam.target   = Vector3Add(ratCenter,  Vector3Scale(right, shiftRight));
        cam.fovy     = 37.0f;
    } else {
        if (sectorBannerTimer_ > 2.6f) {
            float warpT = (sectorBannerTimer_ - 2.6f) / 0.9f;
            cam.fovy += std::sin(warpT * 3.14159265f) * 6.5f;
        }
        if (cameraShake_ > 0.0f) {
            float mag = cameraShake_ * cameraShake_ * 0.65f * save_.screenShakeScale;
            cam.position.x += std::sin(globalTime_ * 73.0f) * mag;
            cam.position.y += std::cos(globalTime_ * 61.0f) * mag;
        }
    }

    if (worldHighpShaderLoaded_) {
        BeginShaderMode(worldHighpShader_);
    }
    BeginMode3D(cam);
    rlEnableDepthTest();
    rlEnableDepthMask();

    // Dynamic wide frustum with optimal near/far clipping for mobile GPUs:
    // near = 0.90, far = 220.0 paired with 32-bit highp vertex transform guarantees >250x depth precision!
    double aspect = static_cast<double>(virtualWidth_) / 720.0;
    Matrix customProj = MatrixPerspective(cam.fovy * DEG2RAD, aspect, 0.90, 220.0);
    rlSetMatrixProjection(customProj);

    RenderSkyline3D();
    RenderGrindRails3D();
    if (mode_ == ScreenMode::PLAYING && sectorBannerTimer_ > 2.7f) {
        float t = (sectorBannerTimer_ - 2.7f) / 0.8f;
        float rz = t * 44.0f - 4.0f;
        Vector3 rc = ApplyTrackCurve(0.0f, 1.4f, std::max(0.0f, rz));
        Color wCol = GetSectorPalette(GetCurrentSector()).neonLight;
        for (int s = 0; s < 16; ++s) {
            float a1 = (s * 22.5f + globalTime_ * 160.0f) * DEG2RAD;
            float a2 = ((s + 1) * 22.5f + globalTime_ * 160.0f) * DEG2RAD;
            Vector3 p1 = { rc.x + std::cos(a1) * 11.5f, rc.y + std::sin(a1) * 8.5f, rz };
            Vector3 p2 = { rc.x + std::cos(a2) * 11.5f, rc.y + std::sin(a2) * 8.5f, rz };
            DrawCylinderEx(p1, p2, 0.24f, 0.24f, 5, wCol);
        }
    }

    if (mode_ == ScreenMode::ARMOR_WARDROBE) {
        // Seated cleanly above the silver/gold grindrail top (+0.06m) so the rail never slices the pedestal!
        Vector3 pedCenter = { 0.0f, 0.12f, 0.0f };
        DrawCylinderEx({ pedCenter.x, -0.32f, pedCenter.z }, pedCenter, 1.65f, 1.50f, 24, { 18, 24, 38, 255 });
        DrawCylinderWiresEx({ pedCenter.x, -0.32f, pedCenter.z }, pedCenter, 1.67f, 1.52f, 24, { 45, 160, 240, 220 });

        // Concentric holographic floor rings
        constexpr int P_SEGS = 24;
        for (int i = 0; i < P_SEGS; ++i) {
            float a1 = (i * (360.0f / P_SEGS) + globalTime_ * 40.0f) * DEG2RAD;
            float a2 = ((i + 1) * (360.0f / P_SEGS) + globalTime_ * 40.0f) * DEG2RAD;
            Vector3 p1 = { pedCenter.x + std::cos(a1) * 1.35f, pedCenter.y + 0.03f, pedCenter.z + std::sin(a1) * 1.35f };
            Vector3 p2 = { pedCenter.x + std::cos(a2) * 1.35f, pedCenter.y + 0.03f, pedCenter.z + std::sin(a2) * 1.35f };
            DrawLine3D(p1, p2, (i % 2 == 0) ? SKYBLUE : GOLD);
        }

        // Render Ratchet & Clank preview on the pedestal
        RenderLombaxHero3D(pedCenter, std::sin(globalTime_ * 2.2f) * 4.0f, false, false, 0.0f, save_.equippedWeapon);
    } else if (mode_ == ScreenMode::PLAYING || mode_ == ScreenMode::PAUSED || mode_ == ScreenMode::GAME_OVER) {
        RenderHazards3D();
        RenderBolts3D();
        RenderProjectiles3D();
        RenderBossDropship3D();
        RenderParticles3D();

        bool blinkHide = (player_.invulnTimer > 0.0f && std::fmod(globalTime_, 0.12f) < 0.06f);
        if (!blinkHide) {
            float hopArcY = (player_.isHopping) ? std::sin(std::clamp(player_.hopTimer / player_.hopDuration, 0.0f, 1.0f) * 3.14159265f) * 1.25f : 0.0f;
            float landingSquash = (player_.landingSquashTimer > 0.0f) ? std::sin((player_.landingSquashTimer / 0.22f) * 3.14159265f) * 0.18f : 0.0f;
            Vector3 heroPos = { player_.x, player_.y + hopArcY - landingSquash, 0.0f };
            RenderLombaxHero3D(heroPos, player_.laneTiltDeg,
                               player_.slideTimer > 0.0f, player_.heliGliding,
                               player_.wrenchSwingTimer, save_.equippedWeapon);
        }
    } else {
        // Menu showcase: Sample Hex Bolts and a glowing Red-Orange Rail Ring Collar ahead of Ratchet!
        for (int i = 0; i < 4; ++i) {
            Vector3 bp = ApplyTrackCurve(0.0f, 0.72f, 5.5f + i * 3.0f);
            RenderSingleHexBolt3D(bp, globalTime_ * 180.0f + i * 45.0f, 1.0f, i == 2);
        }
        Vector3 ringPos = ApplyTrackCurve(GetLaneX(1), 0.15f, 13.5f);
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
    if (worldHighpShaderLoaded_) {
        EndShaderMode();
    }
}

void RailGame::RenderSkyline3D() const {
    constexpr float PORTAL_Z = 148.0f;
    bool inRun = (mode_ == ScreenMode::PLAYING || mode_ == ScreenMode::PAUSED || mode_ == ScreenMode::GAME_OVER);
    GalaxySector sec = inRun ? GetCurrentSector() : GalaxySector::METROPOLIS_KERWAN;
    GalaxySector nextSec = inRun ? GetSectorAtDistance(distanceMeters_ + 500.0f) : GalaxySector::METROPOLIS_KERWAN;
    float sectorRem = inRun ? std::fmod(distanceMeters_, 500.0f) : 0.0f;
    float blend = inRun ? std::clamp((sectorRem - 460.0f) / 40.0f, 0.0f, 1.0f) : 0.0f;

    SectorPalette palA = GetSectorPalette(sec);
    SectorPalette palB = GetSectorPalette(nextSec);
    bool isEnclosed = palA.enclosed;

    Color floorCol   = LerpCol(palA.floorCol,   palB.floorCol,   blend);
    Color wallUpper  = LerpCol(palA.wallUpper,  palB.wallUpper,  blend);
    Color wallLower  = LerpCol(palA.wallLower,  palB.wallLower,  blend);
    Color neonLight  = LerpCol(palA.neonLight,  palB.neonLight,  blend);
    Color portalBack = LerpCol(palA.portalBack, palB.portalBack, blend);
    Color towerCol   = LerpCol(palA.towerCol,   palB.towerCol,   blend);
    Color archCol    = LerpCol(palA.archCol,    palB.archCol,    blend);

    // 1. Wide Planetary Floor / Ocean / Magma Basin
    DrawCube({ 0.0f, -16.0f, 70.0f }, 160.0f, 2.0f, 210.0f, floorCol);

    // Animated water ripples or magma fissures below the track
    if (sec == GalaxySector::POKITARU_RESORT || sec == GalaxySector::GASPAR_VOLCANIC) {
        Color bandCol = (sec == GalaxySector::POKITARU_RESORT) ? Color{ 75, 210, 245, 160 } : Color{ 255, 165, 35, 210 };
        float bandOff = std::fmod(distanceMeters_ * 1.4f, 18.0f);
        for (float bz = 6.0f - bandOff; bz < PORTAL_Z - 4.0f; bz += 18.0f) {
            if (bz < 0.0f) continue;
            float cx = GetTrackCurveX(bz);
            DrawCube({ cx, -14.8f, bz }, 110.0f, 0.3f, 2.2f, bandCol);
        }
    }

    // 2. Trackside Curbs (Open-sky worlds) vs Tall Corridor Walls (Megacorp / Zordoom)
    constexpr float wallStep = 10.0f;
    for (float z = -12.0f; z < PORTAL_Z; z += wallStep) {
        float midZ = z + wallStep * 0.5f;
        float cx = GetTrackCurveX(std::max(0.0f, midZ));
        float cy = GetTrackCurveY(std::max(0.0f, midZ));

        for (int side = -1; side <= 1; side += 2) {
            if (isEnclosed) {
                float wx = cx + side * 17.5f;
                DrawCube({ wx, cy + 5.0f, midZ }, 5.0f, 30.0f, wallStep + 0.4f, wallUpper);
                DrawCube({ wx - side * 1.8f, cy - 6.8f, midZ }, 5.8f, 9.5f, wallStep + 0.4f, wallLower);
                DrawCube({ wx - side * 2.5f, cy - 1.9f, midZ }, 0.8f, 0.7f, wallStep + 0.4f, { 42, 48, 58, 255 });
            } else {
                // Sleek low trackside guardrail/parapet so the sky, planet, and distant skyline shine through!
                float wx = cx + side * 15.2f;
                DrawCube({ wx, cy - 6.8f, midZ }, 2.2f, 9.5f, wallStep + 0.4f, wallLower);
                DrawCube({ wx, cy - 1.8f, midZ }, 2.6f, 0.9f, wallStep + 0.4f, wallUpper);
                DrawCube({ wx - side * 1.1f, cy - 1.8f, midZ }, 0.35f, 0.45f, wallStep + 0.4f, neonLight);
            }
        }
    }

    // 3. Scrolling Sector Landmarks & Architecture (Smoothly scaled via GetPortalEmergenceScale -> Zero Pop-In!)
    float baySpacing = 24.0f;
    float totalBayDist = distanceMeters_ * 1.8f;
    long long baseBay = static_cast<long long>(std::floor(totalBayDist / baySpacing));
    float bayOffset = totalBayDist - static_cast<float>(baseBay) * baySpacing;

    for (int i = 0; i < 8; ++i) {
        int idx = static_cast<int>(std::abs(baseBay + i));
        float z = -10.0f - bayOffset + i * baySpacing;
        if (z >= PORTAL_Z - 2.0f) continue;
        float emerge = GetPortalEmergenceScale(z);
        if (emerge <= 0.01f) continue;

        float cx = GetTrackCurveX(std::max(0.0f, z));
        float cy = GetTrackCurveY(std::max(0.0f, z));
        GalaxySector zSec = inRun ? GetSectorAtDistance(distanceMeters_ + z / 1.8f) : sec;
        bool zEnclosed = (zSec == GalaxySector::MEGACORP_FACILITY || zSec == GalaxySector::ZORDOOM_PRISON);

        if (zEnclosed) {
            for (int side = -1; side <= 1; side += 2) {
                float wx = cx + side * 14.6f;
                DrawCube({ wx, cy + 4.0f, z }, 0.9f * emerge, 26.0f, 1.8f * emerge, { 45, 52, 64, 255 });
            }
            // Overhead industrial catwalk arch connecting both walls!
            DrawCube({ cx, cy + 12.2f, z }, 29.0f * emerge, 1.4f * emerge, 1.8f * emerge, { 38, 44, 54, 255 });
            DrawCube({ cx, cy + 11.3f, z }, 14.0f * emerge, 0.35f * emerge, 0.5f * emerge, neonLight);

            float pz = z + 8.0f;
            float pEmerge = GetPortalEmergenceScale(pz);
            if (pEmerge > 0.02f) {
                float px = GetTrackCurveX(std::max(0.0f, pz));
                float py = GetTrackCurveY(std::max(0.0f, pz));
                Vector3 panelPos = { px + 14.35f, py + 2.2f, pz };
                DrawCube(panelPos, 0.40f, 4.4f * pEmerge, 9.2f * pEmerge, { 35, 52, 68, 255 });
                DrawCube({ px + 13.88f, panelPos.y, panelPos.z },
                         0.24f, 3.8f * pEmerge, 8.5f * pEmerge, neonLight);
            }
        } else if (zSec == GalaxySector::METROPOLIS_KERWAN || zSec == GalaxySector::CORSON_V) {
            // Futuristic Metropolis / Cyberpunk Skyscrapers flanking the skyway
            for (int side = -1; side <= 1; side += 2) {
                float bHeight = 24.0f + ((idx + (side > 0 ? 1 : 0)) % 3) * 11.0f;
                float bx = cx + side * (27.5f + ((idx % 2) * 5.0f));
                Color bCol = (zSec == GalaxySector::CORSON_V)
                             ? Color{ 34, 24, 52, 255 } : Color{ 62, 76, 94, 255 };
                Color bTrim = (zSec == GalaxySector::CORSON_V)
                              ? neonLight : Color{ 95, 165, 205, 185 };
                DrawCube({ bx, cy - 15.0f + bHeight * 0.5f * emerge, z },
                         7.5f * emerge, bHeight * emerge, 7.5f * emerge, bCol);
                // Glowing window/neon bands on inner face
                DrawCube({ bx - side * 3.85f * emerge, cy - 3.0f + bHeight * 0.35f * emerge, z },
                         0.3f * emerge, (bHeight * 0.55f) * emerge, 4.2f * emerge, bTrim);
                // Rooftop antenna beacon
                DrawCylinderEx({ bx, cy - 15.0f + bHeight * emerge, z },
                               { bx, cy - 15.0f + (bHeight + 6.0f) * emerge, z },
                               0.25f * emerge, 0.08f * emerge, 5, DARKGRAY);
                DrawSphere({ bx, cy - 15.0f + (bHeight + 6.2f) * emerge, z }, 0.45f * emerge, neonLight);
            }
        } else if (zSec == GalaxySector::GASPAR_VOLCANIC) {
            // Jagged Obsidian Volcanic Spires rising from the lava sea
            for (int side = -1; side <= 1; side += 2) {
                float sH = 18.0f + ((idx + (side > 0 ? 2 : 0)) % 3) * 8.0f;
                float sx = cx + side * 23.0f;
                DrawCylinderEx({ sx, -15.5f, z }, { sx, -15.5f + sH * emerge, z },
                               5.2f * emerge, 0.7f * emerge, 6, { 46, 26, 24, 255 });
                DrawCylinderEx({ sx - side * 1.4f, -15.5f, z + 3.0f },
                               { sx - side * 1.4f, -15.5f + (sH * 0.65f) * emerge, z + 3.0f },
                               3.4f * emerge, 0.5f * emerge, 5, { 68, 32, 24, 255 });
                // Glowing lava fissure at base
                DrawCube({ sx - side * 3.5f, -14.3f, z }, 2.2f * emerge, 0.6f, 4.5f * emerge, { 255, 115, 20, 255 });
            }
        } else if (zSec == GalaxySector::POKITARU_RESORT) {
            // Sandy Resort Islands & Tropical Palm Trees flanking the ocean rails
            for (int side = -1; side <= 1; side += 2) {
                float ix = cx + side * 22.5f;
                DrawCylinderEx({ ix, -15.2f, z }, { ix, -12.6f, z }, 6.8f * emerge, 5.2f * emerge, 8, { 228, 205, 148, 255 });
                // Curved palm trunk
                Vector3 baseP = { ix, -12.8f, z };
                Vector3 topP  = { ix - side * 2.2f * emerge, -12.8f + 13.5f * emerge, z };
                DrawCylinderEx(baseP, topP, 0.7f * emerge, 0.42f * emerge, 6, { 128, 86, 48, 255 });
                // Palm frond crown
                for (int f = 0; f < 5; ++f) {
                    float fa = (f * 72.0f + idx * 25.0f) * DEG2RAD;
                    Vector3 tip = { topP.x + std::cos(fa) * 4.5f * emerge, topP.y - 1.2f * emerge, topP.z + std::sin(fa) * 4.5f * emerge };
                    DrawCylinderEx(topP, tip, 0.65f * emerge, 0.12f * emerge, 4, { 42, 165, 68, 255 });
                }
            }
        } else if (zSec == GalaxySector::TORREN_IV_SCRAP) {
            // Rusted Scrapyard Towers & Crane Booms
            for (int side = -1; side <= 1; side += 2) {
                float tx = cx + side * 23.5f;
                float tH = 20.0f + (idx % 2) * 9.0f;
                DrawCube({ tx, -15.0f + tH * 0.5f * emerge, z }, 5.5f * emerge, tH * emerge, 5.5f * emerge, { 112, 62, 38, 255 });
                DrawCube({ tx - side * 3.0f * emerge, -15.0f + tH * 0.85f * emerge, z },
                         8.0f * emerge, 1.2f * emerge, 1.6f * emerge, { 165, 115, 48, 255 });
            }
        } else if (zSec == GalaxySector::DIMENSIONATOR_RIFT) {
            // Floating Psionic Crystal Shards & Dimensional Rings
            for (int side = -1; side <= 1; side += 2) {
                float kx = cx + side * 22.0f;
                float ky = cy + 2.0f + std::sin(globalTime_ * 2.0f + idx + side) * 1.4f;
                DrawCylinderEx({ kx, ky - 5.5f * emerge, z }, { kx, ky + 5.5f * emerge, z },
                               0.2f * emerge, 2.2f * emerge, 5, { 185, 75, 255, 220 });
                DrawCylinderEx({ kx, ky + 5.5f * emerge, z }, { kx, ky + 9.0f * emerge, z },
                               2.2f * emerge, 0.1f * emerge, 5, { 225, 155, 255, 240 });
            }
        }
    }

    // 4. Distant Flying Hover-Traffic in City Sectors (Metropolis / Corson V)
    if (sec == GalaxySector::METROPOLIS_KERWAN || sec == GalaxySector::CORSON_V) {
        for (int s = 0; s < 5; ++s) {
            float dir = (s % 2 == 0) ? 1.0f : -1.0f;
            float rawShipX = globalTime_ * (18.0f + s * 4.5f) * dir + s * 37.0f;
            float shipX = std::fmod(std::fmod(rawShipX, 120.0f) + 120.0f, 120.0f) - 60.0f;
            float shipY = 10.0f + (s % 3) * 5.5f;
            float shipZ = 58.0f + s * 16.0f;
            DrawCube({ shipX, shipY, shipZ }, 3.2f, 0.8f, 1.2f, (s % 2 == 0) ? LIGHTGRAY : SKYBLUE);
            DrawCube({ shipX - dir * 2.2f, shipY, shipZ }, 1.6f, 0.35f, 0.5f, neonLight);
        }
    }

    // 5. Distant Circular Reactor / Portal Bulkhead & Twin Towers (anchored at Z = 148, tinted per biome)
    Vector3 portalCenter = ApplyTrackCurve(0.0f, 0.5f, PORTAL_Z);

    if (isEnclosed) {
        DrawCube({ portalCenter.x, portalCenter.y + 8.0f, PORTAL_Z + 12.0f }, 68.0f, 44.0f, 4.0f, portalBack);
        for (int tx = -3; tx <= 3; ++tx) {
            DrawCube({ portalCenter.x + tx * 7.2f, portalCenter.y + 4.8f, PORTAL_Z + 8.5f },
                     6.4f, 14.0f, 1.2f, { neonLight.r, neonLight.g, neonLight.b, 140 });
        }
    }

    // Twin Cylindrical Towers with Glowing Reactor Vents on Top
    for (int side = -1; side <= 1; side += 2) {
        float towerX = portalCenter.x + side * 12.6f;
        float towerZ = PORTAL_Z + 3.0f;
        DrawCylinderEx({ towerX, portalCenter.y - 10.0f, towerZ },
                       { towerX, portalCenter.y + 12.5f, towerZ }, 3.8f, 3.8f, 12, towerCol);
        DrawCylinderEx({ towerX, portalCenter.y + 12.5f, towerZ },
                       { towerX, portalCenter.y + 15.5f, towerZ }, 4.6f, 3.1f, 12, towerCol);
        DrawCylinderEx({ towerX, portalCenter.y + 15.5f, towerZ },
                       { towerX, portalCenter.y + 19.2f, towerZ }, 2.5f, 2.5f, 10, neonLight);
    }

    // Giant Bolted Circular Archway Ring around the Portal
    constexpr int ARCH_SEGMENTS = 16;
    float archRadius = 9.6f;
    for (int i = 0; i < ARCH_SEGMENTS; ++i) {
        float a1 = (static_cast<float>(i) / ARCH_SEGMENTS) * 6.283185f;
        float a2 = (static_cast<float>(i + 1) / ARCH_SEGMENTS) * 6.283185f;
        Vector3 p1 = { portalCenter.x + std::cos(a1) * archRadius, portalCenter.y + std::sin(a1) * archRadius, PORTAL_Z };
        Vector3 p2 = { portalCenter.x + std::cos(a2) * archRadius, portalCenter.y + std::sin(a2) * archRadius, PORTAL_Z };
        DrawCylinderEx(p1, p2, 1.75f, 1.75f, 8, archCol);
        DrawSphere({ p1.x, p1.y, PORTAL_Z - 1.5f }, 0.48f, { 28, 32, 42, 255 });
    }

    DrawCylinderEx({ portalCenter.x, portalCenter.y + 7.4f, PORTAL_Z - 1.2f },
                   { portalCenter.x, portalCenter.y + 14.8f, PORTAL_Z - 1.2f }, 1.8f, 3.8f, 4, archCol);

    // Central Spherical Reactor / Warp Core inside the Archway
    DrawSphere(portalCenter, 5.4f, { 26, 78, 82, 255 });
    DrawSphereWires(portalCenter, 5.45f, 8, 8, neonLight);
    DrawSphere({ portalCenter.x - 1.5f, portalCenter.y + 2.4f, portalCenter.z - 4.6f }, 0.40f, { 210, 250, 255, 255 });
    DrawSphere({ portalCenter.x + 1.3f, portalCenter.y + 2.6f, portalCenter.z - 4.6f }, 0.40f, { 210, 250, 255, 255 });

    // Twin Glowing Intake Ports where the Grindrails connect into the Reactor Sphere
    Vector3 leftPort  = { portalCenter.x + 1.85f, portalCenter.y - 0.35f, portalCenter.z - 4.9f };
    Vector3 rightPort = { portalCenter.x - 1.85f, portalCenter.y - 0.35f, portalCenter.z - 4.9f };
    DrawSphere(leftPort,  0.95f, { 255, 65, 35, 255 });
    DrawSphere(rightPort, 0.95f, { 255, 65, 35, 255 });
    DrawSphere(leftPort,  0.50f, YELLOW);
    DrawSphere(rightPort, 0.50f, YELLOW);
}
void RailGame::RenderGrindRails3D() const {
    // 1. Draw Tubular Metallic Grindrails (Strictly capped at active 3-rail corridor!)
    constexpr float segLen = 3.5f;
    for (int l = 0; l < NUM_LANES; ++l) {
        float lx = GetLaneX(l);
        for (float z = -10.0f; z < 145.0f; z += segLen) {
            float midZ = z + segLen * 0.5f;
            if (!IsRailActiveAt(l, midZ)) {
                // Not active or in broken gap: draw terminus cap if preceding segment was active
                if (z > 0.0f && IsRailActiveAt(l, midZ - segLen)) {
                    Vector3 pTerm = ApplyTrackCurve(lx, -0.10f, z);
                    DrawCube(pTerm, 0.72f, 0.44f, 0.36f, { 255, 120, 20, 240 });
                    DrawCubeWires(pTerm, 0.76f, 0.48f, 0.40f, YELLOW);
                }
                continue;
            }
            Vector3 pA = ApplyTrackCurve(lx, -0.10f, z);
            Vector3 pB = ApplyTrackCurve(lx, -0.10f, z + segLen + 0.06f);

            // Central dark-steel structural spine underneath
            DrawCylinderEx({ pA.x, pA.y - 0.14f, pA.z }, { pB.x, pB.y - 0.14f, pB.z },
                           0.22f, 0.22f, 6, { 34, 42, 48, 255 });

            // Bright Silver/Chrome Center Grind Pipe
            DrawCylinderEx(pA, pB, 0.16f, 0.16f, 8, { 205, 218, 228, 255 });

            // Twin Warm Gold/Brass Side Guide Tubes (drawn full corridor depth to eliminate pop-in)
            DrawCylinderEx({ pA.x - 0.24f, pA.y - 0.03f, pA.z }, { pB.x - 0.24f, pB.y - 0.03f, pB.z },
                           0.09f, 0.09f, 5, { 222, 188, 86, 255 });
            DrawCylinderEx({ pA.x + 0.24f, pA.y - 0.03f, pA.z }, { pB.x + 0.24f, pB.y - 0.03f, pB.z },
                           0.09f, 0.09f, 5, { 222, 188, 86, 255 });
        }
    }

    // 2. Clean Dark-Steel H-Frame Support Trusses (spaced cleanly so rails stay readable)
    float trussSpacing = 22.0f;
    float trussOffset = std::fmod(distanceMeters_ * 2.2f, trussSpacing);
    Color trussCol = { 34, 44, 56, 255 };

    for (float z = 4.0f - trussOffset; z < 142.0f; z += trussSpacing) {
        if (z < -4.0f) continue;
        float emerge = GetPortalEmergenceScale(z);
        if (emerge <= 0.02f) continue;

        Vector3 cCenter = ApplyTrackCurve(GetLaneX(2), -0.42f, z);
        Vector3 cLeft   = ApplyTrackCurve((GetLaneX(0) + 0.9f) * emerge, -0.42f, z);
        Vector3 cRight  = ApplyTrackCurve((GetLaneX(NUM_LANES - 1) - 0.9f) * emerge, -0.42f, z);

        DrawCylinderEx(cLeft, cRight, 0.13f * emerge, 0.13f * emerge, 6, trussCol);
        DrawCylinderEx({ cLeft.x * 0.7f, cCenter.y - 2.6f * emerge, z },
                       { cRight.x * 0.7f, cCenter.y - 2.6f * emerge, z }, 0.12f * emerge, 0.12f * emerge, 6, trussCol);
        DrawCylinderEx({ cLeft.x * 0.75f, cCenter.y, z },
                       { cLeft.x * 0.75f, cCenter.y - 12.0f * emerge, z }, 0.22f * emerge, 0.22f * emerge, 6, trussCol);
        DrawCylinderEx({ cRight.x * 0.75f, cCenter.y, z },
                       { cRight.x * 0.75f, cCenter.y - 12.0f * emerge, z }, 0.22f * emerge, 0.22f * emerge, 6, trussCol);

        // Black & Yellow Striped Warning Track Posts along the outer rail supports!
        for (int s = 0; s < 4; ++s) {
            Color bandCol = (s % 2 == 0) ? Color{ 245, 205, 35, 255 } : Color{ 25, 28, 32, 255 };
            DrawCylinderEx({ cLeft.x + 0.4f * emerge, cLeft.y + s * 0.32f * emerge, z },
                           { cLeft.x + 0.4f * emerge, cLeft.y + (s + 1) * 0.32f * emerge, z },
                           0.12f * emerge, 0.12f * emerge, 6, bandCol);
            DrawCylinderEx({ cRight.x - 0.4f * emerge, cRight.y + s * 0.32f * emerge, z },
                           { cRight.x - 0.4f * emerge, cRight.y + (s + 1) * 0.32f * emerge, z },
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

                // 1. Sheared, jagged glowing metal stumps at gap entrance & exit
                DrawCylinderEx({ pStart.x, pStart.y, pStart.z - 0.6f }, pStart, 0.28f * s, 0.20f * s, 8, { 55, 62, 74, 255 });
                DrawCylinderEx(pStart, { pStart.x + 0.05f, pStart.y - 0.08f, pStart.z + 0.40f }, 0.22f * s, 0.04f * s, 6, { 255, 120, 30, 255 });

                DrawCylinderEx({ pEnd.x, pEnd.y, pEnd.z + 0.6f }, pEnd, 0.28f * s, 0.20f * s, 8, { 55, 62, 74, 255 });
                DrawCylinderEx(pEnd, { pEnd.x - 0.05f, pEnd.y - 0.08f, pEnd.z - 0.40f }, 0.22f * s, 0.04f * s, 6, { 255, 120, 30, 255 });

                // 2. Dangling frayed high-voltage spark cables
                float cableSag = std::sin(h.animPhase * 2.8f) * 0.16f;
                DrawCylinderEx(pStart, { pStart.x + 0.12f, pStart.y - 1.25f + cableSag, pStart.z + 0.4f }, 0.04f * s, 0.02f * s, 4, DARKGRAY);
                DrawCylinderEx(pEnd, { pEnd.x - 0.12f, pEnd.y - 1.25f - cableSag, pEnd.z - 0.4f }, 0.04f * s, 0.02f * s, 4, DARKGRAY);

                if (std::fmod(h.animPhase, 0.8f) < 0.35f) {
                    DrawSphere({ pStart.x + 0.12f, pStart.y - 1.25f + cableSag, pStart.z + 0.4f }, 0.10f * s, SKYBLUE);
                    DrawSphere({ pEnd.x - 0.12f, pEnd.y - 1.25f - cableSag, pEnd.z - 0.4f }, 0.10f * s, YELLOW);
                }

                // 3. Floating 3D Holographic Hazard Caution Zone in the abyss
                Vector3 midChasm = ApplyTrackCurve(rawLx, 0.80f, h.z);
                float pulse = 0.80f + std::sin(h.animPhase * 4.0f) * 0.20f;
                DrawCubeWires(midChasm, 1.6f * s, 1.2f * s, h.lengthZ * 0.85f,
                              Color{ 255, 55, 35, static_cast<unsigned char>(160 * pulse) });
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
            case HazardKind::MAGMA_GEYSER: {
                // Smoldering basalt geyser vent on rail with erupting volcanic flame column!
                Vector3 ventBase = { lx, ry - 0.1f * s, h.z };
                DrawCylinderEx(ventBase, { lx, ry + 0.45f * s, h.z }, 0.95f * s, 0.65f * s, 8, { 42, 28, 26, 255 });
                DrawCylinderWiresEx(ventBase, { lx, ry + 0.45f * s, h.z }, 0.98f * s, 0.68f * s, 8, ORANGE);

                float eruptH = (1.8f + std::sin(h.animPhase * 5.0f) * 0.7f) * s;
                Vector3 jetTop = { lx, ry + eruptH, h.z };
                DrawCylinderEx({ lx, ry + 0.35f * s, h.z }, jetTop, 0.55f * s, 0.22f * s, 8, { 255, 95, 15, 240 });
                DrawCylinderEx({ lx, ry + 0.35f * s, h.z }, jetTop, 0.32f * s, 0.08f * s, 6, YELLOW);
                DrawSphere(jetTop, 0.45f * s, (std::fmod(h.animPhase, 0.4f) < 0.2f) ? YELLOW : ORANGE);
                break;
            }
            case HazardKind::HOVER_TAXI: {
                // Cyberpunk Aerodynamic Neon Yellow/Cyan Flying Hover-Taxi
                Vector3 taxiPos = { lx, ry + 1.25f * s, h.z };
                float bob = std::sin(h.animPhase * 3.0f) * 0.12f * s;
                taxiPos.y += bob;

                DrawCube(taxiPos, 2.35f * s, 0.88f * s, 4.2f * s, { 235, 195, 30, 255 });
                DrawCubeWires(taxiPos, 2.38f * s, 0.90f * s, 4.24f * s, BLACK);
                DrawCube({ taxiPos.x, taxiPos.y + 0.58f * s, taxiPos.z - 0.2f * s },
                         1.85f * s, 0.65f * s, 2.2f * s, { 38, 48, 62, 255 });
                DrawCube({ taxiPos.x, taxiPos.y + 0.58f * s, taxiPos.z - 1.25f * s },
                         1.70f * s, 0.45f * s, 0.18f * s, { 65, 225, 255, 220 });
                DrawSphere({ taxiPos.x - 0.85f * s, taxiPos.y - 0.1f * s, taxiPos.z - 2.1f * s }, 0.24f * s, WHITE);
                DrawSphere({ taxiPos.x + 0.85f * s, taxiPos.y - 0.1f * s, taxiPos.z - 2.1f * s }, 0.24f * s, WHITE);
                for (int hx = -1; hx <= 1; hx += 2) {
                    for (int hz = -1; hz <= 1; hz += 2) {
                        Vector3 thr = { taxiPos.x + hx * 1.35f * s, taxiPos.y - 0.35f * s, taxiPos.z + hz * 1.3f * s };
                        DrawCylinderEx({ thr.x, thr.y + 0.1f * s, thr.z }, { thr.x, thr.y - 0.25f * s, thr.z },
                                       0.35f * s, 0.25f * s, 6, { 52, 60, 74, 255 });
                        DrawCircle3D({ thr.x, thr.y - 0.26f * s, thr.z }, 0.28f * s, { 0, 1, 0 }, 0.0f, SKYBLUE);
                    }
                }
                break;
            }
            case HazardKind::PIRANHA_LEAP: {
                // Razor-Tooth Cyber-Piranha Leaping over Rails
                float leapY = (0.4f + std::sin(h.animPhase) * 2.6f) * s;
                Vector3 fishPos = { lx, ry + leapY, h.z };

                DrawSphere(fishPos, 0.72f * s, { 180, 195, 210, 255 });
                DrawSphereWires(fishPos, 0.75f * s, 6, 6, { 55, 145, 190, 255 });
                DrawSphere({ fishPos.x - 0.22f * s, fishPos.y + 0.15f * s, fishPos.z - 0.55f * s }, 0.16f * s, RED);
                DrawSphere({ fishPos.x + 0.22f * s, fishPos.y + 0.15f * s, fishPos.z - 0.55f * s }, 0.16f * s, RED);
                DrawCube({ fishPos.x, fishPos.y - 0.18f * s, fishPos.z - 0.65f * s }, 0.38f * s, 0.24f * s, 0.42f * s, { 90, 100, 115, 255 });
                DrawCube({ fishPos.x, fishPos.y - 0.24f * s, fishPos.z - 0.70f * s }, 0.32f * s, 0.12f * s, 0.22f * s, WHITE);
                float tailWag = std::sin(h.animPhase * 8.0f) * 0.25f * s;
                DrawTriangle3D({ fishPos.x + tailWag, fishPos.y, fishPos.z + 0.7f * s },
                               { fishPos.x + tailWag, fishPos.y + 0.6f * s, fishPos.z + 1.2f * s },
                               { fishPos.x + tailWag, fishPos.y - 0.6f * s, fishPos.z + 1.2f * s }, { 45, 120, 175, 255 });
                if (leapY < 0.6f * s) {
                    DrawCircle3D({ lx, ry + 0.05f * s, h.z }, 1.1f * s, { 0, 1, 0 }, 0.0f, { 120, 225, 255, 200 });
                }
                break;
            }
            case HazardKind::SWINGING_CRANE_SCRAP: {
                // Heavy Molonoth Scrap Crane Gantry with Swinging Debris
                Vector3 gantryTop = { lx, ry + 5.8f * s, h.z };
                DrawCube(gantryTop, 4.8f * s, 0.65f * s, 0.85f * s, { 145, 75, 38, 255 });
                DrawCubeWires(gantryTop, 4.85f * s, 0.70f * s, 0.90f * s, GOLD);

                float swingAngle = std::sin(h.animPhase * 2.4f) * 0.75f;
                float cableLen = 3.6f * s;
                Vector3 debrisPos = { gantryTop.x + std::sin(swingAngle) * cableLen,
                                      gantryTop.y - std::cos(swingAngle) * cableLen,
                                      gantryTop.z };
                DrawCylinderEx(gantryTop, debrisPos, 0.05f * s, 0.05f * s, 4, DARKGRAY);

                DrawCube(debrisPos, 1.65f * s, 1.45f * s, 1.65f * s, { 120, 58, 32, 255 });
                DrawCubeWires(debrisPos, 1.70f * s, 1.50f * s, 1.70f * s, RED);
                DrawSphere(debrisPos, 0.55f * s, { 255, 140, 30, 200 });
                break;
            }
            case HazardKind::SWINGSHOT_NODE: {
                // Glowing Emerald Versa-Fuse Swingshot Anchor Node (Hovering Overhead!)
                Vector3 nodePos = { lx, ry + 3.8f * s, h.z };
                float pulse = 1.0f + std::sin(h.animPhase * 4.0f) * 0.12f;

                DrawSphere(nodePos, 0.55f * s * pulse, GREEN);
                DrawSphere(nodePos, 0.32f * s, WHITE);

                constexpr int RING_PTS = 8;
                float r1 = 1.15f * s;
                float r2 = 1.55f * s;
                for (int pt = 0; pt < RING_PTS; ++pt) {
                    float a1 = (pt * (360.0f / RING_PTS)) * DEG2RAD + h.animPhase * 3.0f;
                    float a2 = ((pt + 1) * (360.0f / RING_PTS)) * DEG2RAD + h.animPhase * 3.0f;
                    Vector3 p1 = { nodePos.x + std::cos(a1) * r1, nodePos.y + std::sin(a1) * r1, nodePos.z };
                    Vector3 p2 = { nodePos.x + std::cos(a2) * r1, nodePos.y + std::sin(a2) * r1, nodePos.z };
                    DrawCylinderEx(p1, p2, 0.07f * s, 0.07f * s, 5, { 60, 255, 120, 230 });
                }
                DrawCircle3D(nodePos, r2, { 0, 0, 1 }, 0.0f, { 120, 255, 160, 160 });
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

    if (boss_.bossType == BossType::EMPEROR_NEFARIOUS_FRIGATE) {
        // EMPEROR NEFARIOUS IMPERIAL DREAD-FRIGATE (Imperial Purple & 24K Gold Filigree)
        DrawCube(hullPos, 10.5f, 2.4f, 4.6f, { 78, 24, 112, 255 });
        DrawCubeWires(hullPos, 10.6f, 2.5f, 4.7f, GOLD);

        // Angled Imperial Prow & Outriggers
        DrawCube({ hullPos.x - 5.5f, hullPos.y + 0.3f, hullPos.z + 0.6f }, 2.4f, 1.3f, 5.2f, { 98, 32, 142, 255 });
        DrawCubeWires({ hullPos.x - 5.5f, hullPos.y + 0.3f, hullPos.z + 0.6f }, 2.45f, 1.35f, 5.25f, GOLD);
        DrawCube({ hullPos.x + 5.5f, hullPos.y + 0.3f, hullPos.z + 0.6f }, 2.4f, 1.3f, 5.2f, { 98, 32, 142, 255 });
        DrawCubeWires({ hullPos.x + 5.5f, hullPos.y + 0.3f, hullPos.z + 0.6f }, 2.45f, 1.35f, 5.25f, GOLD);

        // Imperial Green Energy Reactor Intake on Top
        DrawCube({ hullPos.x, hullPos.y + 1.45f, hullPos.z - 0.8f }, 3.6f, 0.95f, 2.4f, { 45, 235, 110, 220 });
        DrawCubeWires({ hullPos.x, hullPos.y + 1.45f, hullPos.z - 0.8f }, 3.65f, 0.98f, 2.45f, WHITE);

        // Floating Giant Holographic Dr. Nefarious Head Projection over Bridge!
        Vector3 holoHead = { hullPos.x, hullPos.y + 3.2f, hullPos.z - 0.4f };
        float hPulse = 1.0f + std::sin(globalTime_ * 8.0f) * 0.08f;
        DrawSphere(holoHead, 1.15f * hPulse, { 50, 220, 100, 145 });
        DrawSphereWires(holoHead, 1.20f * hPulse, 8, 8, { 80, 255, 130, 200 });
        // Glowing red/yellow holographic eyes
        DrawSphere({ holoHead.x - 0.35f, holoHead.y + 0.15f, holoHead.z - 0.95f }, 0.22f, RED);
        DrawSphere({ holoHead.x + 0.35f, holoHead.y + 0.15f, holoHead.z - 0.95f }, 0.22f, RED);

        // Sweeping Dual Imperial Death Rays sweeping across the rails!
        Vector3 leftLance  = { hullPos.x - 4.2f, hullPos.y - 0.6f, hullPos.z - 1.2f };
        Vector3 rightLance = { hullPos.x + 4.2f, hullPos.y - 0.6f, hullPos.z - 1.2f };
        Vector3 sweepTarget = ApplyTrackCurve(boss_.laserSweepX, 0.15f, 0.0f);

        DrawCylinderEx(leftLance, sweepTarget, 0.26f, 0.12f, 8, { 215, 45, 255, 225 });
        DrawCylinderEx(leftLance, sweepTarget, 0.12f, 0.05f, 6, WHITE);
        DrawCylinderEx(rightLance, sweepTarget, 0.26f, 0.12f, 8, { 215, 45, 255, 225 });
        DrawCylinderEx(rightLance, sweepTarget, 0.12f, 0.05f, 6, WHITE);

        // Scorching Laser Sweep Reticle on Rail
        DrawCircle3D(sweepTarget, 1.2f, { 0, 1, 0 }, 0.0f, { 255, 45, 220, 240 });
        DrawCircle3D(sweepTarget, 0.6f, { 0, 1, 0 }, 0.0f, WHITE);

        // Turrets (Glowing Emerald/Gold Plasma Emitters)
        for (int l = 0; l < NUM_LANES; ++l) {
            Vector3 tPos = ApplyTrackCurve(GetLaneX(l), boss_.hoverY - 1.15f, boss_.z - 1.8f);
            if (boss_.turretHp[l] > 0.0f) {
                DrawSphere(tPos, 0.98f, { 45, 235, 110, 255 });
                DrawSphereWires(tPos, 1.15f, 6, 6, GOLD);
                DrawCylinderEx(tPos, { tPos.x, tPos.y - 0.2f, tPos.z - 1.3f }, 0.24f, 0.18f, 8, { 55, 24, 82, 255 });
            } else {
                DrawSphere(tPos, 0.60f, DARKGRAY);
            }
        }
    } else if (boss_.bossType == BossType::CAPTAIN_SLAG_CORSAIR) {
        // CAPTAIN SLAG // SPACE PIRATE SKY-GALLEON (Dark Mahogany, Brass & Glowing Energy Sails)
        DrawCube(hullPos, 8.2f, 2.5f, 6.2f, { 76, 44, 28, 255 });
        DrawCubeWires(hullPos, 8.3f, 2.6f, 6.3f, GOLD);
        // Prow figurehead & twin broadside cannon sponsons
        DrawCylinderEx({ hullPos.x, hullPos.y - 0.2f, hullPos.z - 3.1f },
                       { hullPos.x, hullPos.y + 0.4f, hullPos.z - 5.2f }, 1.2f, 0.2f, 6, GOLD);
        DrawCube({ hullPos.x - 4.8f, hullPos.y, hullPos.z }, 1.6f, 1.3f, 4.6f, { 52, 34, 24, 255 });
        DrawCube({ hullPos.x + 4.8f, hullPos.y, hullPos.z }, 1.6f, 1.3f, 4.6f, { 52, 34, 24, 255 });
        // Tall Pirate Mast & Twin Glowing Cyan Energy Sails
        DrawCylinderEx({ hullPos.x, hullPos.y + 1.2f, hullPos.z },
                       { hullPos.x, hullPos.y + 6.8f, hullPos.z }, 0.28f, 0.22f, 6, { 165, 130, 65, 255 });
        DrawCube({ hullPos.x, hullPos.y + 4.4f, hullPos.z + 0.2f }, 6.8f, 3.4f, 0.22f, { 45, 210, 245, 185 });
        DrawCubeWires({ hullPos.x, hullPos.y + 4.4f, hullPos.z + 0.2f }, 6.85f, 3.45f, 0.25f, GOLD);

        for (int l = 0; l < NUM_LANES; ++l) {
            Vector3 tPos = ApplyTrackCurve(GetLaneX(l), boss_.hoverY - 1.15f, boss_.z - 1.8f);
            if (boss_.turretHp[l] > 0.0f) {
                DrawCylinderEx(tPos, { tPos.x, tPos.y - 0.25f, tPos.z - 1.8f }, 0.55f, 0.38f, 8, { 42, 44, 52, 255 });
                DrawSphere(tPos, 0.90f, SKYBLUE);
                DrawSphereWires(tPos, 1.05f, 6, 6, GOLD);
            } else {
                DrawSphere(tPos, 0.60f, DARKGRAY);
            }
        }
    } else if (boss_.bossType == BossType::DR_NEFARIOUS_MECH) {
        // BIOLITERATOR // NEFARIOUS DREAD-MECH (Vertical Cybernetic Walker + 4 Claw Arms + Lock-On Beam)
        DrawCube({ hullPos.x, hullPos.y + 1.0f, hullPos.z }, 4.6f, 4.8f, 3.6f, { 32, 42, 48, 255 });
        DrawCubeWires({ hullPos.x, hullPos.y + 1.0f, hullPos.z }, 4.7f, 4.9f, 3.7f, GREEN);
        // Glowing Crimson Cyclops Visor
        DrawCube({ hullPos.x, hullPos.y + 2.2f, hullPos.z - 1.85f }, 2.6f, 0.85f, 0.35f, RED);
        // Rotating Emerald Shield Ring
        for (int r = 0; r < 8; ++r) {
            float a1 = (r * 45.0f + globalTime_ * 90.0f) * DEG2RAD;
            float a2 = ((r + 1) * 45.0f + globalTime_ * 90.0f) * DEG2RAD;
            Vector3 p1 = { hullPos.x + std::cos(a1) * 3.8f, hullPos.y + 1.0f + std::sin(a1) * 3.8f, hullPos.z - 0.5f };
            Vector3 p2 = { hullPos.x + std::cos(a2) * 3.8f, hullPos.y + 1.0f + std::sin(a2) * 3.8f, hullPos.z - 0.5f };
            DrawCylinderEx(p1, p2, 0.16f, 0.16f, 5, GREEN);
        }
        // 4 Mechanical Claw Arms
        for (int side = -1; side <= 1; side += 2) {
            for (int tier = 0; tier < 2; ++tier) {
                Vector3 sh = { hullPos.x + side * 2.4f, hullPos.y + 2.2f - tier * 1.8f, hullPos.z };
                Vector3 el = { hullPos.x + side * 5.2f, hullPos.y + 1.6f - tier * 2.2f, hullPos.z - 1.5f };
                Vector3 cl = { hullPos.x + side * 4.4f, hullPos.y - 0.4f - tier * 1.6f, hullPos.z - 3.2f };
                DrawCylinderEx(sh, el, 0.35f, 0.28f, 6, { 58, 66, 76, 255 });
                DrawCylinderEx(el, cl, 0.28f, 0.16f, 6, { 58, 66, 76, 255 });
                DrawSphere(cl, 0.45f, RED);
            }
        }
        // Lock-On Laser Telegraph Line down the targeted rail!
        if (boss_.telegraphTimer > 0.0f && boss_.telegraphLane >= 0) {
            Vector3 eyePos = { hullPos.x, hullPos.y + 2.2f, hullPos.z - 1.9f };
            Vector3 lockTarget = ApplyTrackCurve(GetLaneX(boss_.telegraphLane), 0.35f, 0.0f);
            DrawCylinderEx(eyePos, lockTarget, 0.14f, 0.26f, 6, { 255, 45, 45, 220 });
            DrawCircle3D(lockTarget, 1.15f, { 0, 1, 0 }, 0.0f, RED);
        }

        for (int l = 0; l < NUM_LANES; ++l) {
            Vector3 tPos = ApplyTrackCurve(GetLaneX(l), boss_.hoverY - 1.15f, boss_.z - 1.8f);
            if (boss_.turretHp[l] > 0.0f) {
                DrawSphere(tPos, 0.95f, GREEN);
                DrawSphereWires(tPos, 1.1f, 6, 6, RED);
            } else {
                DrawSphere(tPos, 0.60f, DARKGRAY);
            }
        }
    } else {
        // PROG TWINS // MEGACORP COMMAND SHIP (Industrial Gunmetal & Hazard Orange)
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
}

void RailGame::RenderLombaxHero3D(Vector3 pos, float tiltDeg, bool sliding, bool gliding,
                                  float wrenchTimer, int weaponId) const {
    // Animation Blend Weights & Procedural Timers
    float aimBlend   = std::clamp(player_.gunAimTimer / 0.16f, 0.0f, 1.0f);
    float recoilFrac = std::clamp(player_.gunRecoilTimer / 0.20f, 0.0f, 1.0f);
    float recoilKick = std::sin(recoilFrac * 3.14159265f); // Snappy impulse curve 0 -> 1 -> 0
    float squash     = std::sin(std::clamp(player_.landingSquashTimer / 0.25f, 0.0f, 1.0f) * 3.14159265f);
    bool airborne    = !player_.grounded;
    float vy         = player_.vy;

    // Rhythmic Grind Suspension Bob
    float grindBob = airborne ? 0.0f : (std::sin(globalTime_ * 18.0f) * 0.04f);
    float crouchDrop = (sliding ? -0.45f : 0.0f) - squash * 0.20f + grindBob;
    float leanX = tiltDeg * 0.014f;

    // Authentic Ratchet & Clank Palette driven by equipped / previewed Armor & Skin
    int armIdx = (mode_ == ScreenMode::ARMOR_WARDROBE) ? wardrobePreviewArmor_ : save_.equippedArmor;
    int clkIdx = (mode_ == ScreenMode::ARMOR_WARDROBE) ? wardrobePreviewClank_ : save_.equippedClankSkin;
    armIdx = std::clamp(armIdx, 0, static_cast<int>(NUM_ARMOR_SUITS) - 1);
    clkIdx = std::clamp(clkIdx, 0, static_cast<int>(NUM_CLANK_SKINS) - 1);

    bool isRivet = (armIdx == static_cast<int>(ArmorSuitId::REBEL_LOMBAX));
    bool isKit   = (clkIdx == 4);

    const auto& armDef = ARMOR_CATALOG[armIdx];
    const auto& clkDef = CLANK_SKIN_CATALOG[clkIdx];

    Color furGold     = isRivet ? Color{ 232, 236, 246, 255 } : Color{ 245, 186, 52, 255 };
    Color furInner    = isRivet ? Color{ 192, 182, 218, 255 } : Color{ 215, 138, 68, 255 };
    Color furStripe   = isRivet ? Color{ 118, 44, 172, 255 }  : Color{ 128, 52, 16, 255 };
    Color armorOrange = armDef.primaryColor;
    Color armorDark   = armDef.accentColor;
    Color pantsBlue   = armDef.pantsColor;
    Color visorCol    = armDef.visorColor;
    Color leatherBrn  = (armIdx == static_cast<int>(ArmorSuitId::CLASSIC_COMMANDO)) ? Color{ 92, 50, 22, 255 } : isRivet ? Color{ 48, 38, 32, 255 } : armDef.accentColor;
    Color gloveBrn    = (armIdx == static_cast<int>(ArmorSuitId::CLASSIC_COMMANDO)) ? Color{ 72, 38, 16, 255 } : armDef.accentColor;

    Color chromeSilv  = clkDef.chassisColor;
    Color chromeBright= clkDef.brightColor;
    Color clankEyeGreen = clkDef.eyeColor;
    Color clankBeaconCol = clkDef.beaconColor;
    Color clankTrim   = isKit ? Color{ 88, 52, 32, 255 } : Color{ 36, 42, 50, 255 };

    // =========================================================================
    // 1. LEGS & MAGNETIC GRIND BOOTS (ARMOR SKINS & ENERGY GLOW SOLES)
    // =========================================================================
    float recoilLeanZ = -recoilKick * 0.10f;
    Vector3 shoulderCenter = { pos.x + leanX * 0.4f, pos.y + 1.15f + crouchDrop, pos.z + recoilLeanZ };

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

    Vector3 leftFoot  = { pos.x + 0.13f, pos.y + 0.12f + jumpTuck + legSway, pos.z + (sliding ? 0.38f : 0.20f) };
    Vector3 rightFoot = { pos.x - 0.13f, pos.y + 0.12f + jumpTuck * 0.82f - legSway, pos.z - (sliding ? 0.28f : 0.20f) };

    auto DrawGrindBoot = [&](Vector3 fPos) {
        Color bBase = (armIdx == static_cast<int>(ArmorSuitId::CLASSIC_COMMANDO)) ? chromeSilv :
                      (armIdx == static_cast<int>(ArmorSuitId::LOMBAX_PRAETORIAN)) ? armDef.primaryColor :
                      isRivet ? Color{ 28, 34, 46, 255 } : armorDark;
        Color bCap  = (armIdx == static_cast<int>(ArmorSuitId::CLASSIC_COMMANDO)) ? armorOrange :
                      (armIdx == static_cast<int>(ArmorSuitId::LOMBAX_PRAETORIAN)) ? armorDark :
                      isRivet ? armorDark : armorOrange;
        DrawCylinderEx({ fPos.x, fPos.y - 0.06f, fPos.z - 0.15f },
                       { fPos.x, fPos.y - 0.06f, fPos.z + 0.18f }, 0.120f, 0.100f, 8, bBase);
        DrawSphere({ fPos.x, fPos.y - 0.04f, fPos.z + 0.16f }, 0.105f, bCap);
        DrawCylinderEx({ fPos.x, fPos.y - 0.02f, fPos.z - 0.03f },
                       { fPos.x, fPos.y + 0.12f, fPos.z - 0.02f }, 0.100f, 0.090f, 8,
                       (armIdx == static_cast<int>(ArmorSuitId::LOMBAX_PRAETORIAN) || isRivet) ? armorDark : armorOrange);
        // Magnetic rail lock soles glow with each suit's energy signature!
        DrawCube({ fPos.x, fPos.y - 0.09f, fPos.z + 0.01f }, 0.22f, 0.05f, 0.36f, visorCol);
        DrawCubeWires({ fPos.x, fPos.y - 0.09f, fPos.z + 0.01f }, 0.225f, 0.052f, 0.365f,
                      (armIdx == static_cast<int>(ArmorSuitId::LOMBAX_PRAETORIAN)) ? armorDark : WHITE);
    };
    DrawGrindBoot(leftFoot);
    DrawGrindBoot(rightFoot);

    // Overdrive Hoverboots Supersonic Rocket Thruster Flames!
    if (player_.overdriveDuration > 0.0f) {
        float fLen = 0.95f + std::sin(globalTime_ * 32.0f) * 0.25f;
        Vector3 lHeel = { leftFoot.x, leftFoot.y - 0.04f, leftFoot.z - 0.18f };
        Vector3 rHeel = { rightFoot.x, rightFoot.y - 0.04f, rightFoot.z - 0.18f };
        DrawCylinderEx(lHeel, { lHeel.x, lHeel.y + 0.04f, lHeel.z - fLen }, 0.15f, 0.02f, 6, ORANGE);
        DrawCylinderEx(lHeel, { lHeel.x, lHeel.y + 0.04f, lHeel.z - fLen * 0.6f }, 0.08f, 0.01f, 4, YELLOW);
        DrawCylinderEx(rHeel, { rHeel.x, rHeel.y + 0.04f, rHeel.z - fLen }, 0.15f, 0.02f, 6, ORANGE);
        DrawCylinderEx(rHeel, { rHeel.x, rHeel.y + 0.04f, rHeel.z - fLen * 0.6f }, 0.08f, 0.01f, 4, YELLOW);
    }

    // Swingshot Versa-Fuse Energy Tether (Cinematic Grapple Arc)
    if (player_.isSwinging) {
        Vector3 handPos = { pos.x - 0.35f, pos.y + 1.65f, pos.z + 0.2f };
        DrawCylinderEx(handPos, player_.swingNodePos, 0.08f, 0.04f, 6, GREEN);
        DrawCylinderEx(handPos, player_.swingNodePos, 0.035f, 0.015f, 4, WHITE);
        for (int r = 1; r <= 3; ++r) {
            float frac = static_cast<float>(r) / 4.0f;
            Vector3 ringP = Vector3Lerp(handPos, player_.swingNodePos, frac);
            DrawCircle3D(ringP, 0.22f, { 0, 0, 1 }, 0.0f, { 120, 255, 140, 220 });
        }
    }

    float kneeForwardTuck = airborne ? (jumpTuck * 0.32f) : 0.0f;
    Vector3 pelvisPos = { pos.x + leanX * 0.6f, pos.y + 0.72f + crouchDrop, pos.z };
    Vector3 leftKnee  = { pos.x + 0.17f + leanX * 0.3f, pos.y + 0.44f + crouchDrop * 0.55f + jumpTuck * 0.55f, pos.z + 0.16f + kneeForwardTuck };
    Vector3 rightKnee = { pos.x - 0.17f + leanX * 0.3f, pos.y + 0.42f + crouchDrop * 0.55f + jumpTuck * 0.45f, pos.z - 0.08f + kneeForwardTuck };
    Vector3 leftHip   = { pelvisPos.x + 0.12f, pelvisPos.y, pelvisPos.z + 0.04f };
    Vector3 rightHip  = { pelvisPos.x - 0.12f, pelvisPos.y, pelvisPos.z - 0.04f };

    DrawCylinderEx({ leftFoot.x, leftFoot.y + 0.08f, leftFoot.z }, leftKnee, 0.080f, 0.095f, 8, pantsBlue);
    DrawCylinderEx({ rightFoot.x, rightFoot.y + 0.08f, rightFoot.z }, rightKnee, 0.080f, 0.095f, 8, pantsBlue);
    DrawSphere(leftKnee, 0.095f, armorDark);
    DrawSphere(rightKnee, 0.095f, armorDark);
    if (armIdx == static_cast<int>(ArmorSuitId::CARBONOX)) { // Carbonox: Inset nanotech cyan pips
        DrawSphere({ leftKnee.x, leftKnee.y, leftKnee.z + 0.08f }, 0.024f, visorCol);
        DrawSphere({ rightKnee.x, rightKnee.y, rightKnee.z + 0.08f }, 0.024f, visorCol);
    } else if (isRivet) { // Rebel Lombax: Indigo combat pants with reinforced yellow knee guard
        DrawSphere({ leftKnee.x, leftKnee.y, leftKnee.z + 0.075f }, 0.026f, armorDark);
        DrawSphere({ rightKnee.x, rightKnee.y, rightKnee.z + 0.075f }, 0.026f, armorDark);
    } else if (armIdx == static_cast<int>(ArmorSuitId::INFERNOX)) { // Infernox: Magma ember kneecaps
        DrawSphere({ leftKnee.x, leftKnee.y, leftKnee.z + 0.08f }, 0.026f, visorCol);
        DrawSphere({ rightKnee.x, rightKnee.y, rightKnee.z + 0.08f }, 0.026f, visorCol);
    } else if (armIdx == static_cast<int>(ArmorSuitId::DEADLOCKED_MARAUDER)) { // Marauder: Spiked combat kneecaps
        DrawCylinderEx(leftKnee, { leftKnee.x, leftKnee.y + 0.02f, leftKnee.z + 0.11f }, 0.030f, 0.008f, 6, armorDark);
        DrawCylinderEx(rightKnee, { rightKnee.x, rightKnee.y + 0.02f, rightKnee.z + 0.11f }, 0.030f, 0.008f, 6, armorDark);
    } else if (armIdx == static_cast<int>(ArmorSuitId::LOMBAX_PRAETORIAN)) { // Praetorian: 24K Gold kneecaps with purple psionic gems
        DrawSphere({ leftKnee.x, leftKnee.y, leftKnee.z + 0.08f }, 0.028f, visorCol);
        DrawSphere({ rightKnee.x, rightKnee.y, rightKnee.z + 0.08f }, 0.028f, visorCol);
    }

    DrawCylinderEx(leftKnee, leftHip, 0.095f, 0.115f, 8, pantsBlue);
    DrawCylinderEx(rightKnee, rightHip, 0.095f, 0.115f, 8, pantsBlue);

    // =========================================================================
    // 2. PELVIS, BELT, TAIL & COMPACT ATHLETIC TORSO (CUSTOM ARMOR SUITS)
    // =========================================================================
    DrawCylinderEx({ pelvisPos.x, pelvisPos.y - 0.04f, pelvisPos.z },
                   { pelvisPos.x, pelvisPos.y + 0.07f, pelvisPos.z }, 0.21f, 0.20f, 8, leatherBrn);
    // Side Utility Pouches / Nanotech Battery Packs
    DrawCube({ pelvisPos.x - 0.19f, pelvisPos.y + 0.015f, pelvisPos.z }, 0.06f, 0.07f, 0.08f, (armIdx == 0) ? leatherBrn : armorDark);
    DrawCube({ pelvisPos.x + 0.19f, pelvisPos.y + 0.015f, pelvisPos.z }, 0.06f, 0.07f, 0.08f, (armIdx == 0) ? leatherBrn : armorDark);

    // Custom Belt Buckle / Power Conduit per Skin
    Vector3 bucklePos = { pelvisPos.x, pelvisPos.y + 0.015f, pelvisPos.z + 0.21f };
    if (armIdx == static_cast<int>(ArmorSuitId::CLASSIC_COMMANDO)) { // Classic: Silver aviator pilot buckle
        DrawCube(bucklePos, 0.08f, 0.06f, 0.02f, chromeBright);
        DrawCube({ bucklePos.x, bucklePos.y, bucklePos.z + 0.01f }, 0.04f, 0.04f, 0.015f, armorDark);
    } else if (armIdx == static_cast<int>(ArmorSuitId::CARBONOX)) { // Carbonox: Nanotech graphite power pack with cyan energy core
        DrawCube(bucklePos, 0.09f, 0.06f, 0.025f, armorDark);
        DrawCube({ bucklePos.x, bucklePos.y, bucklePos.z + 0.01f }, 0.06f, 0.02f, 0.015f, visorCol);
    } else if (isRivet) { // Rebel Lombax: Rebel insurgent buckle with glowing cyan cell
        DrawCube(bucklePos, 0.085f, 0.06f, 0.025f, armorDark);
        DrawSphere({ bucklePos.x, bucklePos.y, bucklePos.z + 0.01f }, 0.022f, visorCol);
        DrawSphere({ bucklePos.x, bucklePos.y, bucklePos.z + 0.015f }, 0.010f, WHITE);
    } else if (armIdx == static_cast<int>(ArmorSuitId::INFERNOX)) { // Infernox: Volcanic basalt core with magma ember
        DrawCube(bucklePos, 0.09f, 0.06f, 0.025f, armorDark);
        DrawSphere({ bucklePos.x, bucklePos.y, bucklePos.z + 0.01f }, 0.025f, visorCol);
    } else if (armIdx == static_cast<int>(ArmorSuitId::DEADLOCKED_MARAUDER)) { // Marauder: Heavy Dreadzone combat buckle with combat slit
        DrawCube(bucklePos, 0.10f, 0.07f, 0.03f, armorDark);
        DrawCube({ bucklePos.x, bucklePos.y, bucklePos.z + 0.01f }, 0.07f, 0.02f, 0.015f, visorCol);
    } else if (armIdx == static_cast<int>(ArmorSuitId::LOMBAX_PRAETORIAN)) { // Lombax Praetorian: Ornate 24K gold royal crest with psionic violet gem
        DrawSphere(bucklePos, 0.045f, armorDark);
        DrawSphere({ bucklePos.x, bucklePos.y, bucklePos.z + 0.01f }, 0.026f, visorCol);
        DrawSphere({ bucklePos.x - 0.005f, bucklePos.y + 0.005f, bucklePos.z + 0.018f }, 0.009f, WHITE);
    }

    // Whipping Striped Lombax Tail
    Vector3 t0 = { pelvisPos.x, pelvisPos.y - 0.02f, pelvisPos.z - 0.16f };
    for (int seg = 0; seg < 4; ++seg) {
        float f1 = static_cast<float>(seg + 1) * 0.25f;
        float wagX = std::sin(globalTime_ * 10.0f - seg * 0.85f) * (0.24f * f1) - leanX * 0.7f * f1;
        float wagY = std::cos(globalTime_ * 5.0f - seg * 0.6f) * 0.06f + f1 * 0.10f;
        Vector3 t1 = { pelvisPos.x + wagX, pelvisPos.y - 0.04f + wagY, pelvisPos.z - 0.16f - f1 * 0.95f };
        float r0 = 0.075f - seg * 0.009f;
        float r1 = (seg == 3) ? 0.095f : (0.068f - seg * 0.009f);
        DrawCylinderEx(t0, t1, r0, r1, 6, (seg % 2 == 0) ? furGold : furStripe);
        if (seg == 3) DrawSphere(t1, 0.095f, furStripe);
        t0 = t1;
    }

    // Slim, Athletic Lombax Torso & Fitted Armor
    shoulderCenter = { pelvisPos.x + leanX * 0.4f, pelvisPos.y + 0.44f, pelvisPos.z + recoilLeanZ };
    DrawCylinderEx(pelvisPos, shoulderCenter, 0.19f, 0.24f, 8, (armIdx == static_cast<int>(ArmorSuitId::LOMBAX_PRAETORIAN)) ? pantsBlue : armorOrange);

    // Form-fitting vest / tunic across chest
    Color vestCol = (armIdx == static_cast<int>(ArmorSuitId::CLASSIC_COMMANDO)) ? leatherBrn :
                    (armIdx == static_cast<int>(ArmorSuitId::LOMBAX_PRAETORIAN)) ? armDef.primaryColor : armorOrange;
    DrawCylinderEx({ pelvisPos.x, pelvisPos.y + 0.06f, pelvisPos.z },
                   { shoulderCenter.x, shoulderCenter.y - 0.04f, shoulderCenter.z }, 0.20f, 0.245f, 6, vestCol);

    if (armIdx == static_cast<int>(ArmorSuitId::LOMBAX_PRAETORIAN)) {
        // Imperial 24K Gold ceremonial diagonal sash across chest & waist
        DrawCylinderEx({ shoulderCenter.x - 0.12f, shoulderCenter.y + 0.02f, shoulderCenter.z + 0.11f },
                       { pelvisPos.x + 0.10f, pelvisPos.y + 0.06f, pelvisPos.z + 0.10f }, 0.032f, 0.032f, 6, armorDark);
        DrawSphere({ shoulderCenter.x - 0.12f, shoulderCenter.y + 0.02f, shoulderCenter.z + 0.12f }, 0.026f, visorCol);
    }

    // Custom Chestplate Insignia & Energetic Circuitry per Skin (Form-fitting, beautifully integrated)
    Vector3 chestCenter = { shoulderCenter.x, shoulderCenter.y - 0.06f, shoulderCenter.z + 0.14f };
    if (armIdx == static_cast<int>(ArmorSuitId::CLASSIC_COMMANDO)) {
        // Classic Commando: Solana aviator flight harness seam & Gadgetron comm badge
        DrawCylinderEx({ chestCenter.x, chestCenter.y - 0.08f, chestCenter.z },
                       { chestCenter.x, chestCenter.y + 0.06f, chestCenter.z }, 0.012f, 0.012f, 4, armorDark);
        DrawCube({ chestCenter.x - 0.038f, chestCenter.y + 0.018f, chestCenter.z + 0.018f }, 0.075f, 0.048f, 0.020f, armorDark);
        DrawCube({ chestCenter.x - 0.038f, chestCenter.y + 0.018f, chestCenter.z + 0.026f }, 0.055f, 0.030f, 0.014f, visorCol);
        DrawSphere({ chestCenter.x + 0.048f, chestCenter.y + 0.018f, chestCenter.z + 0.018f }, 0.015f, chromeBright);
    } else if (armIdx == static_cast<int>(ArmorSuitId::CARBONOX)) {
        // Carbonox: Multi-panel graphite breastplate with glowing nanotech arc reactor
        DrawCube(chestCenter, 0.19f, 0.13f, 0.028f, armorOrange); // Outer graphite plate
        DrawCube({ chestCenter.x, chestCenter.y, chestCenter.z + 0.010f }, 0.15f, 0.09f, 0.022f, armorDark); // Obsidian inset
        // Central Nanotech Arc Core
        DrawSphere({ chestCenter.x, chestCenter.y, chestCenter.z + 0.022f }, 0.030f, visorCol);
        DrawSphere({ chestCenter.x, chestCenter.y, chestCenter.z + 0.026f }, 0.014f, WHITE);
        // Integrated diagonal nanotech power conduits
        DrawCylinderEx({ chestCenter.x - 0.065f, chestCenter.y + 0.025f, chestCenter.z + 0.018f },
                       { chestCenter.x - 0.025f, chestCenter.y - 0.020f, chestCenter.z + 0.020f }, 0.007f, 0.007f, 4, visorCol);
        DrawCylinderEx({ chestCenter.x + 0.065f, chestCenter.y + 0.025f, chestCenter.z + 0.018f },
                       { chestCenter.x + 0.025f, chestCenter.y - 0.020f, chestCenter.z + 0.020f }, 0.007f, 0.007f, 4, visorCol);
    } else if (isRivet) {
        // Rivet: Dark Teal Insurgent Rebel Tunic with diagonal strap & cyan comms beacon
        DrawCube(chestCenter, 0.20f, 0.14f, 0.028f, armorOrange); // Teal flight tunic
        // Diagonal harness strap across chest
        DrawCylinderEx({ chestCenter.x - 0.10f, chestCenter.y + 0.07f, chestCenter.z + 0.01f },
                       { chestCenter.x + 0.08f, chestCenter.y - 0.07f, chestCenter.z + 0.01f }, 0.020f, 0.020f, 6, leatherBrn);
        // Insurgent cyan comm badge
        DrawSphere({ chestCenter.x - 0.045f, chestCenter.y + 0.020f, chestCenter.z + 0.025f }, 0.026f, visorCol);
        DrawSphere({ chestCenter.x - 0.045f, chestCenter.y + 0.020f, chestCenter.z + 0.030f }, 0.012f, WHITE);
        // Yellow accent clasp
        DrawCube({ chestCenter.x + 0.01f, chestCenter.y - 0.01f, chestCenter.z + 0.022f }, 0.035f, 0.035f, 0.014f, armorDark);
    } else if (armIdx == static_cast<int>(ArmorSuitId::INFERNOX)) {
        // Infernox: Scorched volcanic basalt cuirass with active molten magma core
        DrawCube(chestCenter, 0.19f, 0.13f, 0.028f, armorDark);
        DrawSphere({ chestCenter.x, chestCenter.y, chestCenter.z + 0.018f }, 0.040f, visorCol);
        DrawSphere({ chestCenter.x, chestCenter.y, chestCenter.z + 0.022f }, 0.022f, GOLD);
        DrawSphere({ chestCenter.x, chestCenter.y, chestCenter.z + 0.026f }, 0.010f, WHITE);
        DrawCube({ chestCenter.x - 0.060f, chestCenter.y - 0.030f, chestCenter.z + 0.016f }, 0.045f, 0.022f, 0.016f, armorOrange);
        DrawCube({ chestCenter.x + 0.060f, chestCenter.y - 0.030f, chestCenter.z + 0.016f }, 0.045f, 0.022f, 0.016f, armorOrange);
    } else if (armIdx == static_cast<int>(ArmorSuitId::DEADLOCKED_MARAUDER)) {
        // Deadlocked Marauder: Dreadzone heavy steel ballistic chestplate with crimson slit
        DrawCube(chestCenter, 0.20f, 0.14f, 0.030f, armorDark); // Heavy steel base
        DrawCube({ chestCenter.x, chestCenter.y, chestCenter.z + 0.012f }, 0.16f, 0.10f, 0.022f, armorOrange); // Battle crimson plate
        DrawCube({ chestCenter.x, chestCenter.y, chestCenter.z + 0.024f }, 0.12f, 0.018f, 0.014f, visorCol); // Crimson targeting slit
        // 4 Industrial rivets
        DrawSphere({ chestCenter.x - 0.075f, chestCenter.y + 0.042f, chestCenter.z + 0.020f }, 0.013f, armorDark);
        DrawSphere({ chestCenter.x + 0.075f, chestCenter.y + 0.042f, chestCenter.z + 0.020f }, 0.013f, armorDark);
        DrawSphere({ chestCenter.x - 0.075f, chestCenter.y - 0.042f, chestCenter.z + 0.020f }, 0.013f, armorDark);
        DrawSphere({ chestCenter.x + 0.075f, chestCenter.y - 0.042f, chestCenter.z + 0.020f }, 0.013f, armorDark);
    } else if (armIdx == static_cast<int>(ArmorSuitId::LOMBAX_PRAETORIAN)) {
        // Lombax Praetorian: Royal pearl breastplate with 24K gold filigree wings & faceted purple matrix
        DrawCube(chestCenter, 0.22f, 0.14f, 0.032f, armorOrange); // Pearl white plate
        DrawCubeWires(chestCenter, 0.225f, 0.145f, 0.035f, armorDark); // Gold filigree border
        DrawCylinderEx({ chestCenter.x - 0.12f, chestCenter.y + 0.04f, chestCenter.z },
                       { chestCenter.x + 0.12f, chestCenter.y + 0.04f, chestCenter.z }, 0.022f, 0.022f, 6, armorDark);
        DrawSphere({ chestCenter.x, chestCenter.y - 0.01f, chestCenter.z + 0.020f }, 0.042f, visorCol);
        DrawSphere({ chestCenter.x - 0.007f, chestCenter.y + 0.007f, chestCenter.z + 0.032f }, 0.014f, WHITE);
    }

    // Neck collar & base
    Vector3 neckTop = { shoulderCenter.x, shoulderCenter.y - 0.02f, shoulderCenter.z + 0.02f };
    if (!ratchetModelLoaded_) {
        neckTop.y = shoulderCenter.y + 0.14f;
        DrawCylinderEx(shoulderCenter, neckTop, 0.13f, 0.11f, 8, furGold);
    } else {
        // Form-fitting collar surrounding the base of the neck
        DrawCylinderEx(shoulderCenter, { shoulderCenter.x, shoulderCenter.y + 0.04f, shoulderCenter.z + 0.01f }, 0.155f, 0.135f, 8, leatherBrn);
    }

    // Form-Fitting Armor Pauldrons on Left and Right Shoulders (SOLID, CONTOURED, ZERO FLOATING WIREFRAMES!)
    for (int side = -1; side <= 1; side += 2) {
        float s = static_cast<float>(side);
        Vector3 shP = { shoulderCenter.x + s * 0.23f, shoulderCenter.y - 0.01f, shoulderCenter.z };
        if (armIdx == static_cast<int>(ArmorSuitId::CLASSIC_COMMANDO)) {
            // Classic Commando: Contoured leather shoulder pad with brass harness rivet
            DrawSphere(shP, 0.082f, armorOrange);
            DrawSphere({ shP.x + s * 0.015f, shP.y + 0.015f, shP.z }, 0.072f, armorDark);
            DrawSphere({ shP.x + s * 0.030f, shP.y + 0.020f, shP.z }, 0.014f, chromeBright);
        } else if (armIdx == static_cast<int>(ArmorSuitId::CARBONOX)) {
            // Carbonox: Sleek graphite composite shoulder cowl with inset cyan LED line (NO WIREFRAME BOX!)
            DrawSphere(shP, 0.088f, armorOrange); // Solid graphite shoulder cap
            DrawSphere({ shP.x + s * 0.015f, shP.y + 0.015f, shP.z }, 0.074f, armorDark);
            // Inset glowing cyan nanotech arc line
            DrawCylinderEx({ shP.x - 0.04f, shP.y + 0.032f, shP.z + 0.035f },
                           { shP.x + 0.04f, shP.y + 0.032f, shP.z + 0.035f }, 0.009f, 0.009f, 6, visorCol);
        } else if (isRivet) {
            // Rivet: Asymmetrical insurgent shoulder pauldrons
            if (side == -1) {
                // Right shoulder: Yellow mechanical prosthetic housing mount
                DrawSphere(shP, 0.092f, armorDark); // Bright yellow cybernetic cowl
                DrawSphere({ shP.x - 0.015f, shP.y, shP.z }, 0.072f, { 38, 42, 48, 255 }); // Dark rotary ring
                DrawSphere({ shP.x - 0.025f, shP.y, shP.z }, 0.032f, visorCol); // Cyan power core
            } else {
                // Left shoulder: Reinforced teal leather flight pad with yellow clasp
                DrawSphere(shP, 0.085f, armorOrange);
                DrawSphere({ shP.x + 0.02f, shP.y + 0.01f, shP.z }, 0.024f, armorDark);
            }
        } else if (armIdx == static_cast<int>(ArmorSuitId::INFERNOX)) {
            // Infernox: Curved molten basalt pauldron with glowing magma fissure
            DrawSphere(shP, 0.088f, armorOrange); // Volcanic base
            DrawSphere({ shP.x + s * 0.015f, shP.y + 0.015f, shP.z }, 0.075f, armorDark); // Basalt armor shell
            DrawSphere({ shP.x + s * 0.030f, shP.y + 0.025f, shP.z + 0.025f }, 0.024f, visorCol); // Glowing magma fissure
            DrawSphere({ shP.x + s * 0.030f, shP.y + 0.025f, shP.z + 0.025f }, 0.012f, GOLD);
        } else if (armIdx == static_cast<int>(ArmorSuitId::DEADLOCKED_MARAUDER)) {
            // Deadlocked Marauder: Curved gladiator combat plate with heavy gunmetal rim
            DrawSphere(shP, 0.088f, armorOrange); // Battle crimson plate
            DrawCube({ shP.x, shP.y + 0.02f, shP.z }, 0.11f, 0.035f, 0.11f, armorDark); // Gunmetal clamp
            // Dual solid combat studs
            DrawCylinderEx(shP, { shP.x + s * 0.075f, shP.y + 0.055f, shP.z + 0.02f }, 0.022f, 0.006f, 6, armorDark);
            DrawCylinderEx(shP, { shP.x + s * 0.075f, shP.y + 0.010f, shP.z - 0.03f }, 0.019f, 0.006f, 6, armorDark);
        } else if (armIdx == static_cast<int>(ArmorSuitId::LOMBAX_PRAETORIAN)) {
            // Lombax Praetorian: Regal Royal Pearl & 24K Gold Winged Crest Pauldrons
            DrawSphere(shP, 0.090f, armorOrange); // Pearl white plate
            Vector3 wingTip = { shP.x + s * 0.11f, shP.y + 0.08f, shP.z - 0.02f };
            DrawCylinderEx(shP, wingTip, 0.034f, 0.010f, 6, armorDark); // 24K gold winged crest
            DrawSphere({ shP.x + s * 0.035f, shP.y + 0.035f, shP.z + 0.030f }, 0.024f, visorCol); // Psionic jewel
            DrawSphere({ shP.x + s * 0.035f, shP.y + 0.035f, shP.z + 0.036f }, 0.008f, WHITE);
        }
    }

    // =========================================================================
    // 3. AUTHENTIC CLANK CHARACTER MODEL (PROPORTIONAL, COMPACT BACKPACK ROBOT!)
    // =========================================================================
    // Sits snugly against Ratchet's upper back (Z = -0.22m gives flush, realistic backpack mount!)
    Vector3 clankPos = { shoulderCenter.x, shoulderCenter.y - 0.06f, shoulderCenter.z - 0.22f };

    // --- A. Iconic Beveled Silver Backpack Housing ---
    // Compact, tapered backpack block proportional to Ratchet's torso
    DrawCube({ clankPos.x, clankPos.y, clankPos.z }, 0.32f, 0.38f, 0.16f, chromeSilv);
    DrawCubeWires({ clankPos.x, clankPos.y, clankPos.z }, 0.325f, 0.385f, 0.165f, clankTrim);

    // Beveled outer vertical edge frame around the backpack
    DrawCylinderEx({ clankPos.x - 0.15f, clankPos.y - 0.17f, clankPos.z - 0.07f },
                   { clankPos.x - 0.15f, clankPos.y + 0.17f, clankPos.z - 0.07f }, 0.020f, 0.020f, 6, chromeBright);
    DrawCylinderEx({ clankPos.x + 0.15f, clankPos.y - 0.17f, clankPos.z - 0.07f },
                   { clankPos.x + 0.15f, clankPos.y + 0.17f, clankPos.z - 0.07f }, 0.020f, 0.020f, 6, chromeBright);

    // --- B. Top Stepped Collar with 3 Iconic Metallic Rivets / Circular Ports ---
    Vector3 topArch = { clankPos.x, clankPos.y + 0.155f, clankPos.z - 0.08f };
    DrawCube({ topArch.x, topArch.y, topArch.z }, 0.29f, 0.065f, 0.038f, chromeBright);
    // 3 Embossed Circular Rivets / Sockets across the top band
    float rivetY = topArch.y + 0.003f;
    float rivetZ = topArch.z - 0.020f;
    // Left Rivet
    DrawCylinderEx({ clankPos.x - 0.088f, rivetY, rivetZ + 0.01f },
                   { clankPos.x - 0.088f, rivetY, rivetZ }, 0.024f, 0.024f, 8, clankTrim);
    DrawSphere({ clankPos.x - 0.088f, rivetY, rivetZ - 0.004f }, 0.018f, chromeBright);
    // Center Rivet
    DrawCylinderEx({ clankPos.x, rivetY + 0.008f, rivetZ + 0.01f },
                   { clankPos.x, rivetY + 0.008f, rivetZ }, 0.027f, 0.027f, 8, clankTrim);
    DrawSphere({ clankPos.x, rivetY + 0.008f, rivetZ - 0.004f }, 0.020f, chromeBright);
    // Right Rivet
    DrawCylinderEx({ clankPos.x + 0.088f, rivetY, rivetZ + 0.01f },
                   { clankPos.x + 0.088f, rivetY, rivetZ }, 0.024f, 0.024f, 8, clankTrim);
    DrawSphere({ clankPos.x + 0.088f, rivetY, rivetZ - 0.004f }, 0.018f, chromeBright);

    // --- C. Large Recessed Dark Vent / Screen in Center of Back ---
    Vector3 ventPos = { clankPos.x, clankPos.y - 0.01f, clankPos.z - 0.084f };
    // Raised chrome frame border
    DrawCube({ ventPos.x, ventPos.y, ventPos.z }, 0.23f, 0.21f, 0.020f, chromeBright);
    DrawCubeWires({ ventPos.x, ventPos.y, ventPos.z }, 0.236f, 0.216f, 0.022f, clankTrim);
    // Recessed dark metallic screen pane (clear Z offset eliminates diagonal clipping on 16-bit depth)
    DrawCube({ ventPos.x, ventPos.y, ventPos.z - 0.020f }, 0.18f, 0.16f, 0.016f, { 22, 26, 34, 255 });
    // Horizontal dark grey vent slats
    for (int slat = -1; slat <= 1; ++slat) {
        float slatY = ventPos.y + slat * 0.042f;
        DrawCube({ ventPos.x, slatY, ventPos.z - 0.036f }, 0.16f, 0.013f, 0.010f, { 52, 60, 74, 255 });
    }

    // --- D. Clank's Rounded Metallic Feet Tucked at Bottom of Backpack ---
    Vector3 footL = { clankPos.x - 0.085f, clankPos.y - 0.20f, clankPos.z - 0.04f };
    Vector3 footR = { clankPos.x + 0.085f, clankPos.y - 0.20f, clankPos.z - 0.04f };
    DrawSphere(footL, 0.058f, chromeBright);
    DrawSphere(footR, 0.058f, chromeBright);
    // Bottom notched rim soles
    DrawCylinderEx({ footL.x, footL.y - 0.025f, footL.z }, { footL.x, footL.y + 0.008f, footL.z }, 0.060f, 0.056f, 8, clankTrim);
    DrawCylinderEx({ footR.x, footR.y - 0.025f, footR.z }, { footR.x, footR.y + 0.008f, footR.z }, 0.060f, 0.056f, 8, clankTrim);

    // --- E. Clank's Head & Face LOOKING DIRECTLY AT CAMERA ---
    float clankHeadNod = airborne ? std::clamp(-player_.vy * 0.007f, -0.04f, 0.04f) : 0.0f;
    Vector3 clankHead = { clankPos.x, clankPos.y + 0.275f + clankHeadNod, clankPos.z - 0.02f };

    // Metallic spherical skull dome (Proportional, natural companion scale!)
    DrawSphere(clankHead, 0.138f, chromeSilv);

    // Beveled cylindrical lower jaw collar
    DrawCylinderEx({ clankHead.x, clankHead.y - 0.085f, clankHead.z - 0.010f },
                   { clankHead.x, clankHead.y - 0.025f, clankHead.z - 0.010f },
                   0.115f, 0.128f, 16, chromeBright);

    // Subtle dark neck joint ring
    DrawCylinderEx({ clankHead.x, clankHead.y - 0.105f, clankHead.z },
                   { clankHead.x, clankHead.y - 0.065f, clankHead.z }, 0.078f, 0.085f, 8, clankTrim);

    // Side cylindrical ear dials
    Vector3 earL = { clankHead.x - 0.138f, clankHead.y + 0.010f, clankHead.z };
    Vector3 earR = { clankHead.x + 0.138f, clankHead.y + 0.010f, clankHead.z };
    DrawCylinderEx({ earL.x - 0.022f, earL.y, earL.z }, { earL.x + 0.015f, earL.y, earL.z }, 0.036f, 0.036f, 8, clankTrim);
    DrawCylinderEx({ earR.x - 0.015f, earR.y, earR.z }, { earR.x + 0.022f, earR.y, earR.z }, 0.036f, 0.036f, 8, clankTrim);
    DrawSphere({ earL.x - 0.022f, earL.y, earL.z }, 0.026f, chromeBright);
    DrawSphere({ earR.x + 0.022f, earR.y, earR.z }, 0.026f, chromeBright);

    // --- F. CLANK'S ANIMATED EMERALD-GREEN EYES & FACIAL EXPRESSIONS ---
    float eyeOffsetX = 0.062f;
    float eyeCenterY = clankHead.y + 0.034f;
    float eyeBaseZ   = clankHead.z - 0.118f;

    Vector3 eyeL = { clankHead.x - eyeOffsetX, eyeCenterY, eyeBaseZ };
    Vector3 eyeR = { clankHead.x + eyeOffsetX, eyeCenterY, eyeBaseZ };

    // 1. Natural periodic blinking (every ~3.4s, quick 0.16s organic blink)
    float blinkTimer = std::fmod(globalTime_ + 0.8f, 3.4f);
    float blinkFrac = 0.0f;
    if (blinkTimer < 0.16f) {
        blinkFrac = std::sin((blinkTimer / 0.16f) * 3.14159265f);
    }
    if (player_.invulnTimer > 0.7f) {
        blinkFrac = std::max(blinkFrac, 0.88f);
    }

    // 2. Contextual Facial Expressions
    float eyeExcitement = (airborne && vy > 2.0f) ? 1.15f : 1.0f;
    float eyeFocus = (player_.heliDeploy > 0.25f) ? (0.22f * player_.heliDeploy) : 0.0f;
    float baseEyeRadius = 0.054f * eyeExcitement;

    Color clankEyeBright = isKit ? Color{ 255, 248, 175, 255 } : Color{ 205, 255, 220, 255 };
    if (player_.hyperBoostTimer > 0.0f) {
        clankEyeGreen = isKit ? Color{ 255, 235, 120, 255 } : Color{ 90, 255, 180, 255 };
    }

    auto DrawClankEye = [&](Vector3 center) {
        // A. Dark circular socket bezel ring surrounding eye (Uses authentic Clank dark trim, not armor accent!)
        DrawCylinderEx({ center.x, center.y, center.z + 0.015f },
                       { center.x, center.y, center.z - 0.022f }, 0.062f, 0.062f, 16, clankTrim);

        // B. Dark inner socket backdrop
        DrawSphere({ center.x, center.y, center.z - 0.010f }, baseEyeRadius * 0.95f, BLACK);

        Vector3 lensPos = { center.x, center.y, center.z - 0.022f };

        if (blinkFrac < 0.85f) {
            // C. Luminous glowing emerald green lens
            DrawSphere(lensPos, baseEyeRadius, clankEyeGreen);
            // D. Brilliant radiant core / pupil
            DrawSphere({ lensPos.x, lensPos.y, lensPos.z - 0.011f }, baseEyeRadius * 0.55f, clankEyeBright);
            // E. Crisp specular reflection glint
            DrawSphere({ lensPos.x - 0.012f, lensPos.y + 0.015f, lensPos.z - 0.022f }, 0.015f, WHITE);

            // F. Animated chrome eyelid shutters
            float closeAmount = std::max(blinkFrac, eyeFocus);
            if (closeAmount > 0.06f) {
                float lidY = center.y + baseEyeRadius * 1.05f - closeAmount * (baseEyeRadius * 1.25f);
                DrawCylinderEx({ center.x - 0.052f, lidY, center.z - 0.028f },
                               { center.x + 0.052f, lidY, center.z - 0.028f },
                               0.024f, 0.024f, 6, chromeBright);
            }
        } else {
            // Fully closed eyelid line
            DrawCylinderEx({ center.x - 0.048f, center.y, center.z - 0.025f },
                           { center.x + 0.048f, center.y, center.z - 0.025f },
                           0.018f, 0.018f, 6, chromeBright);
            DrawCylinderEx({ center.x - 0.034f, center.y, center.z - 0.030f },
                           { center.x + 0.034f, center.y, center.z - 0.030f },
                           0.007f, 0.007f, 4, clankEyeGreen);
        }
    };

    DrawClankEye(eyeL);
    DrawClankEye(eyeR);

    // Secret Agent Clank Bowtie
    if (clkIdx == 2) {
        Vector3 bowCenter = { clankHead.x, clankHead.y - 0.085f, clankHead.z - 0.09f };
        DrawSphere(bowCenter, 0.018f, { 220, 30, 40, 255 });
        DrawCylinderEx({ bowCenter.x - 0.032f, bowCenter.y + 0.010f, bowCenter.z },
                       { bowCenter.x, bowCenter.y, bowCenter.z }, 0.019f, 0.008f, 4, { 200, 25, 35, 255 });
        DrawCylinderEx({ bowCenter.x + 0.032f, bowCenter.y + 0.010f, bowCenter.z },
                       { bowCenter.x, bowCenter.y, bowCenter.z }, 0.019f, 0.008f, 4, { 200, 25, 35, 255 });
    }

    // Subtle dark mouth / smile grille
    float smileCurve = (airborne ? 0.007f : 0.002f);
    DrawCylinderEx({ clankHead.x - 0.038f, clankHead.y - 0.055f + smileCurve, clankHead.z - 0.105f },
                   { clankHead.x + 0.038f, clankHead.y - 0.055f + smileCurve, clankHead.z - 0.105f }, 0.010f, 0.010f, 4, clankTrim);

    // --- G. Top Hatch Collar, Antenna & Organic Red Beacon Disappearance ---
    bool isActiveRun = (mode_ == ScreenMode::PLAYING || mode_ == ScreenMode::PAUSED || mode_ == ScreenMode::GAME_OVER);
    float deploy = isActiveRun ? player_.heliDeploy : 0.0f;
    Vector3 hatchPos = { clankHead.x, clankHead.y + 0.136f, clankHead.z };
    // Circular titanium hatch collar
    DrawCylinderEx(hatchPos, { hatchPos.x, hatchPos.y + 0.018f, hatchPos.z }, 0.058f, 0.052f, 10, clankTrim);

    float antFrac = std::clamp(deploy / 0.15f, 0.0f, 1.0f);
    float antVisible = 1.0f - antFrac;
    if (antVisible > 0.01f) {
        if (isKit) {
            // Kit's signature dual angled horn-like sensor antennae with sunny gold beacon tips
            for (int hSide = -1; hSide <= 1; hSide += 2) {
                float hs = static_cast<float>(hSide);
                Vector3 hornBase = { hatchPos.x + hs * 0.045f, hatchPos.y + 0.014f, hatchPos.z };
                Vector3 hornTip  = { hatchPos.x + hs * 0.088f, hatchPos.y + 0.014f + 0.095f * antVisible, hatchPos.z - 0.015f };
                DrawCylinderEx(hornBase, hornTip, 0.014f * antVisible, 0.007f * antVisible, 6, chromeSilv);
                DrawSphere(hornTip, 0.022f * antVisible, clankBeaconCol);
                DrawSphere(hornTip, 0.011f * antVisible, WHITE);
            }
        } else {
            float stemH = 0.12f * antVisible;
            Vector3 stemTip = { hatchPos.x, hatchPos.y + 0.018f + stemH, hatchPos.z };
            DrawCylinderEx({ hatchPos.x, hatchPos.y + 0.018f, hatchPos.z }, stemTip,
                           0.011f * antVisible, 0.008f * antVisible, 6, chromeSilv);
            // Red glowing beacon bulb at antenna tip
            float redRadius = 0.032f * antVisible;
            Color redCol  = ColorAlpha(clankBeaconCol, antVisible);
            Color redCore = ColorAlpha(WHITE, antVisible * 0.90f);
            DrawSphere(stemTip, redRadius, redCol);
            DrawSphere(stemTip, redRadius * 0.45f, redCore);
        }
    }

    // --- H. Clank's Canonical Backpack Form: Stowed Arms & Dual Shoulder Harness Straps ---
    // 1. Clank's arms resting neatly folded along his torso flanks (Canonical backpack mode)
    for (int side = -1; side <= 1; side += 2) {
        float s = static_cast<float>(side);
        Vector3 clankShoulder = { clankPos.x + s * 0.130f, clankPos.y + 0.07f, clankPos.z };
        Vector3 clankElbow    = { clankPos.x + s * 0.135f, clankPos.y - 0.05f, clankPos.z };
        Vector3 clankWrist    = { clankPos.x + s * 0.110f, clankPos.y - 0.08f, clankPos.z - 0.02f };

        DrawSphere(clankShoulder, 0.025f, clankTrim);
        DrawCylinderEx(clankShoulder, clankElbow, 0.022f, 0.019f, 6, chromeSilv);
        DrawSphere(clankElbow, 0.024f, clankTrim);
        DrawCylinderEx(clankElbow, clankWrist, 0.019f, 0.016f, 6, chromeSilv);
        DrawSphere(clankWrist, 0.022f, chromeBright); // Folded robot hand tucked against chassis
    }

    // 2. Heavy-duty backpack harness straps securing Clank to Ratchet's shoulders & chest
    Color harnessStrapCol = (armIdx == static_cast<int>(ArmorSuitId::CLASSIC_COMMANDO)) ? Color{ 62, 38, 20, 255 } :
                            isRivet ? Color{ 36, 44, 56, 255 } :
                            (armIdx == static_cast<int>(ArmorSuitId::LOMBAX_PRAETORIAN)) ? Color{ 48, 38, 62, 255 } :
                            Color{ 38, 42, 50, 255 };
    for (int side = -1; side <= 1; side += 2) {
        float s = static_cast<float>(side);
        Vector3 strapBack = { clankPos.x + s * 0.095f, clankPos.y + 0.06f, clankPos.z + 0.03f };
        Vector3 strapTop  = { shoulderCenter.x + s * 0.11f, shoulderCenter.y + 0.04f, shoulderCenter.z };
        Vector3 strapFront= { chestCenter.x + s * 0.10f, chestCenter.y + 0.02f, chestCenter.z + 0.08f };

        // Rear strap from backpack to shoulder top
        DrawCylinderEx(strapBack, strapTop, 0.022f, 0.024f, 6, harnessStrapCol);
        // Front strap from shoulder top to chest harness anchor
        DrawCylinderEx(strapTop, strapFront, 0.024f, 0.020f, 6, harnessStrapCol);
        // Polished alloy buckle & clamp on top of each shoulder
        DrawCube(strapTop, 0.050f, 0.020f, 0.040f, (armIdx == static_cast<int>(ArmorSuitId::LOMBAX_PRAETORIAN)) ? armorDark : chromeBright);
        DrawCubeWires(strapTop, 0.052f, 0.022f, 0.042f, clankTrim);
    }

    // --- I. Procedural Clank Heli-Pack Deployment (Smooth mechanical unfolding - NO POP-IN!) ---
    if (deploy > 0.008f) {
        // 1. Telescoping chrome mast extending from Clank's head hatch (0.05m -> 0.70m)
        float mastHeight = Lerp(0.05f, 0.70f, deploy);
        Vector3 mastBase = { hatchPos.x, hatchPos.y + 0.018f, hatchPos.z };
        Vector3 rotorHub = { hatchPos.x, hatchPos.y + 0.018f + mastHeight, hatchPos.z };
        DrawCylinderEx(mastBase, rotorHub, 0.036f, 0.030f, 8, chromeSilv);
        DrawSphere(rotorHub, 0.055f * deploy, clankTrim);

        // 2. Twin titanium blades unfolding from vertical stowage to horizontal flight plane
        float unfoldFrac = std::clamp((deploy - 0.10f) / 0.60f, 0.0f, 1.0f);
        float s = unfoldFrac * unfoldFrac * (3.0f - 2.0f * unfoldFrac);
        float bladeSpan  = Lerp(0.20f, 1.45f, s);
        float bladeTipY  = -0.30f * (1.0f - s);
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
        Color plumeCol = isKit ? ColorAlpha(Color{ 255, 205, 55, 255 }, deploy * 0.90f) : ColorAlpha(SKYBLUE, deploy * 0.90f);
        DrawCylinderEx({ footL.x, footL.y - 0.04f, footL.z },
                       { footL.x, footL.y - 0.04f - thrustLen, footL.z },
                       0.075f * deploy, 0.015f, 8, plumeCol);
        DrawCylinderEx({ footR.x, footR.y - 0.04f, footR.z },
                       { footR.x, footR.y - 0.04f - thrustLen, footR.z },
                       0.075f * deploy, 0.015f, 8, plumeCol);
    }

    // =========================================================================
    // 4. RATCHET'S AUTHENTIC LOMBAX HEAD (3D MESH OR PROCEDURAL FALLBACK)
    // =========================================================================
    Vector3 rightHand = { shoulderCenter.x - 0.48f, shoulderCenter.y - 0.18f, shoulderCenter.z + 0.08f };
    Vector3 wrenchForwardDir = Vector3Normalize({ -0.82f, 0.42f, 0.18f });
    Vector3 wrenchJawPerp    = { 0.0f, 1.0f, 0.0f };

    Vector3 headPos = { neckTop.x, neckTop.y + 0.22f, neckTop.z + 0.02f };

    if (ratchetModelLoaded_) {
        // --- SYMMETRIC TEXTURE BINDING FOR RATCHET & RIVET ---
        if (isRivet && rivetTexturesLoaded_) {
            if (ratchetModel_.materialCount > 0 && rivetHead2Diff_.id != 0)
                ratchetModel_.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = rivetHead2Diff_;
            if (ratchetModel_.materialCount > 1 && rivetFur2_.id != 0)
                ratchetModel_.materials[1].maps[MATERIAL_MAP_DIFFUSE].texture = rivetFur2_;
            if (ratchetModel_.materialCount > 2 && rivetFur_.id != 0)
                ratchetModel_.materials[2].maps[MATERIAL_MAP_DIFFUSE].texture = rivetFur_;
            if (ratchetModel_.materialCount > 3 && rivetEyeDiff_.id != 0)
                ratchetModel_.materials[3].maps[MATERIAL_MAP_DIFFUSE].texture = rivetEyeDiff_;
            if (ratchetModel_.materialCount > 4 && rivetHeadDiff_.id != 0)
                ratchetModel_.materials[4].maps[MATERIAL_MAP_DIFFUSE].texture = rivetHeadDiff_;
        } else if (ratchetTexturesLoaded_) {
            if (ratchetModel_.materialCount > 0 && ratchetHead2Diff_.id != 0)
                ratchetModel_.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = ratchetHead2Diff_;
            if (ratchetModel_.materialCount > 1 && ratchetFur2_.id != 0)
                ratchetModel_.materials[1].maps[MATERIAL_MAP_DIFFUSE].texture = ratchetFur2_;
            if (ratchetModel_.materialCount > 2 && ratchetFur_.id != 0)
                ratchetModel_.materials[2].maps[MATERIAL_MAP_DIFFUSE].texture = ratchetFur_;
            if (ratchetModel_.materialCount > 3 && ratchetEyeDiff_.id != 0)
                ratchetModel_.materials[3].maps[MATERIAL_MAP_DIFFUSE].texture = ratchetEyeDiff_;
            if (ratchetModel_.materialCount > 4 && ratchetHeadDiff_.id != 0)
                ratchetModel_.materials[4].maps[MATERIAL_MAP_DIFFUSE].texture = ratchetHeadDiff_;
        } else {
            for (int m = 0; m < ratchetModel_.materialCount && m < static_cast<int>(defaultModelTextures_.size()); ++m) {
                ratchetModel_.materials[m].maps[MATERIAL_MAP_DIFFUSE].texture = defaultModelTextures_[m];
            }
        }

        // --- AUTHENTIC 3D MESH RATCHET/RIVET HEAD (PS3 A Crack in Time) ---
        float mScale = 0.170f;
        float rotDeg = (tiltDeg * 0.45f);
        Matrix matScale = MatrixScale(mScale, mScale, mScale);
        Matrix matRot   = MatrixRotateY(rotDeg * DEG2RAD);
        Matrix matTrans = MatrixTranslate(neckTop.x, neckTop.y, neckTop.z);
        Matrix matTransform = MatrixMultiply(MatrixMultiply(matScale, matRot), matTrans);

        // Submesh Breakdown:
        // 0: Head_Tail (Opaque head & ears)
        // 1: Ear_Fur (Transparent fur cards)
        // 2: Face_Tail_Fur (Transparent cheek fur cards)
        // 3: Eyes (Opaque emerald-green eyes, sclera, irises, pupils)
        // 4: Cap_Teeth (Opaque aviator cap rivets & stitching)
        // 5: Eyebrow_Fur (Transparent arched eyebrows)
        
        // Pass 1: Solid Opaque Meshes with full Depth Write
        int opaqueSubmeshes[] = { 0, 3, 4 };
        for (int idx : opaqueSubmeshes) {
            if (idx < ratchetModel_.meshCount) {
                DrawMesh(ratchetModel_.meshes[idx], ratchetModel_.materials[ratchetModel_.meshMaterial[idx]], matTransform);
            }
        }

        // Pass 2: Transparent Fur Cards with Depth Write Disabled (no dark halos, black cutouts, or depth holes!)
        rlDisableDepthMask();
        int transparentSubmeshes[] = { 1, 2, 5 };
        for (int idx : transparentSubmeshes) {
            if (idx < ratchetModel_.meshCount) {
                DrawMesh(ratchetModel_.meshes[idx], ratchetModel_.materials[ratchetModel_.meshMaterial[idx]], matTransform);
            }
        }
        rlEnableDepthMask();
    } else {
        Color furCream   = { 246, 230, 198, 255 }; // Light cream muzzle fur
        Color noseBlack  = { 24, 20, 26, 255 };    // Dark leather nose button
        Color eyeSclera  = { 252, 252, 255, 255 }; // Pure white eye sclera
        Color eyeGreen   = isRivet ? Color{ 45, 190, 255, 255 } : Color{ 36, 196, 84, 255 };
        Color eyeBright  = isRivet ? Color{ 135, 230, 255, 255 } : Color{ 108, 246, 150, 255 };
        Color eyePupil   = { 16, 18, 22, 255 };    // Deep dark pupil
        Color mouthLine  = { 110, 52, 26, 255 };   // Warm brown smile seam
        Color earPeach   = isRivet ? Color{ 205, 185, 225, 255 } : Color{ 228, 175, 148, 255 };

        // A. Golden Lombax Head Dome & Cheek Fur Tufts (Bare Head matching Image Reference)
        DrawSphere(headPos, 0.300f, furGold);
        DrawSphere({ headPos.x - 0.13f, headPos.y - 0.05f, headPos.z + 0.07f }, 0.15f, furGold);
        DrawSphere({ headPos.x + 0.13f, headPos.y - 0.05f, headPos.z + 0.07f }, 0.15f, furGold);

        // B. Forehead Fur Crest & 3 Iconic Tiger Stripes (Top of head between ears)
        DrawCylinderEx({ headPos.x, headPos.y + 0.22f, headPos.z + 0.02f },
                       { headPos.x, headPos.y + 0.34f, headPos.z - 0.06f }, 0.055f, 0.018f, 6, furGold);
        // 3 Dark-brown vertical stripes running down the forehead
        DrawCylinderEx({ headPos.x, headPos.y + 0.14f, headPos.z + 0.24f },
                       { headPos.x, headPos.y + 0.29f, headPos.z + 0.04f }, 0.026f, 0.018f, 4, furStripe);
        DrawCylinderEx({ headPos.x - 0.070f, headPos.y + 0.14f, headPos.z + 0.22f },
                       { headPos.x - 0.065f, headPos.y + 0.27f, headPos.z + 0.04f }, 0.020f, 0.014f, 4, furStripe);
        DrawCylinderEx({ headPos.x + 0.070f, headPos.y + 0.14f, headPos.z + 0.22f },
                       { headPos.x + 0.065f, headPos.y + 0.27f, headPos.z + 0.04f }, 0.020f, 0.014f, 4, furStripe);

        // C. Iconic Cream Muzzle, Whisker Pads, Chin & Black Leather Nose (Reference Image)
        // Snout bridge connecting between eyes to muzzle cushions
        DrawCylinderEx({ headPos.x, headPos.y + 0.025f, headPos.z + 0.230f },
                       { headPos.x, headPos.y - 0.030f, headPos.z + 0.315f }, 0.075f, 0.055f, 8, furCream);
        // Rounded cream whisker cushions under the eyes
        DrawSphere({ headPos.x - 0.062f, headPos.y - 0.045f, headPos.z + 0.285f }, 0.072f, furCream);
        DrawSphere({ headPos.x + 0.062f, headPos.y - 0.045f, headPos.z + 0.285f }, 0.072f, furCream);
        // Cream chin
        DrawSphere({ headPos.x, headPos.y - 0.115f, headPos.z + 0.245f }, 0.068f, furCream);

        // Cute Black Leather Button Nose with specular shine glint
        Vector3 nosePos = { headPos.x, headPos.y - 0.020f, headPos.z + 0.345f };
        DrawSphere(nosePos, 0.028f, noseBlack);
        DrawSphere({ nosePos.x - 0.007f, nosePos.y + 0.007f, nosePos.z + 0.015f }, 0.008f, { 110, 105, 115, 255 });

        // Mischievous Lombax Smirk (curving slightly upward on right)
        DrawCylinderEx({ headPos.x, nosePos.y - 0.018f, nosePos.z - 0.010f },
                       { headPos.x, nosePos.y - 0.048f, nosePos.z - 0.015f }, 0.006f, 0.006f, 4, mouthLine);
        DrawCylinderEx({ headPos.x - 0.045f, nosePos.y - 0.052f, nosePos.z - 0.030f },
                       { headPos.x,          nosePos.y - 0.054f, nosePos.z - 0.015f }, 0.007f, 0.007f, 4, mouthLine);
        DrawCylinderEx({ headPos.x,          nosePos.y - 0.054f, nosePos.z - 0.015f },
                       { headPos.x + 0.052f, nosePos.y - 0.040f, nosePos.z - 0.030f }, 0.007f, 0.007f, 4, mouthLine);

        // D. RATCHET'S BIG EXPRESSIVE EMERALD-GREEN LOMBAX EYES (MATCHING REFERENCE EXACTLY!)
        float eyeX   = 0.108f;
        float eyeY   = headPos.y + 0.060f;
        float eyeZ   = headPos.z + 0.260f;
        float eyeRad = 0.078f;

        for (int side = -1; side <= 1; side += 2) {
            float s = static_cast<float>(side);
            Vector3 eyePos = { headPos.x + s * eyeX, eyeY, eyeZ };

            // 1. Dark almond socket contour & eyeliner
            DrawSphere({ eyePos.x, eyePos.y, eyePos.z - 0.004f }, eyeRad + 0.004f, furStripe);

            // 2. Pure White Sclera
            DrawSphere(eyePos, eyeRad, eyeSclera);

            // 3. Radiant Emerald-Green Lombax Iris
            Vector3 irisPos = { eyePos.x, eyePos.y, eyePos.z + 0.032f };
            DrawSphere(irisPos, 0.054f, eyeGreen);
            // Radiant inner green gradient
            DrawSphere({ irisPos.x, irisPos.y, irisPos.z + 0.008f }, 0.036f, eyeBright);

            // 4. Deep Black Pupil
            DrawSphere({ irisPos.x, irisPos.y, irisPos.z + 0.015f }, 0.026f, eyePupil);

            // 5. Specular highlight sparkles (Pixar / Insomniac look!)
            DrawSphere({ irisPos.x + 0.014f, irisPos.y + 0.016f, irisPos.z + 0.026f }, 0.013f, WHITE);
            DrawSphere({ irisPos.x - 0.011f, irisPos.y - 0.012f, irisPos.z + 0.026f }, 0.007f, WHITE);

            // 6. Arched dark-brown Lombax Eyebrows
            Vector3 browIn  = { headPos.x + s * 0.038f, eyePos.y + 0.078f, eyePos.z + 0.025f };
            Vector3 browMid = { headPos.x + s * 0.118f, eyePos.y + 0.096f, eyePos.z + 0.020f };
            Vector3 browOut = { headPos.x + s * 0.185f, eyePos.y + 0.064f, eyePos.z - 0.010f };
            DrawCylinderEx(browIn, browMid, 0.020f, 0.018f, 6, furStripe);
            DrawCylinderEx(browMid, browOut, 0.018f, 0.010f, 6, furStripe);
        }

        // E. True Upright Pointed Lombax Ears (52° upward angle matching Image Reference)
        float earBob = std::sin(globalTime_ * 20.0f) * 0.025f - std::clamp(player_.vy * 0.010f, -0.12f, 0.12f);
        for (int side = -1; side <= 1; side += 2) {
            float s = static_cast<float>(side);
            Vector3 earTip   = { headPos.x + s * 0.52f, headPos.y + 0.48f + earBob, headPos.z - 0.06f };
            Vector3 rootTop  = { headPos.x + s * 0.22f, headPos.y + 0.16f, headPos.z - 0.01f };
            Vector3 rootMid  = { headPos.x + s * 0.24f, headPos.y + 0.08f, headPos.z - 0.03f };
            Vector3 rootBot  = { headPos.x + s * 0.22f, headPos.y + 0.00f, headPos.z - 0.04f };

            // 1. Dark-brown/auburn upper ear rim tapering to a sharp point
            DrawCylinderEx(rootTop, earTip, 0.060f, 0.016f, 6, furStripe);
            // 2. Golden fur middle & lower ear blade
            DrawCylinderEx(rootMid, earTip, 0.078f, 0.014f, 6, furGold);
            DrawCylinderEx(rootBot, Vector3Lerp(rootBot, earTip, 0.88f), 0.068f, 0.016f, 6, furGold);
            // 3. Warm peach inner ear cup
            Vector3 innerRoot = { headPos.x + s * 0.24f, headPos.y + 0.09f, headPos.z + 0.00f };
            Vector3 innerTip  = Vector3Lerp(innerRoot, earTip, 0.82f);
            DrawCylinderEx(innerRoot, innerTip, 0.048f, 0.012f, 6, earPeach);

            // 4. Tiger stripes along the back of the ear
            Vector3 st1Top = Vector3Lerp(rootTop, earTip, 0.35f);
            Vector3 st1Bot = Vector3Lerp(rootBot, earTip, 0.30f);
            st1Top.z -= 0.025f; st1Bot.z -= 0.025f;
            DrawCylinderEx(st1Top, st1Bot, 0.028f, 0.020f, 4, furStripe);

            Vector3 st2Top = Vector3Lerp(rootTop, earTip, 0.65f);
            Vector3 st2Bot = Vector3Lerp(rootBot, earTip, 0.60f);
            st2Top.z -= 0.025f; st2Bot.z -= 0.025f;
            DrawCylinderEx(st2Top, st2Bot, 0.024f, 0.016f, 4, furStripe);
        }
    }

    // (Section 4F: Head accessories omitted so the authentic 3D PS3 head model remains 100% clean and free of floating detached objects!)

    // =========================================================================
    // 5. CLEAN, NATURAL ARMS WITH SIDE-MOUNTED GUN AIMING & RECOIL!
    // =========================================================================
    Vector3 leftShoulder  = { shoulderCenter.x + 0.23f, shoulderCenter.y - 0.02f, shoulderCenter.z };
    Vector3 rightShoulder = { shoulderCenter.x - 0.23f, shoulderCenter.y - 0.02f, shoulderCenter.z };
    DrawSphere(leftShoulder,  0.082f, armorOrange);
    DrawSphere(rightShoulder, 0.082f, armorOrange);

    // Smooth alternating skater balance arm swing
    float swingL = std::sin(globalTime_ * 8.0f) * 0.06f;
    float swingR = std::cos(globalTime_ * 8.0f) * 0.06f;

    Vector3 leftElbow = {
        leftShoulder.x + 0.18f,
        leftShoulder.y - 0.20f + swingL * 0.5f,
        leftShoulder.z - 0.03f
    };
    Vector3 leftHand = {
        leftShoulder.x + 0.36f,
        leftShoulder.y - 0.28f + swingL,
        leftShoulder.z + 0.12f
    };

    Vector3 surfRightElbow = {
        rightShoulder.x - 0.18f,
        rightShoulder.y - 0.20f + swingR * 0.5f,
        rightShoulder.z - 0.03f
    };
    Vector3 surfRightHand = {
        rightShoulder.x - 0.36f,
        rightShoulder.y - 0.28f + swingR,
        rightShoulder.z + 0.10f
    };

    Vector3 aimRightElbow = {
        rightShoulder.x - 0.18f,
        rightShoulder.y - 0.12f - recoilKick * 0.04f,
        rightShoulder.z + 0.06f - recoilKick * 0.16f
    };
    Vector3 aimRightHand = {
        rightShoulder.x - 0.22f,
        rightShoulder.y + 0.02f + recoilKick * 0.12f,
        rightShoulder.z + 0.34f - recoilKick * 0.22f
    };

    Vector3 rightElbow = Vector3Lerp(surfRightElbow, aimRightElbow, aimBlend);
    rightHand          = Vector3Lerp(surfRightHand,  aimRightHand,  aimBlend);

    if (airborne && wrenchTimer <= 0.0f && player_.gunAimTimer <= 0.0f) {
        float deploy = player_.heliDeploy;
        if (deploy > 0.22f) {
            float gripFrac = (deploy - 0.22f) / 0.78f;
            Vector3 gripLeftHand   = { leftShoulder.x - 0.08f, leftShoulder.y + 0.16f, leftShoulder.z + 0.06f };
            Vector3 gripRightHand  = { rightShoulder.x + 0.08f, rightShoulder.y + 0.16f, rightShoulder.z + 0.06f };
            Vector3 gripLeftElbow  = { leftShoulder.x + 0.14f, leftShoulder.y + 0.06f, leftShoulder.z };
            Vector3 gripRightElbow = { rightShoulder.x - 0.14f, rightShoulder.y + 0.06f, rightShoulder.z };
            leftHand   = Vector3Lerp(leftHand, gripLeftHand, gripFrac);
            rightHand  = Vector3Lerp(rightHand, gripRightHand, gripFrac);
            leftElbow  = Vector3Lerp(leftElbow, gripLeftElbow, gripFrac);
            rightElbow = Vector3Lerp(rightElbow, gripRightElbow, gripFrac);
        }
    }

    // Orthogonal coordinate frame relative to aiming direction
    Vector3 gunDir = Vector3Normalize({
        (1.0f - aimBlend) * (-0.12f),
        recoilKick * 0.22f - (1.0f - aimBlend) * 0.12f,
        1.0f
    });
    Vector3 gunUp = Vector3Normalize({ 0.0f, 1.0f, -gunDir.y });
    Vector3 gunRight = Vector3Normalize(Vector3CrossProduct(gunDir, gunUp));

    auto DrawCleanArm = [&](Vector3 sh, Vector3 elb, Vector3 hnd, int side) {
        float s = static_cast<float>(side);
        if (side == -1 && isRivet) {
            // =====================================================================
            // RIVET'S SIGNATURE YELLOW CYBERNETIC MECHANICAL PROSTHETIC ARM
            // =====================================================================
            Color mechYellow = { 235, 185, 30, 255 };  // Rivet's hazard-yellow chassis
            Color mechJoint  = { 38, 42, 48, 255 };    // Dark gunmetal / carbon joints
            Color mechChrome = { 195, 205, 218, 255 }; // Chrome hydraulic piston rods
            Color cyanCore   = { 60, 215, 255, 255 };  // Glowing blue power conduit

            // Upper arm: Heavy yellow industrial plating over exposed chrome hydraulic cylinder
            DrawCylinderEx(sh, elb, 0.078f, 0.068f, 8, mechYellow);
            DrawCylinderEx(Vector3Lerp(sh, elb, 0.20f), Vector3Lerp(sh, elb, 0.85f), 0.030f, 0.030f, 6, mechChrome);
            DrawSphere(sh, 0.084f, mechJoint);
            DrawSphere({ sh.x - 0.025f, sh.y, sh.z }, 0.040f, mechChrome);

            // Elbow: Articulated dual-disc rotary servo joint
            DrawSphere(elb, 0.072f, mechJoint);
            DrawCylinderEx({ elb.x - 0.035f, elb.y, elb.z }, { elb.x + 0.035f, elb.y, elb.z }, 0.052f, 0.052f, 8, mechYellow);
            DrawSphere({ elb.x - 0.036f, elb.y, elb.z }, 0.028f, mechChrome);

            // Forearm: Dual-segment yellow cybernetic chassis with power conduits
            DrawCylinderEx(elb, hnd, 0.072f, 0.086f, 8, mechYellow);
            Vector3 midArm = Vector3Lerp(elb, hnd, 0.50f);
            DrawCylinderEx(Vector3Lerp(elb, hnd, 0.22f), Vector3Lerp(elb, hnd, 0.78f), 0.024f, 0.024f, 6, mechJoint);
            DrawCylinderEx(Vector3Lerp(elb, hnd, 0.28f), Vector3Lerp(elb, hnd, 0.72f), 0.012f, 0.012f, 4, cyanCore);
            DrawSphere(midArm, 0.020f, cyanCore);
            DrawSphere(midArm, 0.010f, WHITE);

            // Heavy industrial wrist clamp
            Vector3 wristPos = Vector3Lerp(elb, hnd, 0.88f);
            DrawCylinderEx(wristPos, hnd, 0.088f, 0.080f, 8, mechJoint);

            // Cybernetic Hand & Articulated Mechanical Fingers
            DrawSphere(hnd, 0.066f, mechJoint);
            DrawSphere({ hnd.x - 0.022f, hnd.y, hnd.z }, 0.038f, mechYellow);

            // Chrome articulated thumb
            Vector3 thumbTip = Vector3Add(hnd, Vector3Add(Vector3Scale(gunUp, 0.028f), Vector3Scale(gunRight, 0.042f)));
            DrawCylinderEx(hnd, thumbTip, 0.026f, 0.020f, 6, mechChrome);
            DrawSphere(thumbTip, 0.018f, mechYellow);

            // Chrome articulated mechanical fingers with yellow knuckle pads
            for (int f = 0; f < 3; ++f) {
                float fY = -0.012f - f * 0.024f;
                Vector3 knuckle = Vector3Add(hnd, Vector3Add(Vector3Scale(gunUp, fY), Vector3Scale(gunRight, -0.030f)));
                Vector3 fTip    = Vector3Add(knuckle, Vector3Add(Vector3Scale(gunDir, 0.048f), Vector3Scale(gunRight, 0.038f)));
                DrawSphere(knuckle, 0.020f, mechYellow);
                DrawCylinderEx(knuckle, fTip, 0.020f, 0.016f, 6, mechChrome);
                DrawSphere(fTip, 0.016f, mechYellow);
            }
            // Trigger finger
            Vector3 trigA = Vector3Add(hnd, Vector3Add(Vector3Scale(gunUp, 0.022f), Vector3Scale(gunRight, -0.028f)));
            Vector3 trigB = Vector3Add(trigA, Vector3Scale(gunDir, 0.062f));
            DrawCylinderEx(trigA, trigB, 0.018f, 0.014f, 6, mechChrome);
            return;
        }

        // Upper arm (Sleeve / Armor plate + Fur)
        Color sleeveCol = (armIdx == static_cast<int>(ArmorSuitId::CLASSIC_COMMANDO)) ? furGold :
                          (armIdx == static_cast<int>(ArmorSuitId::LOMBAX_PRAETORIAN)) ? pantsBlue : armorOrange;
        DrawCylinderEx(sh, elb, 0.075f, 0.065f, 8, sleeveCol);
        // Sleeve cuff / shoulder bracket
        DrawCylinderEx(sh, Vector3Lerp(sh, elb, 0.35f), 0.078f, 0.074f, 8,
                       (armIdx == static_cast<int>(ArmorSuitId::CLASSIC_COMMANDO)) ? armorOrange : armorDark);
        DrawSphere(elb, 0.068f, (armIdx == static_cast<int>(ArmorSuitId::CLASSIC_COMMANDO)) ? furGold : armorDark);

        // Forearm Gauntlet / Bracer
        Color forearmCol = (armIdx == static_cast<int>(ArmorSuitId::CLASSIC_COMMANDO)) ? leatherBrn :
                           (armIdx == static_cast<int>(ArmorSuitId::LOMBAX_PRAETORIAN)) ? armDef.primaryColor :
                           (armIdx == static_cast<int>(ArmorSuitId::CARBONOX)) ? armorOrange : armorDark;
        DrawCylinderEx(elb, hnd, 0.068f, 0.082f, 8, forearmCol);

        // Custom gauntlet accents per skin
        Vector3 midArm = Vector3Lerp(elb, hnd, 0.50f);
        if (armIdx == static_cast<int>(ArmorSuitId::CLASSIC_COMMANDO)) {
            // Classic: Aviator bracer leather strap ring with brass buckle
            DrawCylinderEx(Vector3Lerp(elb, hnd, 0.38f), Vector3Lerp(elb, hnd, 0.46f), 0.075f, 0.077f, 8, armorDark);
            DrawSphere({ midArm.x + s * 0.045f, midArm.y, midArm.z }, 0.016f, GOLD);
        } else if (armIdx == static_cast<int>(ArmorSuitId::CARBONOX)) {
            // Carbonox: Glowing cyan nanotech LED strip along outer arm
            Vector3 ledP = { midArm.x + s * 0.065f, midArm.y, midArm.z };
            DrawSphere(ledP, 0.022f, visorCol);
            DrawSphere(ledP, 0.010f, WHITE);
        } else if (isRivet) {
            // Rivet: Left arm dark teal flight sleeve with reinforced utility communicator
            DrawCylinderEx(Vector3Lerp(elb, hnd, 0.35f), Vector3Lerp(elb, hnd, 0.55f), 0.076f, 0.078f, 8, armorDark);
            Vector3 commP = { midArm.x + s * 0.055f, midArm.y, midArm.z };
            DrawSphere(commP, 0.018f, visorCol);
        } else if (armIdx == static_cast<int>(ArmorSuitId::INFERNOX)) {
            // Infernox: Glowing molten magma vent along basalt heat bracer
            Vector3 magP = { midArm.x + s * 0.065f, midArm.y, midArm.z };
            DrawSphere(magP, 0.026f, visorCol);
            DrawSphere({ magP.x, magP.y, magP.z + 0.01f }, 0.012f, GOLD);
        } else if (armIdx == static_cast<int>(ArmorSuitId::DEADLOCKED_MARAUDER)) {
            // Deadlocked Marauder: Spiked gladiator forearm plate
            DrawCylinderEx(Vector3Lerp(elb, hnd, 0.25f), Vector3Lerp(elb, hnd, 0.65f), 0.082f, 0.088f, 8, armorOrange);
            Vector3 spk = { midArm.x + s * 0.075f, midArm.y, midArm.z };
            DrawCylinderEx(spk, { spk.x + s * 0.045f, spk.y + 0.02f, spk.z }, 0.016f, 0.004f, 4, armorDark);
        } else if (armIdx == static_cast<int>(ArmorSuitId::LOMBAX_PRAETORIAN)) {
            // Lombax Praetorian: Dual 24K Gold filigree bands & glowing violet psionic matrix gem
            DrawCylinderEx(Vector3Lerp(elb, hnd, 0.28f), Vector3Lerp(elb, hnd, 0.40f), 0.078f, 0.080f, 8, armorDark);
            DrawCylinderEx(Vector3Lerp(elb, hnd, 0.60f), Vector3Lerp(elb, hnd, 0.72f), 0.084f, 0.086f, 8, armorDark);
            Vector3 gemP = { midArm.x + s * 0.065f, midArm.y, midArm.z };
            DrawSphere(gemP, 0.024f, visorCol);
            DrawSphere({ gemP.x + s * 0.005f, gemP.y, gemP.z }, 0.010f, WHITE);
        }

        // Hand & Glove
        Color handCol = (armIdx == static_cast<int>(ArmorSuitId::CLASSIC_COMMANDO)) ? gloveBrn :
                        (armIdx == static_cast<int>(ArmorSuitId::LOMBAX_PRAETORIAN)) ? armDef.primaryColor : armorDark;
        if (side == 1) {
            // Left Hand: Relaxed athletic glove
            DrawSphere(hnd, 0.075f, handCol);
            DrawSphere({ hnd.x + 0.022f, hnd.y - 0.018f, hnd.z + 0.028f }, 0.034f, handCol);
        } else {
            // Right Hand: Articulated Lombax Combat Glove wrapping around weapon handle!
            DrawSphere(hnd, 0.065f, handCol);
            // Thumb gripping inward
            Vector3 thumbTip = Vector3Add(hnd, Vector3Add(Vector3Scale(gunUp, 0.028f), Vector3Scale(gunRight, 0.042f)));
            DrawCylinderEx(hnd, thumbTip, 0.028f, 0.022f, 6, handCol);
            // Curled fingers wrapping the front of the grip
            for (int f = 0; f < 3; ++f) {
                float fY = -0.012f - f * 0.024f;
                Vector3 knuckle = Vector3Add(hnd, Vector3Add(Vector3Scale(gunUp, fY), Vector3Scale(gunRight, -0.030f)));
                Vector3 fTip    = Vector3Add(knuckle, Vector3Add(Vector3Scale(gunDir, 0.048f), Vector3Scale(gunRight, 0.038f)));
                DrawCylinderEx(knuckle, fTip, 0.022f, 0.018f, 6, handCol);
                DrawSphere(fTip, 0.018f, handCol);
            }
            // Trigger finger
            Vector3 trigA = Vector3Add(hnd, Vector3Add(Vector3Scale(gunUp, 0.022f), Vector3Scale(gunRight, -0.028f)));
            Vector3 trigB = Vector3Add(trigA, Vector3Scale(gunDir, 0.062f));
            DrawCylinderEx(trigA, trigB, 0.020f, 0.016f, 6, handCol);

            if (armIdx == static_cast<int>(ArmorSuitId::LOMBAX_PRAETORIAN)) {
                DrawSphere({ hnd.x - 0.025f, hnd.y, hnd.z + 0.025f }, 0.035f, armorDark);
            }
        }
    };
    DrawCleanArm(leftShoulder,  leftElbow,  leftHand,  1);
    DrawCleanArm(rightShoulder, rightElbow, rightHand, -1);

    // Dynamic right-hand positioning for weapon swing/aiming across both 3D mesh and fallback
    bool wrenchSwinging = (wrenchTimer > 0.0f);
    float swingProg = wrenchSwinging ? (1.0f - std::clamp(wrenchTimer / 0.38f, 0.0f, 1.0f)) : 0.0f;
    if (wrenchSwinging) {
        float s = swingProg * swingProg * (3.0f - 2.0f * swingProg);
        float sweepAngle = -0.15f + s * 3.30f;
        float dirX = -std::cos(sweepAngle);
        float dirZ =  std::sin(sweepAngle);
        rightHand = {
            shoulderCenter.x + dirX * 0.60f,
            shoulderCenter.y - 0.04f,
            shoulderCenter.z + std::max(0.14f, dirZ * 0.58f)
        };
        wrenchForwardDir = Vector3Normalize({ dirX, 0.05f - s * 0.10f, dirZ });
    }

    // =========================================================================
    // 6. 6 CUSTOM HIGH-FIDELITY 3D WEAPON MODELS (AUTHENTIC INSOMNIAC GADGETRON ARSENAL)
    // =========================================================================
    WeaponId wEnum = static_cast<WeaponId>(weaponId);
    bool isGolden = (weaponId >= 0 && weaponId < static_cast<int>(NUM_WEAPONS)) && save_.goldenWeapons[weaponId];
    Color weapCol = isGolden ? GOLDEN_WEAPON_CATALOG[weaponId].projectileColor : GetWeaponCatalog()[weaponId].projectileColor;
    if (isGolden) {
        armorDark = GOLDEN_WEAPON_CATALOG[weaponId].goldPrimary;
        armorOrange = GOLDEN_WEAPON_CATALOG[weaponId].goldTrim;
    }
    // Elevated receiver mount sitting naturally atop Ratchet's articulated combat grip
    Vector3 gunBreech = Vector3Add(Vector3Subtract(rightHand, Vector3Scale(gunDir, 0.08f)),
                                  Vector3Scale(gunUp, 0.072f));
    Vector3 gunMuzzle = Vector3Add(gunBreech, Vector3Scale(gunDir, 0.82f));

    if (wEnum == WeaponId::OMNIWRENCH_8000 || wrenchSwinging) {
        // --- 1. OMNIWRENCH 8000 (Machined Fluted Shaft, Ergonomic Grip, Heavy Jaws & Slash Arc) ---
        Vector3 wDir = wrenchForwardDir;
        Vector3 wPerp = wrenchJawPerp;
        Vector3 wGripBot = Vector3Subtract(rightHand, Vector3Scale(wDir, 0.16f));
        Vector3 wNeck    = Vector3Add(rightHand, Vector3Scale(wDir, 0.58f));

        // Textured knurled handle grip
        DrawCylinderEx(wGripBot, rightHand, 0.046f, 0.048f, 8, armorDark);
        DrawCylinderEx(Vector3Lerp(wGripBot, rightHand, 0.25f), Vector3Lerp(wGripBot, rightHand, 0.75f), 0.052f, 0.052f, 8, { 52, 44, 38, 255 });
        // Weighted pommel base with kinetic blue core
        DrawSphere(wGripBot, 0.056f, chromeSilv);
        DrawSphere(Vector3Subtract(wGripBot, Vector3Scale(wDir, 0.02f)), 0.024f, SKYBLUE);

        // Fluted chrome main shaft
        DrawCylinderEx(rightHand, wNeck, 0.048f, 0.058f, 8, chromeSilv);
        DrawCylinderEx(Vector3Lerp(rightHand, wNeck, 0.45f), Vector3Lerp(rightHand, wNeck, 0.55f), 0.062f, 0.062f, 8, chromeBright);

        // Heavy Hexagonal Wrench Jaw Crossbar
        Vector3 jawBaseL = Vector3Add(wNeck, Vector3Scale(wPerp, -0.18f));
        Vector3 jawBaseR = Vector3Add(wNeck, Vector3Scale(wPerp, +0.18f));
        DrawCylinderEx(jawBaseL, jawBaseR, 0.068f, 0.068f, 6, armorDark);
        // Bolt locking nuts on left and right
        DrawSphere(jawBaseL, 0.048f, chromeBright);
        DrawSphere(jawBaseR, 0.048f, chromeBright);

        // Twin Open Parallel Crescent Wrench Prongs
        Vector3 jawTipL = Vector3Add(jawBaseL, Vector3Scale(wDir, 0.28f));
        Vector3 jawTipR = Vector3Add(jawBaseR, Vector3Scale(wDir, 0.28f));
        DrawCylinderEx(jawBaseL, jawTipL, 0.066f, 0.038f, 6, chromeSilv);
        DrawCylinderEx(jawBaseR, jawTipR, 0.066f, 0.038f, 6, chromeSilv);
        // Striking contact edges
        DrawSphere(jawTipL, 0.034f, chromeBright);
        DrawSphere(jawTipR, 0.034f, chromeBright);
        DrawSphere(jawTipL, 0.018f, SKYBLUE);
        DrawSphere(jawTipR, 0.018f, SKYBLUE);

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
        // --- 2. COMBUSTER (Iconic Gadgetron Orange Blaster Pistol with Plasma Chamber & Vented Barrel) ---
        gunMuzzle = Vector3Add(gunBreech, Vector3Scale(gunDir, 0.76f));
        Vector3 gripTop = Vector3Subtract(gunBreech, Vector3Scale(gunUp, 0.035f));
        Vector3 gripBot = Vector3Add(rightHand, Vector3Add(Vector3Scale(gunUp, -0.16f), Vector3Scale(gunDir, -0.04f)));
        // Seamless ergonomic pistol grip connecting receiver through Ratchet's hand
        DrawCylinderEx(gripTop, gripBot, 0.040f, 0.034f, 6, armorDark);
        // Gadgetron trigger guard loop
        Vector3 guardF = Vector3Add(rightHand, Vector3Add(Vector3Scale(gunDir, 0.058f), Vector3Scale(gunUp, 0.024f)));
        DrawCylinderEx(Vector3Add(gripTop, Vector3Scale(gunDir, 0.025f)), guardF, 0.011f, 0.011f, 4, armorDark);
        DrawCylinderEx(guardF, Vector3Subtract(rightHand, Vector3Scale(gunUp, 0.035f)), 0.011f, 0.011f, 4, armorDark);

        // Gadgetron orange receiver chassis
        DrawCylinderEx(gunBreech, Vector3Lerp(gunBreech, gunMuzzle, 0.65f), 0.095f, 0.080f, 8, { 235, 105, 20, 255 });
        // Upper titanium slide rib
        DrawCylinderEx(Vector3Add(gunBreech, Vector3Scale(gunUp, 0.065f)),
                       Vector3Add(Vector3Lerp(gunBreech, gunMuzzle, 0.75f), Vector3Scale(gunUp, 0.065f)),
                       0.048f, 0.042f, 6, chromeSilv);

        // Illuminated Plasma Accelerator Chamber
        Vector3 coilCenter = Vector3Lerp(gunBreech, gunMuzzle, 0.38f);
        DrawSphere(coilCenter, 0.052f, { 255, 150, 20, 255 });
        DrawSphere(coilCenter, 0.028f, YELLOW);
        DrawSphere(coilCenter, 0.012f, WHITE);

        // Extended primary barrel & underbarrel power cylinder
        DrawCylinderEx(Vector3Lerp(gunBreech, gunMuzzle, 0.50f), gunMuzzle, 0.052f, 0.046f, 8, chromeSilv);
        DrawCylinderEx(Vector3Add(Vector3Lerp(gunBreech, gunMuzzle, 0.40f), Vector3Scale(gunUp, -0.045f)),
                       Vector3Add(Vector3Lerp(gunBreech, gunMuzzle, 0.85f), Vector3Scale(gunUp, -0.045f)),
                       0.034f, 0.030f, 6, armorDark);

        // Vented rectangular muzzle brake with glowing emitter ring
        DrawCube(gunMuzzle, 0.095f, 0.090f, 0.065f, armorDark);
        DrawSphere(gunMuzzle, 0.042f, YELLOW);
        DrawSphere(Vector3Add(gunMuzzle, Vector3Scale(gunDir, 0.02f)), 0.020f, WHITE);
    } else if (wEnum == WeaponId::SHOCK_BLASTER) {
        // --- 3. SHOCK BLASTER (Dual-Barrel Scattergun with High-Voltage Cyan Arc Coils) ---
        gunMuzzle = Vector3Add(gunBreech, Vector3Scale(gunDir, 0.96f));
        Vector3 gripTop = Vector3Subtract(gunBreech, Vector3Scale(gunUp, 0.040f));
        Vector3 gripBot = Vector3Add(rightHand, Vector3Add(Vector3Scale(gunUp, -0.16f), Vector3Scale(gunDir, -0.05f)));
        DrawCylinderEx(gripTop, gripBot, 0.044f, 0.038f, 6, armorDark);

        // Heavy receiver housing in dark obsidian steel
        Vector3 midBreech = Vector3Lerp(gunBreech, gunMuzzle, 0.35f);
        DrawCylinderEx(gunBreech, midBreech, 0.135f, 0.125f, 8, { 32, 36, 46, 255 });
        // Textured pump-action forend
        Vector3 pumpA = Vector3Add(Vector3Lerp(gunBreech, gunMuzzle, 0.25f), Vector3Scale(gunUp, -0.05f));
        Vector3 pumpB = Vector3Add(Vector3Lerp(gunBreech, gunMuzzle, 0.55f), Vector3Scale(gunUp, -0.05f));
        DrawCylinderEx(pumpA, pumpB, 0.068f, 0.068f, 6, { 52, 58, 70, 255 });

        // Twin Side-by-Side Heavy Chrome Barrels
        Vector3 barrelLeftA  = Vector3Add(midBreech, Vector3Scale(gunRight, -0.058f));
        Vector3 barrelRightA = Vector3Add(midBreech, Vector3Scale(gunRight, +0.058f));
        Vector3 barrelLeftB  = Vector3Add(gunMuzzle, Vector3Scale(gunRight, -0.058f));
        Vector3 barrelRightB = Vector3Add(gunMuzzle, Vector3Scale(gunRight, +0.058f));
        DrawCylinderEx(barrelLeftA,  barrelLeftB,  0.054f, 0.050f, 8, chromeSilv);
        DrawCylinderEx(barrelRightA, barrelRightB, 0.054f, 0.050f, 8, chromeSilv);

        // Triple High-Voltage Cyan Arc Capacitor Rings
        for (int c = 0; c < 3; ++c) {
            float frac = 0.42f + c * 0.14f;
            Vector3 cPos = Vector3Lerp(gunBreech, gunMuzzle, frac);
            DrawCylinderEx(Vector3Subtract(cPos, Vector3Scale(gunDir, 0.015f)),
                           Vector3Add(cPos, Vector3Scale(gunDir, 0.015f)), 0.118f, 0.118f, 8, SKYBLUE);
            DrawSphere(Vector3Add(cPos, Vector3Scale(gunUp, 0.075f)), 0.018f, WHITE);
        }

        // Flared Dual Muzzle Crown Ports with Electric Energy Glow
        DrawSphere(barrelLeftB,  0.048f, armorDark);
        DrawSphere(barrelRightB, 0.048f, armorDark);
        DrawSphere(barrelLeftB,  0.026f, SKYBLUE);
        DrawSphere(barrelRightB, 0.026f, SKYBLUE);
        DrawSphere(barrelLeftB,  0.010f, WHITE);
        DrawSphere(barrelRightB, 0.010f, WHITE);
    } else if (wEnum == WeaponId::BUZZ_BLADES) {
        // --- 4. BUZZ BLADES (Tactical Disc Slinger with High-Speed Spinning Plasma Saw Blade) ---
        gunMuzzle = Vector3Add(gunBreech, Vector3Scale(gunDir, 0.82f));
        Vector3 gripTop = Vector3Subtract(gunBreech, Vector3Scale(gunUp, 0.040f));
        Vector3 gripBot = Vector3Add(rightHand, Vector3Add(Vector3Scale(gunUp, -0.16f), Vector3Scale(gunDir, -0.04f)));
        DrawCylinderEx(gripTop, gripBot, 0.042f, 0.036f, 6, armorDark);

        // Tactical Military Green Frame with Titanium Reinforcements
        Vector3 bodyMid = Vector3Lerp(gunBreech, gunMuzzle, 0.60f);
        DrawCylinderEx(gunBreech, bodyMid, 0.115f, 0.105f, 8, { 42, 75, 48, 255 });
        // Top disc magazine guide tracks
        DrawCylinderEx(Vector3Add(gunBreech, Vector3Scale(gunUp, 0.075f)),
                       Vector3Add(bodyMid, Vector3Scale(gunUp, 0.075f)), 0.052f, 0.045f, 6, armorDark);

        // Motorized High-Speed Spinning Saw Blade Disc at Ejection Throat
        Vector3 discCenter = Vector3Lerp(gunBreech, gunMuzzle, 0.72f);
        float spinAngle = globalTime_ * 1200.0f * DEG2RAD;
        DrawCylinderEx(Vector3Subtract(discCenter, Vector3Scale(gunUp, 0.015f)),
                       Vector3Add(discCenter, Vector3Scale(gunUp, 0.015f)), 0.24f, 0.24f, 12, { 180, 210, 190, 255 });
        // Energized Green Plasma Perimeter Blade Ring
        DrawCylinderEx(Vector3Subtract(discCenter, Vector3Scale(gunUp, 0.018f)),
                       Vector3Add(discCenter, Vector3Scale(gunUp, 0.018f)), 0.265f, 0.265f, 12, GREEN);
        // Spinning Saw Teeth Spark Points
        for (int t = 0; t < 6; ++t) {
            float ta = spinAngle + t * 1.047197f;
            Vector3 toothTip = Vector3Add(discCenter, Vector3Add(Vector3Scale(gunRight, std::cos(ta) * 0.28f),
                                                                 Vector3Scale(gunDir,   std::sin(ta) * 0.28f)));
            DrawSphere(toothTip, 0.022f, LIME);
            DrawSphere(toothTip, 0.009f, WHITE);
        }

        // Twin Forward Launch Guide Prongs with Green Laser Emitter Tips
        Vector3 prongLA = Vector3Add(discCenter, Vector3Scale(gunRight, -0.15f));
        Vector3 prongRA = Vector3Add(discCenter, Vector3Scale(gunRight, +0.15f));
        Vector3 prongLB = Vector3Add(gunMuzzle, Vector3Scale(gunRight, -0.12f));
        Vector3 prongRB = Vector3Add(gunMuzzle, Vector3Scale(gunRight, +0.12f));
        DrawCylinderEx(prongLA, prongLB, 0.032f, 0.022f, 6, chromeSilv);
        DrawCylinderEx(prongRA, prongRB, 0.032f, 0.022f, 6, chromeSilv);
        DrawSphere(prongLB, 0.020f, GREEN);
        DrawSphere(prongRB, 0.020f, GREEN);
    } else if (wEnum == WeaponId::PLASMA_STRIKER) {
        // --- 5. PLASMA STRIKER (Precision Particle Accelerator Sniper with Holographic Scope) ---
        gunMuzzle = Vector3Add(gunBreech, Vector3Scale(gunDir, 1.30f));
        Vector3 gripTop = Vector3Subtract(gunBreech, Vector3Scale(gunUp, 0.040f));
        Vector3 gripBot = Vector3Add(rightHand, Vector3Add(Vector3Scale(gunUp, -0.16f), Vector3Scale(gunDir, -0.05f)));
        DrawCylinderEx(gripTop, gripBot, 0.042f, 0.036f, 6, armorDark);

        // Extended precision rifle chassis in imperial dark violet & titanium
        Vector3 midRifle = Vector3Lerp(gunBreech, gunMuzzle, 0.35f);
        DrawCylinderEx(gunBreech, midRifle, 0.095f, 0.075f, 8, { 85, 42, 125, 255 });
        // Contoured buttstock / cheek rest
        DrawCylinderEx(Vector3Subtract(gunBreech, Vector3Scale(gunDir, 0.12f)), gunBreech, 0.075f, 0.095f, 6, armorDark);

        // Long Fluted Chrome Particle Accelerator Barrel
        DrawCylinderEx(midRifle, gunMuzzle, 0.048f, 0.038f, 8, chromeSilv);

        // 4 Sequential Magnetic Accelerator Rings pulsing with ultraviolet plasma
        for (int r = 0; r < 4; ++r) {
            float frac = 0.40f + r * 0.14f;
            Vector3 rPos = Vector3Lerp(gunBreech, gunMuzzle, frac);
            DrawCylinderEx(Vector3Subtract(rPos, Vector3Scale(gunDir, 0.018f)),
                           Vector3Add(rPos, Vector3Scale(gunDir, 0.018f)), 0.076f, 0.076f, 8, { 195, 75, 255, 255 });
            DrawSphere(Vector3Add(rPos, Vector3Scale(gunUp, 0.055f)), 0.015f, WHITE);
        }

        // Elevated Telescopic Holographic Scope with Glowing Reticle
        Vector3 scopeA = Vector3Add(Vector3Lerp(gunBreech, gunMuzzle, 0.12f), Vector3Scale(gunUp, 0.11f));
        Vector3 scopeB = Vector3Add(Vector3Lerp(gunBreech, gunMuzzle, 0.48f), Vector3Scale(gunUp, 0.11f));
        DrawCylinderEx(scopeA, scopeB, 0.038f, 0.044f, 8, armorDark);
        // Scope mounts connecting to receiver rail
        DrawCylinderEx(Vector3Lerp(gunBreech, gunMuzzle, 0.18f), Vector3Add(Vector3Lerp(gunBreech, gunMuzzle, 0.18f), Vector3Scale(gunUp, 0.11f)), 0.020f, 0.020f, 4, chromeSilv);
        DrawCylinderEx(Vector3Lerp(gunBreech, gunMuzzle, 0.42f), Vector3Add(Vector3Lerp(gunBreech, gunMuzzle, 0.42f), Vector3Scale(gunUp, 0.11f)), 0.020f, 0.020f, 4, chromeSilv);
        // Glowing forward ocular lens & eyepiece
        DrawSphere(scopeB, 0.036f, { 210, 110, 255, 255 });
        DrawSphere(Vector3Add(scopeB, Vector3Scale(gunDir, 0.015f)), 0.018f, WHITE);
        DrawSphere(scopeA, 0.030f, chromeBright);

        // Folded Tactical Bipod under forend
        Vector3 bipodBase = Vector3Add(Vector3Lerp(gunBreech, gunMuzzle, 0.52f), Vector3Scale(gunUp, -0.045f));
        DrawCylinderEx(bipodBase, Vector3Add(bipodBase, Vector3Add(Vector3Scale(gunDir, 0.15f), Vector3Scale(gunRight, -0.06f))), 0.014f, 0.010f, 4, chromeSilv);
        DrawCylinderEx(bipodBase, Vector3Add(bipodBase, Vector3Add(Vector3Scale(gunDir, 0.15f), Vector3Scale(gunRight, +0.06f))), 0.014f, 0.010f, 4, chromeSilv);

        // Linear multi-port muzzle brake with purple plasma crown
        DrawCube(gunMuzzle, 0.075f, 0.075f, 0.060f, armorDark);
        DrawSphere(gunMuzzle, 0.032f, { 210, 110, 255, 255 });
        DrawSphere(gunMuzzle, 0.014f, WHITE);
    } else if (wEnum == WeaponId::RYNO_INFINITE) {
        // --- 6. RYNO INFINITE (Massive 6-Barrel Gatling Minigun with Central Rocket Silo) ---
        gunMuzzle = Vector3Add(gunBreech, Vector3Scale(gunDir, 1.08f));
        Vector3 gripTop = Vector3Subtract(gunBreech, Vector3Scale(gunUp, 0.050f));
        Vector3 gripBot = Vector3Add(rightHand, Vector3Add(Vector3Scale(gunUp, -0.16f), Vector3Scale(gunDir, -0.05f)));
        DrawCylinderEx(gripTop, gripBot, 0.046f, 0.040f, 6, armorDark);

        // Heavy Dreadzone Battle Chassis in Scorched Crimson & 24K Gold Hazard Trim
        Vector3 midRyno = Vector3Lerp(gunBreech, gunMuzzle, 0.45f);
        DrawCylinderEx(gunBreech, midRyno, 0.195f, 0.185f, 10, { 180, 24, 32, 255 });
        DrawCylinderWiresEx(gunBreech, midRyno, 0.200f, 0.190f, 10, GOLD);
        // Heat-dissipating cooling louvers on top
        DrawCube(Vector3Add(Vector3Lerp(gunBreech, midRyno, 0.50f), Vector3Scale(gunUp, 0.13f)),
                 0.18f, 0.05f, 0.22f, armorDark);

        // Central High-Explosive Rocket Silo Bore
        DrawCylinderEx(midRyno, gunMuzzle, 0.068f, 0.068f, 8, armorDark);
        // Armed rocket warhead tip inside silo
        DrawSphere(Vector3Lerp(midRyno, gunMuzzle, 0.85f), 0.052f, { 235, 120, 20, 255 });
        DrawSphere(Vector3Lerp(midRyno, gunMuzzle, 0.88f), 0.024f, YELLOW);

        // 6 High-Speed Rotary Chrome Gatling Barrels
        float barrelAngle = player_.barrelSpinAngle * DEG2RAD;
        float rynoRadius = 0.125f;
        for (int b = 0; b < 6; ++b) {
            float ba = barrelAngle + b * 1.047197f;
            Vector3 offset = Vector3Add(Vector3Scale(gunRight, std::cos(ba) * rynoRadius),
                                        Vector3Scale(gunUp,    std::sin(ba) * rynoRadius));
            Vector3 bA = Vector3Add(midRyno, offset);
            Vector3 bB = Vector3Add(gunMuzzle, offset);
            DrawCylinderEx(bA, bB, 0.036f, 0.034f, 6, chromeSilv);
            DrawSphere(bB, 0.032f, chromeBright);
        }

        // Heavy Muzzle Stabilization Collar locking the 6 barrels together
        Vector3 collarPos = Vector3Lerp(midRyno, gunMuzzle, 0.75f);
        DrawCylinderEx(Vector3Subtract(collarPos, Vector3Scale(gunDir, 0.025f)),
                       Vector3Add(collarPos, Vector3Scale(gunDir, 0.025f)), 0.170f, 0.170f, 12, armorDark);
        DrawCylinderWiresEx(Vector3Subtract(collarPos, Vector3Scale(gunDir, 0.025f)),
                            Vector3Add(collarPos, Vector3Scale(gunDir, 0.025f)), 0.172f, 0.172f, 12, GOLD);

        // Gold Ammunition Belt Feed Chute entering side receiver
        Vector3 ammoChuteA = Vector3Add(Vector3Lerp(gunBreech, midRyno, 0.35f), Vector3Scale(gunRight, -0.16f));
        Vector3 ammoChuteB = Vector3Add(ammoChuteA, Vector3Scale(gunUp, -0.14f));
        DrawCylinderEx(ammoChuteA, ammoChuteB, 0.046f, 0.042f, 6, GOLD);
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
    // Project 3D world positions accurately into the virtual canvas (virtualWidth_ x 720)
    // instead of GetScreenWidth()/GetScreenHeight() so reticles & callouts align identically on Android & PC!
    Camera3D activeCam = camera_;
    if (cameraShake_ > 0.0f) {
        float mag = cameraShake_ * cameraShake_ * 0.65f * save_.screenShakeScale;
        activeCam.position.x += std::sin(globalTime_ * 73.0f) * mag;
        activeCam.position.y += std::cos(globalTime_ * 61.0f) * mag;
    }
    Matrix matView = MatrixLookAt(activeCam.position, activeCam.target, activeCam.up);
    Matrix matProj = MatrixPerspective(activeCam.fovy * DEG2RAD, static_cast<double>(virtualWidth_) / 720.0, 0.90, 220.0);
    auto ProjectToVirtual = [&](Vector3 worldPos, Vector2& outScr) -> bool {
        Quaternion wp = { worldPos.x, worldPos.y, worldPos.z, 1.0f };
        wp = QuaternionTransform(wp, matView);
        wp = QuaternionTransform(wp, matProj);
        if (wp.w <= 0.1f) return false;
        float ndcX = wp.x / wp.w;
        float ndcY = -wp.y / wp.w;
        outScr.x = (ndcX + 1.0f) * 0.5f * virtualWidth_;
        outScr.y = (ndcY + 1.0f) * 0.5f * 720.0f;
        return (outScr.x >= -40.0f && outScr.x <= virtualWidth_ + 40.0f &&
                outScr.y >= -40.0f && outScr.y <= 760.0f);
    };

    // 2D Tactical Action Callouts & Target Reticles projected over 3D Crates, Obstacles & Enemies!
    for (const auto& h : hazards_) {
        if (!h.active || h.z < 3.5f || h.z > 58.0f) continue;

        float rawLx = GetLaneX(h.lane);
        Vector3 worldTop = ApplyTrackCurve(rawLx, 2.25f, h.z);
        Vector2 scr{};
        if (!ProjectToVirtual(worldTop, scr)) continue;
        int sx = static_cast<int>(scr.x);
        int sy = static_cast<int>(scr.y);

        // Enemy Lock-On Reticle + Floating HP Bar
        if (h.kind == HazardKind::THUG_DRONE || h.kind == HazardKind::SHIELDED_ENFORCER) {
            Vector3 bodyWorld = ApplyTrackCurve(rawLx, 1.40f, h.z);
            Vector2 bodyScr = scr;
            ProjectToVirtual(bodyWorld, bodyScr);
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
            } else if (h.kind == HazardKind::MAGMA_GEYSER && h.lane == player_.currentLane) {
                tag = "! MAGMA GEYSER !";
                tagCol = ORANGE;
            } else if (h.kind == HazardKind::HOVER_TAXI && h.lane == player_.currentLane) {
                tag = "! HOVER-TAXI !";
                tagCol = YELLOW;
            } else if (h.kind == HazardKind::PIRANHA_LEAP && h.lane == player_.currentLane) {
                tag = "! CYBER-PIRANHA !";
                tagCol = SKYBLUE;
            } else if (h.kind == HazardKind::SWINGING_CRANE_SCRAP && h.lane == player_.currentLane) {
                tag = "v SLIDE UNDER CRANE v";
                tagCol = ORANGE;
            } else if (h.kind == HazardKind::SWINGSHOT_NODE && h.lane == player_.currentLane) {
                tag = "* SWINGSHOT NODE *";
                tagCol = GREEN;
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
    DrawRectangle(static_cast<int>(virtualWidth_ - 320), 20, 300, 96, { 12, 18, 32, 215 });
    DrawRectangleLinesEx({ virtualWidth_ - 320.0f, 20.0f, 300.0f, 96.0f }, 2.0f, { 65, 215, 255, 190 });

    char distBuf[64], boltBuf[64], rariBuf[64];
    std::snprintf(distBuf, sizeof(distBuf), "DISTANCE: %d m", static_cast<int>(distanceMeters_));
    std::snprintf(boltBuf, sizeof(boltBuf), "BOLTS: %d (+%d)", save_.bolts, runBoltsEarned_);
    std::snprintf(rariBuf, sizeof(rariBuf), "RARITANIUM: %d (+%d)", save_.raritanium, runRaritaniumEarned_);

    DrawText(distBuf, static_cast<int>(virtualWidth_ - 304), 28, 24, WHITE);
    DrawText(boltBuf, static_cast<int>(virtualWidth_ - 304), 58, 18, GOLD);
    DrawText(rariBuf, static_cast<int>(virtualWidth_ - 304), 82, 16, SKYBLUE);

    // On-Screen Holographic Touch Pause Button (Mobile-Friendly)
    Rectangle pauseBtn = { virtualWidth_ - 386.0f, 20.0f, 54.0f, 54.0f };
    bool pauseActive = (touchFeedbackPause_ > 0.0f);
    bool pauseHover = IsRectHovered(pauseBtn) || pauseActive;
    DrawRectangleRec(pauseBtn, pauseActive ? Color{ 45, 75, 115, 255 } : (pauseHover ? Color{ 24, 38, 62, 230 } : Color{ 12, 18, 32, 200 }));
    DrawRectangleLinesEx(pauseBtn, pauseActive ? 2.5f : 1.8f, pauseActive ? GOLD : (pauseHover ? WHITE : SKYBLUE));
    DrawRectangle(static_cast<int>(pauseBtn.x + 17), static_cast<int>(pauseBtn.y + 13), 6, 22, pauseActive ? GOLD : WHITE);
    DrawRectangle(static_cast<int>(pauseBtn.x + 31), static_cast<int>(pauseBtn.y + 13), 6, 22, pauseActive ? GOLD : WHITE);
    DrawText("PAUSE", static_cast<int>(pauseBtn.x + 11), static_cast<int>(pauseBtn.y + 38), 9, pauseActive ? GOLD : LIGHTGRAY);

    // Boss Health Bar
    if (boss_.active) {
        float bw = 520.0f;
        float bx = (virtualWidth_ - bw) * 0.5f;
        DrawRectangle(static_cast<int>(bx - 6), 24, static_cast<int>(bw + 12), 54, { 18, 12, 24, 230 });
        DrawText(boss_.name.c_str(), static_cast<int>(bx + 8), 28, 18, GOLD);

        float totalHp = 0.0f;
        for (int l = 0; l < NUM_LANES; ++l) totalHp += boss_.turretHp[l];
        float ratio = std::clamp(totalHp / (boss_.maxTurretHp * 3.0f), 0.0f, 1.0f);
        DrawRectangle(static_cast<int>(bx), 52, static_cast<int>(bw), 16, { 50, 20, 20, 255 });
        DrawRectangle(static_cast<int>(bx), 52, static_cast<int>(bw * ratio), 16, RED);
        DrawRectangleLines(static_cast<int>(bx), 52, static_cast<int>(bw), 16, WHITE);
    }

    // Bottom-Right Quick-Select Weapon Hotbar (1..6)
    const auto& weaps = GetWeaponCatalog();
    for (int w = 0; w < static_cast<int>(NUM_WEAPONS); ++w) {
        Rectangle sr = { virtualWidth_ - 418.0f + w * 66.0f, 720.0f - 84.0f, 60.0f, 64.0f };
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
            DrawText(lvlTxt, static_cast<int>(sr.x + 17), static_cast<int>(sr.y + 22), 18,
                     (save_.weaponLevels[w] >= 5) ? GOLD : weaps[w].projectileColor);
            if (w == 0) {
                DrawText("INF", static_cast<int>(sr.x + 19), static_cast<int>(sr.y + 44), 12, SKYBLUE);
            } else {
                int curA = player_.weaponAmmo[w];
                int maxA = std::max(1, player_.weaponMaxAmmo[w]);
                float frac = static_cast<float>(curA) / static_cast<float>(maxA);
                Color aCol = (curA <= 0) ? RED : (frac <= 0.25f) ? ORANGE : WHITE;
                char ammoTxt[16];
                std::snprintf(ammoTxt, sizeof(ammoTxt), "%d", curA);
                int aw = MeasureText(ammoTxt, 13);
                DrawText(ammoTxt, static_cast<int>(sr.x + (sr.width - aw) * 0.5f), static_cast<int>(sr.y + 43), 13, aCol);
                DrawRectangle(static_cast<int>(sr.x + 4), static_cast<int>(sr.y + sr.height - 5),
                              static_cast<int>((sr.width - 8) * frac), 3, aCol);
            }
        } else {
            DrawText("LOCK", static_cast<int>(sr.x + 9), static_cast<int>(sr.y + 30), 15, GRAY);
        }
    }

    // Equipped Weapon Banner (Bottom-Left)
    int eq = save_.equippedWeapon;
    int eqLvl = save_.weaponLevels[eq];
    const char* wTitle = (eqLvl >= 5) ? weaps[eq].omegaName : weaps[eq].name;
    DrawRectangle(20, static_cast<int>(720.0f - 96.0f), 480, 76, { 12, 18, 32, 220 });
    DrawRectangleLinesEx({ 20.0f, 720.0f - 96.0f, 480.0f, 76.0f }, 1.5f, weaps[eq].projectileColor);
    DrawText(wTitle, 32, static_cast<int>(720.0f - 88.0f), 18, WHITE);
    char xpLine[96];
    char ammoSeg[32];
    if (eq == 0) std::snprintf(ammoSeg, sizeof(ammoSeg), "AMMO: INF");
    else std::snprintf(ammoSeg, sizeof(ammoSeg), "AMMO: %d/%d", player_.weaponAmmo[eq], player_.weaponMaxAmmo[eq]);
    if (eqLvl >= 5) std::snprintf(xpLine, sizeof(xpLine), "OMEGA MASTERWORK V5  |  %s", ammoSeg);
    else std::snprintf(xpLine, sizeof(xpLine), "LEVEL V%d  |  WEAP XP: %d / %d  |  %s", eqLvl, save_.weaponXp[eq], eqLvl * 120, ammoSeg);
    DrawText(xpLine, 32, static_cast<int>(720.0f - 65.0f), 14, GOLD);
    DrawText(weaps[eq].specialPerk, 32, static_cast<int>(720.0f - 44.0f), 12, SKYBLUE);

    // On-Screen Holographic Touch Controls (Optimized for Mobile & Touch Screen Play)
    Rectangle btnJumpRec = save_.leftHandedControls
                         ? Rectangle{ 30.0f, 720.0f - 216.0f, 96.0f, 96.0f }
                         : Rectangle{ virtualWidth_ - 128.0f, 720.0f - 216.0f, 96.0f, 96.0f };
    Rectangle btnFireRec = save_.leftHandedControls
                         ? Rectangle{ 140.0f, 720.0f - 208.0f, 82.0f, 82.0f }
                         : Rectangle{ virtualWidth_ - 238.0f, 720.0f - 208.0f, 82.0f, 82.0f };

    bool isGlidingNow = player_.heliGliding;
    Color jumpRingCol = isGlidingNow ? Color{ 60, 255, 95, 255 } : (player_.grounded ? Color{ 80, 180, 255, 220 } : Color{ 255, 200, 50, 255 });

    bool hasGamepad = IsGamepadAvailable(0);
    const char* jumpLabel = isGlidingNow ? "GLIDING" : (hasGamepad ? "(A) JUMP" : "JUMP");
    const char* fireLabel = hasGamepad ? "(X) FIRE" : "FIRE";

    // 1. [JUMP / GLIDE] Holographic Button
    bool jumpActive = (touchFeedbackJump_ > 0.0f);
    DrawCircle(static_cast<int>(btnJumpRec.x + 48), static_cast<int>(btnJumpRec.y + 48), 44.0f, jumpActive ? Color{ 40, 85, 140, 240 } : Color{ 14, 22, 38, 200 });
    DrawCircleLines(static_cast<int>(btnJumpRec.x + 48), static_cast<int>(btnJumpRec.y + 48), jumpActive ? 47.0f : 44.0f, jumpActive ? WHITE : jumpRingCol);
    if (isGlidingNow) {
        // Glowing Clank Emerald Heli-Ring + Fuel Arc
        DrawCircleLines(static_cast<int>(btnJumpRec.x + 48), static_cast<int>(btnJumpRec.y + 48), 47.0f, { 60, 255, 95, 255 });
    }
    // Mini Rotor Icon
    Vector2 jc = { btnJumpRec.x + 48, btnJumpRec.y + 42 };
    DrawCircle(static_cast<int>(jc.x), static_cast<int>(jc.y), 7.0f, isGlidingNow ? Color{ 60, 255, 95, 255 } : WHITE);
    float bladeRot = isGlidingNow ? (globalTime_ * 1200.0f * DEG2RAD) : 0.0f;
    Vector2 b1 = { jc.x + std::cos(bladeRot) * 22.0f, jc.y + std::sin(bladeRot) * 22.0f };
    Vector2 b2 = { jc.x - std::cos(bladeRot) * 22.0f, jc.y - std::sin(bladeRot) * 22.0f };
    DrawLineEx(b1, b2, 3.0f, isGlidingNow ? Color{ 60, 255, 95, 255 } : LIGHTGRAY);
    DrawText(jumpLabel, static_cast<int>(btnJumpRec.x + (isGlidingNow ? 14 : (hasGamepad ? 12 : 27))), static_cast<int>(btnJumpRec.y + 68), 13, jumpRingCol);

    // 2. [FIRE / WRENCH] Holographic Button
    bool fireActive = (touchFeedbackFire_ > 0.0f);
    DrawCircle(static_cast<int>(btnFireRec.x + 41), static_cast<int>(btnFireRec.y + 41), 38.0f, fireActive ? Color{ 110, 55, 20, 240 } : Color{ 18, 20, 32, 190 });
    DrawCircleLines(static_cast<int>(btnFireRec.x + 41), static_cast<int>(btnFireRec.y + 41), fireActive ? 42.0f : 38.0f, fireActive ? WHITE : Color{ 255, 120, 45, 220 });
    Vector2 fc = { btnFireRec.x + 41, btnFireRec.y + 36 };
    DrawCircleLines(static_cast<int>(fc.x), static_cast<int>(fc.y), 12.0f, ORANGE);
    DrawLine(static_cast<int>(fc.x - 16), static_cast<int>(fc.y), static_cast<int>(fc.x + 16), static_cast<int>(fc.y), ORANGE);
    DrawLine(static_cast<int>(fc.x), static_cast<int>(fc.y - 16), static_cast<int>(fc.x), static_cast<int>(fc.y + 16), ORANGE);
    DrawText(fireLabel, static_cast<int>(btnFireRec.x + (hasGamepad ? 12 : 24)), static_cast<int>(btnFireRec.y + 58), 13, GOLD);

    // 3. [OVERDRIVE HOVERBOOTS BOOST] Holographic Button (If Unlocked!)
    if (save_.hoverbootsUnlocked) {
        Rectangle btnBoostRec = save_.leftHandedControls
                              ? Rectangle{ 236.0f, 720.0f - 200.0f, 74.0f, 74.0f }
                              : Rectangle{ virtualWidth_ - 326.0f, 720.0f - 200.0f, 74.0f, 74.0f };
        bool boostActive = (player_.overdriveDuration > 0.0f || touchFeedbackBoost_ > 0.0f);
        bool boostReady = (player_.overdriveCooldown <= 0.0f);
        Color boostRingCol = boostActive ? GOLD : (boostReady ? ORANGE : DARKGRAY);

        DrawCircle(static_cast<int>(btnBoostRec.x + 37), static_cast<int>(btnBoostRec.y + 37), 34.0f,
                   boostActive ? Color{ 145, 95, 20, 240 } : Color{ 16, 20, 32, 190 });
        DrawCircleLines(static_cast<int>(btnBoostRec.x + 37), static_cast<int>(btnBoostRec.y + 37),
                        boostActive ? 37.0f : 34.0f, boostRingCol);

        // Rocket Flame Icon
        Vector2 rc = { btnBoostRec.x + 37, btnBoostRec.y + 30 };
        DrawTriangle({ rc.x, rc.y - 12.0f }, { rc.x - 8.0f, rc.y + 8.0f }, { rc.x + 8.0f, rc.y + 8.0f }, boostReady ? GOLD : GRAY);
        DrawTriangle({ rc.x, rc.y - 4.0f }, { rc.x - 4.0f, rc.y + 10.0f }, { rc.x + 4.0f, rc.y + 10.0f }, boostReady ? YELLOW : DARKGRAY);

        const char* bLabel = boostActive ? "BOOST!" : (boostReady ? (hasGamepad ? "(Y) BOOST" : "BOOST") : "COOLDOWN");
        DrawText(bLabel, static_cast<int>(btnBoostRec.x + (boostReady ? (hasGamepad ? 8 : 16) : 6)),
                 static_cast<int>(btnBoostRec.y + 50), 11, boostRingCol);

        if (!boostReady) {
            float cdFrac = player_.overdriveCooldown / 11.0f;
            DrawRectangle(static_cast<int>(btnBoostRec.x + 8), static_cast<int>(btnBoostRec.y + 64),
                          static_cast<int>(58.0f * (1.0f - cdFrac)), 4, GOLD);
        }
    }

    // Emergency Auto-Hover Alert & Track Terminus Warning
    if (player_.emergencyHoverTimer > 0.0f) {
        char hoverMsg[64];
        std::snprintf(hoverMsg, sizeof(hoverMsg), "! CLANK EMERGENCY HOVER (%.1fs): HOP TO ACTIVE RAIL !", player_.emergencyHoverTimer);
        int mw = MeasureText(hoverMsg, 18);
        DrawRectangle(static_cast<int>(virtualWidth_ * 0.5f - mw * 0.5f - 20), 168, mw + 40, 36, { 55, 14, 18, 245 });
        DrawRectangleLines(static_cast<int>(virtualWidth_ * 0.5f - mw * 0.5f - 20), 168, mw + 40, 36, RED);
        Color txtCol = (std::fmod(globalTime_ * 10.0f, 1.0f) < 0.5f) ? YELLOW : WHITE;
        DrawText(hoverMsg, static_cast<int>(virtualWidth_ * 0.5f - mw * 0.5f), 177, 18, txtCol);
    } else if (!IsRailActiveAt(player_.currentLane, 32.0f)) {
        DrawRectangle(static_cast<int>(virtualWidth_ * 0.5f - 210), 168, 420, 36, { 35, 12, 16, 230 });
        DrawRectangleLines(static_cast<int>(virtualWidth_ * 0.5f - 210), 168, 420, 36, RED);
        DrawText("! TRACK ENDING AHEAD: HOP OVER !", static_cast<int>(virtualWidth_ * 0.5f - 185), 177, 18, RED);
    }

    // First-Run Control Hint Pill (fades cleanly after 4.5s on a player's very first run)
    if (save_.totalRunsStarted <= 1 && runTime_ < 4.5f) {
        float fade = (runTime_ > 3.5f) ? std::clamp(1.0f - (runTime_ - 3.5f), 0.0f, 1.0f) : 1.0f;
        unsigned char bgA = static_cast<unsigned char>(fade * 210.0f);
        unsigned char txtA = static_cast<unsigned char>(fade * 245.0f);
#if defined(PLATFORM_ANDROID)
        const char* hint = "CONTROLS: Swipe L/R = Switch Rail  |  Swipe Down = Slide  |  [JUMP] = Jump / Glide  |  [FIRE] = Wrench / Gun";
#else
        const char* hint = hasGamepad
            ? "CONTROLS: [L-Stick] Rail  |  [(A)] Jump / Glide  |  [(B)] Slide  |  [(X) / RT] Attack"
            : "CONTROLS: [A/D] Switch Rail  |  [SPACE] Jump / Hold Glide  |  [S] Slide  |  [CLICK / J] Attack";
#endif
        int hw = MeasureText(hint, 14);
        DrawRectangle(static_cast<int>(virtualWidth_ * 0.5f - hw * 0.5f - 18), 86, hw + 36, 32, { 10, 16, 28, bgA });
        DrawRectangleLines(static_cast<int>(virtualWidth_ * 0.5f - hw * 0.5f - 18), 86, hw + 36, 32, Color{ 80, 195, 255, txtA });
        DrawText(hint, static_cast<int>(virtualWidth_ * 0.5f - hw * 0.5f), 95, 14, Color{ 230, 245, 255, txtA });
    }

    // Sector Entry Banner
    if (sectorBannerTimer_ > 0.0f) {
        const char* sName = GetSectorName(GetCurrentSector());
        int sw = MeasureText(sName, 20);
        DrawRectangle(static_cast<int>(virtualWidth_ * 0.5f - sw * 0.5f - 20), 128, sw + 40, 38, { 12, 18, 32, 220 });
        DrawRectangleLines(static_cast<int>(virtualWidth_ * 0.5f - sw * 0.5f - 20), 128, sw + 40, 38, ORANGE);
        DrawText(sName, static_cast<int>(virtualWidth_ * 0.5f - sw * 0.5f), 137, 20, GOLD);
    }

    // Retro Holographic Radio Comm Chatter Box
    RenderRadioComm2D();
}

void RailGame::RenderSplashIntro2D() {
    // Fill screen with vibrant Sebio sky-blue background
    DrawRectangle(0, 0, static_cast<int>(virtualWidth_), 720, Color{ 128, 219, 255, 255 });

    float t = splashTimer_;
    // Total splash duration: 2.4s (0.0s - 0.5s fade in, 0.5s - 1.2s flash glow, 1.2s - 1.9s hold, 1.9s - 2.4s fade out)
    float alpha = 1.0f;
    if (t < 0.5f) {
        alpha = t / 0.5f;
    } else if (t > 1.9f) {
        alpha = std::clamp((2.4f - t) / 0.5f, 0.0f, 1.0f);
    }

    // Flash burst pulse between 0.5s and 1.1s
    float flash = 0.0f;
    if (t >= 0.5f && t <= 1.1f) {
        flash = std::sin((t - 0.5f) / 0.6f * 3.14159265f);
    }

    if (splashLogo_.id != 0) {
        float scale = 0.92f + flash * 0.05f;
        float w = splashLogo_.width * scale;
        float h = splashLogo_.height * scale;
        Rectangle dest = { (virtualWidth_ - w) * 0.5f, (720.0f - h) * 0.5f, w, h };
        Color logoCol = { 255, 255, 255, static_cast<unsigned char>(255 * alpha) };
        DrawTexturePro(splashLogo_, { 0, 0, (float)splashLogo_.width, (float)splashLogo_.height }, dest, { 0, 0 }, 0.0f, logoCol);
    } else {
        // Fallback procedural logo if texture fails to load
        const char* l1 = "SEBINY LABS";
        int w1 = MeasureText(l1, 56);
        DrawText(l1, static_cast<int>((virtualWidth_ - w1) / 2), 310, 56, Color{ 235, 30, 45, static_cast<unsigned char>(255 * alpha) });
        const char* l2 = "Discover new adventures";
        int w2 = MeasureText(l2, 22);
        DrawText(l2, static_cast<int>((virtualWidth_ - w2) / 2), 385, 22, Color{ 20, 60, 180, static_cast<unsigned char>(255 * alpha) });
    }

    if (flash > 0.0f) {
        DrawRectangle(0, 0, static_cast<int>(virtualWidth_), 720, Color{ 255, 255, 255, static_cast<unsigned char>(flash * 140) });
    }

    // Skip prompt indicator
    const char* skipHint = "TAP OR PRESS SPACE TO SKIP";
    int shw = MeasureText(skipHint, 13);
    DrawText(skipHint, static_cast<int>((virtualWidth_ - shw) / 2), 720 - 38, 13, Color{ 40, 80, 140, static_cast<unsigned char>(160 * alpha) });
}

void RailGame::RenderBrandingFooter2D() const {
    DrawRectangle(0, 720 - 56, static_cast<int>(virtualWidth_), 56, { 8, 12, 22, 240 });
    DrawLine(0, 720 - 56, static_cast<int>(virtualWidth_), 720 - 56, { 255, 165, 40, 160 });

    const char* studio = "SEBINY LABS";
    int sw = MeasureText(studio, 18);
    DrawText(studio, static_cast<int>((virtualWidth_ - sw) / 2), 720 - 48, 18, GOLD);

    const char* ipOwner = "Non-commercial fan tribute. Ratchet & Clank and Before the Nexus are trademarks of Sony Interactive Entertainment LLC / Insomniac Games.";
    int iw = MeasureText(ipOwner, 14);
    DrawText(ipOwner, static_cast<int>((virtualWidth_ - iw) / 2), 720 - 22, 14, { 185, 205, 225, 235 });
}

void RailGame::RenderTitleFrontPage2D() {
    // Insomniac / Rift Apart style Title Screen (Dimensional Rift glow, Lombax silhouette, Press Any Key)
    DrawRectangle(0, 0, static_cast<int>(virtualWidth_), 720, { 6, 10, 20, 160 });

    Vector2 center = { virtualWidth_ * 0.5f, 260.0f };
    float pulse = std::sin(globalTime_ * 2.2f);

    // Dimensionator Rift Ring
    DrawCircleGradient(static_cast<int>(center.x), static_cast<int>(center.y), 230.0f,
                       { 255, 140, 40, static_cast<unsigned char>(40 + pulse * 15) }, BLANK);
    DrawPolyLinesEx(center, 8, 190.0f + pulse * 8.0f, globalTime_ * 24.0f, 3.0f, { 255, 160, 45, 180 });
    DrawPolyLinesEx(center, 6, 145.0f - pulse * 6.0f, -globalTime_ * 32.0f, 2.5f, { 70, 220, 255, 190 });

    const char* preTitle = "INSOMNIAC GAMES FAN TRIBUTE // NEXUS SECTOR";
    int ptw = MeasureText(preTitle, 16);
    DrawText(preTitle, static_cast<int>((virtualWidth_ - ptw) / 2), 380, 16, { 255, 185, 95, 230 });

    const char* title = "RATCHET & CLANK: RAILFINITY";
    int tw = MeasureText(title, 58);
    DrawText(title, static_cast<int>((virtualWidth_ - tw) / 2), 410, 58, GOLD);

    DrawLine(static_cast<int>(virtualWidth_ * 0.5f - 320), 485, static_cast<int>(virtualWidth_ * 0.5f + 320), 485, { 255, 165, 40, 120 });

    // Pulsing "Press Any Key / Tap Screen to Start" Call to Action
    float blink = std::sin(globalTime_ * 4.2f) * 0.5f + 0.5f;
    Color promptCol = { 255, 255, 255, static_cast<unsigned char>(140 + blink * 115) };
    const char* prompt = "PRESS ANY KEY OR TAP SCREEN TO START";
    int pw = MeasureText(prompt, 22);
    DrawText(prompt, static_cast<int>((virtualWidth_ - pw) / 2), 525, 22, promptCol);

    const char* ostNotice = "PLAYING: RATCHET & CLANK: RIFT APART OST - RIFT APART";
    int onw = MeasureText(ostNotice, 14);
    DrawText(ostNotice, static_cast<int>((virtualWidth_ - onw) / 2), 595, 14, { 140, 210, 255, 200 });

    RenderBrandingFooter2D();
}

void RailGame::RenderMainMenu2D() {
    DrawRectangle(0, 0, static_cast<int>(virtualWidth_), 165, { 10, 14, 26, 210 });
    DrawLine(0, 165, static_cast<int>(virtualWidth_), 165, ORANGE);

    const char* pre = "SOLANA & POLARIS INTERGALACTIC GRINDWAY NETWORK";
    int pw = MeasureText(pre, 16);
    DrawText(pre, static_cast<int>((virtualWidth_ - pw) / 2), 24, 16, SKYBLUE);

    const char* title = "RATCHET & CLANK: RAILFINITY";
    int tw = MeasureText(title, 52);
    DrawText(title, static_cast<int>((virtualWidth_ - tw) / 2), 50, 52, GOLD);

    const char* sub = "INFINITE 3D BEFORE-THE-NEXUS GRINDRAIL ACTION";
    int subw = MeasureText(sub, 18);
    DrawText(sub, static_cast<int>((virtualWidth_ - subw) / 2), 110, 18, WHITE);

    char stats[160];
    std::snprintf(stats, sizeof(stats),
                  "BOLTS: %d   |   RARITANIUM: %d   |   BEST RUN: %d m   |   DAILY BEST: %d m",
                  save_.bolts, save_.raritanium, save_.bestDistanceMeters, save_.bestDailyMeters);
    int stw = MeasureText(stats, 16);
    DrawText(stats, static_cast<int>((virtualWidth_ - stw) / 2), 140, 16, LIGHTGRAY);

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

        int textBlockH = labelFs + 5 + descFs;
        int topPad = std::max(8, (static_cast<int>(r.height) - textBlockH) / 2);
        DrawText(label, static_cast<int>(r.x + 18), static_cast<int>(r.y) + topPad, labelFs, WHITE);
        DrawText(desc, static_cast<int>(r.x + 18), static_cast<int>(r.y) + topPad + labelFs + 5, descFs, LIGHTGRAY);
        return IsButtonClicked(r);
    };

    Rectangle btnPlay     = { 50.0f, 182.0f, 400.0f, 58.0f };
    Rectangle btnDaily    = { 50.0f, 247.0f, 400.0f, 58.0f };
    Rectangle btnVendor   = { 50.0f, 312.0f, 400.0f, 58.0f };
    Rectangle btnLab      = { 50.0f, 377.0f, 400.0f, 58.0f };
    Rectangle btnWardrobe = { 50.0f, 442.0f, 400.0f, 58.0f };
    Rectangle btnCareer   = { 50.0f, 507.0f, 400.0f, 58.0f };
    Rectangle btnSkills   = { virtualWidth_ - 380.0f, 182.0f, 330.0f, 58.0f };
    Rectangle btnSettings = { virtualWidth_ - 380.0f, 247.0f, 330.0f, 58.0f };

    if (MenuButton(btnPlay, "LAUNCH GRINDRAIL RUN", "High-Speed Planetary Grindrails", ORANGE)) {
        StartNewRun(false);
    }

    std::string dailyDesc = std::string("Seeded Daily Track [") + PCG32::GetDailyDateString() + "]";
    if (MenuButton(btnDaily, "DAILY CHALLENGE SEED", dailyDesc.c_str(), GOLD)) {
        StartNewRun(true);
    }

    if (MenuButton(btnVendor, "GADGETRON WEAPON VENDOR", "Unlock & Upgrade 6 Weapons (V1-V5)", SKYBLUE)) {
        SetScreenMode(ScreenMode::GADGETRON_VENDOR);
        audio_.PlayUiClick();
    }

    if (MenuButton(btnLab, "RARITANIUM MOD LAB", "Upgrade Armor, Magnet & Thrusters", GREEN)) {
        SetScreenMode(ScreenMode::RARITANIUM_LAB);
        audio_.PlayUiClick();
    }

    if (MenuButton(btnWardrobe, "ARMOR & CLANK WARDROBE", "Equip Rivet, Kit, Carbonox & Infernox", PURPLE)) {
        wardrobePreviewArmor_ = save_.equippedArmor;
        wardrobePreviewClank_ = save_.equippedClankSkin;
        wardrobeRotAngle_ = 180.0f; // Start facing camera
        SetScreenMode(ScreenMode::ARMOR_WARDROBE);
        audio_.PlayUiClick();
    }

    char carTxt[80];
    std::snprintf(carTxt, sizeof(carTxt), "Runs: %d | Grind: %d m | Foes: %d",
                  save_.totalRunsStarted, save_.totalDistanceGrinded, save_.totalEnemiesDestroyed);
    if (MenuButton(btnCareer, "CAREER DOSSIER [C]", carTxt, GOLD)) {
        SetScreenMode(ScreenMode::STATS_VIEW);
        audio_.PlayUiClick();
    }

    int spDone = 0;
    for (bool b : save_.skillPoints) if (b) spDone++;
    char spTxt[64];
    std::snprintf(spTxt, sizeof(spTxt), "Unlocked: %d / %d Skill Points", spDone, static_cast<int>(NUM_SKILL_POINTS));
    if (MenuButton(btnSkills, "SKILL POINTS LOG", spTxt, MAGENTA)) {
        SetScreenMode(ScreenMode::SKILL_POINTS_VIEW);
        audio_.PlayUiClick();
    }

    if (MenuButton(btnSettings, "SYSTEM SETTINGS", "Audio, Visuals & Controller", PURPLE)) {
        settingsPrevMode_ = ScreenMode::MAIN_MENU;
        SetScreenMode(ScreenMode::SETTINGS_VIEW);
        audio_.PlayUiClick();
    }

    // Controls Reference Box on Right
    DrawRectangle(static_cast<int>(virtualWidth_ - 380), 314, 330, 251, { 12, 18, 32, 225 });
    DrawRectangleLines(static_cast<int>(virtualWidth_ - 380), 314, 330, 251, SKYBLUE);
    DrawText("CONTROLS & GAMEPAD", static_cast<int>(virtualWidth_ - 362), 326, 16, GOLD);
    DrawText("> Left Stick / A-D: Switch Rail", static_cast<int>(virtualWidth_ - 362), 350, 13, WHITE);
    DrawText("> (A) / SPACE / Swipe Up: Jump", static_cast<int>(virtualWidth_ - 362), 372, 13, WHITE);
    DrawText("> Hold (A) / SPACE: Clank Glide", static_cast<int>(virtualWidth_ - 362), 394, 13, WHITE);
    DrawText("> (B) / S / Swipe Down: Slide", static_cast<int>(virtualWidth_ - 362), 416, 13, WHITE);
    DrawText("> (X) / RT / Click / F: Fire / Wrench", static_cast<int>(virtualWidth_ - 362), 438, 13, WHITE);
    DrawText("> (Y) / L-Shift: Overdrive Boots", static_cast<int>(virtualWidth_ - 362), 460, 13, GOLD);
    DrawText("> LB / RB / 1-6: Cycle Weapons", static_cast<int>(virtualWidth_ - 362), 482, 13, WHITE);
    DrawText("> N: Skip OST Track  |  C: Career Stats", static_cast<int>(virtualWidth_ - 362), 506, 12, SKYBLUE);
    DrawText("> START / ESC / P: Pause  |  F11: Fullscreen", static_cast<int>(virtualWidth_ - 362), 528, 12, LIGHTGRAY);

    RenderBrandingFooter2D();
}

void RailGame::RenderGadgetronVendor2D() {
    DrawRectangle(0, 0, static_cast<int>(virtualWidth_), 720, { 8, 12, 24, 235 });

    float cardW = 1160.0f;
    float cardX = (virtualWidth_ - cardW) * 0.5f;

    DrawText("GADGETRON WEAPONS VENDOR", static_cast<int>(cardX), 26, 26, GOLD);

    char curr[96];
    std::snprintf(curr, sizeof(curr), "BOLTS: %d   |   RARITANIUM: %d", save_.bolts, save_.raritanium);
    int cw = MeasureText(curr, 20);
    DrawText(curr, static_cast<int>(cardX + cardW - cw), 30, 20, SKYBLUE);

    const auto& cat = GetWeaponCatalog();
    for (int i = 0; i < static_cast<int>(NUM_WEAPONS); ++i) {
        Rectangle r = { cardX, 68.0f + i * 82.0f, cardW, 76.0f };
        bool unlocked = save_.unlockedWeapons[i];
        bool equipped = (save_.equippedWeapon == i);
        int lvl = save_.weaponLevels[i];

        DrawRectangleRec(r, equipped ? Color{ 28, 44, 70, 245 } : Color{ 16, 22, 36, 230 });
        DrawRectangleLinesEx(r, equipped ? 2.5f : 1.5f, equipped ? GOLD : cat[i].projectileColor);

        bool isGold = save_.goldenWeapons[i];
        const char* title = isGold ? GOLDEN_WEAPON_CATALOG[i].name : ((lvl >= 5) ? cat[i].omegaName : cat[i].name);
        Color wCol = isGold ? GOLD : cat[i].projectileColor;
        DrawText(title, static_cast<int>(r.x + 18), static_cast<int>(r.y + 8), 19, wCol);
        DrawText(isGold ? GOLDEN_WEAPON_CATALOG[i].masteryPerk : cat[i].specialPerk,
                 static_cast<int>(r.x + 18), static_cast<int>(r.y + 31), 14, isGold ? GOLD : GOLD);
        DrawText(cat[i].description, static_cast<int>(r.x + 18), static_cast<int>(r.y + 51), 13, LIGHTGRAY);

        char lvlStr[32];
        if (isGold) std::snprintf(lvlStr, sizeof(lvlStr), "24K GOLD");
        else std::snprintf(lvlStr, sizeof(lvlStr), "LEVEL V%d", lvl);
        DrawText(lvlStr, static_cast<int>(r.x + 695), static_cast<int>(r.y + 28), 19, (isGold || lvl >= 5) ? GOLD : WHITE);

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
            if (lvl < 5) {
                int rCost = cat[i].upgradeRaritaniumCost * lvl;
                bool canUp = (save_.raritanium >= rCost);
                DrawRectangleRec(upBtn, canUp ? Color{ 30, 75, 105, 255 } : Color{ 35, 35, 45, 255 });
                char upStr[48];
                std::snprintf(upStr, sizeof(upStr), "+V%d (%d RARI)", lvl + 1, rCost);
                DrawText(upStr, static_cast<int>(upBtn.x + 14), static_cast<int>(upBtn.y + 14), 15,
                         canUp ? SKYBLUE : GRAY);
                if (IsButtonClicked(upBtn) && canUp) {
                    save_.raritanium -= rCost;
                    save_.weaponLevels[i]++;
                    SaveProgress();
                    audio_.PlayLevelUp();
                    if (save_.weaponLevels[i] >= 5) AwardSkillPoint(5);
                }
            } else if (!isGold) {
                int gRariCost = GOLDEN_WEAPON_CATALOG[i].unlockRaritaniumCost;
                int gBoltCost = GOLDEN_WEAPON_CATALOG[i].unlockBoltCost;
                bool canForge = (save_.raritanium >= gRariCost && save_.bolts >= gBoltCost);
                DrawRectangleRec(upBtn, canForge ? Color{ 140, 95, 20, 255 } : Color{ 45, 35, 30, 255 });
                DrawRectangleLinesEx(upBtn, 2.0f, canForge ? GOLD : GRAY);
                DrawText("FORGE 24K GOLD", static_cast<int>(upBtn.x + 10), static_cast<int>(upBtn.y + 8), 13, canForge ? GOLD : GRAY);
                char gCostStr[48];
                std::snprintf(gCostStr, sizeof(gCostStr), "%dB, %dR", gBoltCost, gRariCost);
                DrawText(gCostStr, static_cast<int>(upBtn.x + 18), static_cast<int>(upBtn.y + 24), 13, canForge ? WHITE : GRAY);
                if (IsButtonClicked(upBtn) && canForge) {
                    save_.bolts -= gBoltCost;
                    save_.raritanium -= gRariCost;
                    save_.goldenWeapons[i] = true;
                    SaveProgress();
                    audio_.PlayGoldenUpgrade();
                }
            } else {
                DrawRectangleRec(upBtn, Color{ 48, 38, 18, 255 });
                DrawRectangleLinesEx(upBtn, 1.5f, GOLD);
                DrawText("24K MASTERED", static_cast<int>(upBtn.x + 14), static_cast<int>(upBtn.y + 14), 14, GOLD);
            }
        }
    }

    Rectangle backBtn = { cardX, 568.0f, 220.0f, 46.0f };
    DrawRectangleRec(backBtn, IsRectHovered(backBtn) ? Color{ 45, 68, 100, 255 } : Color{ 24, 36, 56, 255 });
    DrawRectangleLinesEx(backBtn, 2.0f, ORANGE);
    DrawText("< BACK TO MENU", static_cast<int>(backBtn.x + 34), static_cast<int>(backBtn.y + 16), 16, WHITE);
    if (IsButtonClicked(backBtn)) {
        SetScreenMode(ScreenMode::MAIN_MENU);
        audio_.PlayUiClick();
    }
}

void RailGame::RenderRaritaniumLab2D() {
    DrawRectangle(0, 0, static_cast<int>(virtualWidth_), 720, { 8, 12, 24, 235 });

    float labW = 1150.0f;
    float labX = (virtualWidth_ - labW) * 0.5f;

    DrawText("RARITANIUM ENGINEERING LAB", static_cast<int>(labX), 40, 26, SKYBLUE);

    char curr[96];
    std::snprintf(curr, sizeof(curr), "BOLTS: %d   |   RARITANIUM: %d", save_.bolts, save_.raritanium);
    int cw = MeasureText(curr, 20);
    DrawText(curr, static_cast<int>(labX + labW - cw), 44, 20, GOLD);

    auto DrawModCard = [&](int idx, const char* title, const char* desc, int& lvl, int maxLvl, int baseBoltCost) {
        Rectangle r = { labX + (idx % 2) * 585.0f, 110.0f + (idx / 2) * 210.0f, 565.0f, 185.0f };
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

    // Overdrive Hoverboots Flagship Gadget Matrix Card
    Rectangle hgRec = { labX, 510.0f, 1150.0f, 85.0f };
    DrawRectangleRec(hgRec, save_.hoverbootsUnlocked ? Color{ 22, 38, 56, 240 } : Color{ 16, 24, 40, 235 });
    DrawRectangleLinesEx(hgRec, 2.0f, save_.hoverbootsUnlocked ? GOLD : ORANGE);
    DrawText("OVERDRIVE HOVERBOOTS (FLAGSHIP GADGET)", static_cast<int>(hgRec.x + 20), static_cast<int>(hgRec.y + 12), 20, GOLD);
    DrawText("Rocket-propelled boots. Press [SHIFT] / [B] / [BOOST] during runs for 3s supersonic rush obliterating crates & laser gates!",
             static_cast<int>(hgRec.x + 20), static_cast<int>(hgRec.y + 42), 14, LIGHTGRAY);

    Rectangle hgBtn = { hgRec.x + 865.0f, hgRec.y + 18.0f, 265.0f, 48.0f };
    if (save_.hoverbootsUnlocked) {
        DrawRectangleRec(hgBtn, Color{ 35, 75, 45, 255 });
        DrawRectangleLinesEx(hgBtn, 1.5f, GREEN);
        DrawText("GADGET ONLINE [ACTIVE]", static_cast<int>(hgBtn.x + 22), static_cast<int>(hgBtn.y + 16), 16, GREEN);
    } else {
        bool canBuyHg = (save_.bolts >= 1800 && save_.raritanium >= 10);
        DrawRectangleRec(hgBtn, canBuyHg ? Color{ 140, 95, 20, 255 } : Color{ 48, 32, 38, 255 });
        DrawRectangleLinesEx(hgBtn, 2.0f, canBuyHg ? GOLD : GRAY);
        DrawText("UNLOCK (1800 B, 10 RARI)", static_cast<int>(hgBtn.x + 18), static_cast<int>(hgBtn.y + 16), 15, canBuyHg ? WHITE : GRAY);
        if (IsButtonClicked(hgBtn) && canBuyHg) {
            save_.bolts -= 1800;
            save_.raritanium -= 10;
            save_.hoverbootsUnlocked = true;
            SaveProgress();
            audio_.PlayGoldenUpgrade();
        }
    }

    Rectangle backBtn = { labX, 608.0f, 220.0f, 46.0f };
    DrawRectangleRec(backBtn, IsRectHovered(backBtn) ? Color{ 45, 68, 100, 255 } : Color{ 24, 36, 56, 255 });
    DrawRectangleLinesEx(backBtn, 2.0f, ORANGE);
    DrawText("< BACK TO MENU", static_cast<int>(backBtn.x + 34), static_cast<int>(backBtn.y + 15), 16, WHITE);
    if (IsButtonClicked(backBtn)) {
        SetScreenMode(ScreenMode::MAIN_MENU);
        audio_.PlayUiClick();
    }
}

void RailGame::RenderArmorWardrobe2D() {
    // Top Header Banner
    DrawRectangle(0, 0, static_cast<int>(virtualWidth_), 115, { 10, 14, 26, 235 });
    DrawLine(0, 115, static_cast<int>(virtualWidth_), 115, PURPLE);

    const char* pre = "BOLTAX ADVANCED DEFENSE SYSTEMS & WARDROBE LAB";
    int pw = MeasureText(pre, 15);
    DrawText(pre, static_cast<int>((virtualWidth_ - pw) / 2), 16, 15, SKYBLUE);

    const char* title = "ARMOR SUITS & CLANK OUTFITS";
    int tw = MeasureText(title, 42);
    DrawText(title, static_cast<int>((virtualWidth_ - tw) / 2), 36, 42, GOLD);

    char curBuf[96];
    std::snprintf(curBuf, sizeof(curBuf), "BOLTS: %d   |   RARITANIUM: %d", save_.bolts, save_.raritanium);
    int cw = MeasureText(curBuf, 18);
    DrawText(curBuf, static_cast<int>((virtualWidth_ - cw) / 2), 85, 18, (save_.raritanium > 0) ? SKYBLUE : LIGHTGRAY);

    // -------------------------------------------------------------
    // LEFT SIDE: 3D CHARACTER SHOWCASE PLAQUE
    // -------------------------------------------------------------
    Rectangle plaque = { 50.0f, 524.0f, 500.0f, 122.0f };
    DrawRectangleRec(plaque, { 12, 18, 30, 220 });
    DrawRectangleLinesEx(plaque, 2.0f, PURPLE);

    const char* prevName = (wardrobeTab_ == 0)
        ? ARMOR_CATALOG[wardrobePreviewArmor_].name
        : CLANK_SKIN_CATALOG[wardrobePreviewClank_].name;
    const char* prevOrigin = (wardrobeTab_ == 0)
        ? ARMOR_CATALOG[wardrobePreviewArmor_].origin
        : CLANK_SKIN_CATALOG[wardrobePreviewClank_].origin;
    const char* prevPerk = (wardrobeTab_ == 0)
        ? ARMOR_CATALOG[wardrobePreviewArmor_].perkDesc
        : CLANK_SKIN_CATALOG[wardrobePreviewClank_].perkDesc;

    DrawText("3D LIVE PREVIEW:", static_cast<int>(plaque.x + 16), static_cast<int>(plaque.y + 8), 12, SKYBLUE);
    DrawText(prevName, static_cast<int>(plaque.x + 16), static_cast<int>(plaque.y + 24), 20, GOLD);
    DrawText(prevOrigin, static_cast<int>(plaque.x + 16), static_cast<int>(plaque.y + 49), 12, LIGHTGRAY);
    DrawText(prevPerk, static_cast<int>(plaque.x + 16), static_cast<int>(plaque.y + 68), 12, GREEN);

    DrawText("< DRAG MOUSE / SWIPE OR [A / D] TO ROTATE 3D CHARACTER >",
             static_cast<int>(plaque.x + 16), static_cast<int>(plaque.y + 94), 11, { 180, 205, 235, 200 });

    // -------------------------------------------------------------
    // RIGHT SIDE: TABS & CATALOG
    // -------------------------------------------------------------
    float rightColX = virtualWidth_ - 700.0f;
    Rectangle tabLombax = { rightColX, 130.0f, 320.0f, 44.0f };
    Rectangle tabClank  = { rightColX + 330.0f, 130.0f, 320.0f, 44.0f };

    bool hovL = IsRectHovered(tabLombax);
    bool hovC = IsRectHovered(tabClank);

    // Tab Lombax
    Color tabLCol = (wardrobeTab_ == 0) ? Color{ 48, 32, 85, 240 } : (hovL ? Color{ 26, 36, 56, 220 } : Color{ 14, 20, 32, 210 });
    DrawRectangleRec(tabLombax, tabLCol);
    DrawRectangleLinesEx(tabLombax, (wardrobeTab_ == 0) ? 2.5f : 1.0f, (wardrobeTab_ == 0) ? GOLD : GRAY);
    const char* tabLText = "LOMBAX HERO SUITS (6)";
    int tLw = MeasureText(tabLText, 17);
    DrawText(tabLText, static_cast<int>(tabLombax.x + (tabLombax.width - tLw) / 2), static_cast<int>(tabLombax.y + 13), 17, (wardrobeTab_ == 0) ? GOLD : WHITE);
    if (IsButtonClicked(tabLombax)) {
        wardrobeTab_ = 0;
        audio_.PlayUiClick();
    }

    // Tab Clank
    Color tabCCol = (wardrobeTab_ == 1) ? Color{ 48, 32, 85, 240 } : (hovC ? Color{ 26, 36, 56, 220 } : Color{ 14, 20, 32, 210 });
    DrawRectangleRec(tabClank, tabCCol);
    DrawRectangleLinesEx(tabClank, (wardrobeTab_ == 1) ? 2.5f : 1.0f, (wardrobeTab_ == 1) ? GOLD : GRAY);
    const char* tabCText = "COMPANION ROBOTS (5)";
    int tCw = MeasureText(tabCText, 17);
    DrawText(tabCText, static_cast<int>(tabClank.x + (tabClank.width - tCw) / 2), static_cast<int>(tabClank.y + 13), 17, (wardrobeTab_ == 1) ? GOLD : WHITE);
    if (IsButtonClicked(tabClank)) {
        wardrobeTab_ = 1;
        audio_.PlayUiClick();
    }

    // Content Cards
    float startY = 184.0f;
    float cardH  = 74.0f;
    float gapY   = 6.0f;

    if (wardrobeTab_ == 0) {
        for (int i = 0; i < static_cast<int>(NUM_ARMOR_SUITS); ++i) {
            const auto& a = ARMOR_CATALOG[i];
            Rectangle cardRec = { rightColX, startY + i * (cardH + gapY), 650.0f, cardH };
            bool isPrev = (wardrobePreviewArmor_ == i);
            bool isEquipped = (save_.equippedArmor == i);
            bool isUnlocked = save_.unlockedArmors[i];
            bool hov = IsRectHovered(cardRec);

            DrawRectangleRec(cardRec, isPrev ? Color{ 28, 42, 68, 240 } : (hov ? Color{ 20, 30, 48, 220 } : Color{ 12, 18, 30, 200 }));
            DrawRectangleLinesEx(cardRec, isPrev ? 2.0f : 1.0f, isEquipped ? GREEN : (isPrev ? GOLD : GRAY));

            // Color Swatch Indicator Bar
            DrawRectangle(static_cast<int>(cardRec.x), static_cast<int>(cardRec.y), 8, static_cast<int>(cardRec.height), a.visorColor);

            // Title & Lore
            DrawText(a.name, static_cast<int>(cardRec.x + 20), static_cast<int>(cardRec.y + 8), 18, isEquipped ? GREEN : (isPrev ? GOLD : WHITE));
            DrawText(a.origin, static_cast<int>(cardRec.x + 20), static_cast<int>(cardRec.y + 30), 12, SKYBLUE);
            DrawText(a.perkDesc, static_cast<int>(cardRec.x + 20), static_cast<int>(cardRec.y + 50), 12, LIGHTGRAY);

            // Action / Status Button on Right
            Rectangle btnAction = { cardRec.x + cardRec.width - 165.0f, cardRec.y + 16.0f, 150.0f, 42.0f };
            if (isEquipped) {
                DrawRectangleRec(btnAction, { 22, 65, 34, 220 });
                DrawRectangleLinesEx(btnAction, 1.5f, GREEN);
                int ew = MeasureText("EQUIPPED", 15);
                DrawText("EQUIPPED", static_cast<int>(btnAction.x + (btnAction.width - ew) / 2), static_cast<int>(btnAction.y + 13), 15, GREEN);
            } else if (isUnlocked) {
                bool bHov = IsRectHovered(btnAction);
                DrawRectangleRec(btnAction, bHov ? Color{ 40, 85, 130, 240 } : Color{ 22, 50, 80, 220 });
                DrawRectangleLinesEx(btnAction, 1.5f, bHov ? WHITE : SKYBLUE);
                int ew = MeasureText("EQUIP", 16);
                DrawText("EQUIP", static_cast<int>(btnAction.x + (btnAction.width - ew) / 2), static_cast<int>(btnAction.y + 13), 16, WHITE);
                if (IsButtonClicked(btnAction)) {
                    save_.equippedArmor = i;
                    wardrobePreviewArmor_ = i;
                    audio_.PlayUiClick();
                    SaveProgress();
                }
            } else {
                bool canAfford = (save_.bolts >= a.boltCost && save_.raritanium >= a.raritaniumCost);
                bool bHov = IsRectHovered(btnAction);
                DrawRectangleRec(btnAction, canAfford ? (bHov ? Color{ 140, 75, 20, 240 } : Color{ 90, 48, 14, 220 }) : Color{ 36, 36, 42, 200 });
                DrawRectangleLinesEx(btnAction, 1.5f, canAfford ? ORANGE : DARKGRAY);

                char costBuf[48];
                if (a.raritaniumCost > 0) {
                    std::snprintf(costBuf, sizeof(costBuf), "%d B / %d R", a.boltCost, a.raritaniumCost);
                } else {
                    std::snprintf(costBuf, sizeof(costBuf), "%d BOLTS", a.boltCost);
                }
                int bw = MeasureText(costBuf, 12);
                int uw = MeasureText("UNLOCK", 13);
                DrawText("UNLOCK", static_cast<int>(btnAction.x + (btnAction.width - uw) / 2), static_cast<int>(btnAction.y + 6), 13, canAfford ? GOLD : GRAY);
                DrawText(costBuf, static_cast<int>(btnAction.x + (btnAction.width - bw) / 2), static_cast<int>(btnAction.y + 23), 12, canAfford ? WHITE : LIGHTGRAY);

                if (canAfford && IsButtonClicked(btnAction)) {
                    save_.bolts -= a.boltCost;
                    save_.raritanium -= a.raritaniumCost;
                    save_.unlockedArmors[i] = true;
                    save_.equippedArmor = i;
                    wardrobePreviewArmor_ = i;
                    audio_.PlayLevelUp();
                    SaveProgress();
                }
            }

            if (IsButtonClicked(cardRec) && !IsRectHovered(btnAction)) {
                wardrobePreviewArmor_ = i;
                audio_.PlayUiClick();
            }
        }
    } else {
        // Clank Outfits Tab
        for (int i = 0; i < static_cast<int>(NUM_CLANK_SKINS); ++i) {
            const auto& c = CLANK_SKIN_CATALOG[i];
            Rectangle cardRec = { rightColX, startY + i * (cardH + 10.0f), 650.0f, cardH };
            bool isPrev = (wardrobePreviewClank_ == i);
            bool isEquipped = (save_.equippedClankSkin == i);
            bool isUnlocked = save_.unlockedClankSkins[i];
            bool hov = IsRectHovered(cardRec);

            DrawRectangleRec(cardRec, isPrev ? Color{ 28, 42, 68, 240 } : (hov ? Color{ 20, 30, 48, 220 } : Color{ 12, 18, 30, 200 }));
            DrawRectangleLinesEx(cardRec, isPrev ? 2.0f : 1.0f, isEquipped ? GREEN : (isPrev ? GOLD : GRAY));

            // Eye Glow Color Bar
            DrawRectangle(static_cast<int>(cardRec.x), static_cast<int>(cardRec.y), 8, static_cast<int>(cardRec.height), c.eyeColor);

            DrawText(c.name, static_cast<int>(cardRec.x + 20), static_cast<int>(cardRec.y + 8), 18, isEquipped ? GREEN : (isPrev ? GOLD : WHITE));
            DrawText(c.origin, static_cast<int>(cardRec.x + 20), static_cast<int>(cardRec.y + 30), 12, SKYBLUE);
            DrawText(c.perkDesc, static_cast<int>(cardRec.x + 20), static_cast<int>(cardRec.y + 50), 12, LIGHTGRAY);

            Rectangle btnAction = { cardRec.x + cardRec.width - 165.0f, cardRec.y + 16.0f, 150.0f, 42.0f };
            if (isEquipped) {
                DrawRectangleRec(btnAction, { 22, 65, 34, 220 });
                DrawRectangleLinesEx(btnAction, 1.5f, GREEN);
                int ew = MeasureText("EQUIPPED", 15);
                DrawText("EQUIPPED", static_cast<int>(btnAction.x + (btnAction.width - ew) / 2), static_cast<int>(btnAction.y + 13), 15, GREEN);
            } else if (isUnlocked) {
                bool bHov = IsRectHovered(btnAction);
                DrawRectangleRec(btnAction, bHov ? Color{ 40, 85, 130, 240 } : Color{ 22, 50, 80, 220 });
                DrawRectangleLinesEx(btnAction, 1.5f, bHov ? WHITE : SKYBLUE);
                int ew = MeasureText("EQUIP", 16);
                DrawText("EQUIP", static_cast<int>(btnAction.x + (btnAction.width - ew) / 2), static_cast<int>(btnAction.y + 13), 16, WHITE);
                if (IsButtonClicked(btnAction)) {
                    save_.equippedClankSkin = i;
                    wardrobePreviewClank_ = i;
                    audio_.PlayUiClick();
                    SaveProgress();
                }
            } else {
                bool canAfford = (save_.bolts >= c.boltCost && save_.raritanium >= c.raritaniumCost);
                bool bHov = IsRectHovered(btnAction);
                DrawRectangleRec(btnAction, canAfford ? (bHov ? Color{ 140, 75, 20, 240 } : Color{ 90, 48, 14, 220 }) : Color{ 36, 36, 42, 200 });
                DrawRectangleLinesEx(btnAction, 1.5f, canAfford ? ORANGE : DARKGRAY);

                char costBuf[48];
                if (c.raritaniumCost > 0) {
                    std::snprintf(costBuf, sizeof(costBuf), "%d B / %d R", c.boltCost, c.raritaniumCost);
                } else {
                    std::snprintf(costBuf, sizeof(costBuf), "%d BOLTS", c.boltCost);
                }
                int bw = MeasureText(costBuf, 12);
                int uw = MeasureText("UNLOCK", 13);
                DrawText("UNLOCK", static_cast<int>(btnAction.x + (btnAction.width - uw) / 2), static_cast<int>(btnAction.y + 6), 13, canAfford ? GOLD : GRAY);
                DrawText(costBuf, static_cast<int>(btnAction.x + (btnAction.width - bw) / 2), static_cast<int>(btnAction.y + 23), 12, canAfford ? WHITE : LIGHTGRAY);

                if (canAfford && IsButtonClicked(btnAction)) {
                    save_.bolts -= c.boltCost;
                    save_.raritanium -= c.raritaniumCost;
                    save_.unlockedClankSkins[i] = true;
                    save_.equippedClankSkin = i;
                    wardrobePreviewClank_ = i;
                    audio_.PlayLevelUp();
                    SaveProgress();
                }
            }

            if (IsButtonClicked(cardRec) && !IsRectHovered(btnAction)) {
                wardrobePreviewClank_ = i;
                audio_.PlayUiClick();
            }
        }
    }

    // Back to Menu Button
    Rectangle backBtn = { 50.0f, 655.0f, 220.0f, 44.0f };
    bool bHov = IsRectHovered(backBtn);
    DrawRectangleRec(backBtn, bHov ? Color{ 45, 68, 100, 255 } : Color{ 24, 36, 56, 255 });
    DrawRectangleLinesEx(backBtn, 1.5f, bHov ? WHITE : ORANGE);
    DrawText("< BACK TO MENU", static_cast<int>(backBtn.x + 36), static_cast<int>(backBtn.y + 14), 16, WHITE);
    if (IsButtonClicked(backBtn)) {
        SetScreenMode(ScreenMode::MAIN_MENU);
        audio_.PlayUiClick();
    }
}

void RailGame::RenderSettings2D() {
    DrawRectangle(0, 0, static_cast<int>(virtualWidth_), 720, { 8, 12, 24, 245 });

    float setW = 1100.0f;
    float setX = (virtualWidth_ - setW) * 0.5f;
    float rightColX = setX + 590.0f;

    const char* sTitle = "SYSTEM SETTINGS // AUDIO, VISUALS & CONTROLS";
    DrawText(sTitle, static_cast<int>(setX), 32, 26, WHITE);

    char devBuf[128];
    std::snprintf(devBuf, sizeof(devBuf), "ACTIVE INPUT: %s", activeInputDevice_.c_str());
    int dw = MeasureText(devBuf, 16);
    DrawText(devBuf, static_cast<int>(setX + setW - dw), 38, 16, GOLD);

    DrawLine(static_cast<int>(setX), 72, static_cast<int>(setX + setW), 72, { 255, 255, 255, 55 });

    // --- LEFT COLUMN: AUDIO CONTROLS ---
    DrawText("AUDIO BALANCING", static_cast<int>(setX), 92, 18, SKYBLUE);

    auto DrawSlider = [&](float y, const char* label, float& val, Color barCol) {
        DrawText(label, static_cast<int>(setX), static_cast<int>(y), 16, WHITE);
        Rectangle track = { setX, y + 26.0f, 480.0f, 22.0f };
        DrawRectangleRec(track, { 18, 24, 38, 255 });
        DrawRectangleLinesEx(track, 1.5f, { 80, 110, 150, 200 });

        float fillW = std::clamp(val, 0.0f, 1.0f) * track.width;
        DrawRectangle(static_cast<int>(track.x), static_cast<int>(track.y), static_cast<int>(fillW), static_cast<int>(track.height), barCol);

        char pct[16];
        std::snprintf(pct, sizeof(pct), "%d%%", static_cast<int>(val * 100.0f));
        DrawText(pct, static_cast<int>(track.x + track.width + 16.0f), static_cast<int>(track.y + 2.0f), 16, WHITE);

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || GetTouchPointCount() > 0) {
            Vector2 p = (GetTouchPointCount() > 0) ? ToVirtualCoords(GetTouchPosition(0)) : GetVirtualMouse();
            Rectangle hit = { track.x - 10.0f, track.y - 10.0f, track.width + 20.0f, track.height + 20.0f };
            if (CheckCollisionPointRec(p, hit)) {
                val = std::clamp((p.x - track.x) / track.width, 0.0f, 1.0f);
                audio_.SetMasterVolume(save_.masterVolume);
                audio_.SetMusicVolume(save_.musicVolume);
                audio_.SetSfxVolume(save_.sfxVolume);
            }
        }
    };

    DrawSlider(124.0f, "MASTER VOLUME", save_.masterVolume, GOLD);
    DrawSlider(188.0f, "MUSIC VOLUME (BGM)", save_.musicVolume, SKYBLUE);
    DrawSlider(252.0f, "SOUND EFFECTS (SFX)", save_.sfxVolume, ORANGE);

    // Mute Toggle Button
    Rectangle muteBtn = { setX, 324.0f, 240.0f, 44.0f };
    bool isMuted = audio_.IsMuted();
    DrawRectangleRec(muteBtn, isMuted ? Color{ 75, 25, 30, 255 } : Color{ 22, 45, 38, 255 });
    DrawRectangleLinesEx(muteBtn, 1.8f, isMuted ? RED : GREEN);
    DrawText(isMuted ? "MUTED [M]" : "AUDIO ACTIVE [M]", static_cast<int>(muteBtn.x + 38), static_cast<int>(muteBtn.y + 13), 16, WHITE);
    if (IsButtonClicked(muteBtn)) {
        audio_.ToggleMute();
        audio_.ApplyVolumes();
    }

    // Screen Flashes Toggle
    DrawText("SCREEN FLASH EFFECTS:", static_cast<int>(setX), 382, 15, WHITE);
    Rectangle flashOnBtn = { setX, 404.0f, 115.0f, 36.0f };
    Rectangle flashOffBtn = { setX + 125.0f, 404.0f, 115.0f, 36.0f };
    DrawRectangleRec(flashOnBtn, save_.screenFlashes ? Color{ 35, 65, 105, 255 } : Color{ 18, 24, 38, 220 });
    DrawRectangleLinesEx(flashOnBtn, save_.screenFlashes ? 2.0f : 1.2f, save_.screenFlashes ? GOLD : GRAY);
    DrawText("ENABLED", static_cast<int>(flashOnBtn.x + 22), static_cast<int>(flashOnBtn.y + 10), 13, save_.screenFlashes ? GOLD : LIGHTGRAY);
    if (IsButtonClicked(flashOnBtn)) { save_.screenFlashes = true; audio_.PlayUiClick(); }

    DrawRectangleRec(flashOffBtn, !save_.screenFlashes ? Color{ 35, 65, 105, 255 } : Color{ 18, 24, 38, 220 });
    DrawRectangleLinesEx(flashOffBtn, !save_.screenFlashes ? 2.0f : 1.2f, !save_.screenFlashes ? GOLD : GRAY);
    DrawText("DISABLED", static_cast<int>(flashOffBtn.x + 18), static_cast<int>(flashOffBtn.y + 10), 13, !save_.screenFlashes ? GOLD : LIGHTGRAY);
    if (IsButtonClicked(flashOffBtn)) { save_.screenFlashes = false; audio_.PlayUiClick(); }

    // --- GAMEPLAY SOUNDTRACK JUKEBOX SELECTOR ---
    DrawText("GAMEPLAY SOUNDTRACK JUKEBOX [N]:", static_cast<int>(setX), 456, 15, WHITE);
    Rectangle prevTrackBtn = { setX, 478.0f, 42.0f, 38.0f };
    Rectangle nextTrackBtn = { setX + 478.0f, 478.0f, 42.0f, 38.0f };
    Rectangle trackCardRec = { setX + 48.0f, 478.0f, 424.0f, 38.0f };

    DrawRectangleRec(prevTrackBtn, IsRectHovered(prevTrackBtn) ? Color{ 40, 65, 100, 255 } : Color{ 20, 28, 44, 220 });
    DrawRectangleLinesEx(prevTrackBtn, 1.5f, SKYBLUE);
    DrawText("<", static_cast<int>(prevTrackBtn.x + 15), static_cast<int>(prevTrackBtn.y + 9), 18, WHITE);
    if (IsButtonClicked(prevTrackBtn)) {
        save_.musicTrackChoice = (save_.musicTrackChoice + 4) % 5;
        audio_.SetMusicTrackPreference(save_.musicTrackChoice);
        audio_.PlayUiClick();
    }

    DrawRectangleRec(nextTrackBtn, IsRectHovered(nextTrackBtn) ? Color{ 40, 65, 100, 255 } : Color{ 20, 28, 44, 220 });
    DrawRectangleLinesEx(nextTrackBtn, 1.5f, SKYBLUE);
    DrawText(">", static_cast<int>(nextTrackBtn.x + 16), static_cast<int>(nextTrackBtn.y + 9), 18, WHITE);
    if (IsButtonClicked(nextTrackBtn)) {
        save_.musicTrackChoice = (save_.musicTrackChoice + 1) % 5;
        audio_.SetMusicTrackPreference(save_.musicTrackChoice);
        audio_.PlayUiClick();
    }

    DrawRectangleRec(trackCardRec, { 16, 24, 38, 240 });
    DrawRectangleLinesEx(trackCardRec, 1.5f, save_.musicTrackChoice == 0 ? GOLD : SKYBLUE);
    const char* tLabel = (save_.musicTrackChoice == 0)
        ? "AUTO-SHUFFLE ALL 4 TRACKS [DEFAULT]"
        : audio_.GetActionTrackName(save_.musicTrackChoice - 1);
    int tlw = MeasureText(tLabel, 12);
    DrawText(tLabel, static_cast<int>(trackCardRec.x + (trackCardRec.width - tlw) / 2.0f), static_cast<int>(trackCardRec.y + 12), 12, save_.musicTrackChoice == 0 ? GOLD : WHITE);

    DrawText("* Press [N] during gameplay to skip track instantly", static_cast<int>(setX), 524, 12, { 140, 180, 220, 200 });

    // --- RIGHT COLUMN: VISUALS, GAMEPAD & CONTROLS ---
    DrawText("VISUALS, CONTROLS & GAMEPAD", static_cast<int>(rightColX), 92, 18, SKYBLUE);

    // Screen Shake Intensity
    DrawText("CAMERA SHAKE INTENSITY:", static_cast<int>(rightColX), 124, 16, WHITE);
    const char* shakeLabels[3] = { "OFF (0%)", "NORMAL (50%)", "INTENSE (100%)" };
    float shakeVals[3] = { 0.0f, 0.5f, 1.0f };
    for (int i = 0; i < 3; ++i) {
        Rectangle sb = { rightColX + i * 168.0f, 150.0f, 156.0f, 38.0f };
        bool sel = (std::abs(save_.screenShakeScale - shakeVals[i]) < 0.15f);
        DrawRectangleRec(sb, sel ? Color{ 35, 65, 105, 255 } : Color{ 18, 24, 38, 220 });
        DrawRectangleLinesEx(sb, sel ? 2.0f : 1.2f, sel ? GOLD : GRAY);
        DrawText(shakeLabels[i], static_cast<int>(sb.x + 16), static_cast<int>(sb.y + 11), 14, sel ? GOLD : LIGHTGRAY);
        if (IsButtonClicked(sb)) {
            save_.screenShakeScale = shakeVals[i];
            audio_.PlayUiClick();
        }
    }

    // Target FPS
    DrawText("TARGET PERFORMANCE FPS:", static_cast<int>(rightColX), 206, 16, WHITE);
    int fpsVals[3] = { 60, 120, 30 };
    const char* fpsLabels[3] = { "60 FPS (BALANCED)", "120 FPS (ULTRA)", "30 FPS (SAVER)" };
    for (int f = 0; f < 3; ++f) {
        Rectangle fb = { rightColX + f * 168.0f, 232.0f, 156.0f, 38.0f };
        bool sel = (save_.targetFps == fpsVals[f]);
        DrawRectangleRec(fb, sel ? Color{ 35, 65, 105, 255 } : Color{ 18, 24, 38, 220 });
        DrawRectangleLinesEx(fb, sel ? 2.0f : 1.2f, sel ? GOLD : GRAY);
        DrawText(fpsLabels[f], static_cast<int>(fb.x + 8), static_cast<int>(fb.y + 11), 12, sel ? GOLD : LIGHTGRAY);
        if (IsButtonClicked(fb)) {
            save_.targetFps = fpsVals[f];
            SetTargetFPS(save_.targetFps);
            audio_.PlayUiClick();
        }
    }

    // Swipe Sensitivity
    DrawText("SWIPE RAIL-SWITCH SENSITIVITY:", static_cast<int>(rightColX), 288, 16, WHITE);
    const char* sensLabels[3] = { "LOW (PRECISE)", "MEDIUM (NORMAL)", "HIGH (FAST)" };
    for (int s = 0; s < 3; ++s) {
        Rectangle sensBtn = { rightColX + s * 168.0f, 314.0f, 156.0f, 38.0f };
        bool sel = (save_.swipeSensitivity == s);
        DrawRectangleRec(sensBtn, sel ? Color{ 35, 65, 105, 255 } : Color{ 18, 24, 38, 220 });
        DrawRectangleLinesEx(sensBtn, sel ? 2.0f : 1.2f, sel ? GOLD : GRAY);
        DrawText(sensLabels[s], static_cast<int>(sensBtn.x + 12), static_cast<int>(sensBtn.y + 11), 12, sel ? GOLD : LIGHTGRAY);
        if (IsButtonClicked(sensBtn)) {
            save_.swipeSensitivity = s;
            audio_.PlayUiClick();
        }
    }

    // Handedness
    DrawText("MOBILE ACTION BUTTONS HANDEDNESS:", static_cast<int>(rightColX), 370, 16, WHITE);
    Rectangle handRight = { rightColX, 396.0f, 210.0f, 38.0f };
    Rectangle handLeft  = { rightColX + 222.0f, 396.0f, 210.0f, 38.0f };
    DrawRectangleRec(handRight, !save_.leftHandedControls ? Color{ 35, 65, 105, 255 } : Color{ 18, 24, 38, 220 });
    DrawRectangleLinesEx(handRight, !save_.leftHandedControls ? 2.0f : 1.2f, !save_.leftHandedControls ? GOLD : GRAY);
    DrawText("RIGHT-HANDED (DEFAULT)", static_cast<int>(handRight.x + 12), static_cast<int>(handRight.y + 11), 13, !save_.leftHandedControls ? GOLD : LIGHTGRAY);
    if (IsButtonClicked(handRight)) { save_.leftHandedControls = false; audio_.PlayUiClick(); }

    DrawRectangleRec(handLeft, save_.leftHandedControls ? Color{ 35, 65, 105, 255 } : Color{ 18, 24, 38, 220 });
    DrawRectangleLinesEx(handLeft, save_.leftHandedControls ? 2.0f : 1.2f, save_.leftHandedControls ? GOLD : GRAY);
    DrawText("LEFT-HANDED (MIRRORED)", static_cast<int>(handLeft.x + 12), static_cast<int>(handLeft.y + 11), 13, save_.leftHandedControls ? GOLD : LIGHTGRAY);
    if (IsButtonClicked(handLeft)) { save_.leftHandedControls = true; audio_.PlayUiClick(); }

    // Clear Save Data Button (Right Column)
    Rectangle resetBtn = { rightColX, 468.0f, 310.0f, 42.0f };
    DrawRectangleRec(resetBtn, showResetConfirm_ ? Color{ 110, 30, 35, 255 } : Color{ 48, 20, 26, 230 });
    DrawRectangleLinesEx(resetBtn, 2.0f, RED);
    DrawText(showResetConfirm_ ? "CONFIRM RESET PROGRESS?" : "CLEAR SAVE DATA (RESET)", static_cast<int>(resetBtn.x + 18), static_cast<int>(resetBtn.y + 13), 14, WHITE);
    if (IsButtonClicked(resetBtn)) {
        if (!showResetConfirm_) {
            showResetConfirm_ = true;
            audio_.PlayUiClick();
        } else {
            SaveData fresh{};
            fresh.masterVolume = save_.masterVolume;
            fresh.musicVolume = save_.musicVolume;
            fresh.sfxVolume = save_.sfxVolume;
            fresh.screenShakeScale = save_.screenShakeScale;
            fresh.screenFlashes = save_.screenFlashes;
            fresh.targetFps = save_.targetFps;
            fresh.leftHandedControls = save_.leftHandedControls;
            fresh.swipeSensitivity = save_.swipeSensitivity;
            fresh.musicTrackChoice = save_.musicTrackChoice;
            save_ = fresh;
            SaveProgress();
            showResetConfirm_ = false;
            audio_.PlayExplosion();
        }
    }
    if (showResetConfirm_) {
        Rectangle cancelReset = { rightColX + 325.0f, 468.0f, 110.0f, 42.0f };
        DrawRectangleRec(cancelReset, { 28, 38, 52, 240 });
        DrawRectangleLinesEx(cancelReset, 1.5f, SKYBLUE);
        DrawText("CANCEL", static_cast<int>(cancelReset.x + 20), static_cast<int>(cancelReset.y + 13), 14, WHITE);
        if (IsButtonClicked(cancelReset)) { showResetConfirm_ = false; audio_.PlayUiClick(); }
    }

    // --- BOTTOM ROW: FOOTER INFO & BACK ---
    DrawLine(static_cast<int>(setX), 545, static_cast<int>(setX + setW), 545, { 255, 255, 255, 45 });

    const char* bInfo = "RATCHET & CLANK: RAILFINITY  |  v1.3.0 EXPANSION  |  SEBINY LABS ENGINE";
    int biw = MeasureText(bInfo, 13);
    DrawText(bInfo, static_cast<int>((virtualWidth_ - biw) / 2), 555, 13, { 140, 175, 215, 180 });

    Rectangle backBtn = { setX, 580.0f, 260.0f, 48.0f };
    DrawRectangleRec(backBtn, IsRectHovered(backBtn) ? Color{ 45, 68, 100, 255 } : Color{ 24, 36, 56, 255 });
    DrawRectangleLinesEx(backBtn, 2.0f, ORANGE);
    DrawText("< BACK", static_cast<int>(backBtn.x + 98), static_cast<int>(backBtn.y + 16), 16, WHITE);
    if (IsButtonClicked(backBtn) || IsKeyPressed(KEY_ESCAPE) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) {
        SaveProgress();
        SetScreenMode(settingsPrevMode_);
        audio_.PlayUiClick();
    }
}

void RailGame::RenderCareerStats2D() {
    DrawRectangle(0, 0, static_cast<int>(virtualWidth_), 720, { 8, 12, 24, 245 });

    float cardW = 1160.0f;
    float cardX = (virtualWidth_ - cardW) * 0.5f;

    const char* sTitle = "GALACTIC CAREER DOSSIER";
    DrawText(sTitle, static_cast<int>(cardX), 32, 22, GOLD);

    char boltBuf[128];
    std::snprintf(boltBuf, sizeof(boltBuf), "CURRENT: %d BOLTS  |  %d RARITANIUM", save_.bolts, save_.raritanium);
    int bbw = MeasureText(boltBuf, 15);
    DrawText(boltBuf, static_cast<int>(cardX + cardW - bbw), 36, 15, SKYBLUE);

    DrawLine(static_cast<int>(cardX), 68, static_cast<int>(cardX + cardW), 68, { 255, 255, 255, 55 });

    auto DrawStatRow = [](float x, float y, float w, const char* label, const std::string& val, Color valCol) {
        DrawText(label, static_cast<int>(x), static_cast<int>(y), 15, WHITE);
        int vw = MeasureText(val.c_str(), 15);
        DrawText(val.c_str(), static_cast<int>(x + w - vw), static_cast<int>(y), 15, valCol);
        DrawLine(static_cast<int>(x), static_cast<int>(y + 24), static_cast<int>(x + w), static_cast<int>(y + 24), { 255, 255, 255, 20 });
    };

    // LEFT CARD: FLIGHT & COMBAT RECORDS
    Rectangle leftCard = { cardX, 84.0f, 560.0f, 475.0f };
    DrawRectangleRec(leftCard, { 14, 20, 36, 230 });
    DrawRectangleLinesEx(leftCard, 2.0f, ORANGE);
    DrawRectangle(static_cast<int>(leftCard.x), static_cast<int>(leftCard.y), static_cast<int>(leftCard.width), 34, { 38, 22, 16, 240 });
    DrawText("FLIGHT, COMBAT & ENDURANCE RECORDS", static_cast<int>(leftCard.x + 18), static_cast<int>(leftCard.y + 9), 15, GOLD);

    float sy = leftCard.y + 48.0f;
    DrawStatRow(leftCard.x + 20, sy, leftCard.width - 40, "Total Grind Distance (Lifetime):", std::to_string(save_.totalDistanceGrinded) + " m", SKYBLUE);
    sy += 38.0f;
    DrawStatRow(leftCard.x + 20, sy, leftCard.width - 40, "Best Single Run Distance:", std::to_string(save_.bestDistanceMeters) + " m", GOLD);
    sy += 38.0f;
    DrawStatRow(leftCard.x + 20, sy, leftCard.width - 40, "Best Seeded Daily Challenge:", std::to_string(save_.bestDailyMeters) + " m", GOLD);
    sy += 38.0f;
    DrawStatRow(leftCard.x + 20, sy, leftCard.width - 40, "Total Runs Launched:", std::to_string(save_.totalRunsStarted), WHITE);
    sy += 38.0f;
    DrawStatRow(leftCard.x + 20, sy, leftCard.width - 40, "Drones & Thugs Vaporized:", std::to_string(save_.totalEnemiesDestroyed), ORANGE);
    sy += 38.0f;
    DrawStatRow(leftCard.x + 20, sy, leftCard.width - 40, "Warships & Dread-Frigates Defeated:", std::to_string(save_.totalBossesDefeated), RED);
    sy += 38.0f;
    DrawStatRow(leftCard.x + 20, sy, leftCard.width - 40, "Crates & Chests Smashed:", std::to_string(save_.totalCratesSmashed), GREEN);
    sy += 38.0f;
    DrawStatRow(leftCard.x + 20, sy, leftCard.width - 40, "Overdrive Thruster Blasts Fired:", std::to_string(save_.totalOverdriveBoosts), GOLD);
    sy += 38.0f;
    DrawStatRow(leftCard.x + 20, sy, leftCard.width - 40, "Swingshot Grapples Cleared:", std::to_string(save_.totalSwingshotGrapples), GREEN);
    sy += 38.0f;
    DrawStatRow(leftCard.x + 20, sy, leftCard.width - 40, "Equipped Gadgetron Weapon:", GetWeaponCatalog()[save_.equippedWeapon].name, SKYBLUE);

    // RIGHT CARD: ARSENAL & HARDWARE ACCREDITATION
    Rectangle rightCard = { cardX + 600.0f, 84.0f, 560.0f, 475.0f };
    DrawRectangleRec(rightCard, { 14, 20, 36, 230 });
    DrawRectangleLinesEx(rightCard, 2.0f, SKYBLUE);
    DrawRectangle(static_cast<int>(rightCard.x), static_cast<int>(rightCard.y), static_cast<int>(rightCard.width), 34, { 18, 28, 48, 240 });
    DrawText("ARSENAL & HARDWARE ACCREDITATION", static_cast<int>(rightCard.x + 18), static_cast<int>(rightCard.y + 9), 15, SKYBLUE);

    int wepCount = 0;
    for (bool b : save_.unlockedWeapons) if (b) wepCount++;
    int goldCount = 0;
    for (bool b : save_.goldenWeapons) if (b) goldCount++;
    int spCount = 0;
    for (bool b : save_.skillPoints) if (b) spCount++;

    sy = rightCard.y + 48.0f;
    DrawStatRow(rightCard.x + 20, sy, rightCard.width - 40, "All-Time Titanium Bolts Collected:", std::to_string(save_.totalBoltsEarnedLifetime) + " B", GOLD);
    sy += 38.0f;
    DrawStatRow(rightCard.x + 20, sy, rightCard.width - 40, "All-Time Raritanium Ore Mined:", std::to_string(save_.totalRaritaniumEarnedLifetime) + " Rari", SKYBLUE);
    sy += 38.0f;
    DrawStatRow(rightCard.x + 20, sy, rightCard.width - 40, "Gadgetron Weapons Owned:", std::to_string(wepCount) + " / " + std::to_string(NUM_WEAPONS), WHITE);
    sy += 38.0f;
    DrawStatRow(rightCard.x + 20, sy, rightCard.width - 40, "24K Gold Weapons Forged:", std::to_string(goldCount) + " / " + std::to_string(NUM_WEAPONS), GOLD);
    sy += 38.0f;
    DrawStatRow(rightCard.x + 20, sy, rightCard.width - 40, "Overdrive Hoverboots Status:", save_.hoverbootsUnlocked ? "UNLOCKED & OPERATIONAL" : "LOCKED IN RARITANIUM LAB", save_.hoverbootsUnlocked ? GREEN : RED);
    sy += 38.0f;
    DrawStatRow(rightCard.x + 20, sy, rightCard.width - 40, "Galactic Skill Points Mastered:", std::to_string(spCount) + " / " + std::to_string(NUM_SKILL_POINTS), MAGENTA);
    sy += 38.0f;
    DrawStatRow(rightCard.x + 20, sy, rightCard.width - 40, "Active Nanotech Armor Suit:", ARMOR_CATALOG[save_.equippedArmor].name, PURPLE);
    sy += 38.0f;
    DrawStatRow(rightCard.x + 20, sy, rightCard.width - 40, "Equipped Clank Robotic Skin:", CLANK_SKIN_CATALOG[save_.equippedClankSkin].name, SKYBLUE);
    sy += 38.0f;
    DrawStatRow(rightCard.x + 20, sy, rightCard.width - 40, "Current Soundtrack:", audio_.GetCurrentActionTrackName(), ORANGE);

    // Bottom Navigation Bar
    DrawLine(static_cast<int>(cardX), 580, static_cast<int>(cardX + cardW), 580, { 255, 255, 255, 45 });
    Rectangle statsBackBtn = { cardX, 600.0f, 260.0f, 48.0f };
    DrawRectangleRec(statsBackBtn, IsRectHovered(statsBackBtn) ? Color{ 45, 68, 100, 255 } : Color{ 24, 36, 56, 255 });
    DrawRectangleLinesEx(statsBackBtn, 2.0f, ORANGE);
    DrawText("< BACK TO MENU [ESC]", static_cast<int>(statsBackBtn.x + 36), static_cast<int>(statsBackBtn.y + 16), 16, WHITE);
    if (IsButtonClicked(statsBackBtn) || IsKeyPressed(KEY_ESCAPE) || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))) {
        SetScreenMode(ScreenMode::MAIN_MENU);
        audio_.PlayUiClick();
    }
}

void RailGame::RenderSkillPoints2D() {
    DrawRectangle(0, 0, static_cast<int>(virtualWidth_), 720, { 8, 12, 24, 235 });

    float spW = 1140.0f;
    float spX = (virtualWidth_ - spW) * 0.5f;

    DrawText("INTERGALACTIC SKILL POINTS // CHALLENGES & REWARDS", static_cast<int>(spX), 38, 28, GOLD);

    const auto& cat = GetSkillPointCatalog();
    for (int i = 0; i < static_cast<int>(NUM_SKILL_POINTS); ++i) {
        int col = i % 2;
        int row = i / 2;
        Rectangle r = { spX + col * 580.0f, 92.0f + row * 92.0f, 560.0f, 82.0f };
        bool done = save_.skillPoints[i];

        DrawRectangleRec(r, done ? Color{ 26, 46, 62, 240 } : Color{ 16, 20, 32, 225 });
        DrawRectangleLinesEx(r, 1.5f, done ? GOLD : GRAY);

        DrawText(cat[i].title, static_cast<int>(r.x + 20), static_cast<int>(r.y + 12), 18, done ? GOLD : WHITE);
        DrawTextWrapped(cat[i].requirement, static_cast<int>(r.x + 20), static_cast<int>(r.y + 38), 520, 14, 18, LIGHTGRAY);

        char rew[48];
        std::snprintf(rew, sizeof(rew), done ? "EARNED (+%d B)" : "+%d BOLTS", cat[i].boltReward);
        DrawText(rew, static_cast<int>(r.x + 410), static_cast<int>(r.y + 12), 15, done ? GREEN : SKYBLUE);
    }

    Rectangle backBtn = { spX, 575.0f, 220.0f, 48.0f };
    DrawRectangleRec(backBtn, IsRectHovered(backBtn) ? Color{ 45, 68, 100, 255 } : Color{ 24, 36, 56, 255 });
    DrawRectangleLinesEx(backBtn, 2.0f, ORANGE);
    DrawText("< BACK TO MENU", static_cast<int>(backBtn.x + 34), static_cast<int>(backBtn.y + 16), 16, WHITE);
    if (IsButtonClicked(backBtn)) {
        SetScreenMode(ScreenMode::MAIN_MENU);
        audio_.PlayUiClick();
    }
}

void RailGame::RenderPauseOverlay2D() {
    DrawRectangle(0, 0, static_cast<int>(virtualWidth_), 720, { 8, 12, 22, 200 });
    const char* t = "HOLOCOMM PAUSED";
    int tw = MeasureText(t, 40);
    DrawText(t, static_cast<int>((virtualWidth_ - tw) / 2), 220, 40, GOLD);

    Rectangle resBtn      = { virtualWidth_ / 2.0f - 160.0f, 285.0f, 320.0f, 52.0f };
    Rectangle settingsBtn = { virtualWidth_ / 2.0f - 160.0f, 350.0f, 320.0f, 52.0f };
    Rectangle menuBtn     = { virtualWidth_ / 2.0f - 160.0f, 415.0f, 320.0f, 52.0f };

    DrawRectangleRec(resBtn, IsRectHovered(resBtn) ? Color{ 40, 75, 110, 255 } : Color{ 22, 38, 60, 255 });
    DrawRectangleLinesEx(resBtn, 2.0f, SKYBLUE);
    DrawText("RESUME GRIND [SPACE / (A)]", static_cast<int>(resBtn.x + 40), static_cast<int>(resBtn.y + 17), 17, WHITE);

    DrawRectangleRec(settingsBtn, IsRectHovered(settingsBtn) ? Color{ 55, 45, 80, 255 } : Color{ 28, 22, 42, 255 });
    DrawRectangleLinesEx(settingsBtn, 2.0f, PURPLE);
    DrawText("SYSTEM SETTINGS", static_cast<int>(settingsBtn.x + 85), static_cast<int>(settingsBtn.y + 17), 17, WHITE);

    DrawRectangleRec(menuBtn, IsRectHovered(menuBtn) ? Color{ 85, 40, 40, 255 } : Color{ 45, 24, 30, 255 });
    DrawRectangleLinesEx(menuBtn, 2.0f, ORANGE);
    DrawText("MAIN MENU [ESC]", static_cast<int>(menuBtn.x + 76), static_cast<int>(menuBtn.y + 17), 17, WHITE);

    if (IsButtonClicked(resBtn)) {
        SetScreenMode(ScreenMode::PLAYING);
        audio_.PlayUiClick();
    }
    if (IsButtonClicked(settingsBtn)) {
        settingsPrevMode_ = ScreenMode::PAUSED;
        SetScreenMode(ScreenMode::SETTINGS_VIEW);
        audio_.PlayUiClick();
    }
    if (IsButtonClicked(menuBtn)) {
        SaveProgress();
        SetScreenMode(ScreenMode::MAIN_MENU);
        audio_.PlayUiClick();
    }
}

void RailGame::RenderGameOver2D() {
    DrawRectangle(0, 0, static_cast<int>(virtualWidth_), 720, { 12, 8, 18, 220 });

    const char* goTitle = "NANOTECH DEPLETED // WIPEOUT!";
    int gow = MeasureText(goTitle, 44);
    DrawText(goTitle, static_cast<int>((virtualWidth_ - gow) / 2), 165, 44, ORANGE);

    DrawRectangle(static_cast<int>(virtualWidth_ / 2 - 250), 240, 500, 190, { 16, 22, 38, 240 });
    DrawRectangleLinesEx({ virtualWidth_ / 2.0f - 250.0f, 240.0f, 500.0f, 190.0f }, 2.0f, GOLD);

    char l1[96], l2[96], l3[96], l4[96];
    std::snprintf(l1, sizeof(l1), "DISTANCE GRINDED: %d m   (BEST: %d m)",
                  static_cast<int>(distanceMeters_), isDailyRun_ ? save_.bestDailyMeters : save_.bestDistanceMeters);
    std::snprintf(l2, sizeof(l2), "TITANIUM BOLTS COLLECTED: +%d", runBoltsEarned_);
    std::snprintf(l3, sizeof(l3), "RARITANIUM MINED: +%d", runRaritaniumEarned_);
    std::snprintf(l4, sizeof(l4), "CRATES SMASHED: %d   |   DRONES BLASTED: %d", runCrates_, runKills_);

    DrawText(l1, static_cast<int>(virtualWidth_ / 2 - 215), 264, 20, WHITE);
    DrawText(l2, static_cast<int>(virtualWidth_ / 2 - 215), 304, 19, GOLD);
    DrawText(l3, static_cast<int>(virtualWidth_ / 2 - 215), 342, 18, SKYBLUE);
    DrawText(l4, static_cast<int>(virtualWidth_ / 2 - 215), 380, 17, LIGHTGRAY);

    Rectangle retryBtn = { virtualWidth_ / 2.0f - 250.0f, 455.0f, 240.0f, 56.0f };
    Rectangle menuBtn  = { virtualWidth_ / 2.0f + 10.0f,  455.0f, 240.0f, 56.0f };

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
        SetScreenMode(ScreenMode::MAIN_MENU);
        audio_.PlayUiClick();
    }
}

void RailGame::RenderSkillToast2D() const {
    if (!toast_.active) return;

    float alpha = std::clamp(toast_.timer * 1.5f, 0.0f, 1.0f);
    Rectangle box = { virtualWidth_ / 2.0f - 260.0f, 105.0f, 520.0f, 64.0f };
    DrawRectangleRec(box, { 16, 28, 48, static_cast<unsigned char>(235 * alpha) });
    DrawRectangleLinesEx(box, 2.0f, GOLD);

    DrawText(toast_.title.c_str(), static_cast<int>(box.x + 18), static_cast<int>(box.y + 12), 18, GOLD);
    DrawText(toast_.subtitle.c_str(), static_cast<int>(box.x + 18), static_cast<int>(box.y + 36), 14, WHITE);
}

void RailGame::RenderRadioComm2D() const {
    if (!radioComm_.active || radioComm_.timer <= 0.0f) return;

    float alpha = std::clamp(radioComm_.timer / 0.4f, 0.0f, 1.0f);
    if (radioComm_.timer > radioComm_.maxTimer - 0.4f) {
        alpha = std::clamp((radioComm_.maxTimer - radioComm_.timer) / 0.4f, 0.0f, 1.0f);
    }
    unsigned char aByte = static_cast<unsigned char>(alpha * 240.0f);

    float boxW = 580.0f;
    float boxH = 92.0f;
    float boxX = (virtualWidth_ - boxW) * 0.5f;
    float boxY = 82.0f;

    Color borderCol = (radioComm_.speaker == CommSpeaker::DR_NEFARIOUS) ? Color{ 210, 60, 255, aByte } :
                      (radioComm_.speaker == CommSpeaker::CLANK) ? Color{ 60, 235, 110, aByte } : Color{ 255, 185, 45, aByte };

    // Holographic Translucent Backplate
    DrawRectangle(static_cast<int>(boxX), static_cast<int>(boxY), static_cast<int>(boxW), static_cast<int>(boxH), Color{ 10, 16, 28, aByte });
    DrawRectangleLinesEx({ boxX, boxY, boxW, boxH }, 2.0f, borderCol);

    // Subtle Retro Scanlines across communicator
    for (int y = static_cast<int>(boxY) + 3; y < static_cast<int>(boxY + boxH) - 2; y += 4) {
        DrawLine(static_cast<int>(boxX) + 2, y, static_cast<int>(boxX + boxW) - 2, y, Color{ 255, 255, 255, static_cast<unsigned char>(18 * alpha) });
    }

    // Left Hologram Avatar Frame
    Rectangle avRec = { boxX + 10.0f, boxY + 10.0f, 72.0f, 72.0f };
    DrawRectangleRec(avRec, Color{ 16, 24, 42, aByte });
    DrawRectangleLinesEx(avRec, 1.5f, borderCol);

    // Procedural Avatar Glyphs
    if (radioComm_.speaker == CommSpeaker::CAPTAIN_QWARK) {
        Vector2 c = { avRec.x + 36.0f, avRec.y + 36.0f };
        DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 22.0f, Color{ 40, 140, 60, aByte });
        DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 22.0f, GOLD);
        DrawTriangle({ c.x, c.y - 14.0f }, { c.x - 12.0f, c.y + 10.0f }, { c.x + 12.0f, c.y + 10.0f }, GOLD);
        DrawText("Q", static_cast<int>(c.x - 7), static_cast<int>(c.y - 6), 18, BLACK);
    } else if (radioComm_.speaker == CommSpeaker::DR_NEFARIOUS) {
        Vector2 c = { avRec.x + 36.0f, avRec.y + 36.0f };
        DrawRectangle(static_cast<int>(c.x - 18), static_cast<int>(c.y - 16), 36, 32, Color{ 90, 35, 125, aByte });
        DrawRectangleLines(static_cast<int>(c.x - 18), static_cast<int>(c.y - 16), 36, 32, borderCol);
        DrawTriangle({ c.x - 10.0f, c.y - 6.0f }, { c.x - 14.0f, c.y + 2.0f }, { c.x - 6.0f, c.y + 2.0f }, GREEN);
        DrawTriangle({ c.x + 10.0f, c.y - 6.0f }, { c.x + 6.0f, c.y + 2.0f }, { c.x + 14.0f, c.y + 2.0f }, GREEN);
        DrawRectangle(static_cast<int>(c.x - 10), static_cast<int>(c.y + 8), 20, 4, borderCol);
    } else {
        Vector2 c = { avRec.x + 36.0f, avRec.y + 38.0f };
        DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 20.0f, Color{ 190, 205, 220, aByte });
        DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 20.0f, WHITE);
        DrawCircle(static_cast<int>(c.x - 7), static_cast<int>(c.y - 2), 6.0f, GREEN);
        DrawCircle(static_cast<int>(c.x + 7), static_cast<int>(c.y - 2), 6.0f, GREEN);
        DrawLine(static_cast<int>(c.x), static_cast<int>(c.y - 20), static_cast<int>(c.x), static_cast<int>(c.y - 28), DARKGRAY);
        DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y - 28), 3.0f, RED);
    }

    char hdr[64];
    std::snprintf(hdr, sizeof(hdr), "%s // COMM FREQ 142.8", radioComm_.speakerName.c_str());
    DrawText(hdr, static_cast<int>(boxX + 94), static_cast<int>(boxY + 12), 15, borderCol);

    DrawTextWrapped(radioComm_.text.c_str(), static_cast<int>(boxX + 94), static_cast<int>(boxY + 34),
                    static_cast<int>(boxW - 106), 14, 18, WHITE);
}

} // namespace Railfinity
