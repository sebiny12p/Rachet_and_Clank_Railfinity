#include "RailAudio.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace Railfinity {

namespace {
constexpr int SAMPLE_RATE = 44100;
constexpr float TWO_PI = 6.28318530718f;

template <typename Fn>
Sound MakeWaveSound(float durationSec, Fn&& fn) {
    int frames = static_cast<int>(durationSec * SAMPLE_RATE);
    Wave w{};
    w.frameCount = static_cast<unsigned int>(frames);
    w.sampleRate = SAMPLE_RATE;
    w.sampleSize = 16;
    w.channels = 1;

    short* buf = static_cast<short*>(RL_MALLOC(sizeof(short) * frames));
    for (int i = 0; i < frames; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(SAMPLE_RATE);
        float p = static_cast<float>(i) / static_cast<float>(frames);
        float s = std::clamp(fn(t, p), -1.0f, 1.0f);
        buf[i] = static_cast<short>(s * 28000.0f);
    }
    w.data = buf;
    Sound snd = LoadSoundFromWave(w);
    UnloadWave(w);
    return snd;
}
} // namespace

void RailAudio::Init() {
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        initialized_ = false;
        return;
    }
    initialized_ = true;

    // 1. Metallic Rail Hop clank
    sndRailHop_ = MakeWaveSound(0.11f, [](float t, float p) {
        float env = (1.0f - p) * (1.0f - p);
        float f1 = 420.0f + 260.0f * (1.0f - p);
        float f2 = 1180.0f;
        return 0.40f * env * (0.6f * std::sin(TWO_PI * f1 * t) + 0.4f * std::sin(TWO_PI * f2 * t));
    });

    // 2. Jump + Clank Thruster whoosh
    sndJump_ = MakeWaveSound(0.16f, [](float t, float p) {
        float env = 1.0f - p;
        float freq = 260.0f + 540.0f * p;
        return 0.38f * env * std::sin(TWO_PI * freq * t);
    });

    // 3. OmniWrench Smash & Crate Crunch
    sndWrench_ = MakeWaveSound(0.18f, [](float t, float p) {
        float env = 1.0f - p;
        float noise = (static_cast<float>(std::rand() % 2000) / 1000.0f - 1.0f);
        float thump = std::sin(TWO_PI * (180.0f - 110.0f * p) * t);
        return 0.48f * env * (0.55f * thump + 0.45f * noise);
    });

    // 4. Titanium Bolt Metallic Clink
    sndBolt_ = MakeWaveSound(0.09f, [](float t, float p) {
        float env = (1.0f - p) * (1.0f - p);
        float f = (p < 0.35f) ? 1560.0f : 2340.0f;
        return 0.32f * env * std::sin(TWO_PI * f * t);
    });

    // 5. Raritanium Crystal Chime
    sndRaritanium_ = MakeWaveSound(0.38f, [](float t, float p) {
        float env = 1.0f - p;
        float c1 = std::sin(TWO_PI * 1046.5f * t);
        float c2 = std::sin(TWO_PI * 1318.5f * t);
        float c3 = std::sin(TWO_PI * 1567.98f * t);
        return 0.42f * env * 0.33f * (c1 + c2 + c3);
    });

    // 6. Combuster Plasma Blaster Shot
    sndBlaster_ = MakeWaveSound(0.12f, [](float t, float p) {
        float env = 1.0f - p;
        float freq = 880.0f - 660.0f * p;
        return 0.36f * env * std::sin(TWO_PI * freq * t);
    });

    // 7. Constructo Shotgun Blast
    sndShotgun_ = MakeWaveSound(0.22f, [](float t, float p) {
        float env = (1.0f - p);
        float noise = (static_cast<float>(std::rand() % 2000) / 1000.0f - 1.0f);
        float punch = std::sin(TWO_PI * (220.0f - 160.0f * p) * t);
        return 0.50f * env * (0.5f * punch + 0.5f * noise);
    });

    // 8. Plasma Striker Railgun Beam
    sndRailgun_ = MakeWaveSound(0.28f, [](float t, float p) {
        float env = 1.0f - p;
        float f = 1400.0f - 950.0f * p + 120.0f * std::sin(TWO_PI * 60.0f * t);
        return 0.45f * env * std::sin(TWO_PI * f * t);
    });

    // 9. TNT / Boss Explosion
    sndExplosion_ = MakeWaveSound(0.45f, [](float t, float p) {
        float env = (1.0f - p);
        float noise = (static_cast<float>(std::rand() % 2000) / 1000.0f - 1.0f);
        float sub = std::sin(TWO_PI * (110.0f - 75.0f * p) * t);
        return 0.55f * env * (0.6f * sub + 0.4f * noise);
    });

    // 10. Weapon Upgrade / Skill Point Fanfare
    sndLevelUp_ = MakeWaveSound(0.60f, [](float t, float p) {
        float env = 1.0f - p * 0.7f;
        float f = (p < 0.25f) ? 523.25f : (p < 0.50f) ? 659.25f : (p < 0.75f) ? 783.99f : 1046.5f;
        return 0.42f * env * (0.7f * std::sin(TWO_PI * f * t) + 0.3f * std::sin(TWO_PI * f * 2.0f * t));
    });

    // 11. Player Hurt
    sndHurt_ = MakeWaveSound(0.26f, [](float t, float p) {
        float env = 1.0f - p;
        float f = 310.0f - 190.0f * p;
        float saw = 2.0f * (t * f - std::floor(0.5f + t * f));
        return 0.45f * env * saw;
    });

    // 12. Hyper-Boost Pad Surge
    sndBoost_ = MakeWaveSound(0.35f, [](float t, float p) {
        float env = std::sin(p * 3.14159f);
        float f = 320.0f + 780.0f * p;
        return 0.42f * env * std::sin(TWO_PI * f * t);
    });

    // 13. UI Click
    sndUi_ = MakeWaveSound(0.05f, [](float t, float p) {
        float env = (1.0f - p) * (1.0f - p);
        return 0.30f * env * std::sin(TWO_PI * 1320.0f * t);
    });

    // 14. Swingshot Energy Grapple Tether
    sndSwingshot_ = MakeWaveSound(0.38f, [](float t, float p) {
        float env = std::sin(p * 3.14159f);
        float freq = 380.0f + 920.0f * (1.0f - p);
        float buzz = 0.65f * std::sin(TWO_PI * freq * t) + 0.35f * std::sin(TWO_PI * freq * 2.05f * t);
        return 0.36f * env * buzz;
    });

    // 15. Radio Comm Hologram Chirp (Retro 8-bit HoloComm)
    sndRadio_ = MakeWaveSound(0.24f, [](float t, float p) {
        float env = 1.0f - p;
        float freq = (p < 0.5f) ? 880.0f : 1760.0f;
        float squ = std::sin(TWO_PI * freq * t) > 0.0f ? 0.3f : -0.3f;
        return 0.35f * env * squ;
    });

    // 16. 24K Golden Weapon Upgrade Triumph Fanfare
    sndGold_ = MakeWaveSound(0.65f, [](float t, float p) {
        float env = std::sin(p * 3.14159f);
        float f = (p < 0.25f) ? 523.25f : (p < 0.50f) ? 659.25f : (p < 0.75f) ? 783.99f : 1046.50f;
        float har = 0.6f * std::sin(TWO_PI * f * t) + 0.4f * std::sin(TWO_PI * f * 2.0f * t);
        return 0.42f * env * har;
    });

    // Load Official Ratchet & Clank (2002) Soundtrack MP3 Streams (Kyzil Plateau Veldin + Metropolis Kerwan)
    auto TryLoadTrack = [](const char* fileName, Music& outMusic) -> bool {
        const char* appDir = GetApplicationDirectory();
        const std::string candidates[4] = {
            std::string("assets/") + fileName,
            std::string(appDir ? appDir : "") + "assets/" + fileName,
            std::string(appDir ? appDir : "") + "../assets/" + fileName,
            std::string(fileName)
        };
        for (const auto& path : candidates) {
            Music m = LoadMusicStream(path.c_str());
            if (m.stream.buffer != nullptr) {
                m.looping = true;
                ::SetMusicVolume(m, 0.55f);
                outMusic = m;
                return true;
            }
        }
        return false;
    };

    hasTitleMusic_   = TryLoadTrack("music_title.mp3",   bgmTitle_);
    hasMenuMusic_    = TryLoadTrack("music_menu.mp3",    bgmMenu_);
    hasActionMusic1_ = TryLoadTrack("music_action.mp3",  bgmAction1_);
    hasActionMusic2_ = TryLoadTrack("music_action2.mp3", bgmAction2_);
    hasActionMusic3_ = TryLoadTrack("music_action3.mp3", bgmAction3_);
    hasActionMusic4_ = TryLoadTrack("music_action4.mp3", bgmAction4_);

    ApplyVolumes();
}

