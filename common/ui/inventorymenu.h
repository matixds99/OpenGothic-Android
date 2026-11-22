#pragma once

#include <Tempest/Widget>
#include <Tempest/Texture2d>
#include <Tempest/Timer>
#include <Tempest/Sprite>

#include <unordered_map>
#include <unordered_set>
#include <array>
#include <string>

#include "graphics/inventoryrenderer.h"
#include "game/inventory.h"
#ifdef OPENGOTHIC_TOUCH_UI
#include "ui/mobilecontrolsmodel.h"
#endif

class Npc;
class Item;
class Inventory;
class Interactive;
class World;
class KeyCodec;

class InventoryMenu : public Tempest::Widget {
  public:
    InventoryMenu(const KeyCodec& key);
    ~InventoryMenu();

    enum class State:uint8_t {
      Closed=0,
      Equip,
      Chest,
      Trade,
      Ransack,
      LockPicking
      };

    enum class LootMode:uint8_t {
      Normal=0,
      Stack,
      Ten,
      Hundred
      };

    enum class DrawPass:uint8_t {
      Back,
      Front
      };

    /// Items of the inventory can be filtered by their kind
    enum Category : int {
      CatAll,
      CatWeapons,
      CatArmor,
      CatJewelry,
      CatMagic,
      CatFood,
      CatPotion,
      CatDocuments,
      CatMisc,
      CatCount
      };

    void  setCategory(int category);
#ifndef OPENGOTHIC_TOUCH_UI
    /// The desktop UI of CoM: the category is shifted with Shift+Left and Shift+Right (Shift+A, Shift+D), its name is a
    /// box next to the gold. The names are in the scripts of the mod (TXT_INV_CAT), without them there are no categories.
    bool  hasCategories() const;
    int   categoryKeyDir(const Tempest::KeyEvent& e) const;
    void  shiftCategory(int dir);
    void  drawCategoryTitle(Tempest::Painter& p, int goldX, int y);
#endif
    int   category() const;
    bool  hasSelectedItem();
    /// NB fix 121: the categories (a bit for each Category) that have a new item - the marks on the tabs of the touch
    /// screen; 0 when the marks are off in the settings or the inventory is closed
    uint32_t nbNewCategories() const;
    void  mobileScroll(float dy);
#ifdef OPENGOTHIC_TOUCH_UI
    void  mobileTap(float x, float y);
    void  mobileTouch(float x, float y, int phase);
    bool  mobileTwoLists() const { return pageOth!=nullptr; }
    std::string_view mobileActionLabel();
    bool  mobileCanHotbar();   // NB fix 110: the selected item can go on the hotbar
    void  setTouchSprites(const Tempest::Sprite& slot, const Tempest::Sprite& selected) {
      touchSlot     = slot;
      touchSelected = selected;
      }
#endif
    void  close();
    void  open(Npc& pl);
    void  trade(Npc& pl,Npc& tr);
    bool  ransack(Npc& pl,Npc& tr);
    void  open(Npc& pl,Interactive& chest);
    State isOpen() const;
    /// NB fix 167: the quick loot of a chest - the next chest or body opened within 4 s is emptied and closed at once
    void  nbArmAutoLoot();
    /// the lines of the taken items in the middle of the screen (the quick loot)
    static void nbPrintTaken(const std::vector<std::pair<std::string,size_t>>& items);
    bool  isActive() const;
    /// Another screen (a document, a video) is shown over the inventory
    void  setCovered(bool c) { covered = c; }
    bool  isCovered() const { return covered; }
    void  onWorldChanged();

    void  tick(uint64_t dt);
    void  draw(Tempest::Encoder<Tempest::CommandBuffer>& cmd);
    void  paintNumOverlay(Tempest::PaintEvent& e);

    void  keyDownEvent  (Tempest::KeyEvent&   e) override;
    void  keyRepeatEvent(Tempest::KeyEvent&   e) override;
    void  keyUpEvent    (Tempest::KeyEvent&   e) override;

