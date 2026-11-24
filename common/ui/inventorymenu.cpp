#include "inventorymenu.h"
#include "ui/cursorshape.h"

#include <Tempest/Painter>
#include <Tempest/SoundEffect>
#include <Tempest/Application>
#include <cmath>

#include "utils/string_frm.h"
#include "world/objects/npc.h"
#include "world/objects/interactive.h"
#include "world/objects/item.h"
#include "world/world.h"
#include "utils/gthfont.h"
#include "utils/keycodec.h"
#include "gothic.h"
#include "resources.h"
#include "game/nbiteminfo.h"
#include "game/nbtexts.h"
#include "game/nbhotbar.h"
#include "game/gamesession.h"

using namespace Tempest;

// CoM's TXT_INV_CAT/INV_CAT_GROUPS order differs from the UI enum at magic/artifacts.
static const InventoryMenu::Category CategoryOrder[InventoryMenu::CatCount] = {
  InventoryMenu::CatAll,     InventoryMenu::CatWeapons, InventoryMenu::CatArmor,     InventoryMenu::CatMagic,
  InventoryMenu::CatJewelry, InventoryMenu::CatFood,    InventoryMenu::CatPotion,    InventoryMenu::CatDocuments,
  InventoryMenu::CatMisc
  };

/// NB fix 220: a bag of New Balance (ITBG_*)
static bool nbIsBag(const Item& item) {
  auto* w = Gothic::inst().world();
  auto* sym = w!=nullptr ? w->script().findSymbol(item.clsId()) : nullptr;
  if(sym==nullptr)
    return false;
  const auto& n = sym->name();
  return n.size()>=5 && (n.compare(0,5,"ITBG_")==0 || n.compare(0,5,"itbg_")==0);
  }

struct InventoryMenu::Page {
  Page()=default;
  Page(const Page&)=delete;
  virtual ~Page()=default;

  Category category = CatAll;
  uint32_t scriptMask = 0;
  uint32_t pocketMask = 0;   // NB fix 220: OpenPocket - only the items of these flags (and the worn ones), no bags

  bool matches(const Item& item) const {
    if(pocketMask!=0) {
      if(item.isEquipped())
        return true;
      if(nbIsBag(item))
        return false;
      return ((uint32_t(item.mainFlag()) | uint32_t(item.itemFlag())) & pocketMask)!=0;
      }
    if(category==CatAll)
      return true;

    const uint32_t flags   = uint32_t(item.mainFlag());
    if(scriptMask!=0)
      return (flags & scriptMask)!=0;
    const bool     weapons = (flags & (ITM_CAT_NF | ITM_CAT_FF | ITM_CAT_MUN))!=0;
    const bool     armor   = item.isArmor();
    const bool     jewelry = item.isRing() || (uint32_t(item.itemFlag()) & ITM_AMULET)!=0;
    const bool     magic   = item.isSpellOrRune();
    const bool     food    = (flags & ITM_CAT_FOOD)!=0;
    const bool     potion  = (flags & ITM_CAT_POTION)!=0;
    const bool     docs    = (flags & ITM_CAT_DOCS)!=0;
    switch(category) {
      case CatWeapons:   return weapons;
      case CatArmor:     return armor;
      case CatJewelry:   return jewelry;
      case CatMagic:     return magic;
      case CatFood:      return food;
      case CatPotion:    return potion;
      case CatDocuments: return docs;
      case CatMisc:      return !weapons && !armor && !jewelry && !magic && !food && !potion && !docs;
      default:           return true;
      }
    }

  /// Iterators below skip items that do not belong to the current category
  Inventory::Iterator first() const {
    auto it = iterator();
    while(it.isValid() && !matches(*it))
      ++it;
    return it;
    }

  void next(Inventory::Iterator& it) const {
    if(it.isValid())
      ++it;
    while(it.isValid() && !matches(*it))
      ++it;
    }

  size_t size() const {
    if(is(nullptr))
      return 0;
    size_t ret = 0;
    for(auto it = first(); it.isValid(); next(it))
      ret++;
    return ret;
    }

  Inventory::Iterator get(size_t id) const {
    auto it = first();
    for(size_t i=0; i<id && it.isValid(); ++i)
      next(it);
    return it;
    }

  virtual bool                is(const Inventory* i) const { return i==nullptr; }
  virtual Inventory::Iterator iterator() const { throw std::runtime_error("index out of range");  }
  };

struct InventoryMenu::InvPage : InventoryMenu::Page {
  InvPage(const Inventory& i):inv(i){}

  bool                is(const Inventory* i) const override { return &inv==i; }
  Inventory::Iterator iterator() const override {
    return inv.iterator(Inventory::T_Inventory);
    }

  const Inventory& inv;
  };

struct InventoryMenu::TradePage : InventoryMenu::Page {
  TradePage(const Inventory& i):inv(i){}

  bool                is(const Inventory* i) const override { return &inv==i; }
  Inventory::Iterator iterator() const override {
    return inv.iterator(Inventory::T_Trade);
    }

  const Inventory& inv;
  };

struct InventoryMenu::RansackPage : InventoryMenu::Page {
  RansackPage(const Inventory& i):inv(i){}

  bool                is(const Inventory* i) const override { return &inv==i; }
  Inventory::Iterator iterator() const override {
    return inv.iterator(Inventory::T_Ransack);
    }

  const Inventory& inv;
  };

InventoryMenu::InventoryMenu(const KeyCodec& key)
  :keycodec(key) {
  slot = Resources::loadTexture("INV_SLOT.TGA");
  selT = Resources::loadTexture("INV_SLOT_HIGHLIGHTED.TGA");
  selU = Resources::loadTexture("INV_SLOT_EQUIPPED.TGA");
  // NB fix 104: the slots, the backgrounds and the arrows of Hedin's inventory (struct hedin) stay off - the player keeps
  // the frames and the parchment of fix 99 with the icons of the categories of Hedin (AndroidInput). To turn them on:
  // load INV_NEW_SLOT, INV_NEW_SLOT_HIGHLIGHTED, INV_NEW_SLOT_EQUIPPED, INV_NEW_SLOT_H_SELECT, INV_SIMPLE_BG_ITEM_1,
  // INV_ARROW_UP/DOWN into hedin at the first drawing (not here: the textures were not found this early)
  tex  = Resources::loadTexture("INV_BACK.TGA"); // INV_TITEL.TGA


  int invMaxColumns = Gothic::settingsGetI("GAME","invMaxColumns");
  if(invMaxColumns>0)
    columsCount = size_t(invMaxColumns); else
    columsCount = 5;
#ifdef OPENGOTHIC_TOUCH_UI
  columsCount = 9;   // NB fix 99: set from the layout, see touchSyncColumns
#endif

  setFocusPolicy(NoFocus);
  setCursorShape(HiddenCursor);
  takeTimer.timeout.bind(this,&InventoryMenu::onTakeStuff);
  }

InventoryMenu::~InventoryMenu() {
  }

void InventoryMenu::close() {
  infoItem.reset();
  if(state!=State::Closed && player!=nullptr)
    nbSnapshot(*player);   // NB fix 121: what the player has seen
  if(state!=State::Closed) {
    if(state==State::Trade)
      Gothic::inst().emitGlobalSound("TRADE_CLOSE"); else
      Gothic::inst().emitGlobalSound("INV_CLOSE");
    }
  renderer.reset(true);
  takeTimer.stop();
  state  = State::Closed;
  }

void InventoryMenu::open(Npc &pl) {
  infoItem.reset();
  if(pl.isDown() || pl.isMonster() || pl.isInAir() || pl.isSlide() || (pl.interactive()!=nullptr))
    return;
  if(pl.bodyStateMasked()==BS_UNCONSCIOUS || pl.bodyStateMasked()==BS_LIE)
    return;
  if(pl.weaponState()!=WeaponState::NoWeapon) {
    pl.stopAnim("");
    pl.closeWeapon(false);
    }
  state  = State::Equip;
  player = &pl;
  trader = nullptr;
  chest  = nullptr;
  page   = 0;
  pagePl .reset(new InvPage  (pl.inventory()));
  pageOth.reset();
  nbFindNew(pl);   // NB fix 121
  syncScriptCategory(false);
  adjustScroll();
  update();

  Gothic::inst().emitGlobalSound("INV_OPEN");
  //Gothic::inst().emitGlobalSound("INV_CHANGE");
  pl.world().script().dmaInventoryOpen(pl);
  }

// NB fix 220: OpenPocket(bag, flags) of the plugin (0x101a8d90): the bags of NB open the inventory of the hero with the
// items of their kinds only (the plugin takes the others out for the time the inventory is open): armors, weapons, potions,
// runes and documents, rings and amulets. The worn items stay in the list, the bags themselves do not
void InventoryMenu::openPocket(Npc& pl, uint32_t mask) {
  if(state!=State::Equip || player!=&pl)
    open(pl);
  if(state!=State::Equip || pagePl==nullptr)
    return;
  pagePl->pocketMask = mask;
  pageLocal[0] = PageLocal();
  pageLocal[1] = PageLocal();
  adjustScroll();
  update();
  }

void InventoryMenu::trade(Npc &pl, Npc &tr) {
  infoItem.reset();
  if(pl.isDown())
    return;
  state  = State::Trade;
  player = &pl;
  trader = &tr;
  chest  = nullptr;
  page   = 0;
  pagePl .reset(new InvPage  (pl.inventory()));
  pageOth.reset(new TradePage(tr.inventory()));
  nbFindNew(pl);   // NB fix 121
  syncScriptCategory(true);
  adjustScroll();
  update();
  Gothic::inst().emitGlobalSound("TRADE_OPEN");
  }

