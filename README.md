# Ratchet & Clank: Railfinity

An infinite, procedural 3D action grindrail runner built in **C++20** with **Raylib 5.0** for PC and Android, inspired by *Ratchet & Clank: Rift Apart* and *Before the Nexus*. Grind high-speed plasma rails across futuristic skylines, deploy Clank's Heli-Pack with authentic procedural animations, smash Bolt Crates and Raritanium caches with the OmniWrench 8000, build a 20x Bolt Multiplier, and upgrade 5 iconic Gadgetron weapons from V1 to V5 Omega!

> **Studio:** Sebiny Labs  
> **IP Disclaimer:** Non-commercial fan tribute created for educational and portfolio purposes. *Ratchet & Clank*, *Rift Apart*, and *Before the Nexus* are registered trademarks and copyright of **Sony Interactive Entertainment LLC / Insomniac Games**.

---

## Key Character & Animation Systems

### 1. Dynamic Procedural Jumping Animation
- **Ascending Leap (`vy > 0`):** Snappy Lombax athletic knee tuck that dynamically tightens as vertical launch momentum increases, with Ratchet's arms flaring backward for aerial balance and ears fluttering in the slipstream.
- **Descending Arc (`vy < 0`):** Legs smoothly extend downward to spot the plasma rail, readying magnetic grind boots for impact.
- **Rail Touchdown:** Reactive knee squash compression bounce (`landingSquashTimer`) with radial gold impact sparks.

### 2. Smooth Clank Heli-Pack Deployment (Zero Pop-In!)
- **Telescoping Chrome Mast:** Upon engaging mid-air glide (`Hold Space / W`), Clank's head hatch opens as a multi-stage polished chrome mast extends upward from `0.0m` to `0.82m` over `0.15s`.
- **Twin Titanium Blades:** Rotor blades unfold from vertical stowage to horizontal flight plane while accelerating from `0 RPM` to a hypersonic spin blur.
- **Aerodynamic Vortex Halo:** A glowing cyan energy halo ring and pulsing disc scale outwards with the rotor diameter.
- **Thruster Plume:** Twin thruster exhausts flare out from beneath Clank's backpack with cyan particle trails.
- **Authentic Glide Pose:** Ratchet raises his arms to grasp Clank's shoulder harness straps while his legs dangle below with rhythmic Lombax kick and sway.
- **Clank's Rig:** Articulated robot backpack with reactive head nod, glowing green power core, and robot hands gripping Ratchet's shoulders.

### 3. OmniWrench 8000 Starting Melee Weapon
- **Instant Melee Sweep:** Ratchet swings the OmniWrench 8000 horizontally across his entire rail lane in a wide 180° arc (`wrenchSwingTimer`).
- **Resource-Free Destruction:** Shatters wooden bolt crates, explosive mines, and proximity barriers without spending weapon ammo.
- **OmniWrench Slide:** Crouching/sliding (`[S] / [Down] / [L-Ctrl]`) initiates a low grind posture with the wrench spinning along the plasma rail to collect low-hanging bolts and duck under overhead laser barriers.

---

## Gadgetron Weapon Catalog (Level V1 to V5 Omega)

Level up weapons through combat usage from base V1 up to devastating V5 Omega forms:

1. **Combuster / Magma Combuster (V1 - V5 Omega):**
   - Rapid-fire twin plasma bolts directly down your active rail lane.
   - V5 Omega upgrades to molten magma rounds that pierce through multiple consecutive crates.
2. **Constructo Shotgun / Shockwave Cannon (V1 - V5 Omega):**
   - Wide-spread concussive blast affecting all 3 rails simultaneously.
   - Ideal for clearing barricades spanning across all tracks.
3. **Buzz Blades / Doom Blades (V1 - V5 Omega):**
   - High-velocity serrated sawblades that ricochet across adjacent rails upon hitting a target.
