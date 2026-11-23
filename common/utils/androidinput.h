#pragma once

#ifdef OPENGOTHIC_TOUCH_UI

#include <Tempest/Sprite>
#include <array>
#include <functional>
#include <string>

#include "ui/gamepadmodel.h"
#include "ui/mobilecontrolsmodel.h"

#ifdef __ANDROID__
struct AInputEvent;
#endif

namespace Tempest {
  class Font;
  class Painter;
  class PaintEvent;
  class TextureAtlas;
  class Window;
  }

namespace AndroidInput {

/// Order matches IconFiles in androidinput.cpp
enum Icon : int {
  NoIcon = -1,
  IconPause, IconStatus, IconJournal, IconMap, IconInventory, IconSneak, IconJump,
  IconAttack, IconAttackLeft, IconAttackRight, IconBlock, IconWeapon,
  IconTarget, IconInteract, IconContext, IconShoot, IconCast, IconSpells,
  IconUp, IconDown, IconConfirm, IconBack, IconTransfer, IconUse,
  IconJoystickBase, IconJoystickThumb, IconButton, IconButtonPressed, IconSlot, IconSlotSelected,
  IconBarEmpty, IconHealth, IconMana, IconOxygen, IconLungs,
  IconSettings, IconWalk, IconLeft, IconRight, IconKeyboard,
  IconCategoryAll, IconCategoryWeapons, IconCategoryArmor, IconCategoryJewelry, IconCategoryMagic,
  IconCategoryFood, IconCategoryPotion, IconCategoryDocuments, IconCategoryMisc,
  IconInvUse, IconInvDrop, IconInvTransfer, IconInvUp, IconInvDown, IconInvSlot, IconInvSlotSelected,
  IconSkip, IconTalk, IconTargetLock, IconFastForward, IconSave, IconHuman, IconInvStack,
  IconSprint, IconQuickAttack,
  IconCount
  };

/// Values of the [MOBILE] section of the ini file.
struct Preferences {
  int buttons     = 120; // percent
#ifdef __ANDROID__
  int resolution  = 50;  // percent of the display resolution
  int shadows     = 256; // shadow map size, 0 disables shadows
  int effects     = 0;   // SSAO
  int fps         = 30;
#else
  int resolution  = 100; // desktop keeps the full quality by default
  int shadows     = 2048;
  int effects     = 0;
  int fps         = 60;
#endif
  int deadzone    = 28;  // percent
  int sensitivity = 100; // percent
  int vibration   = 1;   // feedback on damage
  int gui         = 100; // scale of the interface, percent
  int speed       = 200; // fast forward, percent of the normal speed
  int hudNumbers  = 0;   // 1: the numbers of life and mana (5/10) over the bars
  int hudXp       = 0;   // 1: the experience (500/1000) between the bars
  int strafe      = 1;   // 1: the buttons of the side steps (and the arrows forward and back) on the HUD
  int autoLock    = 0;   // 1: the lock of the target is taken by itself when a weapon is drawn
  int peaceful    = 1;   // 1: the peaceful creatures can be in the focus (to talk to the wisp, to hunt the sheep)
  int acrobatic   = 0;   // 1: the acrobatics of New Balance (rolls), when the hero has learned it
  int reflections = 1; // 1: the water reflects the scene (screen space), 0: only the sky
  int rain        = 2;   // the rain of the outdoor worlds: 0 off, 1 light, 2 normal, 3 heavy
  int fightFocus  = 1;   // NB fix 191: 1 - the enemies in the focus with a weapon drawn (the aim of the spells), 0 - none
  int padStyle    = 0;   // the left stick of a gamepad: 0 walks where it points (a diagonal), 1 as the touch stick (a turn, a side step with a weapon drawn)
  };

Preferences preferences();

class Controller {
  public:
    struct Hud {
      float health = 0;
      float mana   = 0;
      float oxygen = 1;
      int   hp = 0, hpMax = 0;
      int   mp = 0, mpMax = 0;
      bool  diving = false;
      bool  manaVisible = false;
      int   active = -1;
      Icon  actionIcon = IconInteract;
      int   inventoryCategory = 0;
      uint32_t inventoryNew = 0;      // NB fix 121, 122: the tab of the documents with an unread one (a bit for each tab)
      bool  inventoryTransfer = false;
      std::string useLabel;           // action of the selected item in the inventory
      bool  targetLocked = false;
      bool  fastForward  = false;
      bool  sprint       = false;   // the sprint of New Balance is on (NB fix 40)
      bool  summonPassive = false;  // NB fix 131: the summoned creatures do not attack (RX_SummonMode)
      int   summonCallCd  = 0;      // NB fix 176: the seconds until the summons can be called to the hero again
      // NB fix 111: the wheel of the quick choice - the name (UTF-8) and the number of the item of every slot
      std::array<std::string,9> wheelName;    // NB fix 112: 9 slots
      std::array<int,9>         wheelCount{};
      };

    /// NB fix 111: the wheel of the quick choice: open, the places of the models of its slots, the choice
    bool         wheelOpen() const { return wheel.open; }
    Mobile::Rect wheelSlot(int k) const;
    std::function<void(int,bool)> wheelPick;   // slot, a long press