bool InventoryMenu::ransack(Npc &pl, Npc &tr) {
  infoItem.reset();
  if(pl.isDown())
    return false;
  auto it = tr.inventory().iterator(Inventory::T_Ransack);
  if(!it.isValid())
    return false;
  state  = State::Ransack;
  player = &pl;
  trader = &tr;
  chest  = nullptr;
  page   = 0;
  pagePl .reset(new InvPage    (pl.inventory()));
  pageOth.reset(new RansackPage(tr.inventory()));
  nbFindNew(pl);   // NB fix 121
  syncScriptCategory(true);
  adjustScroll();
  update();
  Gothic::inst().emitGlobalSound("INV_OPEN");
  return true;
  }

void InventoryMenu::open(Npc &pl, Interactive &ch) {
  infoItem.reset();
  if(pl.isDown())
    return;
  const bool needToPicklock = ch.needToLockpick(pl);
  if(!pl.setInteraction(&ch))
    return;

  if(needToPicklock && !ch.isCracked()) {
    state = State::LockPicking;
    } else {
    state = State::Chest;
    }
  // NB fix 169: the same chest, just emptied by the quick loot, opens again by its interaction - shut at once
  if(state==State::Chest && &ch==nbAutoClosed && Application::tickCount()<nbAutoCloseUntil) {
    pl.setInteraction(nullptr,true);
    pl.stopAnim("");
    player = &pl;
    chest  = &ch;
    state  = State::Closed;
    return;
    }

  player = &pl;
  trader = nullptr;
  chest  = &ch;
  page   = 0;
  pagePl .reset(new InvPage(pl.inventory()));
  pageOth.reset(new InvPage(ch.inventory()));
  nbFindNew(pl);   // NB fix 121
  syncScriptCategory(true);
  adjustScroll();
  update();
  }

void InventoryMenu::nbArmAutoLoot() {
  nbAutoLootUntil = Application::tickCount()+4000;
  }

bool InventoryMenu::nbAutoLooting() const {
  return nbAutoLootUntil!=0 && Application::tickCount()<nbAutoLootUntil &&
         (state==State::Chest || state==State::Ransack || state==State::LockPicking);
  }

void InventoryMenu::nbPrintTaken(const std::vector<std::pair<std::string,size_t>>& items) {
  const auto& fnt = Resources::font(1.f);
  Gothic::inst().emitGlobalSound("INV_OPEN");
  const size_t maxLines = 6;
  for(size_t i=0; i<items.size() && i<maxLines; ++i) {
    std::string line = items[i].first;
    if(items[i].second>1)
      line += " x"+std::to_string(items[i].second);
    if(i+1==maxLines && items.size()>maxLines)
      line = "... i jeszcze "+std::to_string(items.size()-maxLines+1);
    Gothic::inst().onPrintScreen(line,-1,int(56+i*4),3,fnt);
    }
  }

InventoryMenu::State InventoryMenu::isOpen() const {
  return state;
  }

bool InventoryMenu::isActive() const {
  return state!=State::Closed;
  }

void InventoryMenu::onWorldChanged() {
  close();
  player = nullptr;
  trader = nullptr;
  chest  = nullptr;
  // NB fix 121, 122: the counts of the player of the new world - the items of the start of a new game and of a save are
  // not new (the unread documents of a save come with it, GameSession::nbNewDocs)
  nbSeenWorld = 0;
  // NB fix 123: the marks of the other items stay after a change of the world; a new game or a loaded save has none
  if(auto* gs = Gothic::inst().gameSession()) {
    if(gs->serialNumber()!=nbSession)
      nbFresh.clear();
    nbSession = gs->serialNumber();
    }
  if(auto* pl = Gothic::inst().player())
    nbSnapshot(*pl);
  }

std::unordered_set<size_t>* InventoryMenu::nbDocs() {
  auto* gs = Gothic::inst().gameSession();
  return gs!=nullptr ? &gs->nbNewDocs() : nullptr;
  }

/// NB fix 122: a document - an item of the tab of the documents (the groups of the categories of the mod, or ITM_CAT_DOCS)
bool InventoryMenu::nbIsDocument(const Item& item) const {
  Page docs;
  docs.category = CatDocuments;
  if(player!=nullptr || Gothic::inst().world()!=nullptr) {
    auto& script = player!=nullptr ? player->world().script() : Gothic::inst().world()->script();
    for(int k=0; k<int(CatCount); ++k)
      if(CategoryOrder[k]==CatDocuments)
        docs.scriptMask = script.inventoryCategoryMask(k);
    }
  return docs.matches(item);
  }

void InventoryMenu::nbSnapshot(Npc& pl) {
  nbSeen.clear();
  for(auto it=pl.inventory().iterator(Inventory::T_Inventory); it.isValid(); ++it)
    nbSeen[it->clsId()] += it.count();
  nbSeenWorld = pl.world().serialNumber();
  for(auto i=nbFresh.begin(); i!=nbFresh.end();) {
    if(nbSeen.find(*i)==nbSeen.end())
      i = nbFresh.erase(i); else
      ++i;
    }
  // the documents that the player has not any more
  if(auto* docs = nbDocs()) {
    for(auto i=docs->begin(); i!=docs->end();) {
      if(nbSeen.find(*i)==nbSeen.end())
        i = docs->erase(i); else
        ++i;
      }
    }
  }

void InventoryMenu::nbFindNew(Npc& pl) {
  auto* docs = nbDocs();
  if(docs!=nullptr && nbSeenWorld==pl.world().serialNumber()) {
    std::unordered_map<size_t,std::pair<size_t,const Item*>> now;
    for(auto it=pl.inventory().iterator(Inventory::T_Inventory); it.isValid(); ++it) {
      auto& n = now[it->clsId()];
      n.first  += it.count();
      n.second  = &(*it);
      }
    // NB fix 206: a document is new only when the hero gets one that he did not have (a new letter, a new map) - not a
    // second copy of one he has. Many documents at once are the inventory coming back (a load, a change of the world
    // before the snapshot), not new ones: all 49 documents of the hero were marked new, read or not
    std::vector<size_t> newDocs;
    for(auto& [cls,n]:now) {
      if(docs->find(cls)!=docs->end() || nbFresh.find(cls)!=nbFresh.end())
        continue;
      auto was = nbSeen.find(cls);
      const size_t had = was==nbSeen.end() ? 0 : was->second;
      if(n.first<=had)
        continue;
      if(nbIsDocument(*n.second)) {
        if(had==0)
          newDocs.push_back(cls);
        } else {
        nbFresh.insert(cls);   // NB fix 123
        }
      }
    if(newDocs.size()>8) {
      Log::i("[newdocs] ",newDocs.size()," documents at once - the inventory coming back, not new ones");
      } else {
      for(auto cls:newDocs) {
        docs->insert(cls);
        if(auto* sym = pl.world().script().findSymbol(cls))
          Log::i("[newdocs] new, not read yet: ",sym->name());
        }
      }
    }
  nbSnapshot(pl);
  }

/// NB fix 122: a document that cannot be read (no function of use) stops being new when the player chooses it
void InventoryMenu::nbSeeSelected() {
  if(pagePl==nullptr || &activePage()!=pagePl.get())
    return;
  auto it = activePage().get(activePageSel().sel);
  if(!it.isValid())
    return;
  nbFresh.erase(it->clsId());   // NB fix 123: the other items - chosen, seen
  auto* docs = nbDocs();
  if(docs!=nullptr && it->handle().on_state[0]==0)
    docs->erase(it->clsId());
  }

/// NB fix 122: the document is read
void InventoryMenu::nbRead(size_t cls) {
  if(auto* docs = nbDocs())
    docs->erase(cls);
  }

bool InventoryMenu::nbIsNew(const Page& page, const Item& item) const {
  if(!nbMarkOn || &page!=pagePl.get())
    return false;
  if(nbFresh.find(item.clsId())!=nbFresh.end())
    return true;
  auto* docs = nbDocs();
  return docs!=nullptr && docs->find(item.clsId())!=docs->end();
  }

uint32_t InventoryMenu::nbNewCategories() const {
  if(!nbMarkOn || player==nullptr || state==State::Closed || state==State::LockPicking)
    return 0;
  auto* docs = nbDocs();
  if(nbFresh.empty() && (docs==nullptr || docs->empty()))
    return 0;
  // NB fix 123: the tab of the category of every new item (not "all")
  Page tab[CatCount];
  auto& script = player->world().script();
  for(int i=0; i<int(CatCount); ++i) {
    tab[i].category = Category(i);
    for(int k=0; k<int(CatCount); ++k)
      if(CategoryOrder[k]==Category(i))
        tab[i].scriptMask = script.inventoryCategoryMask(k);
    }
  uint32_t ret = 0;
  for(auto it=player->inventory().iterator(Inventory::T_Inventory); it.isValid(); ++it) {
    const size_t cls = it->clsId();
    if(nbFresh.find(cls)==nbFresh.end() && (docs==nullptr || docs->find(cls)==docs->end()))
      continue;
    for(int i=0; i<int(CatCount); ++i)
      if(Category(i)!=CatAll && tab[i].matches(*it))
        ret |= (1u<<i);
    }
  return ret;
  }