void RailAudio::ApplyVolumes() {
    if (!initialized_) return;
    float effectiveMusic = muted_ ? 0.0f : (masterVol_ * musicVol_);
    float effectiveSfx = muted_ ? 0.0f : (masterVol_ * sfxVol_);

    if (hasTitleMusic_)   ::SetMusicVolume(bgmTitle_, effectiveMusic);
    if (hasMenuMusic_)    ::SetMusicVolume(bgmMenu_, effectiveMusic);
    if (hasActionMusic1_) ::SetMusicVolume(bgmAction1_, effectiveMusic);
    if (hasActionMusic2_) ::SetMusicVolume(bgmAction2_, effectiveMusic);
    if (hasActionMusic3_) ::SetMusicVolume(bgmAction3_, effectiveMusic);
    if (hasActionMusic4_) ::SetMusicVolume(bgmAction4_, effectiveMusic);

    SetSoundVolume(sndRailHop_, effectiveSfx * 0.85f);
    SetSoundVolume(sndJump_, effectiveSfx * 0.85f);
    SetSoundVolume(sndWrench_, effectiveSfx * 0.95f);
    SetSoundVolume(sndBolt_, effectiveSfx * 0.80f);
    SetSoundVolume(sndRaritanium_, effectiveSfx * 0.90f);
    SetSoundVolume(sndBlaster_, effectiveSfx * 0.85f);
    SetSoundVolume(sndShotgun_, effectiveSfx * 0.95f);
    SetSoundVolume(sndRailgun_, effectiveSfx * 0.95f);
    SetSoundVolume(sndExplosion_, effectiveSfx * 1.0f);
    SetSoundVolume(sndLevelUp_, effectiveSfx * 0.90f);
    SetSoundVolume(sndHurt_, effectiveSfx * 0.90f);
    SetSoundVolume(sndBoost_, effectiveSfx * 0.95f);
    SetSoundVolume(sndSwingshot_, effectiveSfx * 0.90f);
    SetSoundVolume(sndRadio_, effectiveSfx * 0.90f);
    SetSoundVolume(sndGold_, effectiveSfx * 1.0f);
    SetSoundVolume(sndUi_, effectiveSfx * 0.80f);
}