    void  bind();
    void  unbind();
    bool  update(Tempest::Window& window, Mobile::Mode mode);
    void  paint(Tempest::PaintEvent& event);
    void  loadIcons(Tempest::TextureAtlas& atlas);
    /// The sprites live in the atlas of the window: they are given back before the window starts to be destroyed
    void  releaseIcons();
    void  cancel();
    void  inventoryLists(bool two) { controls.inventoryLists(two); }
    void  gui(float scale) { controls.gui(scale); }
    void  keyboard(bool on) { controls.showKeyboard(on); }
    void  playerDead(bool on) { controls.playerDead(on); }
    void  sprint(bool available) { controls.sprint(available); }
    void  summons(bool have, bool open) { controls.summons(have,open); }   // NB fix 131
    void  skipScene(bool available) { controls.skipScene(available); }    // NB fix 143
    bool  held(Mobile::Key key) const { return controls.held(key); }        // NB fix 133
    bool  stickHeld() const { return controls.stickActive(); }              // NB fix 134
    void  stamina(bool available) { controls.stamina(available); }
    void  xpBar(bool available) { controls.xpBar(available); }   // NB fix 137
    const std::map<std::string,Mobile::HudPlace>& hudPlaces() const { return controls.hudPlaces(); }
    bool  editingHud() const { return controls.editingHud(); }
    void  modMenu(bool on) { controls.modMenu(on); }
    void  bareMenu(bool on) { controls.bareMenu(on); }

    /// Pointer input: touches on Android, the mouse on desktop builds.
    void  pointerDown(int id, float x, float y);
    void  pointerMove(int id, float x, float y);
    void  pointerUp  (int id, float x, float y);
    void  physical(Mobile::Key key, bool down);

    void  availability(uint64_t keys, bool target, bool spells, bool movement, bool quick=false) {
      controls.availability(keys,target,spells,movement,quick);
      }

    const Tempest::Sprite&     icon(Icon id) const { return icons.at(size_t(id)); }
    const std::vector<Mobile::Button>& buttons() const { return controls.buttons(); }
    bool  settingsVisible() const { return controls.settingsVisible(); }
    /// The player can move: the stick and the hotbar are on the screen (not in a cutscene, on a pause, when the player is down)
    bool  movementAvailable() const { return controls.movementAvailable(); }
    /// The left stick of a gamepad: the angle of a diagonal walk and the push to the side, see Mobile::Controls::padMove
    float moveYaw() const { return controls.moveYaw(); }
    float moveSide() const { return controls.moveSide(); }

    Hud   hud;

    std::function<void(float,float)>     look;
    std::function<void(float,float,int)> menuTouch;
    std::function<void(float,float)>     menuTap;
    std::function<void(float)>           menuScroll;
    std::function<void(int)>             action;
    /// NB fix 159: the quick loot - a long press of the action button on a corpse or a chest takes all of it
    std::function<bool()>                lootFocus;   // the focus can be looted by the long press
    std::function<bool()>                quickLoot;   // true: the action of the game is pressed now (a chest)

  private:
#ifdef __ANDROID__
    int32_t onInput(AInputEvent* event);
    int32_t onPadKey(AInputEvent* event);
    int32_t onPadMotion(AInputEvent* event);
#endif
    void    pushPadAxes();
    void    applyPreferences();
    void    loadHudPlaces();
    void    saveHudPlaces();
    void    dispatch(Tempest::Window& window, Mobile::Mode mode, const Mobile::Event& input);
    void    onSpecial(int id);

    int     iconFor(const Mobile::Button& b) const;
    void    paintButton(Tempest::Painter& p, Tempest::Font& font, size_t index);
    void    paintHotbarSlot(Tempest::Painter& p, Tempest::Font& font, size_t index);
    void    paintInventoryButton(Tempest::Painter& p, Tempest::Font& font, size_t index);
    void    paintStick(Tempest::Painter& p);
    void    paintSprite(Tempest::Painter& p, Icon id, const Mobile::Rect& r, float alpha) const;

    Mobile::Controls controls;
    Mobile::Gamepad  pad;
    // what the gamepad has said last: the sticks and triggers are axes, a pad may also say the triggers and the D-pad as keys
    struct PadState {
      float lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
      bool  keyLt = false, keyRt = false;
      std::array<bool,4> dpadKey{}, dpadHat{};   // up, down, left, right
      } padState;
    float            padSensitivity = 1;
    bool             paused = false;
    // NB fix 111
    struct Wheel {
      bool     open    = false;
      int      pointer = -1;
      int      slot    = -1;
      uint64_t downAt  = 0;
      } wheel;
    void  wheelToggle(bool open);
    int   wheelSlotAt(float x, float y) const;
    void  paintWheel(Tempest::Painter& p, Tempest::Font& font);
    // NB fix 85: the quick attack is a sequence of the keys of the PC (A or D, then W, then Ctrl), sent at these times
    struct ComboKey {
      uint64_t    at   = 0;
      Mobile::Key key  = Mobile::Key::Enter;
      bool        down = false;
      };
    std::vector<ComboKey> combo;
    // NB fix 159: the press of the action button that waits: a tap (the normal action) or a long press (the quick loot)
    struct LootPress {
      enum : uint8_t { Idle, Waiting, Fired } state = Idle;
      uint64_t downAt = 0;
      } lootPress;

    std::array<Tempest::Sprite,IconCount> icons;
    std::array<float,IconCount>           iconPx{}; // side of the icon in pixels
    float                                 screenW = 0, screenH = 0;
    std::array<Mobile::Rect,IconCount>    ink;
  };

}

#endif