void InventoryMenu::tick(uint64_t /*dt*/) {
  // NB fix 121: the marks of the new items (the settings of New Balance in OpenGothic)
  nbMarkOn = Gothic::settingsGetI("NB_OG","bNewItems")!=0;
  if(auto w = Gothic::inst().world())
    nbMarkPl = NbItemInfo::isNb(w->script().getVm());
  // NB fix 122: the documents that come while the inventory is closed (so they are in the set before a save)
  if(state==State::Closed && Application::tickCount()>=nbNextLook) {
    nbNextLook = Application::tickCount()+500;
    if(auto* pl = Gothic::inst().player())
      nbFindNew(*pl);
    }
  if(state!=State::Closed && state!=State::LockPicking)
    syncScriptCategory(false);
  if(player!=nullptr && (player->isDown() || player->bodyStateMasked()==BS_UNCONSCIOUS)) {
    close();
    return;
    }

  if(state==State::LockPicking) {
    if(chest->isCracked()) {
      state = State::Chest;
      return;
      }
    }

  // NB fix 167: the quick loot of a chest - opened by the long press, emptied and closed at once
  if(nbAutoLootUntil!=0 && (state==State::Chest || state==State::Ransack) && pageOth!=nullptr && player!=nullptr) {
    const bool armed = Application::tickCount()<nbAutoLootUntil;
    nbAutoLootUntil = 0;
    if(armed) {
      std::vector<std::pair<std::string,size_t>> taken;
      std::vector<std::pair<size_t,size_t>>      cls;
      for(auto it = pageOth->iterator(); it.isValid(); ++it) {
        taken.emplace_back(std::string(it->displayName()),it.count());
        cls.emplace_back(it->clsId(),it.count());
        }
      for(auto& c:cls) {
        if(state==State::Chest && chest!=nullptr)
          player->addItem(c.first,*chest,c.second); else
        if(state==State::Ransack && trader!=nullptr)
          player->addItem(c.first,*trader,c.second);
        }
      nbPrintTaken(taken);
      // NB fix 169: the interaction of the hero with the chest goes on a moment after (its animation): the chest asks
      // the inventory to open again - it is closed at once, for 2 s
      nbAutoClosed     = chest;
      nbAutoCloseUntil = Application::tickCount()+2000;
      // the hero leaves the chest at once (no animation of closing it): no window, no kneeling
      if(state==State::Chest && player->interactive()!=nullptr) {
        player->setInteraction(nullptr,true);
        player->stopAnim("");
        }
      close();
      return;
      }
    }

  if(state==State::Ransack) {
    if(trader==nullptr){
      close();
      return;
      }

    if(!trader->isDown()) {
      close();
      return;
      }

    auto it = trader->inventory().iterator(Inventory::T_Ransack);
    if(!it.isValid())
      close();
    }

  if(state==State::Closed) {
    if(player!=nullptr){
      if(!player->setInteraction(nullptr))
         return;
      player = nullptr;
      chest  = nullptr;
      }

    page = 0;
    renderer.reset();
    pagePl .reset();
    pageOth.reset();
    update();
    }
  }

void InventoryMenu::processMove(KeyEvent& e) {
  auto key = keycodec.tr(e);
  if(key==KeyCodec::Forward)
    moveUp();
  else if(key==KeyCodec::Back)
    moveDown();
  else if(key==KeyCodec::Left || key==KeyCodec::RotateL)
    moveLeft(true);
  else if(key==KeyCodec::Right || key==KeyCodec::RotateR)
    moveRight(true);
  }

void InventoryMenu::moveLeft(bool usePage) {
  auto& sel = activePageSel();

  if(usePage && sel.sel%columsCount==0 && page>0)
    page--;
  else if(sel.sel>0)
    sel.sel--;
  nbSeeSelected();   // NB fix 121
  }

void InventoryMenu::moveRight(bool usePage) {
  auto&        pg     = activePage();
  auto&        sel    = activePageSel();
  const size_t pCount = pagesCount();

  if(usePage && ((sel.sel+1u)%columsCount==0 || sel.sel+1u==pg.size() || pg.size()==0) && page+1u<pCount)
    page++;
  else if(sel.sel+1<pg.size())
    sel.sel++;
  nbSeeSelected();   // NB fix 121
  }

void InventoryMenu::moveUp() {
  auto& sel = activePageSel();

  if(sel.sel>=columsCount)
    sel.sel -= columsCount;
  else
    moveLeft(false);
  nbSeeSelected();   // NB fix 121
  }

void InventoryMenu::moveDown() {
  auto& pg  = activePage();
  auto& sel = activePageSel();

  if(sel.sel+columsCount<pg.size())
    sel.sel += columsCount;
  else
    moveRight(false);
  nbSeeSelected();   // NB fix 121
  }

void InventoryMenu::keyDownEvent(KeyEvent &e) {
  if(state==State::Closed || state==State::LockPicking){
    e.ignore();
    return;
    }

  // A tap of the touch screen is a press that is released at once: it moves one stack of items once. Only a key that is
  // really held (a keyboard, a gamepad) repeats the transfer.
  keyFromTouch = e.code>=0x110000u;
#ifdef OPENGOTHIC_TOUCH_UI
  touchSyncColumns();
#endif

#ifndef OPENGOTHIC_TOUCH_UI
  if(int dir = categoryKeyDir(e)) {
    shiftCategory(dir);
    adjustScroll();
    update();
    return;
    }
#endif

  processMove(e);

#ifdef OPENGOTHIC_TOUCH_UI
  // NB fix 110: C (the button of the card) puts the selected item on the hotbar or takes it off
  // NB fix 111: Shift (the button of the card) puts the selected item on the wheel of the quick choice or takes it off
  if(e.key==KeyEvent::K_LShift && mobileCanHotbar()) {
    auto& pg  = activePage();
    auto  it  = pg.get(activePageSel().sel);
    if(it.isValid()) {
      const int  s  = NbWheel::toggle(*player,*it);
      const bool on = s>=0 && NbWheel::slotOf(NbHotbar::itemName(*player,*it))==s;
      std::string msg;
      if(s<0)
        msg = "Ko\xB3o pe\xB3ne (8 miejsc)"; else
        msg = on ? "Na kole szybkiego wyboru" : "Zdj\xEAte z ko\xB3""a";
      Gothic::inst().onPrintScreen(msg,-1,70,2,Resources::font(1.f));
      }
    update();
    return;
    }
  if(e.key==KeyEvent::K_C && mobileCanHotbar()) {
    auto& pg  = activePage();
    auto  it  = pg.get(activePageSel().sel);
    if(it.isValid()) {
      const int s = NbHotbar::toggle(*player,*it);
      const bool on = s>=0 && NbHotbar::slotOf(NbHotbar::itemName(*player,*it))==s;
      std::string msg;
      if(s<0)
        msg = "Pasek pe\xB3ny (sloty 3-0 zaj\xEAte)"; else
        msg = std::string(on ? "Na pasku: " : "Zdj\xEAte z paska: ")+std::to_string(s==9 ? 0 : s+1);
      Gothic::inst().onPrintScreen(msg,-1,70,2,Resources::font(1.f));
      }
    update();
    return;
    }
#endif

  if(keycodec.tr(e)==KeyCodec::Jump) {
    lootMode = LootMode::Stack;
    startTakeTimer();
    onTakeStuff();
    }
  else if (keycodec.tr(e)==KeyCodec::ActionGeneric || e.key==KeyEvent::K_Return) {
    onItemAction(Item::NSLOT);
    }
  else if((KeyEvent::K_3<=e.key && e.key<=KeyEvent::K_9) || e.key==KeyEvent::K_0) {
    uint8_t slot = 10;
    if((KeyEvent::K_3<=e.key && e.key<=KeyEvent::K_9))
      slot = uint8_t(e.key-KeyEvent::K_0);
    onItemAction(slot);
    }
  else if(e.key==KeyEvent::K_ESCAPE || keycodec.tr(e)==KeyCodec::Inventory){
    close();
    }
  else if(e.key==KeyEvent::K_Space) {
    lootMode = LootMode::Normal;
    startTakeTimer();
    onTakeStuff();
    }
  else if(e.key==KeyEvent::K_Z) {
    lootMode = LootMode::Ten;
    startTakeTimer();
    onTakeStuff();
    }
  else if(e.key==KeyEvent::K_X) {
    lootMode = LootMode::Hundred;
    startTakeTimer();
    onTakeStuff();
    }

  adjustScroll();
  update();
  }

void InventoryMenu::keyRepeatEvent(KeyEvent& e) {
  if(state==State::LockPicking || state==State::Closed)
    return;
#ifndef OPENGOTHIC_TOUCH_UI
  if(categoryKeyDir(e)!=0)
    return;
#endif
  processMove(e);
  adjustScroll();
  update();
  }

void InventoryMenu::keyUpEvent(KeyEvent&) {
  takeTimer.stop();
  lootMode = LootMode::Normal;
  takeCount = 0;
  }

void InventoryMenu::mouseDownEvent(MouseEvent &e) {
#ifdef OPENGOTHIC_TOUCH_UI
  // Touches are taken by AndroidInput, see mobileTap
  e.ignore();
  return;
#endif
  if(player==nullptr || state==State::Closed) {
    e.ignore();
    return;
    }

  if(state==State::LockPicking)
    return;

  keyFromTouch = false;
  if (e.button==MouseEvent::ButtonLeft)
    onItemAction(Item::NSLOT);
  else if (e.button==MouseEvent::ButtonRight)
    close();

  adjustScroll();
  }

void InventoryMenu::mouseUpEvent(MouseEvent&) {
  takeTimer.stop();
  takeCount=0;
  }

void InventoryMenu::mouseWheelEvent(MouseEvent &e) {
  if(state==State::Closed) {
    e.ignore();
    return;
    }

  if(state==State::LockPicking)
    return;

  scrollDelta += e.delta;
  if(scrollDelta>0) {
    for(int i=0;i<scrollDelta/120;++i)
      moveUp();
    scrollDelta %= 120;
    } else {
    for(int i=0;i<-scrollDelta/120;++i)
      moveDown();
    scrollDelta %= 120;
    }
  adjustScroll();
  }

size_t InventoryMenu::rowsCount() const {
#ifdef OPENGOTHIC_TOUCH_UI
  return size_t(touchLayout().rows);
#else
  int iy=30+34+70;
  return size_t((h()-iy-infoHeight()-20)/slotSize().h);
#endif
  }

