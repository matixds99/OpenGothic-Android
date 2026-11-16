#pragma once

#include <Tempest/Widget>

#include <string>
#include <vector>

#include "gothic.h"

class Npc;

/// NB fix 68: the window of the plugin of New Balance to sell given items for given prices (RX_OpenCraft_PlayerSales:
/// Hanna - jewels and caskets, Kardif - raw meat, Halvor - pearls and valuables, Kreol, Xardas as a necromancer…).
/// In the plugin it is its window of the crafting: the list on the left, the chosen item, the count with - and +
/// and the button on the right. Here the same, drawn for the fingers (and the keys), with the yellow letters of
/// the fonts of Gothic; the rest of the interface is hidden while it is open
class NbSalesMenu : public Tempest::Widget {
  public:
    NbSalesMenu();

    void open(Npc& pl, Npc& trader, const std::vector<Gothic::NbSaleItem>& items);
    void close();
    bool isActive() const { return active; }

    void mobileTap(float x, float y);
    void mobileScroll(float dy);

    void keyDownEvent(Tempest::KeyEvent& e) override;
    void keyUpEvent  (Tempest::KeyEvent& e) override;

  protected:
    void paintEvent    (Tempest::PaintEvent& e) override;
    void mouseDownEvent(Tempest::MouseEvent& e) override;
    void mouseWheelEvent(Tempest::MouseEvent& e) override;

  private:
    struct Layout {
      Tempest::Rect list;     // the list of the items
      int           row = 0;  // the height of a row
      Tempest::Rect panel;    // the chosen item
      Tempest::Rect minus, count, plus, max;
      Tempest::Rect sell;
      Tempest::Rect close;
      // NB fix 69: the categories as the tabs of the plugin over the list (two columns), the arrows of the
      // list in the middle
      std::vector<Tempest::Rect> tabs;
      Tempest::Rect up, down;
      };

    Layout  layout() const;
    size_t  owned(size_t i) const;
    void    select(size_t i);
    void    changeCount(int d);
    void    sell();
    void    clampScroll();
    void    setGroup(size_t g);
    void    scrollBy(int rows);
    int     visibleRows() const;

    bool                              active = false;
    Npc*                              player = nullptr;
    Npc*                              trader = nullptr;
    std::vector<Gothic::NbSaleItem>   all;          // the whole list of the script / the plugin
    std::vector<Gothic::NbSaleItem>   items;        // the items of the chosen category
    std::vector<std::string>          groups;       // the categories in the order of the list (none - one list)
    size_t                            group  = 0;
    size_t                            sel    = 0;
    size_t                            count  = 1;
    int                               scroll = 0;   // the first visible row
    std::string                       message;      // after a sale: "Sprzedano: …"
  };