  protected:
    void  paintEvent     (Tempest::PaintEvent& e) override;

    void  mouseDownEvent (Tempest::MouseEvent& event) override;
    void  mouseUpEvent   (Tempest::MouseEvent& event) override;
    void  mouseWheelEvent(Tempest::MouseEvent& event) override;

  private:
    // NB fix 121, 122: the documents that came into the inventory of the player and are not read yet - a mark on the
    // slot and on the tab of the documents, until the document is read (or chosen, when it cannot be read). nbSeen: the
    // counts of the items at the last look (every 0.5 s while the inventory is closed, and when it closes - what is
    // moved in an open inventory is not new); the set of the documents is in GameSession (in the save-game)
    std::unordered_map<size_t,size_t> nbSeen;
    uint32_t                          nbSeenWorld = 0;   // World::serialNumber of nbSeen, 0 - none
    // NB fix 123: the other new items (weapons, rings, scrolls...) - for this game only, not in the save-game; the mark
    // goes when the item is chosen
    std::unordered_set<size_t>        nbFresh;
    uint32_t                          nbSession   = 0;
    uint64_t                          nbNextLook  = 0;
    bool                              nbMarkOn    = true;
    bool                              nbMarkPl    = false;   // the text of the mark in Polish (New Balance)
    // NB fix 156: what the used item gave the hero (food, potions): the values before the use, shown as floating notes
    // in the middle of the screen ("Inteligencja +1"); the values are watched 6 s after the use
    std::array<int32_t,8>             nbGainBase{};
    uint64_t                          nbGainUntil = 0;
    struct NbFloat {
      std::string text;
      bool        loss  = false;
      uint64_t    start = 0;
      };
    std::vector<NbFloat>              nbFloats;
    uint64_t                          nbAutoLootUntil = 0;   // NB fix 167
    const Interactive*                nbAutoClosed    = nullptr;   // NB fix 169: the chest emptied by the quick loot
    uint64_t                          nbAutoCloseUntil = 0;
    void   nbDrawGains(Tempest::Painter& p);
    std::array<int32_t,8>             nbGainValues(Npc& pl) const;
    void   nbSnapshot(Npc& pl);
    void   nbFindNew (Npc& pl);
    void   nbSeeSelected();
    void   nbRead(size_t cls);
    bool   nbIsDocument(const Item& item) const;
    static std::unordered_set<size_t>* nbDocs();

    struct Page;
    bool   nbIsNew(const Page& page, const Item& item) const;   // NB fix 121
    struct InvPage;
    struct TradePage;
    struct RansackPage;

    struct PageLocal final {
      size_t                  sel    = 0;
      size_t                  scroll = 0;
      float                   scrollPx = 0;   // touch lists are scrolled by pixels, scroll is the first visible row
      };

    const KeyCodec&           keycodec;

    const Tempest::Texture2d* tex =nullptr;
    const Tempest::Texture2d* slot=nullptr;
    const Tempest::Texture2d* selT=nullptr;
    const Tempest::Texture2d* selU=nullptr;
    // NB fix 104: the look of Hedin's Advanced Inventory (Hedin_Union_Adv_Inventory.vdf in Data), when it is there
    struct Hedin {
      const Tempest::Texture2d* slot     = nullptr;   // INV_NEW_SLOT
      const Tempest::Texture2d* sel      = nullptr;   // INV_NEW_SLOT_HIGHLIGHTED
      const Tempest::Texture2d* equip    = nullptr;   // INV_NEW_SLOT_EQUIPPED
      const Tempest::Texture2d* selEquip = nullptr;   // INV_NEW_SLOT_H_SELECT
      const Tempest::Texture2d* panel    = nullptr;   // INV_SIMPLE_BG_ITEM_1
      const Tempest::Texture2d* up       = nullptr;   // INV_ARROW_UP
      const Tempest::Texture2d* down     = nullptr;   // INV_ARROW_DOWN
      bool on() const { return slot!=nullptr && panel!=nullptr; }
      } hedin;

