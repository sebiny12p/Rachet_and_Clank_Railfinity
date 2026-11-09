#pragma once
#include "raylib.h"
#include <array>
#include <cstdint>
#include <string>

namespace Railfinity {

constexpr int VIRTUAL_WIDTH = 1280;
constexpr int VIRTUAL_HEIGHT = 720;
constexpr float FIXED_DT = 1.0f / 120.0f;

constexpr int NUM_LANES = 5; // 0 = Far Left, 1 = Mid Left, 2 = Center, 3 = Mid Right, 4 = Far Right
constexpr float LANE_WIDTH = 3.2f; // Distance between adjacent rails

constexpr size_t MAX_HAZARDS = 96;
constexpr size_t MAX_BOLTS = 256;
constexpr size_t MAX_PROJECTILES = 96;
constexpr size_t MAX_PARTICLES_3D = 512;
constexpr size_t MAX_SKY_BUILDINGS = 48;
constexpr size_t NUM_WEAPONS = 6;
constexpr size_t NUM_SKILL_POINTS = 10;
constexpr size_t NUM_ARMOR_SUITS = 6;
constexpr size_t NUM_CLANK_SKINS = 5;

enum class ScreenMode {
    SPLASH_INTRO,     // Sebiny Labs Logo Flash Intro
    TITLE_FRONT_PAGE, // Rift Apart Front Page: "Press Any Key / Tap to Start"
    MAIN_MENU,
    GADGETRON_VENDOR,
    RARITANIUM_LAB,
    ARMOR_WARDROBE,   // 3D Interactive Lombax & Clank Armor Wardrobe
    SKILL_POINTS_VIEW,
    SETTINGS_VIEW,
    STATS_VIEW,       // Galactic Career Dossier & Lifetime Telemetry
    PLAYING,
    PAUSED,
    GAME_OVER
};

enum class WeaponId : int {
    OMNIWRENCH_8000 = 0, // Starting melee & Comet-Strike boomerang wrench (cleaves crates/enemies + deflects shots)
    COMBUSTER = 1,       // Rapid-fire Magma pistol (2.5x shield-melter damage + twin/triple plasma stream)
    SHOCK_BLASTER = 2,   // 3-Lane spread shockwave shotgun (hits all 3 rails + vaporizes enemy projectiles)
    BUZZ_BLADES = 3,     // Smart cross-rail ricocheting saw discs (seeks targets across all 3 lanes)
    PLASMA_STRIKER = 4,  // Piercing railgun hyperbeam (pierces entire lane + EMP chain-lightning to side rails)
    RYNO_INFINITE = 5    // All-rail homing micro-missile swarm (explosive AoE + 50% bonus Bolt jackpot)
};

enum class GalaxySector : int {
    METROPOLIS_KERWAN = 0,
    CORSON_V = 1,
    GASPAR_VOLCANIC = 2,
    POKITARU_RESORT = 3,
    TORREN_IV_SCRAP = 4,
    MEGACORP_FACILITY = 5,
    ZORDOOM_PRISON = 6,
    DIMENSIONATOR_RIFT = 7
};
constexpr int NUM_SECTORS = 8;

enum class HazardKind : int {
    BOLT_CRATE = 0,         // Wooden/gold Gadgetron crate filled with Bolts
    RARITANIUM_CACHE = 1,   // Crystalline cyan Raritanium cluster
    TNT_CRATE = 2,          // Cylindrical red/yellow explosive barrel
    LOW_LASER_BARRIER = 3,  // Low horizontal plasma beams (Jump over)
    HIGH_LASER_GATE = 4,    // Overhead gantry laser curtain (Slide/Duck under)
    ELECTRIFIED_RAIL = 5,   // Tesla coil lightning across rail (Switch lane or Jump)
    BROKEN_RAIL_GAP = 6,    // Missing rail chasm (Jump/Glide or switch lane)
    HYPER_BOOST_PAD = 7,    // Neon green chevron boost ramp
    THUG_DRONE = 8,         // Hovering Thugs-4-Less chopper drone
    SHIELDED_ENFORCER = 9,  // Armored Megacorp Enforcer mech
    NANOTECH_CRATE = 10,    // Translucent cyan glass crate that restores +1 Nanotech HP
    MAGMA_GEYSER = 11,      // Gaspar: Erupting volcanic lava geyser between rails
    HOVER_TAXI = 12,        // Corson V: Cyberpunk neon flying taxi weaving through lanes
    PIRANHA_LEAP = 13,      // Pokitaru: Leaping razor-tooth cyber-piranha predator
    SWINGING_CRANE_SCRAP = 14,// Torren IV: Magnetic overhead crane swinging rusty debris
    SWINGSHOT_NODE = 15     // Green Versa-Fuse Grapple Node for cinematic leaps!
};

struct WeaponDef {
    int id;
    const char* name;
    const char* omegaName;
    const char* specialPerk;
    const char* description;
    int unlockBoltCost;
    int upgradeRaritaniumCost;
    float fireCooldown;
    float baseDamage;
    Color projectileColor;
};

struct GoldenWeaponDef {
    int id;
    const char* name;
    const char* masteryPerk;
    int unlockRaritaniumCost;
    int unlockBoltCost;
    Color goldPrimary;
    Color goldTrim;
    Color projectileColor;
};

inline const std::array<GoldenWeaponDef, NUM_WEAPONS> GOLDEN_WEAPON_CATALOG = {{
    { 0, "24K GOLDEN OMNIWRENCH", "Mastery: Twin 360-degree shockwaves + Triple bolt yield from smashed crates", 12, 2800,
      Color{ 255, 215, 0, 255 }, Color{ 255, 240, 140, 255 }, Color{ 255, 220, 60, 255 } },
    { 1, "GOLDEN MAGMA COMBUSTER V8", "Mastery: Twin fiery golden streams leaving an incinerating trail on rails", 15, 3600,
      Color{ 255, 215, 0, 255 }, Color{ 255, 140, 20, 255 }, Color{ 255, 200, 30, 255 } },
    { 2, "GOLDEN CONSTRUCTO SHREDDER", "Mastery: 5-Rail wide golden electrostatic arc vaporizing all projectiles", 18, 4500,
      Color{ 255, 215, 0, 255 }, Color{ 120, 235, 255, 255 }, Color{ 255, 235, 80, 255 } },
    { 3, "GOLDEN DOOM BLADES V8", "Mastery: Cluster of 3 gilded ricocheting saw discs seeking targets across rails", 20, 5500,
      Color{ 255, 215, 0, 255 }, Color{ 160, 255, 60, 255 }, Color{ 255, 225, 40, 255 } },
    { 4, "GOLDEN PLASMA STRIKER", "Mastery: Continuous piercing 24K hyperbeam instantly disintegrating lane hazards", 24, 6800,
      Color{ 255, 215, 0, 255 }, Color{ 220, 110, 255, 255 }, Color{ 255, 240, 100, 255 } },
    { 5, "GOLDEN RYNO V8 SUPREME", "Mastery: 24K gilded micro-missile tempest + 2x Bolt Jackpot multiplier on impact", 35, 9500,
      Color{ 255, 215, 0, 255 }, Color{ 255, 60, 80, 255 }, Color{ 255, 215, 0, 255 } }
}};

enum class CommSpeaker : int {
    CAPTAIN_QWARK = 0,
    DR_NEFARIOUS = 1,
    CLANK = 2
};

struct RadioCommMessage {
    bool active = false;
    CommSpeaker speaker = CommSpeaker::CAPTAIN_QWARK;
    std::string speakerName;
    std::string text;
    float timer = 0.0f;
    float maxTimer = 4.5f;
};

enum class ArmorSuitId : int {
    CLASSIC_COMMANDO = 0,
    CARBONOX = 1,
    REBEL_LOMBAX = 2,
    INFERNOX = 3,
    DEADLOCKED_MARAUDER = 4,
    LOMBAX_PRAETORIAN = 5
};

struct SkillPointDef {
    int id;
    const char* title;
    const char* requirement;
    int boltReward;
};

struct ArmorSuitDef {
    const char* name;
    const char* origin;
    const char* perkDesc;
    int boltCost;
    int raritaniumCost;
    Color primaryColor;     // Main armor plate
    Color accentColor;      // Trim / Pauldron
    Color visorColor;       // Glowing visor / energy seams
    Color pantsColor;       // Fabric pants
};

struct ClankSkinDef {
    const char* name;
    const char* origin;
    const char* perkDesc;
    int boltCost;
    int raritaniumCost;
    Color chassisColor;     // Main body chrome
    Color brightColor;      // Highlight bevels & collar
    Color eyeColor;         // Camera lens glow
    Color beaconColor;      // Antenna beacon
};

inline const std::array<ArmorSuitDef, NUM_ARMOR_SUITS> ARMOR_CATALOG = {{
    {
        "CLASSIC COMMANDO",
        "SOLANA & BOGON AVIATOR (DEFAULT)",
        "Ratchet's iconic pilot vest & Bogon denim.",
        0, 0,
        Color{ 224, 98, 24, 255 },  // Solana aviator orange vest
        Color{ 85, 48, 22, 255 },   // Rich leather harness & belt
        Color{ 45, 225, 255, 255 }, // Glowing cyan chest badge & magnetic soles
        Color{ 52, 72, 98, 255 }    // Bogon denim trousers
    },
    {
        "CARBONOX ARMOR",
        "GOING COMMANDO / UP YOUR ARSENAL",
        "+1 Max Nanotech HP & Cyan Arc Sparks.",
        2000, 8,
        Color{ 36, 42, 52, 255 },   // Matte graphite composite
        Color{ 18, 22, 28, 255 },   // Deep obsidian trim
        Color{ 0, 235, 255, 255 },  // Electric cyan nanotech power lines
        Color{ 26, 32, 42, 255 }    // Reinforced carbon weave
    },
    {
        "REBEL LOMBAX",
        "SARGASSO REBEL CELL // RIFT APART",
        "Sargasso freedom fighter with cybernetic arm.",
        2500, 0,
        Color{ 38, 92, 110, 255 },  // Dark teal rebel flight tunic
        Color{ 235, 185, 30, 255 }, // Rivet's cybernetic mechanical prosthetic arm
        Color{ 60, 215, 255, 255 }, // Luminous cyan energy core & eye signature
        Color{ 30, 38, 52, 255 }    // Sargasso dark indigo combat trousers
    },
    {
        "INFERNOX ARMOR",
        "MEGACORP THERMAL WEAPONS LAB",
        "Fiery magma embers & magnetized crates.",
        4500, 15,
        Color{ 175, 40, 16, 255 },  // Scorched molten crimson
        Color{ 38, 24, 22, 255 },   // Basalt heat shield trim
        Color{ 255, 145, 20, 255 }, // Active volcanic magma core glow
        Color{ 32, 22, 20, 255 }    // Ashen thermal weave
    },
    {
        "DEADLOCKED MARAUDER",
        "DREADZONE COMBAT ARENA",
        "1.25x Wrench Melee Damage & Shockwaves.",
        6500, 25,
        Color{ 170, 26, 32, 255 },  // Dreadzone battle crimson
        Color{ 40, 42, 48, 255 },   // Industrial gunmetal steel
        Color{ 255, 36, 36, 255 },  // Combat targeting laser visor
        Color{ 26, 26, 30, 255 }    // Heavy kevlar combat trousers
    },
    {
        "LOMBAX PRAETORIAN",
        "ANCIENT DIMENSIONAL ARCHIVE",
        "1.5x Bolt Multiplier Escalation.",
        9000, 35,
        Color{ 238, 242, 250, 255 },// Polished Royal Pearl & Platinum
        Color{ 230, 180, 50, 255 }, // Ornate 24K Gold Filigree & Winged Crests
        Color{ 200, 90, 255, 255 }, // Luminescent Psionic Violet Matrix
        Color{ 28, 24, 48, 255 }    // Imperial Amethyst Velvet Trousers
    }
}};

inline const std::array<ClankSkinDef, NUM_CLANK_SKINS> CLANK_SKIN_CATALOG = {{
    {
        "TITANIUM CHASSIS",
        "STANDARD ISSUE (DEFAULT)",
        "Insomniac titanium finish & emerald optics.",
        0, 0,
        Color{ 196, 208, 224, 255 }, // Chrome silver
        Color{ 230, 238, 248, 255 }, // Polished highlight
        Color{ 48, 235, 110, 255 },  // Emerald green eyes
        Color{ 255, 30, 30, 255 }    // Red antenna beacon
    },
    {
        "24-KARAT GOLDEN CLANK",
        "GOLD WEAPON FOUNDRY",
        "Gilded alloy with sapphire optics.",
        3500, 12,
        Color{ 235, 185, 48, 255 },  // Pure gold chassis
        Color{ 255, 225, 110, 255 }, // Mirror gold bevels
        Color{ 40, 180, 255, 255 },  // Radiant sapphire eyes
        Color{ 255, 215, 60, 255 }   // Golden antenna beacon
    },
    {
        "SECRET AGENT CLANK",
        "HOLOMEDIA ENTERTAINMENT",
        "Matte black tuxedo with formal bowtie.",
        5000, 18,
        Color{ 32, 34, 40, 255 },    // Tuxedo obsidian
        Color{ 75, 80, 92, 255 },    // Satin lapel grey
        Color{ 255, 220, 80, 255 },  // Golden spectacles
        Color{ 240, 40, 50, 255 }    // Dapper red beacon
    },
    {
        "DREAD PIRATE CLANK",
        "SPACE PIRATE FLOTILLA",
        "Weathered bronze with glowing ruby optics.",
        6500, 22,
        Color{ 160, 105, 64, 255 },  // Weathered bronze
        Color{ 210, 150, 95, 255 },  // Polished brass trim
        Color{ 255, 45, 45, 255 },   // Menacing ruby eyes
        Color{ 255, 160, 20, 255 }   // Amber lantern beacon
    },
    {
        "KT-74-610 // KIT",
        "DIMENSION-HOPPER WARBOT // ZORDOMM PRISON",
        "Warm copper chassis & sunny yellow optics.",
        5000, 16,
        Color{ 185, 115, 68, 255 },  // Warm copper/bronze chassis
        Color{ 235, 180, 85, 255 },  // Polished brass highlight
        Color{ 255, 225, 45, 255 },  // Sunny yellow optics
        Color{ 255, 210, 50, 255 }   // Golden sensor beacon
    }
}};

struct SaveData {
    uint32_t magic = 0x5241494F; // 'RAIO' v3 (Rebel Lombax 3rd skin at 2500 bolts)
    int bolts = 100;
    int raritanium = 0;
    int bestDistanceMeters = 0;
    int bestDailyMeters = 0;
    int totalEnemiesDestroyed = 0;
    int totalCratesSmashed = 0;
    int totalBossesDefeated = 0;
    int equippedWeapon = 0;

