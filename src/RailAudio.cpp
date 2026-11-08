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
                SetMusicVolume(m, 0.55f);
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
}

void RailAudio::Shutdown() {
    if (!initialized_) return;
    if (hasTitleMusic_)   UnloadMusicStream(bgmTitle_);
    if (hasMenuMusic_)    UnloadMusicStream(bgmMenu_);
    if (hasActionMusic1_) UnloadMusicStream(bgmAction1_);
    if (hasActionMusic2_) UnloadMusicStream(bgmAction2_);
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
    UnloadSound(sndUi_);
    CloseAudioDevice();
    initialized_ = false;
}

void RailAudio::UpdateMusic(int musicMode) {
    if (!initialized_) return;
    if (muted_) {
        if (hasTitleMusic_   && IsMusicStreamPlaying(bgmTitle_))   PauseMusicStream(bgmTitle_);
        if (hasMenuMusic_    && IsMusicStreamPlaying(bgmMenu_))    PauseMusicStream(bgmMenu_);
        if (hasActionMusic1_ && IsMusicStreamPlaying(bgmAction1_)) PauseMusicStream(bgmAction1_);
        if (hasActionMusic2_ && IsMusicStreamPlaying(bgmAction2_)) PauseMusicStream(bgmAction2_);
        return;
    }

    // Music mode changed (0 = Title Front Page, 1 = Menu/Vendor, 2 = Rail Action)
    if (musicMode != currentMode_) {
        if (musicMode != 0 && hasTitleMusic_ && IsMusicStreamPlaying(bgmTitle_)) StopMusicStream(bgmTitle_);
        if (musicMode != 1 && hasMenuMusic_  && IsMusicStreamPlaying(bgmMenu_))  StopMusicStream(bgmMenu_);
        if (musicMode != 2) {
            if (hasActionMusic1_ && IsMusicStreamPlaying(bgmAction1_)) PauseMusicStream(bgmAction1_);
            if (hasActionMusic2_ && IsMusicStreamPlaying(bgmAction2_)) PauseMusicStream(bgmAction2_);
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
        Music& activeAction = (currentActionTrackIdx_ == 0 || !hasActionMusic2_) ? bgmAction1_ : bgmAction2_;
        bool& hasActive = (currentActionTrackIdx_ == 0 || !hasActionMusic2_) ? hasActionMusic1_ : hasActionMusic2_;

        if (hasActive) {
            if (!IsMusicStreamPlaying(activeAction)) {
                PlayMusicStream(activeAction);
            }
            UpdateMusicStream(activeAction);

            float played = GetMusicTimePlayed(activeAction);
            float total = GetMusicTimeLength(activeAction);
            if (total > 5.0f && played >= total - 0.25f && hasActionMusic2_) {
                StopMusicStream(activeAction);
                currentActionTrackIdx_ = 1 - currentActionTrackIdx_;
                Music& nextAction = (currentActionTrackIdx_ == 0) ? bgmAction1_ : bgmAction2_;
                PlayMusicStream(nextAction);
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
void RailAudio::PlayUiClick() const { if (initialized_ && !muted_) PlaySound(sndUi_); }

} // namespace Railfinity