void RailAudio::Shutdown() {
    if (!initialized_) return;
    if (hasTitleMusic_)   UnloadMusicStream(bgmTitle_);
    if (hasMenuMusic_)    UnloadMusicStream(bgmMenu_);
    if (hasActionMusic1_) UnloadMusicStream(bgmAction1_);
    if (hasActionMusic2_) UnloadMusicStream(bgmAction2_);
    if (hasActionMusic3_) UnloadMusicStream(bgmAction3_);
    if (hasActionMusic4_) UnloadMusicStream(bgmAction4_);
    UnloadSound(sndRailHop_);
    UnloadSound(sndJump_);
    UnloadSound(sndWrench_);
    UnloadSound(sndBolt_);
    UnloadSound(sndRaritanium_);
    UnloadSound(sndBlaster_);
    UnloadSound(sndShotgun_);
    UnloadSound(sndRailgun_);
    UnloadSound(sndExplosion_);
    UnloadSound(sndLevelUp_);
    UnloadSound(sndHurt_);
    UnloadSound(sndBoost_);
    UnloadSound(sndSwingshot_);
    UnloadSound(sndRadio_);
    UnloadSound(sndGold_);
    UnloadSound(sndUi_);
    CloseAudioDevice();
    initialized_ = false;
}

const char* RailAudio::GetActionTrackName(int idx) const {
    switch (idx) {
        case 0: return "KYZIL PLATEAU (VELDIN - R&C OST)";
        case 1: return "METROPOLIS (KERWAN - R&C OST)";
        case 2: return "SKYTRAIN ASSAULT (HEROIC RUSH)";
        case 3: return "CYBERFLIGHT (CORSON METROPOLIS)";
        default: return "UNKNOWN SOUNDTRACK";
    }
}