    std::array<bool, NUM_WEAPONS> unlockedWeapons = { true, false, false, false, false, false };
    std::array<int, NUM_WEAPONS> weaponLevels = { 1, 1, 1, 1, 1, 1 }; // V1 to V5 Omega
    std::array<int, NUM_WEAPONS> weaponXp = { 0, 0, 0, 0, 0, 0 };

    // Raritanium Lab Upgrades
    int armorLevel = 0;        // +Max Nanotech HP (0..4) -> 4 to 8 HP
    int magnetLevel = 0;       // Bolt & Raritanium attractor radius (0..5)
    int bootThrusterLevel = 0; // Longer Clank Heli-Pack glide & Hyper-Boost duration (0..5)
    int multiplierModLevel = 0;// Faster Bolt Multiplier escalation (0..5)

    // Wardrobe & Cosmetics
    int equippedArmor = 0;     // 0 = Classic Commando, 1 = Carbonox, 2 = Infernox, 3 = Marauder, 4 = Praetorian, 5 = Rivet
    int equippedClankSkin = 0; // 0 = Standard Titanium, 1 = 24K Gold, 2 = Secret Agent, 3 = Dread Pirate, 4 = Kit
    std::array<bool, NUM_ARMOR_SUITS> unlockedArmors = { true, false, false, false, false, false };
    std::array<bool, NUM_CLANK_SKINS> unlockedClankSkins = { true, false, false, false, false };