void InventoryMenu::paintEvent(PaintEvent &e) {
  // NB fix 107: a document read from the inventory (a book, a letter) is shown over it - the frames of the inventory
  // were drawn over the document
  if(player==nullptr || state==State::Closed || covered)
    return;
  if(nbAutoLooting() && state!=State::LockPicking)   // NB fix 170: the quick loot - the window is not shown
    return;
  renderer.reset();

  Painter p(e);
  drawAll(p,*player,DrawPass::Back);
  }

void InventoryMenu::paintNumOverlay(PaintEvent& e) {
  if(player==nullptr || state==State::Closed || covered)
    return;

  Painter p(e);
  drawAll(p,*player,DrawPass::Front);
  }

Size InventoryMenu::slotSize() const {
#ifdef OPENGOTHIC_TOUCH_UI
  const int side = std::max(40,int(touchLayout().side));
  return Size(side,side);
#else
  const float scale = Gothic::interfaceScale(this);
  const float cell  = float(Gothic::options().inventoryCellSize);
  return Size(int(cell*scale),int(cell*scale));
#endif
  }

#ifdef OPENGOTHIC_TOUCH_UI
float InventoryMenu::touchUnit() const {
  return std::min(float(w())/1000.f,float(h())/500.f);
  }

float InventoryMenu::touchTextScale() const {
  return std::max(0.8f,float(h())/720.f)*Gothic::interfaceScale(this);
  }

Mobile::InventoryLayout InventoryMenu::touchLayout() const {
  return Mobile::inventoryLayout(float(w()),float(h()),pageOth!=nullptr,Gothic::interfaceScale(this));
  }
#endif

int InventoryMenu::infoHeight() const {
  const float scale = Gothic::interfaceScale(this);
  return (Item::MAX_UI_ROWS+2)*int(float(Resources::font(scale).pixelSize()))+int(scale*10)/*padding bottom*/;
  }

size_t InventoryMenu::pagesCount() const {
  if(state==State::Chest || state==State::Trade)
    return 2;
  return 1;
  }

const InventoryMenu::Page &InventoryMenu::activePage() {
  if(pageOth!=nullptr)
    return *(page==0 ? pageOth : pagePl);
  if(pagePl)
    return *pagePl;

  static Page n;
  return n;
  }

InventoryMenu::PageLocal &InventoryMenu::activePageSel() {
  if(pageOth!=nullptr)
    return (page==0 ? pageLocal[0] : pageLocal[1]);
  return pageLocal[1];
  }
  
void InventoryMenu::startTakeTimer() {
  if(keyFromTouch) {
    takeTimer.stop();
    return;
    }
  takeTimer.start(200);
  }

std::array<int32_t,8> InventoryMenu::nbGainValues(Npc& pl) const {
  // NB fix 156: life, max life, mana, max mana, strength, dexterity, the intelligence and the max stamina of NB (variables
  // of the scripts)
  std::array<int32_t,8> v{};
  v[0] = pl.attribute(ATR_HITPOINTS);
  v[1] = pl.attribute(ATR_HITPOINTSMAX);
  v[2] = pl.attribute(ATR_MANA);
  v[3] = pl.attribute(ATR_MANAMAX);
  v[4] = pl.attribute(ATR_STRENGTH);
  v[5] = pl.attribute(ATR_DEXTERITY);
  auto& vm  = pl.world().script().getVm();
  auto  var = [&vm](std::string_view n) -> int32_t {
    auto* s = vm.find_symbol_by_name(n);
    return s!=nullptr && s->type()==zenkit::DaedalusDataType::INT && !s->is_member() && s->count()==1 ? s->get_int() : 0;
    };
  v[6] = var("ATR_INTELLECT");
  v[7] = var("ATR_STAMINA_MAX");
  return v;
  }

void InventoryMenu::onItemAction(uint8_t slotHint) {
  infoItem.reset();
  auto& page = activePage();
  auto& sel  = activePageSel();

  auto it = page.get(sel.sel);
  if(!it.isValid())
    return;

  if(state==State::Equip) {
    const size_t clsId = it->clsId();
    nbRead(clsId);   // NB fix 122: a document is read with this action
    if(it.isEquipped() && slotHint==Item::NSLOT) {
      player->unequipItem(clsId);
      } else {
      // NB fix 156: the values before the use - the card shows what the item gave
      if(nbGainUntil<Application::tickCount())
        nbGainBase = nbGainValues(*player);
      nbGainUntil = Application::tickCount()+6000;
      player->useItem(clsId,slotHint,false);
      auto it2 = page.get(sel.sel);
      if((!it2.isValid() || it2->clsId()!=clsId) && sel.sel>0)
        --sel.sel;
      }
    }
  else if(state==State::Chest || state==State::Trade || state==State::Ransack) {
    lootMode = LootMode::Normal;
    startTakeTimer();
    onTakeStuff();
    }
  }

void InventoryMenu::onTakeStuff() {
  infoItem.reset();
  size_t itemCount = 0;
  auto& page = activePage();
  auto& sel  = activePageSel();
  if(sel.sel >= page.size())
    return;
  auto it = page.get(sel.sel);
  if(lootMode==LootMode::Normal) {
    ++takeCount;
    itemCount = uint32_t(std::pow(10,takeCount / 10));
    if(it.count() <= itemCount) {
      itemCount = uint32_t(it.count());
      takeCount = 0;
      }
    }
  else if(lootMode==LootMode::Stack) {
    itemCount = it.count();
    }
  else if(lootMode==LootMode::Ten) {
    itemCount = 10;
    }
  else if(lootMode==LootMode::Hundred) {
    itemCount = 100;
    }

  if(it.count() < itemCount) {
    itemCount = it.count();
    }

  if(state==State::Chest) {
    if(page.is(&player->inventory())) {
      player->moveItem(it->clsId(),*chest,itemCount);
      } else {
      player->addItem (it->clsId(),*chest,itemCount);
      }
    }
  else if(state==State::Trade) {
    if(page.is(&player->inventory())) {
      player->sellItem(it->clsId(),*trader,itemCount);
      } else {
      player->buyItem (it->clsId(),*trader,itemCount);
      }
    }
  else if(state==State::Ransack) {
    if(page.is(&trader->inventory())) {
      player->addItem(it->clsId(),*trader,itemCount);
      }
    }
  else if(state==State::Equip) {
    player->dropItem(it->clsId(),itemCount);
    }
  adjustScroll();
  }

void InventoryMenu::adjustScroll() {
  adjustScrollRows();
#ifdef OPENGOTHIC_TOUCH_UI
  // the first visible row follows the selection, the list moves to it
  auto&       sel  = activePageSel();
  const float side = float(std::max(1,slotSize().h));
  if(size_t(sel.scrollPx/side)!=sel.scroll)
    sel.scrollPx = float(sel.scroll)*side;
#endif
  }

void InventoryMenu::adjustScrollRows() {
  auto& page = activePage();
  auto& sel  = activePageSel();
  sel.sel = std::min(sel.sel, std::max<size_t>(page.size(),1)-1);
  while(sel.sel<sel.scroll*columsCount) {
    if(sel.scroll<=1){
      sel.scroll=0;
      return;
      }
    sel.scroll-=1;
    }

  const size_t hcount=rowsCount();
  while(sel.sel>=(sel.scroll+hcount)*columsCount) {
    sel.scroll+=1;
    }
  }

void InventoryMenu::drawAll(Painter &p, Npc &player, DrawPass pass) {
#ifdef OPENGOTHIC_TOUCH_UI
  drawAllTouch(p,player,pass);
  return;
#else
  const int padd    = 43;
  const int iy      = 30+34+70;
  const int headerY = 70;
  const int goldW   = 2*slotSize().w;

  if(state==State::LockPicking)
    return;

  const int wcount = int(columsCount);
  const int hcount = int(rowsCount());

  if(chest!=nullptr){
    if(pass==DrawPass::Back)
      drawHeader(p,chest->displayName(),padd,headerY);
    drawItems(p,pass,*pageOth,pageLocal[0],padd,iy,wcount,hcount);
    }

  if(trader!=nullptr) {
    if(pass==DrawPass::Back)
      drawHeader(p,trader->displayName(),padd,headerY);
    drawItems(p,pass,*pageOth,pageLocal[0],padd,iy,wcount,hcount);
    }

  if(state!=State::Ransack) {
    if(pass==DrawPass::Back)
      drawGold (p,player,w()-padd-goldW,headerY);
    drawItems(p,pass,*pagePl,pageLocal[1],w()-padd-wcount*slotSize().w,iy,wcount,hcount);
    if(pass==DrawPass::Back)
      drawCategoryTitle(p,w()-padd-goldW,headerY);
    }

  if(pass==DrawPass::Back)
    drawInfo(p);
#endif
  }

