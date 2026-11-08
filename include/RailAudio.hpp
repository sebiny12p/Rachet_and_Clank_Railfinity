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
    void PlayUiClick() const;
    void UpdateMusic(int musicMode); // 0 = Title Front Page, 1 = Menu/Vendor, 2 = Rail Action

    bool IsMuted() const { return muted_; }
    void ToggleMute() { muted_ = !muted_; }

private:
    bool initialized_ = false;
    bool muted_ = false;
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
    Sound sndUi_{};
    Music bgmTitle_{};
    Music bgmMenu_{};
    Music bgmAction1_{};
    Music bgmAction2_{};
};

} // namespace Railfinity
