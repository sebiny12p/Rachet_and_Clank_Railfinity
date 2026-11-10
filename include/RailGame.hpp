#pragma once
#include "RailTypes.hpp"
#include "DeterministicRNG.hpp"
#include "RailAudio.hpp"
#include <vector>
#include <array>
#include <string>

namespace Railfinity {

class RailGame {
public:
    RailGame();
    ~RailGame();

    void Init();
    void RunFrame();
    void Shutdown();

    static const std::array<WeaponDef, NUM_WEAPONS>& GetWeaponCatalog();
    static const std::array<SkillPointDef, NUM_SKILL_POINTS>& GetSkillPointCatalog();

    float GetVirtualWidth() const { return virtualWidth_; }
    float GetVirtualHeight() const { return 720.0f; }
    void UpdateVirtualResolution();

private:
    // Save / Load
    void LoadSave();
    void SaveProgress() const;
    void AwardSkillPoint(int idx);
    void AddWeaponXp(int wIdx, int amount);

    // Run Control
    void StartNewRun(bool dailySeed);
    void SwitchLane(int delta);
    void TriggerJumpOrGlide();
    void TriggerWrenchSlide();
    void TriggerFireWeapon();
    void TriggerOverdriveBoost();
    void TriggerSwingshotGrapple(Vector3 nodePos, int targetLane);
    void TriggerRadioTransmission(CommSpeaker speaker, const std::string& name, const std::string& text);
    void CycleEquippedWeapon(int delta);
    void DamagePlayer(float dmg, bool minorBump = false);
    void AdvanceMultiplier();

    // Fixed-Timestep Simulation
    void HandleInput();
    void UpdateFixed(float dt);
    void UpdatePlayingFixed(float dt);
    void UpdateBossFixed(float dt);
    void UpdateParticlesFixed(float dt);

    // Procedural Spawners
    void SpawnChunkPattern(float startZ);
    void SpawnHazard(HazardKind kind, int lane, float z, float lengthZ = 3.0f);
    void SpawnRailBoltLine(int lane, float startZ, int count, float spacing = 2.4f, bool jumpArc = false);
    void SpawnBoltShower(Vector3 origin, int count, bool includeRaritanium = false);
    void SpawnProjectile(Vector3 pos, Vector3 vel, int lane, WeaponId wId, float dmg, float radius, Color col, bool friendly, int bounces = 0);
    void SpawnParticleBurst3D(Vector3 pos, int count, Color color, float minSpd, float maxSpd, float size, float life);
    void TriggerBossDropship();

    // Helpers (5-Lane Matrix: Lane 0 = +6.4m, Lane 1 = +3.2m, Lane 2 = 0m, Lane 3 = -3.2m, Lane 4 = -6.4m)
    static float GetLaneX(int lane) { return (2 - lane) * LANE_WIDTH; }
    int GetCorridorMinLaneAt(float worldZ) const;
    float GetTrackCurveX(float z) const;
    float GetTrackCurveY(float z) const;
    Vector3 ApplyTrackCurve(float laneX, float y, float z) const;
    bool IsRailActiveAt(int lane, float z) const;
    bool IsRailGapAt(int lane, float z) const;
    GalaxySector GetCurrentSector() const;
    const char* GetSectorName(GalaxySector s) const;
    Vector2 GetVirtualMouse() const;
    Vector2 ToVirtualCoords(Vector2 screenPos) const;
    bool IsRectHovered(Rectangle r) const;
    bool IsButtonClicked(Rectangle r) const;

    // Rendering (implemented in RailRenderer3D.cpp)
    void RenderFrame();
    void RenderWorld3D();
    void RenderGrindRails3D() const;
    void RenderSkyline3D() const;
    void RenderHazards3D() const;
    void RenderBolts3D() const;
    void RenderProjectiles3D() const;
    void RenderParticles3D() const;
    void RenderBossDropship3D() const;
    void RenderLombaxHero3D(Vector3 pos, float tiltDeg, bool sliding, bool gliding, float wrenchTimer, int weaponId) const;
    void RenderSingleHexBolt3D(Vector3 pos, float spinDeg, float scale, bool titanium) const;
    void RenderRaritaniumCrystal3D(Vector3 pos, float spinDeg, float scale) const;
    void RenderHud2D();
    void RenderSplashIntro2D();
    void RenderTitleFrontPage2D();
    void RenderMainMenu2D();
    void RenderGadgetronVendor2D();
    void RenderRaritaniumLab2D();
    void RenderArmorWardrobe2D();
    void RenderSkillPoints2D();
    void RenderSettings2D();
    void RenderCareerStats2D();
    void RenderPauseOverlay2D();
    void RenderGameOver2D();
    void RenderBrandingFooter2D() const;
    void RenderSkillToast2D() const;
    void RenderRadioComm2D() const;
    void SetScreenMode(ScreenMode nextMode);

private:
    float virtualWidth_ = 1280.0f;
    RenderTexture2D renderTarget_{};
    Camera3D camera_{};
    RailAudio audio_{};
    SaveData save_{};
    PCG32 rng_{};