void InventoryMenu::drawItems(Painter &p, DrawPass pass,
                              const Page &inv, const PageLocal& sel, int x0, int y, int wcount, int hcount) {
  if(state==State::LockPicking)
    return;

#ifndef OPENGOTHIC_TOUCH_UI
  if(tex!=nullptr && pass==DrawPass::Back) {
    p.setBrush(*tex);
    p.drawRect(x0,y,slotSize().w*wcount,slotSize().h*hcount,
               0,0,tex->w(),tex->h());
    }
#endif

  int rowsToDraw = hcount;
#ifdef OPENGOTHIC_TOUCH_UI
  // the list is moved by pixels: the first row is cut, one more row shows below
  const int firstRow = int(sel.scrollPx)/std::max(1,slotSize().h);
  y -= int(sel.scrollPx)-firstRow*slotSize().h;
  rowsToDraw = hcount+1;
  touchClip  = Tempest::Rect(x0,y+(int(sel.scrollPx)-firstRow*slotSize().h),slotSize().w*wcount,slotSize().h*hcount);
  p.setScissor(touchClip.x,touchClip.y,touchClip.w,touchClip.h);
#else
  const int firstRow = int(sel.scroll);
#endif
  auto   it = inv.first();
  size_t id = 0;
  for(size_t i=0; it.isValid() && i<size_t(firstRow)*size_t(wcount); ++i) {
    inv.next(it);
    ++id;
    }
  for(int i=0;i<rowsToDraw;++i) {
    for(int r=0;r<wcount;++r) {
      const int x = x0 + r*slotSize().w;
      if(pass==DrawPass::Back) {
#ifdef OPENGOTHIC_TOUCH_UI
        if(hedin.on()) {
          p.setBrush(*hedin.slot);
          p.drawRect(x,y,slotSize().w,slotSize().h,0,0,hedin.slot->w(),hedin.slot->h());
          } else {
          p.setBrush(Brush(Color(0.01f,0.018f,0.025f,0.90f)));
          p.drawRect(x+1,y+1,slotSize().w-2,slotSize().h-2);
          p.setBrush(Brush(touchSlot));
          p.drawRect(float(x),float(y),float(slotSize().w),float(slotSize().h),0.f,0.f,float(touchSlot.w()),float(touchSlot.h()));
          }
#else
        p.setBrush(*slot);
        p.drawRect(x,y,slotSize().w,slotSize().h,
                   0,0,slot->w(),slot->h());
#endif
        }
      if(it.isValid()) {
        drawSlot(p,pass, it,inv,sel, x,y, id);
        inv.next(it);
        ++id;
        }
      }
    y += slotSize().h;
    }
#ifdef OPENGOTHIC_TOUCH_UI
  p.setScissor(0,0,w(),h());
#endif
  }

void InventoryMenu::drawSlot(Painter &p, DrawPass pass, const Inventory::Iterator &it,
                             const Page& page, const PageLocal &sel,
                             int x, int y, size_t id) {
  if(!slot)
    return;

  auto& active = activePage();
#ifdef OPENGOTHIC_TOUCH_UI
  const float scale = touchTextScale();
#else
  const float scale = Gothic::interfaceScale(this);
#endif

  if(pass==DrawPass::Back) {
    const bool selected = (!it.isValid() && id==0) || (id==sel.sel && &page==&active);
    // NB fix 215: an item with ITEM_ACTIVE (Itm_SetFlagActive: the trophies of the scripts) is shown as worn
    const bool worn     = it.isValid() && (it.isEquipped() || (uint32_t(it->handle().flags) & (1u<<30))!=0);
#ifdef OPENGOTHIC_TOUCH_UI
    // NB fix 104: the slots of Hedin's inventory - lit (selected), green (equipped), both
    const Texture2d* hs = !hedin.on() ? nullptr :
                          (selected && worn) ? hedin.selEquip :
                          selected ? hedin.sel : worn ? hedin.equip : nullptr;
    if(hs!=nullptr) {
      p.setBrush(*hs);
      p.drawRect(x,y,slotSize().w,slotSize().h,0,0,hs->w(),hs->h());
      }
    if(hedin.on()) {} else
#endif
    if(selected){
#ifdef OPENGOTHIC_TOUCH_UI
      p.setBrush(Brush(touchSelected));
      p.drawRect(float(x),float(y),float(slotSize().w),float(slotSize().h),0.f,0.f,float(touchSelected.w()),float(touchSelected.h()));
#else
      p.setBrush(*selT);
      p.drawRect(x,y,slotSize().w,slotSize().h,
                 0,0,selT->w(),selT->h());
#endif
      }

    if(worn && selU!=nullptr
#ifdef OPENGOTHIC_TOUCH_UI
       && !hedin.on()
#endif
       ) {
      p.setBrush(*selU);
      p.drawRect(x,y,slotSize().w,slotSize().h,
                 0,0,selU->w(),selU->h());
      }

    const int dsz = (id==sel.sel ? 5 : 0);
#ifdef OPENGOTHIC_TOUCH_UI
    renderer.drawItem(x-dsz, y-dsz, slotSize().w+2*dsz, slotSize().h+2*dsz, *it, &touchClip);
#else
    renderer.drawItem(x-dsz, y-dsz, slotSize().w+2*dsz, slotSize().h+2*dsz, *it);
#endif
    } else {
    auto fnt = Resources::font(scale);

    // NB fix 121: a new item - a tag in the corner of the slot
    if(it.isValid() && nbIsNew(page,*it)) {
      const std::string_view tag = nbMarkPl ? "NOWY" : "NEW";
      auto&     nf  = Resources::font(Resources::FontType::Yellow, scale*0.8f);
      const auto ts = nf.textSize(tag);
      const int pad = std::max(2,int(3.f*scale));
      p.setBrush(Brush(Color(0.f,0.f,0.f,0.70f)));
      p.drawRect(x+4,y+4,ts.w+2*pad,ts.h+pad);
      p.setBrush(Brush(Color(0.95f,0.78f,0.25f,0.95f)));
      p.drawRect(x+4,y+4,std::max(2,int(2.f*scale)),ts.h+pad);
      nf.drawText(p,x+4+pad,y+4+ts.h,tag);
      }

    if(it.count()>1) {
      string_frm vint(int(it.count()));
      auto sz = fnt.textSize(vint);
      fnt.drawText(p,x+slotSize().w-sz.w-10,
                   y+slotSize().h-10,
                   vint);
      }

    if(it.slot()!=Item::NSLOT) {
      fnt = Resources::font(Resources::FontType::Red, scale);

      string_frm vint(int(it.slot()));
      auto sz = fnt.textSize(vint);
      fnt.drawText(p,x+10,
                   y+slotSize().h/2+sz.h/2,
                   vint);
      }
    }
  }

void InventoryMenu::drawGold(Painter &p, Npc &player, int x, int y) {
  if(!slot)
    return;
  auto           w    = Gothic::inst().world();
  auto           txt  = w ? w->script().currencyName() : "";
  const size_t   gold = player.inventory().goldCount();
  if(txt.empty())
    txt="Gold";

  // NB: the plugin writes "Zloto: N" here (itemGold of AB_Translation.lbr), not the name of the coin
  const bool nb = (w!=nullptr && NbItemInfo::isNb(w->script().getVm()));
  string_frm vint(nb ? NbTexts::get("itemGold") : std::string_view(txt), nb ? "" : " : ", int(gold));
#ifdef OPENGOTHIC_TOUCH_UI
  (void)x;
  (void)y;
  drawTouchHeader(p,vint,touchLayout().header[1]);
#else
  drawHeader(p,vint,x,y);
#endif
  }

void InventoryMenu::drawHeader(Painter &p, std::string_view title, int x, int y) {
#ifdef OPENGOTHIC_TOUCH_UI
  const float scale = touchUnit();
#else
  const float scale = Gothic::interfaceScale(this);
#endif
  auto&       fnt   = Resources::font(scale);

  const int   tw    = fnt.textSize(title).w;
  const int   th    = fnt.textSize(title).h;
  const int   padd  = int(8*scale);
  const int   dw    = std::max(slotSize().w*2, tw+padd*2);
  const int   dh    = int(34*scale);

  if(tex) {
    p.setBrush(*tex);
    p.drawRect(x,y,dw,dh, 0,0,tex->w(),tex->h());
    }
  if(slot) {
    p.setBrush(*slot);
    p.drawRect(x,y,dw,dh, 0,0,slot->w(),slot->h());
    }

  fnt.drawText(p,x+(dw-tw)/2,y+dh/2+th/2,title);
  }

void InventoryMenu::drawInfo(Painter &p) {
#ifdef OPENGOTHIC_TOUCH_UI
  drawInfoTouch(p);
  return;
#else
  const float scale = Gothic::interfaceScale(this);
  const int   dh    = infoHeight();
  const int   dw    = std::min(w(), int(720*scale));
  const int   x     = (w()-dw)/2;
  const int   y     = h()-dh-20;

  auto& pg  = activePage();
  auto& sel = activePageSel();

  auto it = pg.get(sel.sel);
  if(!it.isValid())
    return;

  auto& r = *pg.get(sel.sel);
  updateItemInfo(r);
  if(tex) {
    p.setBrush(*tex);
    p.drawRect(x,y,dw,dh,
               0,0,tex->w(),tex->h());
    }

  auto& fnt = Resources::font(scale);
  auto  desc = r.description();
  int   tw   = fnt.textSize(desc).w;

  fnt.drawText(p,x+(dw-tw)/2,y+int(fnt.pixelSize()),desc);

  for(size_t i=0;i<Item::MAX_UI_ROWS;++i){
    auto    txt = r.uiText(i);
    int32_t val = r.uiValue(i);

    if(txt.empty())
      continue;

    if(i+1==Item::MAX_UI_ROWS && state==State::Trade && player!=nullptr && pg.is(&player->inventory())){
      val = r.sellCost();
      }

    string_frm vint(val);
    int tw = fnt.textSize(vint).w;

    fnt.drawText(p, x+20,  y+int(i+2)*fnt.pixelSize(), txt);
    if(val!=0)
      fnt.drawText(p,x+dw-tw-20,y+int(i+2)*fnt.pixelSize(),vint);
    }

  const int sz = dh;
  renderer.drawItem(x+dw-sz-sz/2,y,sz,sz,r);
#endif
  }

void InventoryMenu::draw(Tempest::Encoder<CommandBuffer>& cmd) {
  renderer.draw(cmd);
  }

