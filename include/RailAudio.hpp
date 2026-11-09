#pragma once
#include "raylib.h"

namespace Railfinity {

class RailAudio {
public:
    void Init();
    void Shutdown();

    void PlayRailHop() const;
    void PlayJump() const;
    void PlayWrenchSmash() const;
    void PlayBoltPickup() const;
    void PlayRaritaniumPickup() const;
    void PlayBlasterShot() const;
    void PlayShotgunBlast() const;
    void PlayRailgunBeam() const;
    void PlayExplosion() const;
    void PlayLevelUp() const;
    void PlayHurt() const;
    void PlayHyperBoost() const;
    void PlaySwingshot() const;
    void PlayRadioChirp() const;
    void PlayGoldenUpgrade() const;
    void PlayUiClick() const;
    void UpdateMusic(int musicMode); // 0 = Title Front Page, 1 = Menu/Vendor, 2 = Rail Action

    bool IsMuted() const { return muted_; }
    void ToggleMute() { muted_ = !muted_; }
    void SetMasterVolume(float vol) { masterVol_ = vol; ApplyVolumes(); }
    void SetMusicVolume(float vol) { musicVol_ = vol; ApplyVolumes(); }
    void SetSfxVolume(float vol) { sfxVol_ = vol; ApplyVolumes(); }
    float GetMasterVolume() const { return masterVol_; }
    float GetMusicVolume() const { return musicVol_; }
    float GetSfxVolume() const { return sfxVol_; }
    void ApplyVolumes();

private:
    bool initialized_ = false;
    bool muted_ = false;
    float masterVol_ = 1.0f;
    float musicVol_ = 0.55f;
    float sfxVol_ = 0.80f;
    int currentMode_ = -1;
    int currentActionTrackIdx_ = 0;

    bool hasTitleMusic_ = false;
    bool hasMenuMusic_ = false;
    bool hasActionMusic1_ = false;
    bool hasActionMusic2_ = false;

    Sound sndRailHop_{};
    Sound sndJump_{};
    Sound sndWrench_{};
    Sound sndBolt_{};
    Sound sndRaritanium_{};
    Sound sndBlaster_{};
    Sound sndShotgun_{};
    Sound sndRailgun_{};
    Sound sndExplosion_{};
    Sound sndLevelUp_{};
    Sound sndHurt_{};
    Sound sndBoost_{};
    Sound sndSwingshot_{};
    Sound sndRadio_{};
    Sound sndGold_{};
    Sound sndUi_{};
    Music bgmTitle_{};
    Music bgmMenu_{};
    Music bgmAction1_{};
    Music bgmAction2_{};
};

} // namespace Railfinity
