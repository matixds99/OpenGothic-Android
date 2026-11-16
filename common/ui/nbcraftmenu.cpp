#include "nbcraftmenu.h"

#include <Tempest/Painter>
#include <Tempest/Log>

#include <algorithm>
#include <cmath>

#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "graphics/inventoryrenderer.h"
#include "world/world.h"
#include "game/gamescript.h"
#include "ui/touchdraw.h"
#include "utils/string_frm.h"
#include "gothic.h"
#include "resources.h"

using namespace Tempest;

NbCraftMenu::NbCraftMenu() {
  setFocusPolicy(NoFocus);
  }

NbCraftMenu::~NbCraftMenu() {
  }

const Item* NbCraftMenu::icon(size_t cls) {
  if(player==nullptr)
    return nullptr;
  if(auto i = icons.find(cls); i!=icons.end())
    return i->second.get();
  auto  itm = std::make_unique<Item>(player->world(),cls,Item::T_Inventory);
  auto* ret = itm.get();
  icons[cls] = std::move(itm);
  return ret;
  }

void NbCraftMenu::open(Npc& pl, int32_t c) {
  player = &pl;
  craft  = c;
  message.clear();
  auto& sc = pl.world().script();
  // the texts: the strings of the scripts as the plugin, the Polish ones of the PC window when they are Russian (cp1250)
  tabs = {
    sc.nbCraftText("RX_CRAFT_TABALCHEALTH", "Regeneracja"),
    sc.nbCraftText("RX_CRAFT_TABALCSTA",    "Wytrzyma\xB3o\x9C\xE6"),
    sc.nbCraftText("RX_CRAFT_TABALCPERM",   "Permanentne"),
    sc.nbCraftText("RX_CRAFT_TABALCSPECIAL","Specjalne"),
    sc.nbCraftText("RX_CRAFT_TABALCALCOHOL","Alkohole"),
    sc.nbCraftText("RX_CRAFT_TABALCPOISONS","Trucizny"),
    };
  skillTxt  = "Umiej\xEAtno\x9C\xE6 alchemii: ";
  maxTxt    = sc.nbCraftText("RX_CRAFT_MAX",   "Maksymalnie: ");
  createTxt = sc.nbCraftText("RX_CRAFT_CREATE","Stw\xF3rz");
  active = true;
  reload(-1);
  // the first tab with a recipe that the hero can make, else the first with a recipe
  size_t t0 = tabs.size();
  for(size_t t=0; t<tabs.size() && t0==tabs.size(); ++t)
    if(tabCount(t,true)>0)
      t0 = t;
  for(size_t t=0; t<tabs.size() && t0==tabs.size(); ++t)
    if(tabCount(t,false)>0)
      t0 = t;
  setTab(t0<tabs.size() ? t0 : 0);
  Gothic::inst().nbSalesOpen = true;   // no HUD of NB under the window (as the sales window)
  Log::i("NB craft window: ",all.size()," recipes, skill ",skill);
  update();
  }

void NbCraftMenu::close() {
  if(!active)
    return;
  active = false;
  Gothic::inst().nbSalesOpen = false;
  if(player!=nullptr)
    player->world().script().nbCraftEnd(*player);
  player = nullptr;
  items.clear();
  all.clear();
  icons.clear();
  update();
  }

void NbCraftMenu::reload(int32_t keepId) {
  if(player==nullptr)
    return;
  auto& sc = player->world().script();
  all   = sc.nbCraftList(*player,craft);
  skill = sc.nbCraftSkill();
  if(keepId<0)
    return;
  // the same tab, the same recipe (the list may change: some recipes can be made once)
  items.clear();
  for(auto& it:all)
    if(size_t(it.tab)==tab)
      items.push_back(it);
  sel = 0;
  for(size_t i=0; i<items.size(); ++i)
    if(items[i].id==keepId)
      sel = i;
  clampScroll();
  }

size_t NbCraftMenu::tabCount(size_t t, bool makeable) const {
  size_t n = 0;
  for(auto& it:all)
    if(size_t(it.tab)==t && (!makeable || canMake(it)>0))
      ++n;
  return n;
  }

void NbCraftMenu::setTab(size_t t) {
  tab = std::min(t,tabs.size()-1);
  items.clear();
  for(auto& it:all)
    if(size_t(it.tab)==tab)
      items.push_back(it);
  sel    = 0;
  count  = 1;
  scroll = 0;
  message.clear();
  for(size_t i=0; i<items.size(); ++i)
    if(canMake(items[i])>0) {
      sel = i;
      break;
      }
  clampScroll();
  update();
  }