#ifdef OPENGOTHIC_TOUCH_UI
bool InventoryMenu::touchHit(float px, float py, uint8_t& list, int& column, int& row) const {
  const auto lay = touchLayout();
  for(uint8_t i=0; i<2; ++i) {
    if((i==1 && state==State::Ransack) || (i==0 && pageOth==nullptr))
      continue;
    const auto& r = lay.list[i];
    if(!r.contains(px,py))
      continue;
    list   = i;
    column = std::min(int((px-r.x)/lay.side),lay.cols-1);
    row    = int((py-r.y+pageLocal[i].scrollPx)/lay.side);   // of the whole list
    return true;
    }
  return false;
  }

void InventoryMenu::mobileTap(float px, float py) {
  if(player==nullptr || state==State::Closed || state==State::LockPicking)
    return;
  touchSyncColumns();

  uint8_t list   = 0;
  int     column = 0, row = 0;
  if(!touchHit(px,py,list,column,row))
    return;

  Page*      selected = list==1 ? pagePl.get() : pageOth.get();
  PageLocal* local    = &pageLocal[list];
  page = list==1 ? (pageOth ? 1 : 0) : 0;

  const size_t id = size_t(row)*columsCount+size_t(column);
  if(selected==nullptr || id>=selected->size())
    return;
  local->sel = id;
  nbSeeSelected();   // NB fix 121
  adjustScroll();
  update();
  }

InventoryMenu::PageLocal* InventoryMenu::touchList(float px, float py, const Page*& list, uint8_t& index) {
  int column = 0, row = 0;
  if(!touchHit(px,py,index,column,row))
    return nullptr;
  list = index==1 ? pagePl.get() : pageOth.get();
  return &pageLocal[index];
  }

float InventoryMenu::touchMaxScroll(const Page& list) const {
  const size_t cols = size_t(touchLayout().cols);
  const size_t rows = (list.size()+cols-1)/cols;
  const size_t view = rowsCount();
  return rows>view ? float(rows-view)*float(slotSize().h) : 0.f;
  }

/// The list under the finger follows it. Phases: 0 - down, 1 - move, 2 - up, 3 - cancelled.
void InventoryMenu::mobileTouch(float x, float y, int phase) {
  if(player==nullptr || state==State::Closed || state==State::LockPicking) {
    touchDrag = TouchDrag();
    return;
    }

  if(phase==0) {
    const Page* list  = nullptr;
    uint8_t     index = 0;
    auto*       local = touchList(x,y,list,index);
    touchDrag = TouchDrag();
    if(local==nullptr)
      return;
    touchDrag.active      = true;
    touchDrag.list        = index;
    touchDrag.startY      = y;
    touchDrag.startScroll = local->scrollPx;
    return;
    }

  if(!touchDrag.active)
    return;
  if(phase>=2) {
    touchDrag = TouchDrag();
    return;
    }

  const Page* list = touchDrag.list==1 ? pagePl.get() : pageOth.get();
  if(list==nullptr)
    return;

  // The list follows the finger, dragging up reveals the lower rows
  auto&       local = pageLocal[touchDrag.list];
  const float side  = float(std::max(1,slotSize().h));
  local.scrollPx = std::clamp(touchDrag.startScroll+(touchDrag.startY-y),0.f,touchMaxScroll(*list));
  local.scroll   = size_t(local.scrollPx/side);

  // The selected item stays on the screen, so the buttons never act on a hidden one
  touchSyncColumns();
  const size_t first = local.scroll*columsCount;
  const size_t last  = (local.scroll+rowsCount())*columsCount;
  if(local.sel<first)
    local.sel = first; else
  if(local.sel>=last)
    local.sel = last-1;
  local.sel = std::min(local.sel,std::max<size_t>(list->size(),1)-1);
  update();
  }

void InventoryMenu::drawAllTouch(Painter& p, Npc& player, DrawPass pass) {
  if(state==State::LockPicking)
    return;
  touchSyncColumns();

  const auto lay    = touchLayout();
  const int  wcount = lay.cols;
  const int  hcount = lay.rows;
  const bool own    = state!=State::Ransack;

  // NB fix 99: the frames go first, the slots and the card over them
  if(pass==DrawPass::Back) {
    if(pageOth!=nullptr)
      drawTouchPanel(p,lay.panel[0],false);
    if(own)
      drawTouchPanel(p,lay.panel[1],false);
    drawTouchPanel(p,lay.info,true);
    }

  auto list = [&](const Page& inv, const PageLocal& sel, size_t index) {
    const auto& r = lay.list[index];
    drawItems(p,pass,inv,sel,int(r.x),int(r.y),wcount,hcount);
    if(pass==DrawPass::Front)
      drawScrollHints(p,inv,sel,r);
    };

  if(chest!=nullptr) {
    if(pass==DrawPass::Back)
      drawTouchHeader(p,chest->displayName(),lay.header[0]);
    list(*pageOth,pageLocal[0],0);
    }

  if(trader!=nullptr) {
    if(pass==DrawPass::Back)
      drawTouchHeader(p,trader->displayName(),lay.header[0]);
    list(*pageOth,pageLocal[0],0);
    }

  if(own) {
    if(pass==DrawPass::Back) {
      drawGold(p,player,0,0);
      // the name of the category and the number of the items, on the right of the gold
      const auto  title = touchCategoryTitle();
      const auto& r     = lay.header[1];
      auto&       fnt   = Resources::font(touchTextScale()*1.1f);
      const auto  sz    = fnt.textSize(title);
      fnt.drawText(p,int(r.x+r.w)-sz.w,int(r.y+r.h/2)+sz.h/2,title);
      }
    list(*pagePl,pageLocal[1],1);
    }

  if(pass==DrawPass::Back)
    drawInfo(p);
  if(pass==DrawPass::Front)
    nbDrawGains(p);
  }

void InventoryMenu::nbDrawGains(Painter& p) {
  // NB fix 156: what the used item gave (food, potions) - floating notes in the middle of the screen, they go up and
  // fade in 2.5 s; green - a gain, red - a loss. A change that comes later (the effect after the animation of eating)
  // makes a new note; the values are watched 6 s after the use
  const uint64_t now = Application::tickCount();
  if(player!=nullptr && nbGainUntil>=now) {
    const auto cur   = nbGainValues(*player);
    const bool plTxt = touchPolish();
    static const char* namePl[8] = {"\xAFycie","Maks. \xBFycie","Mana","Maks. mana","Si\xB3""a","Zr\xEA""czno\x9C\xE6","Inteligencja","Maks. wytrzyma\xB3o\x9C\xE6"};
    static const char* nameEn[8] = {"Life","Max life","Mana","Max mana","Strength","Dexterity","Intelligence","Max stamina"};
    for(size_t i=0; i<cur.size(); ++i) {
      const int32_t d = cur[i]-nbGainBase[i];
      if(d==0)
        continue;
      NbFloat f;
      f.text  = std::string(plTxt ? namePl[i] : nameEn[i])+(d>0 ? " +" : " ")+std::to_string(d);
      f.loss  = d<0;
      f.start = now+uint64_t(nbFloats.size()%4)*150;   // a few at once: one after another
      nbFloats.push_back(std::move(f));
      }
    nbGainBase = cur;
    }

  constexpr uint64_t life = 2500;
  nbFloats.erase(std::remove_if(nbFloats.begin(),nbFloats.end(),[now](const NbFloat& f){ return now>f.start+life; }),nbFloats.end());
  if(nbFloats.empty())
    return;

  const float H  = float(h());
  auto&       fb = Resources::font(touchTextScale()*1.4f);
  int         n  = 0;
  for(auto& f:nbFloats) {
    if(now<f.start)
      continue;
    const float t     = float(now-f.start)/float(life);   // 0..1
    const float alpha = t<0.6f ? 1.f : std::max(0.f,1.f-(t-0.6f)/0.4f);
    const Color c     = f.loss ? Color(1.f,0.25f,0.2f,alpha) : Color(0.35f,1.f,0.35f,alpha);
    const auto  fnt   = fb.tinted(c);
    const auto  sz    = fnt.textSize(f.text);
    const int   x     = (w()-sz.w)/2;
    const int   y     = int(H*0.5f - t*H*0.12f) + n*int(float(fb.pixelSize())*1.2f);
    // a dark shadow under the letters, for the light ground
    const auto  sh    = fb.tinted(Color(0.f,0.f,0.f,alpha*0.8f));
    sh.drawText(p,x+2,y+2,f.text);
    fnt.drawText(p,x,y,f.text);
    ++n;
    }
  update();
  }

void InventoryMenu::touchSyncColumns() {
  columsCount = size_t(std::max(1,touchLayout().cols));
  }

bool InventoryMenu::touchPolish() const {
  // New Balance is a Polish mod, the labels of the buttons are in its language
  auto w = Gothic::inst().world();
  return w!=nullptr && NbItemInfo::isNb(w->script().getVm());
  }

std::string InventoryMenu::touchCategoryTitle() const {
  int index = 0;
  for(int i=0; i<int(CatCount); ++i)
    if(CategoryOrder[i]==category())
      index = i;

  std::string name;
  if(player!=nullptr) {
    const auto& names = player->world().script().inventoryCategoryNames();
    if(index<int(names.size()))
      name = names[size_t(index)];
    }
  // the order of CategoryOrder; the Polish ones in the code page of the font
  static const char* en[CatCount] = {"All","Weapons","Armor","Magic","Jewelry","Food","Potions","Documents","Other"};
  static const char* pl[CatCount] = {"Wszystko","Bronie","Pancerze","Magia","Bi\xBF" "uteria","Jedzenie","Mikstury","Dokumenty","Inne"};
  // NB fix 156: TXT_INV_CAT of New Balance is half in English ("Other", "Weapons", "Potions") and its first name is
  // "Other" for the tab of all the items - the Polish names of the port for these
  if(touchPolish() && !name.empty()) {
    static const char* engl[] = {"Other","Weapons","Armor","Magic","Artifacts","Food","Potions","Documents","Misc"};
    for(auto e:engl)
      if(name==e)
        name.clear();
    if(index==0)
      name.clear();
    }
  if(name.empty())
    name = touchPolish() ? pl[index] : en[index];
  const size_t count = pagePl!=nullptr ? pagePl->size() : 0;
  return name+" ("+std::to_string(count)+")";
  }

