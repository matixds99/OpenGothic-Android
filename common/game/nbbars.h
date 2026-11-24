#pragma once

#include <array>
#include <string>

/// NB fix 41: the bars of the Union plugin of New Balance (Bar_SetShow, Bar_SetValue, ...). The scripts fill them every
/// frame (PerFrame -> B_UpdateStaminaBar), the window draws them. Index 1 is the stamina (STAMINABAR_INDEX), 2..5 the
/// life of the target (OpenGothic draws its own).
namespace NbBars {
  struct Bar {
    bool        show     = false;
    std::string tex;
    int         value    = 0;
    int         maxRange = 0;
    };

  constexpr int Count   = 8;
  constexpr int Stamina = 1;

  inline std::array<Bar,Count>& all() {
    static std::array<Bar,Count> bars;
    return bars;
    }

  inline Bar* at(int i) {
    return (0<=i && i<Count) ? &all()[size_t(i)] : nullptr;
    }

  inline void reset() {
    all() = {};
    }

  /// NB fix 221: Ext_DarkScreen of the plugin - the screen goes black (real ms of the application), the window draws it
  struct DarkScreen {
    uint64_t start = 0;
    int      fade  = 0;
    int      hold  = 0;
    };

  inline DarkScreen& darkScreen() {
    static DarkScreen d;
    return d;
    }

  /// the black of the screen at this moment, 0..1; 0 when it is over
  inline float darkAlpha(uint64_t now) {
    auto& d = darkScreen();
    if(d.start==0 || now<d.start)
      return 0.f;
    const float t = float(now-d.start), f = float(d.fade), h = float(d.hold);
    if(t<f)
      return t/f;
    if(t<f+h)
      return 1.f;
    if(t<f+h+f)
      return 1.f-(t-f-h)/f;
    d.start = 0;
    return 0.f;
    }

  /// NB fix 49: the inventory of the player is open (the window sets it every frame, Hlp_InventoryIsOpen reads it)
  inline bool& inventoryOpen() {
    static bool open = false;
    return open;
    }
  }