4. **Plasma Striker (Railgun V1 - V5 Omega):**
   - Instant-hit sniper laser beam that pierces through every obstacle and enemy in the lane from horizon to horizon.
5. **R.Y.N.O. Infinite ("Rip You a New One" - V5 Omega):**
   - The legendary superweapon firing an apocalyptic barrage of micro-missiles accompanied by Tchaikovsky's 1812 Overture fanfare!

---

## 4 Procedural Galactic Sectors & Dropship Bosses

1. **Metropolis Kerwan:** Soaring neon art-deco spires, yellow hovering air-taxis, and bright blue skies.
2. **Megacorp Weapons Park:** Industrial foundries, conveyor tracks, and fiery smelting conduits.
3. **Zordoom Prison Grindway:** Deep-space maximum security containment sector with purple nebulae and security searchlights.
4. **The Dimensionator Rift:** Dimensional rifts tearing open the sky, shifting between alternate reality skylines.
- **Warship Boss Battles:** Every 750m, a Megacorp Dropship arrives overhead firing laser barrages with 3 targetable core turrets (`Left`, `Center`, `Right`).

---

## The Armorizer (Vendor & Progression Kiosk)

Open **The Armorizer** from the Title Screen or pause menu to purchase upgrades with collected Hex Bolts:
- **Weapons:** Purchase ammo and unlock new Gadgetron firearms.
- **Nanotech Armor:** Upgrade from base Lombax Pilot suit to *Wildwater Armor*, *Q-Force Uniform*, and *Carbonox Armor* for up to 8 Nanotech HP.
- **Gadgets & Perks:** Box Breaker bolt magnet, Heli-Pack Overdrive, and Multiplier Boosters.
- **Skill Points:** 10 classic Ratchet & Clank achievements with bolt bonuses.

---

## Controls

| Action | PC (Keyboard & Mouse) | Android / Touch |
| :--- | :--- | :--- |
| **Switch Rail Left / Right** | `A` / `D` or `LEFT` / `RIGHT` | Swipe Left / Right |
| **Jump / Clank Heli-Glide** | `SPACE` / `W` / `UP` (Hold mid-air to glide) | Swipe Up / Hold |
| **Wrench Slide / Duck** | `S` / `DOWN` / `L-CTRL` | Swipe Down |
| **OmniWrench Strike** | `E` / `F` / `Middle Click` | Quick Double Tap |
| **Fire Equipped Weapon** | `Left Click` / `J` / `ENTER` | Tap Screen |
| **Cycle Weapon (1 - 5)** | `1` - `5` / `Q` / `Mouse Wheel` | Tap Weapon Hotbar |
| **Pause / Vendor Kiosk** | `ESC` / `P` | Pause Button |
| **Mute Soundtrack** | `M` | Audio Toggle |

---

## Technical Specifications

- **Engine:** Raylib 5.0 with modern C++20 features.
- **3D Camera System:** Custom third-person `Camera3D` tracking with dynamic rail curvature look-ahead (`GetTrackCurveX/Y`), velocity-based roll tilt, and FOV dilation.
- **Fixed-Timestep Physics:** `120 Hz` accumulator loop ensuring fluid, stutter-free movement on 60 Hz, 120 Hz, 144 Hz, and 240 Hz displays.
- **Zero Runtime Heap Allocations:** All 3D track segments, bolts, hazards, particles, and weapon projectiles utilize pre-allocated static arrays.
- **Dual-Track Audio:** Official rotating orchestral themes (*Rift Apart Main Theme* & *A Crack in Time*) complemented by 13 synthesized C++ waveform SFX.

---

## Building & Running

### Windows (CMake + MinGW / Ninja)
```cmd
cmake -B build -G Ninja
ninja -C build
.\build\RatchetAndClankRailfinity.exe
```

### Android APK Build
Requires Android SDK + NDK (`r26b`+):
```cmd
cmd.exe /c Build_Android_APK.bat
```
Output: `build_android/Railfinity.apk` ready for USB sideloading or emulator deployment.