/// A frame of metal; the lists are dark inside, the card of the item is a parchment
void InventoryMenu::drawTouchPanel(Painter& p, const Mobile::Rect& r, bool parchment) {
  if(r.w<=0 || r.h<=0)
    return;
  if(hedin.on()) {
    // NB fix 104: the background of Hedin's inventory, with its own frame
    p.setBrush(Brush(Color(0.f,0.f,0.f,0.45f)));
    p.drawRect(int(r.x)+4,int(r.y)+6,int(r.w),int(r.h));
    p.setBrush(*hedin.panel);
    p.drawRect(int(r.x),int(r.y),int(r.w),int(r.h),0,0,hedin.panel->w(),hedin.panel->h());
    return;
    }
  const float H     = float(h());
  const float outer = std::max(3.f,H*0.009f);
  const float inner = std::max(1.f,H*0.0025f);
  const float x = r.x, y = r.y, w = r.w, hh = r.h;

  auto box = [&](float bx, float by, float bw, float bh, const Color& c) {
    p.setBrush(Brush(c));
    p.drawRect(int(bx),int(by),int(std::ceil(bw)),int(std::ceil(bh)));
    };

  // shadow, the metal and its dark inner edge
  box(x-outer*0.5f,y-outer*0.5f+outer,w+outer,hh+outer,Color(0.f,0.f,0.f,0.45f));
  box(x-outer,y-outer,w+2*outer,hh+2*outer,Color(0.33f,0.33f,0.32f,0.98f));
  box(x-outer,y-outer,w+2*outer,inner,Color(0.62f,0.62f,0.60f,0.98f));                  // light on the top
  box(x-inner,y-inner,w+2*inner,hh+2*inner,Color(0.10f,0.10f,0.10f,0.98f));

  if(parchment) {
    box(x,y,w,hh,Color(0.62f,0.50f,0.33f,0.98f));
    const float band = H*0.014f;
    box(x+band,y+band,w-2*band,hh-2*band,Color(0.80f,0.70f,0.52f,0.98f));
    box(x+band*2,y+band*2,w-4*band,hh-4*band,Color(0.84f,0.75f,0.58f,0.98f));
    } else {
    box(x,y,w,hh,Color(0.035f,0.045f,0.055f,0.90f));
    }

  // the rivets in the corners
  const float rv = outer*1.6f;
  const Color rc(0.55f,0.55f,0.53f,1.f);
  box(x-outer*1.3f,y-outer*1.3f,rv,rv,rc);
  box(x+w+outer*1.3f-rv,y-outer*1.3f,rv,rv,rc);
  box(x-outer*1.3f,y+hh+outer*1.3f-rv,rv,rv,rc);
  box(x+w+outer*1.3f-rv,y+hh+outer*1.3f-rv,rv,rv,rc);
  }

void InventoryMenu::drawTouchButton(Painter& p, const Mobile::Rect& r, std::string_view label) {
  const float H      = float(h());
  const int   border = std::max(2,int(H*0.004f));
  p.setBrush(Brush(Color(0.36f,0.37f,0.38f,0.98f)));
  p.drawRect(int(r.x),int(r.y),int(r.w),int(r.h));
  p.setBrush(Brush(Color(0.09f,0.12f,0.15f,0.98f)));
  p.drawRect(int(r.x)+border,int(r.y)+border,int(r.w)-2*border,int(r.h)-2*border);

  float sc = touchTextScale()*1.15f;
  for(int i=0; i<8; ++i) {
    if(Resources::font(sc).textSize(label).w<=int(r.w)-4*border)
      break;
    sc *= 0.9f;
    }
  auto&      fnt = Resources::font(sc);
  const auto sz  = fnt.textSize(label);
  fnt.drawText(p,int(r.x+(r.w-float(sz.w))/2),int(r.y+r.h/2)+sz.h/2,label);
  }

void InventoryMenu::drawTouchHeader(Painter& p, std::string_view title, const Mobile::Rect& r) {
  // NB fix 99: the title is a bare text on the top of the frame, on the left
  auto&      fnt = Resources::font(touchTextScale()*1.1f);
  const auto sz  = fnt.textSize(title);
  fnt.drawText(p,int(r.x),int(r.y+r.h/2)+sz.h/2,title);
  }

/// Arrows over the lists, when there are items out of sight
void InventoryMenu::drawScrollHints(Painter& p, const Page& inv, const PageLocal& sel, const Mobile::Rect& r) {
  const float side = touchLayout().side;
  const float a    = side*0.14f;
  const float cx   = r.x+r.w/2;

  if(hedin.up!=nullptr && hedin.down!=nullptr) {
    // NB fix 104: the arrows of Hedin's inventory
    const float s = side*0.42f;
    if(sel.scrollPx<touchMaxScroll(inv)-1.f) {
      p.setBrush(*hedin.down);
      p.drawRect(int(cx-s/2),int(r.y+r.h-s*0.9f),int(s),int(s),0,0,hedin.down->w(),hedin.down->h());
      }
    if(sel.scrollPx>1.f) {
      p.setBrush(*hedin.up);
      p.drawRect(int(cx-s/2),int(r.y-s*0.1f),int(s),int(s),0,0,hedin.up->w(),hedin.up->h());
      }
    return;
    }

  p.setBrush(Brush(Color(1.f,1.f,1.f,0.75f)));
  if(sel.scrollPx<touchMaxScroll(inv)-1.f) {
    const float y = r.y+r.h-side*0.16f;
    p.drawTriangle(cx-a*1.4f,y-a*0.7f,0.f,0.f, cx+a*1.4f,y-a*0.7f,0.f,0.f, cx,y+a*0.7f,0.f,0.f);
    }
  if(sel.scrollPx>1.f) {
    const float y = r.y+side*0.16f;
    p.drawTriangle(cx-a*1.4f,y+a*0.7f,0.f,0.f, cx+a*1.4f,y+a*0.7f,0.f,0.f, cx,y-a*0.7f,0.f,0.f);
    }
  }