size_t NbCraftMenu::canMake(const NbCraft::Item& it) const {
  if(player==nullptr || it.ingredients.empty())
    return 0;
  size_t n = size_t(-1);
  for(auto& [cls,need]:it.ingredients)
    n = std::min(n,player->inventory().itemCount(cls)/size_t(std::max(1,need)));
  return n;
  }

int NbCraftMenu::visibleRows() const {
  const auto l = layout();
  return std::max(1,l.list.h/std::max(1,l.row));
  }

void NbCraftMenu::scrollBy(int rows) {
  scroll += rows;
  scroll  = std::clamp(scroll,0,std::max(0,int(items.size())-visibleRows()));
  update();
  }

void NbCraftMenu::clampScroll() {
  const int visible = visibleRows();
  scroll = std::clamp(scroll,0,std::max(0,int(items.size())-visible));
  if(int(sel)<scroll)
    scroll = int(sel);
  if(int(sel)>=scroll+visible)
    scroll = int(sel)-visible+1;
  }

NbCraftMenu::Layout NbCraftMenu::layout() const {
  // as the sales window (fix 68-69): the left and the right third of the screen, the middle free (the hero at the table)
  const float s = Gothic::interfaceScale(this);
  Layout l;
  const int W = w(), H = h();
  const int pw = int(float(W)*0.34f);
  const int m  = std::max(6,int(10*s));
  // the tabs as in the plugin: two in a row over the list
  const int tabH = std::max(int(34*s), H/18);
  const int rows = int((tabs.size()+1)/2);
  const int tw   = (pw-2*m-m/2)/2;
  for(size_t i=0; i<tabs.size(); ++i) {
    const int c = int(i%2), r = int(i/2);
    l.tabs.emplace_back(m+c*(tw+m/2), m/2+r*(tabH+m/3), tw, tabH);
    }
  const int top = m/2+rows*(tabH+m/3)+m/2;
  l.list  = Rect(m, top, pw-2*m, H-top-m);
  l.row   = std::max(int(48*s), l.list.h/7);
  l.panel = Rect(W-pw, 0, pw, H);
  const int card = std::min(l.panel.w/2,int(float(H)*0.24f));
  l.card  = Rect(l.panel.x+(l.panel.w-card)/2, m, card, card);
  l.infoY = l.card.y+l.card.h+int(4*s);
  const int bh = std::max(int(44*s), H/10);
  const int bw = bh;
  const int bottom = H-m-bh-m;
  l.create = Rect(l.panel.x+2*m, bottom, l.panel.w-4*m, bh);
  const int cy = bottom-m-bh;
  l.minus = Rect(l.panel.x+2*m, cy, bw, bh);
  l.max   = Rect(l.panel.x+l.panel.w-2*m-bw, cy, bw, bh);
  l.plus  = Rect(l.max.x-m/2-bw, cy, bw, bh);
  l.count = Rect(l.minus.x+bw+m/2, cy, l.plus.x-(l.minus.x+bw+m/2)-m/2, bh);
  const int cs = int(40*s);
  l.close = Rect(l.panel.x-cs-m, m, cs, cs);

  // the middle: the skill at the top, the arrows of the list
  const int midX = pw, midW = l.panel.x-pw;
  l.title = Rect(midX+m, m, std::max(1,midW-cs-3*m), cs);
  const int aw = std::max(int(44*s), H/11);
  const int ah = int(float(aw)*1.6f);
  const int ax = midX+std::max(m,midW/14);
  l.up   = Rect(ax, H*35/100-ah/2, aw, ah);
  l.down = Rect(ax, H*68/100-ah/2, aw, ah);
  return l;
  }

void NbCraftMenu::select(size_t i) {
  if(i>=items.size())
    return;
  sel   = i;
  count = 1;
  message.clear();
  clampScroll();
  update();
  }

void NbCraftMenu::changeCount(int d) {
  const size_t can = sel<items.size() ? canMake(items[sel]) : 0;
  if(can==0) {
    count = 1;
    update();
    return;
    }
  const int64_t c = int64_t(count)+d;
  count = size_t(std::clamp<int64_t>(c,1,int64_t(can)));
  update();
  }

