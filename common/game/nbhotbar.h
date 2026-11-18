#pragma once

// NB fix 110: the items that the player puts on the hotbar of the touch screen himself (a weapon, a potion, a plant).
// The slots 3..10 (index 2..9) belong to the runes and the scrolls of the game; an empty one of them can hold an item.
// The items are kept by the name of their instance in Gothic.ini, [MOBILE_HOTBAR] slotN.

#include <string>
#include <string_view>

#include "gothic.h"
#include "world/world.h"
#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "game/gamescript.h"

namespace NbHotbar {

inline constexpr char Section[] = "MOBILE_HOTBAR";
inline constexpr int  First     = 2;   // the index of the first slot (the key 3)
inline constexpr int  Last      = 9;   // the key 0

inline std::string key(int i) { return "slot"+std::to_string(i); }

inline std::string name(int i) {
  return std::string(Gothic::settingsGetS(Section,key(i)));
  }

inline void set(int i, std::string_view v) {
  Gothic::settingsSetS(Section,key(i),v);
  Gothic::flushSettings();
  }

inline std::string itemName(Npc& pl, const Item& it) {
  auto* sym = pl.world().script().findSymbol(it.clsId());
  return sym!=nullptr ? std::string(sym->name()) : std::string();
  }

/// the item of a slot, when the hero has it
inline Item* item(Npc& pl, int i) {
  if(i<First || i>Last)
    return nullptr;
  const auto n = name(i);
  if(n.empty())
    return nullptr;
  const size_t id = pl.world().script().findSymbolIndex(n);
  if(id==size_t(-1))
    return nullptr;
  return const_cast<Inventory&>(pl.inventory()).getItem(id);
  }

/// the slot of an item, -1 when it is not on the hotbar
inline int slotOf(std::string_view n) {
  for(int i=First; i<=Last; ++i)
    if(!n.empty() && name(i)==n)
      return i;
  return -1;
  }

/// puts the item on the first free slot (no rune there, no item), or takes it off; the slot, -1 when all are taken
inline int toggle(Npc& pl, const Item& it) {
  const auto n = itemName(pl,it);
  if(n.empty())
    return -1;
  if(const int s = slotOf(n); s>=0) {
    set(s,"");
    return s;
    }
  for(int i=First; i<=Last; ++i) {
    if(pl.inventory().currentSpell(uint8_t(i-First))!=nullptr)
      continue;
    if(item(pl,i)!=nullptr)
      continue;
    set(i,n);
    return i;
    }
  return -1;
  }


}

// NB fix 111: the wheel of the quick choice (the second hotbar): 8 slots for a weapon, a
// potion, a plant (9 slots, fix 112). [MOBILE_WHEEL] slotN - the name of the instance.
namespace NbWheel {

inline constexpr char Section[] = "MOBILE_WHEEL";
inline constexpr int  Count     = 9;   // NB fix 112: 9 (was 8)

inline std::string name(int i) { return std::string(Gothic::settingsGetS(Section,NbHotbar::key(i))); }

inline void set(int i, std::string_view v) {
  Gothic::settingsSetS(Section,NbHotbar::key(i),v);
  Gothic::flushSettings();
  }

inline Item* item(Npc& pl, int i) {
  if(i<0 || i>=Count)
    return nullptr;
  const auto n = name(i);
  if(n.empty())
    return nullptr;
  const size_t id = pl.world().script().findSymbolIndex(n);
  if(id==size_t(-1))
    return nullptr;
  return const_cast<Inventory&>(pl.inventory()).getItem(id);
  }

inline int slotOf(std::string_view n) {
  for(int i=0; i<Count; ++i)
    if(!n.empty() && name(i)==n)
      return i;
  return -1;
  }

/// puts the item on the first free slot of the wheel or takes it off; the slot, -1 when the wheel is full
inline int toggle(Npc& pl, const Item& it) {
  const auto n = NbHotbar::itemName(pl,it);
  if(n.empty())
    return -1;
  if(const int s = slotOf(n); s>=0) {
    set(s,"");
    return s;
    }
  for(int i=0; i<Count; ++i) {
    if(item(pl,i)!=nullptr)
      continue;
    set(i,n);
    return i;
    }
  return -1;
  }

}