void InventoryMenu::drawInfoTouch(Painter& p) {
  // NB fix 99: the card of the item - the name on the top, the properties on the left of the picture and below it,
  // the actions at the bottom. The text is dark, the font of the books.
  auto& pg  = activePage();
  auto& sel = activePageSel();
  auto  it  = pg.get(sel.sel);
  if(!it.isValid())
    return;
  auto& item = *pg.get(sel.sel);
  updateItemInfo(item);

  const auto  lay  = touchLayout();
  const auto& r    = lay.info;
  if(r.w<=0)
    return;
  const float H    = float(h());
  const int   cpad = int(H*0.03f);
  const int   x0   = int(r.x)+cpad;
  const int   x1   = int(r.x+r.w)-cpad;

  const bool dark = hedin.on();   // NB fix 104: the dark card of Hedin - the white font of the game
  auto book = [dark](float sc) -> const GthFont& {
    sc = std::max(0.5f,std::floor(sc*20.f)/20.f);   // a font is cached per scale
    if(dark)
      return Resources::font(sc);
    return Resources::font("FONT_10_BOOK.TGA",Resources::FontType::Normal,sc);
    };

  // the name, smaller when it is too wide
  const auto desc = item.description();
  float tsc = touchTextScale()*1.3f;   // NB fix 156: 1.45 - smaller with the text below
  for(int i=0; i<10 && book(tsc).textSize(desc).w>x1-x0; ++i)
    tsc *= 0.9f;
  auto& title = book(tsc);
  const int titleY = int(r.y+H*0.035f)+title.pixelSize();
  title.drawText(p,int(r.x+(r.w-float(title.textSize(desc).w))/2),titleY,desc);

  // the picture in a dark box
  const auto& pic = lay.picture;
  {
    const int b = std::max(2,int(H*0.004f));
    p.setBrush(Brush(Color(0.30f,0.27f,0.22f,1.f)));
    p.drawRect(int(pic.x)-b,int(pic.y)-b,int(pic.w)+2*b,int(pic.h)+2*b);
    p.setBrush(Brush(Color(0.10f,0.13f,0.15f,1.f)));
    p.drawRect(int(pic.x),int(pic.y),int(pic.w),int(pic.h));
    const int in = int(pic.w*0.06f);
    renderer.drawItem(int(pic.x)+in,int(pic.y)+in,int(pic.w)-2*in,int(pic.h)-2*in,item);
  }

  // the properties: "text value", broken into lines by the words
  std::vector<std::string> rows;
  for(size_t i=0; i<Item::MAX_UI_ROWS; ++i) {
    auto    txt = item.uiText(i);
    int32_t val = item.uiValue(i);
    if(txt.empty())
      continue;
    if(i+1==Item::MAX_UI_ROWS && state==State::Trade && player!=nullptr && pg.is(&player->inventory()))
      val = item.sellCost();
    std::string row(txt);
    if(val!=0)
      row += " "+std::to_string(val);
    rows.push_back(std::move(row));
    }

  const int top    = int(pic.y);
  const int narrow = int(pic.x)-int(H*0.02f);          // on the left of the picture
  const int below  = int(pic.y+pic.h)+int(H*0.015f);   // from here the lines are as wide as the card
  const int bottom = int(mobileCanHotbar() ? lay.hotbar.y : lay.use.y)-int(H*0.02f);   // NB fix 110

  struct Line { std::string text; int y; };
  std::vector<Line> lines;
  float rsc = touchTextScale()*0.82f;   // NB fix 156: the properties smaller (were as big as the name)
  for(int attempt=0; attempt<8; ++attempt) {
    lines.clear();
    auto&     f    = book(rsc);
    const int step = f.pixelSize()+f.pixelSize()/5;
    int       y    = top+f.pixelSize();
    bool      fits = true;
    for(auto& row:rows) {
      std::string line, word;
      auto flush = [&]() {
        lines.push_back({line,y});
        y += step;
        line.clear();
        };
      auto widthAt = [&](int yy) { return (yy-f.pixelSize()<below ? narrow : x1)-x0; };
      size_t pos = 0;
      while(pos<=row.size()) {
        size_t sp = row.find(' ',pos);
        if(sp==std::string::npos)
          sp = row.size();
        word = row.substr(pos,sp-pos);
        pos  = sp+1;
        std::string next = line.empty() ? word : line+" "+word;
        if(!line.empty() && f.textSize(next).w>widthAt(y))
          flush(), next = word;
        line = next;
        }
      if(!line.empty())
        flush();
      }
    if(!lines.empty() && lines.back().y>bottom)
      fits = false;
    if(fits)
      break;
    rsc *= 0.9f;
    }
  auto& f = book(rsc);
  for(auto& l:lines)
    f.drawText(p,x0,l.y,l.text);


  // the actions
  const bool pl = touchPolish();
  auto label = [&](std::string_view en) -> std::string_view {
    if(!pl) {
      if(en=="Equip")   return "EQUIP";
      if(en=="Unequip") return "UNEQUIP";
      if(en=="Read")    return "READ";
      if(en=="Use")     return "USE";
      if(en=="Store")   return "STORE";
      if(en=="Take")    return "TAKE";
      if(en=="Sell")    return "SELL";
      if(en=="Buy")     return "BUY";
      if(en=="Drop")    return "DROP";
      if(en=="Move")    return "MOVE";
      return en;
      }
    // in the code page of the font: \xA3 Ł, \xD3 Ó, \xAF Ż, \xC6 Ć, \x8F Ź, \x8C Ś
    if(en=="Equip")   return "ZA\xA3\xD3\xAF";
    if(en=="Unequip") return "ZDEJMIJ";
    if(en=="Read")    return "CZYTAJ";
    if(en=="Use")     return "U\xAFYJ";
    if(en=="Store")   return "SCHOWAJ";
    if(en=="Take")    return "WE\x8F";
    if(en=="Sell")    return "SPRZEDAJ";
    if(en=="Buy")     return "KUP";
    if(en=="Drop")    return "WYRZU\xC6";
    if(en=="Move")    return "PRZENIE\x8C";
    return en;
    };
  drawTouchButton(p,lay.use, label(mobileActionLabel()));
  drawTouchButton(p,lay.drop,label(state==State::Equip ? "Drop" : "Move"));
  // NB fix 110: the hotbar
  if(mobileCanHotbar()) {
    const bool onBar = NbHotbar::slotOf(NbHotbar::itemName(*player,item))>=0;
    std::string_view t = pl ? (onBar ? std::string_view("Z PASKA") : std::string_view("NA PASEK"))
                            : (onBar ? std::string_view("OFF THE BAR") : std::string_view("TO THE BAR"));
    drawTouchButton(p,lay.hotbar,t);
    // NB fix 111: the wheel of the quick choice
    const bool onWheel = NbWheel::slotOf(NbHotbar::itemName(*player,item))>=0;
    std::string_view tw = pl ? (onWheel ? std::string_view("Z KO\xA3""A") : std::string_view("NA KO\xA3O"))
                             : (onWheel ? std::string_view("OFF THE WHEEL") : std::string_view("TO THE WHEEL"));
    drawTouchButton(p,lay.wheel,tw);
    }
  }

bool InventoryMenu::mobileCanHotbar() {
  // NB fix 110: an item of the hero that can be used or worn: weapons, potions, food, scrolls...
  if(player==nullptr || state!=State::Equip)
    return false;
  auto& pg = activePage();
  auto  it = pg.get(activePageSel().sel);
  if(!it.isValid())
    return false;
  const auto flag = it->mainFlag();
  return (flag & (ITM_CAT_NF|ITM_CAT_FF|ITM_CAT_FOOD|ITM_CAT_POTION))!=0;   // the runes go there by the game itself
  }

std::string_view InventoryMenu::mobileActionLabel() {
  if(player==nullptr || state==State::Closed || state==State::LockPicking)
    return "";
  auto& pg  = activePage();
  auto& sel = activePageSel();
  auto  it  = pg.get(sel.sel);
  if(!it.isValid())
    return "";

  const bool own = pg.is(&player->inventory());
  switch(state) {
    case State::Chest:   return own ? "Store" : "Take";
    case State::Trade:   return own ? "Sell"  : "Buy";
    case State::Ransack: return "Take";
    default:             break;
    }

  if(it.isEquipped())
    return "Unequip";
  const auto flag = it->mainFlag();
  if(it->nbUsableJewel())   // NB fix 166: the rings of the teleport are used
    return "Use";
  if(flag&(ITM_CAT_NF|ITM_CAT_FF|ITM_CAT_MUN|ITM_CAT_ARMOR|ITM_CAT_RUNE|ITM_CAT_LIGHT|ITM_CAT_MAGIC))
    return "Equip";
  if(flag&ITM_CAT_DOCS)
    return "Read";
  return "Use";
  }
#endif

void InventoryMenu::mobileScroll(float dy) {
  if(player==nullptr || state==State::Closed)
    return;
  if(dy<0)
    moveDown(); else
    moveUp();
  adjustScroll();
  update();
  }

bool InventoryMenu::hasSelectedItem() {
  return player!=nullptr && state!=State::Closed && activePageSel().sel<activePage().size();
  }

#ifndef OPENGOTHIC_TOUCH_UI
bool InventoryMenu::hasCategories() const {
  return player!=nullptr && player->world().script().inventoryCategoryNames().size()>=size_t(CatCount);
  }

/// Shift with the arrows or with A and D: -1 and 1, 0 for the other keys (and for the games without the categories)
int InventoryMenu::categoryKeyDir(const KeyEvent& e) const {
  if((e.modifier&Event::M_Shift)==0 || state==State::Closed || state==State::LockPicking || !hasCategories())
    return 0;
  if(e.key==KeyEvent::K_Right || e.key==KeyEvent::K_D)
    return 1;
  if(e.key==KeyEvent::K_Left || e.key==KeyEvent::K_A)
    return -1;
  return 0;
  }

/// The category does not wrap, as CATINV_SETCATEGORY of the mod: the first and the last stay
void InventoryMenu::shiftCategory(int dir) {
  int now = 0;
  for(int i=0; i<int(CatCount); ++i)
    if(CategoryOrder[i]==category())
      now = i;
  const int next = std::clamp(now+dir,0,int(CatCount)-1);
  if(next==now)
    return;
  setCategory(CategoryOrder[next]);
  Gothic::inst().emitGlobalSound("INV_CHANGE");
  }

/// The name of the category is a box on the left of the gold, as in CoM
void InventoryMenu::drawCategoryTitle(Painter& p, int goldX, int y) {
  if(!hasCategories() || category()==CatAll)
    return;
  int index = 0;
  for(int i=0; i<int(CatCount); ++i)
    if(CategoryOrder[i]==category())
      index = i;
  const auto& names = player->world().script().inventoryCategoryNames();
  if(index>=int(names.size()) || names[size_t(index)].empty())
    return;

  const float scale = Gothic::interfaceScale(this);
  auto&       fnt   = Resources::font(scale);
  const auto& title = names[size_t(index)];
  const int   tw    = fnt.textSize(title).w;
  const int   padd  = int(8*scale);
  const int   dw    = std::max(slotSize().w*2, tw+padd*2);
  drawHeader(p,title,goldX-dw-int(4*scale),y);
  }
#endif

void InventoryMenu::setCategory(int category) {
  takeTimer.stop();
  takeCount = 0;

  auto c = Category(std::clamp(category,0,int(CatCount)-1));
  uint32_t mask = 0;
  if(player!=nullptr) {
    auto& script = player->world().script();
    for(int i=0; i<int(CatCount); ++i) {
      if(CategoryOrder[i]!=c)
        continue;
      script.setInventoryScriptCategory(i);
      const int actual = script.inventoryScriptCategory();
      if(actual>=0 && actual<int(CatCount))
        c = CategoryOrder[actual];
      mask = script.inventoryCategoryMask(actual>=0 ? actual : i);
      break;
      }
    }
  if(pagePl!=nullptr) {
    pagePl->category = c;
    pagePl->scriptMask = mask;
    }
  if(pageOth!=nullptr) {
    pageOth->category = c;
    pageOth->scriptMask = mask;
    }
  pageLocal[0] = PageLocal();
  pageLocal[1] = PageLocal();
  adjustScroll();
  update();
  }

void InventoryMenu::syncScriptCategory(bool reset) {
  if(player==nullptr)
    return;
  auto& script = player->world().script();
  if(script.inventoryScriptCategory()<0)
    return;
  if(reset)
    script.setInventoryScriptCategory(0);
  const auto index = std::clamp(script.inventoryScriptCategory(),0,int(CatCount)-1);
  const auto next = CategoryOrder[index];
  if(category()!=next || reset)
    setCategory(next);
  }

void InventoryMenu::updateItemInfo(const Item& item) {
  if(player==nullptr || infoItem.lock()==item.handlePtr())
    return;
  player->world().script().dmaItemInfo(item);
  infoItem = item.handlePtr();
  }

int InventoryMenu::category() const {
  if(pagePl!=nullptr)
    return pagePl->category;
  if(pageOth!=nullptr)
    return pageOth->category;
  return CatAll;
  }
