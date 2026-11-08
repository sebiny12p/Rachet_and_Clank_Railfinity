# Ratchet & Clank: Railfinity

An infinite, procedural 3D action grindrail runner built in **C++20** with **Raylib 5.0** for PC and Android, inspired by *Ratchet & Clank: Before the Nexus*. Hop across 3 high-speed plasma grindrails, engage Clank's Heli-Pack thrusters, smash Bolt Crates and Raritanium caches with the OmniWrench 8000, build a 20x Bolt Multiplier, and level up 5 Gadgetron weapons from V1 to V5 Omega!

> **Studio:** Sebiny Labs  
> **IP Disclaimer:** Non-commercial fan project created for educational and portfolio purposes. *Ratchet & Clank* and *Before the Nexus* are registered trademarks of **Sony Interactive Entertainment LLC / Insomniac Games**.

---

## Key Engineering & Gameplay Features

- **True 3D Over-the-Shoulder Grindrail Perspective (`Camera3D`):**
  - Dynamic camera follow with roll tilt on lane changes, FOV dilation on Hyper-Boost pads, and trauma-based 3D camera shake.
  - Articulated 3D Lombax & Clank model featuring Grind Boot spark emitters, deployable Clank Heli-Pack rotor blades when gliding, and OmniWrench 8000 melee arcs.
  - 3D-to-2D projected Target Lock-On reticles (`GetWorldToScreen`) on enemies and explosive crates ahead in your lane.
- **5 Unlockable & Upgradeable Gadgetron Weapons (Level V1 to V5 Omega):**
  1. **Combuster (Plasma Blaster):** Rapid-fire plasma bolts down your active rail (evolves to twin-barrel *Magma Combuster V5*).
  2. **Constructo Shotgun:** Fires a 3-lane spread shockwave across all 3 grindrails simultaneously.
  3. **Buzz Blades:** Launches ricocheting saw discs that bounce to adjacent lanes after striking a target.
  4. **Plasma Striker (Railgun):** Instant high-velocity piercing laser beam that vaporizes every crate and enemy down an entire lane.
  5. **RYNO Infinite (V5 Omega):** Unleashes an explosive barrage of micro-missiles across all 3 rails simultaneously.
- **4 Procedural Galaxy Sectors & Boss Encounters:**
  - *Metropolis Kerwan*, *Megacorp Weapons Park*, *Zordoom Prison Grindway*, and *The Dimensionator Rift*.
  - **Command Warship Boss Encounters:** Every 750m, a Boss Dropship arrives with 3 individually destructible turret cores (`left`, `center`, `right`).
- **Raritanium Engineering Lab & Skill Points (`railfinity_save.dat`):**
  - Upgrade Carbonox Nanotech Armor, Box Breaker Bolt Magnet, Clank Heli-Pack Overdrive, and Multiplier Amplifier.
  - 10 classic Ratchet & Clank **Skill Points** with pop-up notifications and Bolt rewards.
- **Engine Hygiene:**
  - **Fixed-Timestep Loop (`120 Hz`)** + **Zero Runtime Heap Allocations** (pre-allocated `std::array` pools for 3D hazards, floating bolts, projectiles, skyline geometry, and 512 3D particles).
  - **Deterministic `PCG32` Daily Challenge Track** seeded by calendar date (`YYYY-MM-DD`).
  - **Procedural C++ Waveform Synthesizer:** 13 real-time synthesized sound effects (metallic rail clanks, OmniWrench crunches, Titanium Bolt clinks, Raritanium chimes, Railgun beams, explosions) with zero external audio asset dependencies.

---

## Controls

| Action | PC (Keyboard / Mouse) | Android / Touch |
| :--- | :--- | :--- |
| **Switch Rail Left / Right** | `A` / `D` or `LEFT` / `RIGHT` | Swipe Left / Right |
| **Jump / Clank Heli-Glide** | `SPACE` / `W` / `UP` (Hold mid-air to glide) | Swipe Up |
| **Wrench Slide / Duck** | `S` / `DOWN` / `L-CTRL` | Swipe Down |
| **Fire Equipped Weapon** | `Left Click` / `F` / `J` / `ENTER` | Tap Screen |
| **Switch Weapon (1–5)** | `1`–`5` / `Q` & `E` / `Mouse Wheel` | Tap Weapon Hotbar |
| **Pause / Menu** | `ESC` / `P` | Back Button |
| **Mute Audio** | `M` | — |

---

## Build & Run (PC & Android)

### Windows (CMake + Ninja / MinGW / MSVC)
```powershell
cmake -B build -G Ninja
cmake --build build
.\Run_Railfinity.bat
```