    std::array<bool, NUM_SKILL_POINTS> skillPoints = { false };

    // Store Expansion & Mastery Gadgets
    bool hoverbootsUnlocked = false;
    std::array<bool, NUM_WEAPONS> goldenWeapons = { false };

    // User Settings & Accessibility (Persisted)
    float masterVolume = 1.0f;
    float musicVolume = 0.55f;
    float sfxVolume = 0.80f;
    float screenShakeScale = 1.0f; // 0.0f = Off, 0.5f = 50%, 1.0f = 100%
    bool screenFlashes = true;
    int targetFps = 120; // 30, 60, 120
    int touchOpacity = 2; // 0 = 40%, 1 = 70%, 2 = 100%
    bool leftHandedControls = false;
    int swipeSensitivity = 1; // 0 = Low (36px), 1 = Med (26px), 2 = High (18px)
    int musicTrackChoice = 0; // 0 = Dynamic Shuffle, 1 = Veldin, 2 = Kerwan, 3 = Skytrain, 4 = Cyberflight

    // Lifetime Career Telemetry
    int totalRunsStarted = 0;
    int totalDistanceGrinded = 0;
    int totalBoltsEarnedLifetime = 100;
    int totalRaritaniumEarnedLifetime = 0;
    int totalSwingshotGrapples = 0;
    int totalOverdriveBoosts = 0;
};

struct PlayerLombax {
    int currentLane = 2;      // 0 = Far Left, 1 = Mid Left, 2 = Center, 3 = Mid Right, 4 = Far Right
    float x = 0.0f;           // Smoothly interpolated X coordinate
    float y = 0.0f;           // Height above rail
    float vy = 0.0f;          // Vertical velocity
    float laneTiltDeg = 0.0f; // Lean angle when switching lanes