void NbCraftMenu::create() {
  if(player==nullptr || sel>=items.size())
    return;
  const size_t can = canMake(items[sel]);
  if(can==0)
    return;
  const auto   rec = items[sel];
  const size_t n   = std::min(count,can);
  message = player->world().script().nbCraftCreate(*player,rec,n);
  // the scripts may have changed what the hero knows (RX_RaiseAlchemy, RX_CraftCallBack)
  const std::string msg = message;
  reload(rec.id);
  message = msg;
  count   = 1;
  update();
  }

void NbCraftMenu::mobileTap(float x, float y) {
  if(!active)
    return;
  const auto  l = layout();
  const Point p{int(x),int(y)};
  // the X, with a margin for the finger (as the sales window after the test 23:39)
  const Rect closeHit(l.close.x-l.close.w/2,l.close.y-l.close.h/2,l.close.w*2,l.close.h*2);
  if(closeHit.contains(p)) {
    close();
    return;
    }
  for(size_t i=0; i<l.tabs.size(); ++i)
    if(l.tabs[i].contains(p)) {
      if(i!=tab)
        setTab(i);
      return;
      }
  if(l.up  .contains(p)) { scrollBy(-std::max(1,visibleRows()-1)); return; }
  if(l.down.contains(p)) { scrollBy(+std::max(1,visibleRows()-1)); return; }
  if(l.list.contains(p)) {
    select(size_t(scroll + (p.y-l.list.y)/std::max(1,l.row)));
    return;
    }
  if(l.minus .contains(p)) { changeCount(-1); return; }
  if(l.plus  .contains(p)) { changeCount(+1); return; }
  if(l.max   .contains(p)) { changeCount(1<<30); return; }
  if(l.create.contains(p)) { create(); return; }
  }

void NbCraftMenu::mobileScroll(float dy) {
  if(!active)
    return;
  const auto l = layout();
  int rows = int(dy/float(std::max(1,l.row)));
  if(rows==0 && std::abs(dy)>float(l.row)/3.f)
    rows = dy>0 ? 1 : -1;
  scrollBy(-rows);
  }

void NbCraftMenu::mouseDownEvent(MouseEvent& e) {
  if(!active) {
    e.ignore();
    return;
    }
  mobileTap(float(e.x),float(e.y));
  }

void NbCraftMenu::mouseWheelEvent(MouseEvent& e) {
  if(!active) {
    e.ignore();
    return;
    }
  mobileScroll(float(e.delta));
  }

void NbCraftMenu::keyDownEvent(KeyEvent& e) {
  if(!active) {
    e.ignore();
    return;
    }
  switch(e.key) {
    case KeyEvent::K_ESCAPE: close(); break;
    case KeyEvent::K_Up:     if(sel>0) select(sel-1); break;
    case KeyEvent::K_Down:   select(sel+1); break;
    case KeyEvent::K_Left:   changeCount(-1); break;
    case KeyEvent::K_Right:  changeCount(+1); break;
    case KeyEvent::K_Return: create(); break;
    case KeyEvent::K_Tab:    setTab((tab+1)%tabs.size()); break;
    default: break;
    }
  }

void NbCraftMenu::keyUpEvent(KeyEvent& e) {
  if(!active)
    e.ignore();
  }