const char* RailAudio::GetCurrentActionTrackName() const {
    if (trackPreference_ > 0 && trackPreference_ <= 4) {
        return GetActionTrackName(trackPreference_ - 1);
    }
    return GetActionTrackName(currentActionTrackIdx_);
}

void RailAudio::SetMusicTrackPreference(int pref) {
    trackPreference_ = pref;
    if (trackPreference_ >= 1 && trackPreference_ <= 4) {
        currentActionTrackIdx_ = trackPreference_ - 1;
    }
    // Stop all action tracks so the next UpdateMusic starts the right one
    if (hasActionMusic1_ && IsMusicStreamPlaying(bgmAction1_)) StopMusicStream(bgmAction1_);
    if (hasActionMusic2_ && IsMusicStreamPlaying(bgmAction2_)) StopMusicStream(bgmAction2_);
    if (hasActionMusic3_ && IsMusicStreamPlaying(bgmAction3_)) StopMusicStream(bgmAction3_);
    if (hasActionMusic4_ && IsMusicStreamPlaying(bgmAction4_)) StopMusicStream(bgmAction4_);
}

int RailAudio::CycleNextActionTrack() {
    if (hasActionMusic1_ && IsMusicStreamPlaying(bgmAction1_)) StopMusicStream(bgmAction1_);
    if (hasActionMusic2_ && IsMusicStreamPlaying(bgmAction2_)) StopMusicStream(bgmAction2_);
    if (hasActionMusic3_ && IsMusicStreamPlaying(bgmAction3_)) StopMusicStream(bgmAction3_);
    if (hasActionMusic4_ && IsMusicStreamPlaying(bgmAction4_)) StopMusicStream(bgmAction4_);

    currentActionTrackIdx_ = (currentActionTrackIdx_ + 1) % 4;
    return currentActionTrackIdx_;
}