    // Acrobatic Rail-to-Rail Hop Animation
    bool isHopping = false;
    float hopTimer = 0.0f;
    float hopDuration = 0.22f;
    float hopStartX = 0.0f;
    float hopTargetX = 0.0f;
    int hopDir = 0;           // -1 = Hop Left, +1 = Hop Right
    float hopRollDeg = 0.0f;

    bool grounded = true;
    bool heliGliding = false;
    float emergencyHoverTimer = 0.0f; // >0: Clank emergency auto-hover when rail ends beneath Ratchet
    float heliFuel = 1.2f;
    float maxHeliFuel = 1.2f;
    float heliDeploy = 0.0f;       // Smooth deployment factor: 0.0f (stowed) -> 1.0f (extended & spinning)
    float heliRotorAngle = 0.0f;   // Continuous spinning angle
    float jumpTime = 0.0f;         // Time spent airborne in current leap

    float slideTimer = 0.0f;  // >0 means crouching/sliding + OmniWrench spin
    float wrenchSwingTimer = 0.0f;
    float weaponCooldown = 0.0f;
    float gunAimTimer = 0.0f;     // Holds forward gun-aiming stance after firing
    float gunRecoilTimer = 0.0f;  // Drives snappy kickback, muzzle climb & muzzle flash
    float barrelSpinAngle = 0.0f; // Spins rotary barrels (RYNO / Buzz Blades)
    float landingSquashTimer = 0.0f; // Knee compression bounce on rail landing or lane hop

