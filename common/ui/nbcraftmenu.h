#pragma once

#include <Tempest/Widget>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "game/nbcraft.h"

class Npc;
class Item;
class InventoryRenderer;

/// NB fix 70, 71: the crafting window of the plugin of New Balance (RX_OpenCraft): every table. As the window
/// of the plugin on the PC: the tabs and the recipes that the hero knows on the left ("name (xN)", N - how many he can
/// make), "Umiejętność alchemii: N" at the top, on the right the chosen potion, "Maksymalnie: N", the ingredients
/// ("have/need"), -, the count, +, Max and "Stwórz". The same look and the same controls as the sales window (fix 68-69)
class NbCraftMenu : public Tempest::Widget {
  public:
    NbCraftMenu();
    ~NbCraftMenu() override;

    void setModels(InventoryRenderer* r) { models = r; }

    void open(Npc& pl, int32_t craft);
    void close();
    bool isActive() const { return active; }

    void mobileTap(float x, float y);
    void mobileScroll(float dy);

    void keyDownEvent(Tempest::KeyEvent& e) override;
    void keyUpEvent  (Tempest::KeyEvent& e) override;

  protected:
    void paintEvent     (Tempest::PaintEvent& e) override;
    void mouseDownEvent (Tempest::MouseEvent& e) override;
    void mouseWheelEvent(Tempest::MouseEvent& e) override;

  private:
    struct Layout {
      std::vector<Tempest::Rect> tabs;
      Tempest::Rect list;
      int           row = 0;
      Tempest::Rect title;          // "Umiejętność alchemii: N"
      Tempest::Rect panel;
      Tempest::Rect card;           // the big parchment of the chosen recipe
      int           infoY = 0;      // under the card: "Maksymalnie: N", the ingredients
      Tempest::Rect minus, count, plus, max;
      Tempest::Rect create;
      Tempest::Rect close;
      Tempest::Rect up, down;
      };

    Layout  layout() const;
    void    reload(int32_t keepId);
    void    setTab(size_t t);
    void    select(size_t i);
    void    changeCount(int d);
    void    create();
    void    scrollBy(int rows);
    void    clampScroll();
    int     visibleRows() const;
    size_t  canMake(const NbCraft::Item& it) const;
    size_t  tabCount(size_t t, bool makeable) const;

    const Item* icon(size_t cls);

    bool                         active = false;
    Npc*                         player = nullptr;
    int32_t                      craft  = 0;
    std::vector<NbCraft::Item>   all;       // the known recipes of every tab
    std::vector<NbCraft::Item>   items;     // of the chosen tab
    std::vector<std::string>     tabs;
    std::vector<int32_t>         tabIds;    // the tab of the plugin for each shown tab
    size_t                       tab    = 0;
    size_t                       sel    = 0;
    size_t                       count  = 1;
    int                          scroll = 0;
    int32_t                      skill  = 0;
    std::string                  skillTxt, maxTxt, createTxt;
    std::string                  message;   // after the crafting: "Stworzono: … xN"
    InventoryRenderer*           models = nullptr;
    std::unordered_map<size_t,std::unique_ptr<Item>> icons;
  };