void NbCraftMenu::paintEvent(PaintEvent& e) {
  if(!active || player==nullptr)
    return;
  using namespace touchdraw;
  Painter     p(e);
  const float s   = Gothic::interfaceScale(this);
  const auto  l   = layout();
  auto&       fnt = Resources::font(Resources::FontType::Normal,s);
  auto&       yel = Resources::font(Resources::FontType::Yellow,s);
  auto&       dis = Resources::font(Resources::FontType::Disabled,s);
  auto&       big = Resources::font("FONT_OLD_20_WHITE.TGA",Resources::FontType::Yellow,s);
  const int   th  = std::max(1,int(2*s));

  // the textures of the window of the plugin (in the data of New Balance), or a plain panel without them
  const auto tex = [](const char* name) -> const Texture2d* {
    auto* t = Resources::loadTexture(name);
    return (t!=nullptr && !t->isEmpty() && t->w()>8) ? t : nullptr;
    };
  const auto image = [&](const Texture2d* t, const Rect& r, const Color& fallback) {
    if(t!=nullptr) {
      p.setBrush(Brush(*t));
      p.drawRect(r.x,r.y,r.w,r.h, 0,0,t->w(),t->h());
      } else {
      frame(p,r,th,PanelLine,fallback);
      }
    };
  const auto textMid = [&](const GthFont& f, const Rect& r, std::string_view txt) {
    const auto sz = f.textSize(txt);
    f.drawText(p, r.x+(r.w-sz.w)/2, r.y+(r.h+sz.h)/2-int(4*s), txt);
    };
  const auto drawIcon = [&](size_t cls, const Rect& r) {
    if(models==nullptr)
      return;
    if(auto* itm = icon(cls))
      models->drawItem(r.x+r.w/8, r.y+r.h/10, r.w*3/4, r.h*3/4, *itm);
    };

  const auto* recipe = tex("AS_BACK_CRAFT_RECIPE.TGA");
  const auto* desc   = tex("AS_BACK_CRAFT_RECIPE_DESC.TGA");
  const auto* ingr   = tex("AS_BACK_CRAFT_INGRIDIENT.TGA");

  // the left panel: the tabs and the list, a dark ground under the half transparent textures of the plugin
  const Rect leftR(0,0,l.list.x*2+l.list.w,h());
  bar(p,leftR.x,leftR.y,leftR.w,leftR.h,Color(0.02f,0.02f,0.02f,0.85f));
  bar(p,l.panel.x,l.panel.y,l.panel.w,l.panel.h,Color(0.02f,0.02f,0.02f,0.85f));
  image(tex("AS_BACK_CRAFT_LEFT.TGA"),leftR,PanelFill);
  for(size_t i=0; i<l.tabs.size() && i<tabs.size(); ++i) {
    const Rect& r  = l.tabs[i];
    const bool  on = (i==tab);
    frame(p,r,th,on ? Gold : PanelLine,on ? Color(0.55f,0.42f,0.18f,0.9f) : Color(0.06f,0.06f,0.06f,0.9f));
    // as the plugin: "Regeneracja (1/1)" - how many can be made of the known ones
    const size_t known = tabCount(i,false);
    std::string  txt   = tabs[i];
    if(known>0) {
      std::string withCnt = txt+" ("+std::to_string(tabCount(i,true))+"/"+std::to_string(known)+")";
      if((on ? yel : fnt).textSize(withCnt).w<=r.w-int(8*s))
        txt = std::move(withCnt);
      }
    textMid(on ? yel : (known>0 ? fnt : dis),r,txt);
    }

  const int visible = std::max(1,l.list.h/std::max(1,l.row));
  for(int r=0; r<visible; ++r) {
    const size_t i = size_t(scroll+r);
    if(i>=items.size())
      break;
    const int  y    = l.list.y+r*l.row;
    const int  side = l.row-int(6*s);
    const Rect slot(l.list.x, y, side, side);
    const Rect box (l.list.x+side+int(8*s), y, l.list.w-side-int(8*s), side);
    image(recipe,slot,RowSel);
    image(desc,box,PanelFill);
    if(i==sel) {
      bar(p,slot.x,slot.y,slot.w,slot.h,Color(0.9f,0.45f,0.1f,0.45f));   // the plugin: an orange parchment
      bar(p,box.x,box.y,box.w,th*2,Gold);
      bar(p,box.x,box.y+box.h-th*2,box.w,th*2,Gold);
      }
    const size_t can = canMake(items[i]);
    const GthFont& f = can==0 ? dis : (i==sel ? yel : fnt);
    textMid(f,box,string_frm(items[i].name," (x",int(can),")"));
    drawIcon(items[i].cls,slot);
    }
  if(items.empty())
    textMid(dis,Rect(l.list.x,l.list.y,l.list.w,l.row),"-");

  // the middle: the skill of the alchemy (as the plugin) and the arrows of the list
  textMid(big,l.title,string_frm(skillTxt,int(skill)));
  const auto arrow = [&](const Rect& r, bool down, bool on) {
    const Color c = on ? Cream : Color(0.5f,0.5f,0.5f,0.6f);
    frame(p,r,th,on ? PanelLine : Color(0.3f,0.3f,0.3f,0.6f),Color(0.f,0.f,0.f,0.65f));
    const float cx = float(r.x+r.w/2), t = 3*s;
    const float y0 = float(r.y+r.h/6), y1 = float(r.y+r.h*5/6), hw = float(r.w)*0.28f;
    const float tip = down ? y1 : y0, back = down ? y1-hw : y0+hw;
    stroke(p,cx,y0,cx,y1,t,c);
    stroke(p,cx,tip,cx-hw,back,t,c);
    stroke(p,cx,tip,cx+hw,back,t,c);
    };
  arrow(l.up,  false,scroll>0);
  arrow(l.down,true, scroll+visible<int(items.size()));

  // the right panel: the chosen recipe
  image(tex("AS_BACK_CRAFT_RIGHT.TGA"),l.panel,PanelFill);
  if(sel<items.size()) {
    const auto&  it  = items[sel];
    const size_t can = canMake(it);
    const size_t n   = can>0 ? std::min(count,can) : 0;
    image(recipe,l.card,RowSel);
    if(models!=nullptr)
      drawIcon(it.cls,l.card); else
      textMid(big,l.card,it.name);
    const int lh = int(float(fnt.pixelSize())*1.6f);
    int y = l.infoY;
    textMid(yel,Rect(l.panel.x,y,l.panel.w,lh),it.name);                               y += lh;
    textMid(can>0 ? fnt : dis,Rect(l.panel.x,y,l.panel.w,lh),string_frm(maxTxt,int(can))); y += lh;

    // the ingredients as in the plugin: four in a row, "have/need" in the corner
    const int m     = std::max(6,int(10*s));
    const int cols  = 4;
    const int gap   = int(6*s);
    const int nIngr = int(it.ingredients.size());
    const int rowsI = std::max(1,(nIngr+cols-1)/cols);
    const int space = std::max(1,l.minus.y-m-y-(message.empty() ? 0 : lh));
    int ib = std::min((l.panel.w-4*m-(cols-1)*gap)/cols,int(float(h())*0.13f));
    ib = std::max(int(24*s),std::min(ib,(space-(rowsI-1)*gap)/rowsI));
    for(int i=0; i<nIngr; ++i) {
      const int  inRow = std::min(cols,nIngr-(i/cols)*cols);
      const int  rowW  = inRow*ib+(inRow-1)*gap;
      const Rect r(l.panel.x+(l.panel.w-rowW)/2+(i%cols)*(ib+gap), y+(i/cols)*(ib+gap), ib, ib);
      const auto [cls,need] = it.ingredients[size_t(i)];
      const size_t have = player->inventory().itemCount(cls);
      image(ingr,r,PanelFill);
      drawIcon(cls,r);
      const GthFont& f = have>=size_t(need)*std::max<size_t>(1,n) ? yel : dis;
      const string_frm txt(int(have),"/",int(need));
      const auto sz = f.textSize(txt);
      f.drawText(p, r.x+r.w-sz.w-int(4*s), r.y+r.h-int(6*s), txt);
      }
    y += rowsI*(ib+gap);
    if(!message.empty())
      textMid(fnt,Rect(l.panel.x,y,l.panel.w,lh),message);

    // -  count  +  max
    const auto* minus = tex("AS_BACK_CRAFT_MINUS.TGA");
    const auto* plus  = tex("AS_BACK_CRAFT_PLUS.TGA");
    image(minus,l.minus,RowSel);
    if(minus==nullptr)
      stroke(p,float(l.minus.x+l.minus.w/4),float(l.minus.y+l.minus.h/2),float(l.minus.x+l.minus.w*3/4),float(l.minus.y+l.minus.h/2),3*s,Gold);
    image(plus,l.plus,RowSel);
    if(plus==nullptr) {
      stroke(p,float(l.plus.x+l.plus.w/4),float(l.plus.y+l.plus.h/2),float(l.plus.x+l.plus.w*3/4),float(l.plus.y+l.plus.h/2),3*s,Gold);
      stroke(p,float(l.plus.x+l.plus.w/2),float(l.plus.y+l.plus.h/4),float(l.plus.x+l.plus.w/2),float(l.plus.y+l.plus.h*3/4),3*s,Gold);
      }
    frame(p,l.max,th,PanelLine,Dark);
    textMid(yel,l.max,"Max");
    bar(p,l.count.x,l.count.y,l.count.w,l.count.h,Color(0,0,0,0.6f));
    textMid(fnt,l.count,string_frm(int(n)));

    image(tex("AS_BACK_CRAFT_CREATE.TGA"),l.create,can>0 ? RowSel : PanelFill);
    textMid(can>0 ? big : dis,l.create,createTxt);
    }

  // close
  frame(p,l.close,th,PanelLine,PanelFill);
  stroke(p,float(l.close.x+l.close.w/4),float(l.close.y+l.close.h/4),float(l.close.x+l.close.w*3/4),float(l.close.y+l.close.h*3/4),3*s,Cream);
  stroke(p,float(l.close.x+l.close.w*3/4),float(l.close.y+l.close.h/4),float(l.close.x+l.close.w/4),float(l.close.y+l.close.h*3/4),3*s,Cream);
  }