    State                     state      =State::Closed;
    Npc*                      player     =nullptr;
    Npc*                      trader     =nullptr;
    Interactive*              chest      =nullptr;

    std::unique_ptr<Page>     pageOth, pagePl;
    std::weak_ptr<zenkit::IItem> infoItem;
    PageLocal                 pageLocal[2];

    uint8_t                   page       =0;
    bool                      covered    =false;
    Tempest::Timer            takeTimer;
    size_t                    takeCount  =0;
    bool                      keyFromTouch=false;   // the key that has started the transfer is a tap of the touch screen
    LootMode                  lootMode   =LootMode::Normal;
    InventoryRenderer         renderer;

    size_t                    columsCount = 5;
    int32_t                   scrollDelta = 0;

#ifdef OPENGOTHIC_TOUCH_UI
    Tempest::Sprite           touchSlot, touchSelected;

    /// Dragging of a list with a finger
    struct TouchDrag final {
      bool                    active      = false;
      uint8_t                 list        = 0;     // index of pageLocal
      float                   startY      = 0;
      float                   startScroll = 0;     // in pixels
      };
    Tempest::Rect             touchClip;
    TouchDrag                 touchDrag;

    PageLocal*                touchList(float x, float y, const Page*& list, uint8_t& index);
    float                     touchMaxScroll(const Page& list) const;

    float                     touchUnit() const;
    float                     touchTextScale() const; // text is larger than on desktop, it is read from a phone
    Mobile::InventoryLayout   touchLayout() const;
    bool                      touchHit(float x, float y, uint8_t& list, int& column, int& row) const;
    void                      drawAllTouch(Tempest::Painter& p, Npc& player, DrawPass pass);
    void                      drawTouchHeader(Tempest::Painter& p, std::string_view title, const Mobile::Rect& r);
    void                      drawScrollHints(Tempest::Painter& p, const Page& inv, const PageLocal& sel, const Mobile::Rect& r);
    void                      drawInfoTouch(Tempest::Painter& p);
    // NB fix 99: the new look of the inventory
    void                      touchSyncColumns();
    bool                      touchPolish() const;
    std::string               touchCategoryTitle() const;
    void                      drawTouchPanel(Tempest::Painter& p, const Mobile::Rect& r, bool parchment);
    void                      drawTouchButton(Tempest::Painter& p, const Mobile::Rect& r, std::string_view label);
#endif

    size_t                    rowsCount() const;

    Tempest::Size             slotSize() const;
    int                       infoHeight() const;
    size_t                    pagesCount() const;

    const Page&               activePage();
    PageLocal&                activePageSel();

    void          adjustScrollRows();
    void          processMove(Tempest::KeyEvent& e);
    void          moveLeft(bool usePage);
    void          moveRight(bool usePage);
    void          moveUp();
    void          moveDown();

    void          onItemAction(uint8_t slotHint);
    void          startTakeTimer();
    void          onTakeStuff();
    void          adjustScroll();
    void          syncScriptCategory(bool reset);
    void          updateItemInfo(const Item& item);
    void          drawAll   (Tempest::Painter& p, Npc& player, DrawPass pass);
    void          drawItems (Tempest::Painter& p, DrawPass pass, const Page &inv, const PageLocal &sel, int x, int y, int wcount, int hcount);
    void          drawSlot  (Tempest::Painter& p, DrawPass pass, const Inventory::Iterator& it,
                             const Page& page, const PageLocal &sel, int x, int y, size_t id);
    void          drawGold  (Tempest::Painter& p, Npc &player, int x, int y);
    void          drawHeader(Tempest::Painter& p, std::string_view title, int x, int y);
    void          drawInfo  (Tempest::Painter& p);
  };