    ScreenMode mode_ = ScreenMode::SPLASH_INTRO;
    float screenTransitionCooldown_ = 0.0f;
    float splashTimer_ = 0.0f;
    Texture2D splashLogo_{};
    bool isDailyRun_ = false;
    uint64_t activeSeed_ = 0;

    float accumulator_ = 0.0f;
    float globalTime_ = 0.0f;
    float runTime_ = 0.0f;
    float distanceMeters_ = 0.0f;
    float totalWorldTravel_ = 0.0f; // Continuous 3D world travel distance
    float grindSpeed_ = 38.0f; // Units per second in 3D space
    float nextChunkZ_ = 40.0f;
    float distanceSinceBoss_ = 0.0f;

    int runBoltsEarned_ = 0;
    int runRaritaniumEarned_ = 0;
    int runKills_ = 0;
    int runCrates_ = 0;

    // Screen & Camera FX
    float cameraShake_ = 0.0f;
    float screenFlashAlpha_ = 0.0f;
    Color screenFlashColor_ = WHITE;
    float sectorBannerTimer_ = 0.0f;
    GalaxySector lastSector_ = GalaxySector::METROPOLIS_KERWAN;

    // Touch / Mouse Swipe & Tap Gesture Tracking
    bool pointerDown_ = false;
    Vector2 pointerStartPos_ = { 0.0f, 0.0f };
    float pointerDownTime_ = 0.0f;
    bool touchHoldingJump_ = false;
    bool gestureConsumed_ = false;

    // Mobile Multi-Touch & Input Feedback
    mutable std::array<int, 10> prevTouchIds_{};
    mutable int prevTouchCount_ = 0;
    float touchFeedbackJump_ = 0.0f;
    float touchFeedbackFire_ = 0.0f;
    float touchFeedbackBoost_ = 0.0f;
    float touchFeedbackPause_ = 0.0f;

    // Settings state & confirmation modal
    bool showResetConfirm_ = false;
    ScreenMode settingsPrevMode_ = ScreenMode::MAIN_MENU;
    std::string activeInputDevice_ = "KEYBOARD & MOUSE";

    // 3D Armor Wardrobe & Cosmetics Preview State
    float wardrobeRotAngle_ = 0.0f;
    int wardrobeTab_ = 0;              // 0 = Lombax Suits, 1 = Clank Skins
    int wardrobePreviewArmor_ = 0;
    int wardrobePreviewClank_ = 0;
    bool isDraggingWardrobe_ = false;
    Vector2 lastMousePos_{};

    // Authentic 3D Mesh Assets & Rivet Alternate Textures
    Model ratchetModel_{};
    bool ratchetModelLoaded_ = false;
    Texture2D rivetHeadDiff_{};
    Texture2D rivetHead2Diff_{};
    Texture2D rivetFur_{};
    Texture2D rivetFur2_{};
    Texture2D rivetEyeDiff_{};
    bool rivetTexturesLoaded_ = false;
    std::vector<Texture2D> defaultModelTextures_{};
    Shader worldHighpShader_{};
    bool worldHighpShaderLoaded_ = false;

    PlayerLombax player_{};
    BossDropship boss_{};
    SkillPointToast toast_{};
    RadioCommMessage radioComm_{};

    // Object Pools
    std::array<RailHazard, MAX_HAZARDS> hazards_{};
    std::array<FloatingBolt, MAX_BOLTS> bolts_{};
    std::array<Projectile3D, MAX_PROJECTILES> projectiles_{};
    std::array<Particle3D, MAX_PARTICLES_3D> particles_{};
    std::array<SkyBuilding, MAX_SKY_BUILDINGS> buildings_{};
};

} // namespace Railfinity