void RailAudio::UpdateMusic(int musicMode) {
    if (!initialized_) return;
    if (muted_) {
        if (hasTitleMusic_   && IsMusicStreamPlaying(bgmTitle_))   PauseMusicStream(bgmTitle_);
        if (hasMenuMusic_    && IsMusicStreamPlaying(bgmMenu_))    PauseMusicStream(bgmMenu_);
        if (hasActionMusic1_ && IsMusicStreamPlaying(bgmAction1_)) PauseMusicStream(bgmAction1_);
        if (hasActionMusic2_ && IsMusicStreamPlaying(bgmAction2_)) PauseMusicStream(bgmAction2_);
        if (hasActionMusic3_ && IsMusicStreamPlaying(bgmAction3_)) PauseMusicStream(bgmAction3_);
        if (hasActionMusic4_ && IsMusicStreamPlaying(bgmAction4_)) PauseMusicStream(bgmAction4_);
        return;
    }

    // Music mode changed (0 = Title Front Page, 1 = Menu/Vendor, 2 = Rail Action)
    if (musicMode != currentMode_) {
        if (musicMode != 0 && hasTitleMusic_ && IsMusicStreamPlaying(bgmTitle_)) StopMusicStream(bgmTitle_);
        if (musicMode != 1 && hasMenuMusic_  && IsMusicStreamPlaying(bgmMenu_))  StopMusicStream(bgmMenu_);
        if (musicMode != 2) {
            if (hasActionMusic1_ && IsMusicStreamPlaying(bgmAction1_)) PauseMusicStream(bgmAction1_);
            if (hasActionMusic2_ && IsMusicStreamPlaying(bgmAction2_)) PauseMusicStream(bgmAction2_);
            if (hasActionMusic3_ && IsMusicStreamPlaying(bgmAction3_)) PauseMusicStream(bgmAction3_);
            if (hasActionMusic4_ && IsMusicStreamPlaying(bgmAction4_)) PauseMusicStream(bgmAction4_);
        }
        currentMode_ = musicMode;
    }

    if (musicMode == 0) {
        if (hasTitleMusic_) {
            if (!IsMusicStreamPlaying(bgmTitle_)) PlayMusicStream(bgmTitle_);
            UpdateMusicStream(bgmTitle_);
        }
    } else if (musicMode == 1) {
        if (hasMenuMusic_) {
            if (!IsMusicStreamPlaying(bgmMenu_)) PlayMusicStream(bgmMenu_);
            UpdateMusicStream(bgmMenu_);
        }
    } else if (musicMode == 2) {
        int trackIdx = (trackPreference_ >= 1 && trackPreference_ <= 4)
                       ? (trackPreference_ - 1) : currentActionTrackIdx_;

        Music* actionList[4] = { &bgmAction1_, &bgmAction2_, &bgmAction3_, &bgmAction4_ };
        bool hasList[4] = { hasActionMusic1_, hasActionMusic2_, hasActionMusic3_, hasActionMusic4_ };

        Music& activeAction = *actionList[trackIdx];
        bool hasActive = hasList[trackIdx];

        if (hasActive) {
            if (!IsMusicStreamPlaying(activeAction)) {
                PlayMusicStream(activeAction);
            }
            UpdateMusicStream(activeAction);

            float played = GetMusicTimePlayed(activeAction);
            float total = GetMusicTimeLength(activeAction);
            // If track reaches near end and preference is auto-shuffle (0), rotate to next track
            if (total > 5.0f && played >= total - 0.25f && trackPreference_ == 0) {
                StopMusicStream(activeAction);
                currentActionTrackIdx_ = (currentActionTrackIdx_ + 1) % 4;
                PlayMusicStream(*actionList[currentActionTrackIdx_]);
            }
        }
    }
}

void RailAudio::PlayRailHop() const { if (initialized_ && !muted_) PlaySound(sndRailHop_); }
void RailAudio::PlayJump() const { if (initialized_ && !muted_) PlaySound(sndJump_); }
void RailAudio::PlayWrenchSmash() const { if (initialized_ && !muted_) PlaySound(sndWrench_); }
void RailAudio::PlayBoltPickup() const { if (initialized_ && !muted_) PlaySound(sndBolt_); }
void RailAudio::PlayRaritaniumPickup() const { if (initialized_ && !muted_) PlaySound(sndRaritanium_); }
void RailAudio::PlayBlasterShot() const { if (initialized_ && !muted_) PlaySound(sndBlaster_); }
void RailAudio::PlayShotgunBlast() const { if (initialized_ && !muted_) PlaySound(sndShotgun_); }
void RailAudio::PlayRailgunBeam() const { if (initialized_ && !muted_) PlaySound(sndRailgun_); }
void RailAudio::PlayExplosion() const { if (initialized_ && !muted_) PlaySound(sndExplosion_); }
void RailAudio::PlayLevelUp() const { if (initialized_ && !muted_) PlaySound(sndLevelUp_); }
void RailAudio::PlayHurt() const { if (initialized_ && !muted_) PlaySound(sndHurt_); }
void RailAudio::PlayHyperBoost() const { if (initialized_ && !muted_) PlaySound(sndBoost_); }
void RailAudio::PlaySwingshot() const { if (initialized_ && !muted_) PlaySound(sndSwingshot_); }
void RailAudio::PlayRadioChirp() const { if (initialized_ && !muted_) PlaySound(sndRadio_); }
void RailAudio::PlayGoldenUpgrade() const { if (initialized_ && !muted_) PlaySound(sndGold_); }
void RailAudio::PlayUiClick() const { if (initialized_ && !muted_) PlaySound(sndUi_); }

} // namespace Railfinity