    float nanotechHp = 4.0f;
    float maxNanotechHp = 4.0f;
    float invulnTimer = 0.0f;
    float hyperBoostTimer = 0.0f;

    // Overdrive Hoverboots (Gadget)
    float overdriveDuration = 0.0f;
    float overdriveCooldown = 0.0f;

    // Swingshot Energy Grapple (Cinematic Jumps)
    bool isSwinging = false;
    float swingTimer = 0.0f;
    float swingStartX = 0.0f;
    int swingTargetLane = 2;
    Vector3 swingNodePos = { 0.0f, 0.0f, 0.0f };

    // Bolt Multiplier (1x -> 2x -> 4x -> 8x -> 12x -> 20x)
    int multiplierTier = 1;
    float multiplierProgress = 0.0f;
};

struct RailHazard {
    bool active = false;
    HazardKind kind = HazardKind::BOLT_CRATE;
    int lane = 1;
    float z = 0.0f;          // Distance ahead of player (player is at Z = 0)
    float lengthZ = 3.0f;
    float hp = 10.0f;
    float maxHp = 10.0f;
    float animPhase = 0.0f;
    float shootTimer = 2.0f;
};

struct FloatingBolt {
    bool active = false;
    bool isRaritanium = false;
    bool isTitanium = false; // Large gleaming silver/gold bolt worth 25 bolts
    bool onRail = false;     // True if spawned in a clean line/arc on the rail
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    Vector3 vel = { 0.0f, 0.0f, 0.0f };
    int value = 10;
    float spin = 0.0f;
};

struct Projectile3D {
    bool active = false;
    bool friendly = true;
    WeaponId sourceWeapon = WeaponId::COMBUSTER;
    int lane = 1;
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    Vector3 vel = { 0.0f, 0.0f, 0.0f };
    float damage = 15.0f;
    float radius = 0.6f;
    float life = 2.5f;
    int bouncesLeft = 0;
    Color color = ORANGE;
};

struct Particle3D {
    bool active = false;
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    Vector3 vel = { 0.0f, 0.0f, 0.0f };
    float size = 0.25f;
    float life = 0.0f;
    float maxLife = 0.5f;
    Color color = ORANGE;
};

struct SkyBuilding {
    Vector3 pos;
    Vector3 size;
    Color bodyColor;
    Color neonColor;
};

enum class BossType : int {
    PROG_TWINS = 0,
    EMPEROR_NEFARIOUS_FRIGATE = 1
};

struct BossDropship {
    bool active = false;
    BossType bossType = BossType::PROG_TWINS;
    int bossTier = 0;
    std::string name = "PROG TWINS // COMMAND WARSHIP";
    float z = 42.0f;
    float hoverX = 0.0f;
    float hoverY = 4.2f;
    float phase = 0.0f;
    std::array<float, NUM_LANES> turretHp = { 0.0f, 120.0f, 120.0f, 120.0f, 0.0f };
    float maxTurretHp = 120.0f;
    int telegraphLane = -1;
    float telegraphTimer = 0.0f;
    float attackCooldown = 2.2f;
    float laserSweepX = 0.0f;
    float laserSweepTimer = 0.0f;
};

struct SkillPointToast {
    bool active = false;
    std::string title;
    std::string subtitle;
    float timer = 0.0f;
};

} // namespace Railfinity
