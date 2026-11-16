#include "nbsalesmenu.h"

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
#include "resources.h"

using namespace Tempest;

NbSalesMenu::NbSalesMenu() {
  setFocusPolicy(NoFocus);
  }

NbSalesMenu::~NbSalesMenu() {
  }

const Item* NbSalesMenu::icon(size_t cls) {
  if(player==nullptr)
    return nullptr;
  if(auto i = icons.find(cls); i!=icons.end())
    return i->second.get();
  auto  itm = std::make_unique<Item>(player->world(),cls,Item::T_Inventory);
  auto* ret = itm.get();
  icons[cls] = std::move(itm);
  return ret;
  }

void NbSalesMenu::open(Npc& pl, Npc& tr, const std::vector<Gothic::NbSaleItem>& list) {
  player  = &pl;
  trader  = &tr;
  all     = list;
  message.clear();
  active  = !all.empty();
  // the categories (Constantino, Alfred): in the order of the list, only when every item has one
  groups.clear();
  bool everyHas = true;
  for(auto& it:all) {
    if(it.group.empty()) {
      everyHas = false;
      break;
      }
    if(std::find(groups.begin(),groups.end(),it.group)==groups.end())
      groups.push_back(it.group);
    }
  if(!everyHas || groups.size()<2)
    groups.clear();
  // the first category with an item that the player has
  size_t g0 = 0;
  for(size_t g=0; g<groups.size(); ++g) {
    bool has = false;
    for(auto& it:all)
      if(it.group==groups[g] && pl.inventory().itemCount(it.cls)>0) {
        has = true;
        break;
        }
    if(has) {
      g0 = g;
      break;
      }
    }
  setGroup(g0);
  if(active)
    Gothic::inst().emitGlobalSound("TRADE_OPEN");
  Gothic::inst().nbSalesOpen = active;
  update();
  }

void NbSalesMenu::close() {
  if(active)
    Gothic::inst().emitGlobalSound("TRADE_CLOSE");
  active = false;
  Gothic::inst().nbSalesOpen = false;
  player = nullptr;
  trader = nullptr;
  items.clear();
  all.clear();
  groups.clear();
  icons.clear();
  update();
  }

void NbSalesMenu::setGroup(size_t g) {
  items.clear();
  if(groups.empty()) {
    group = 0;
    items = all;
    } else {
    group = std::min(g,groups.size()-1);
    for(auto& it:all)
      if(it.group==groups[group])
        items.push_back(it);
    }
  sel    = 0;
  count  = 1;
  scroll = 0;
  // the first item that the player has
  for(size_t i=0; i<items.size(); ++i)
    if(owned(i)>0) {
      sel = i;
      break;
      }
  clampScroll();
  update();
  }

int NbSalesMenu::visibleRows() const {
  const auto l = layout();
  return std::max(1,l.list.h/std::max(1,l.row));
  }

void NbSalesMenu::scrollBy(int rows) {
  scroll += rows;
  scroll  = std::clamp(scroll,0,std::max(0,int(items.size())-visibleRows()));
  update();
  }

size_t NbSalesMenu::owned(size_t i) const {
  if(player==nullptr || i>=items.size())
    return 0;
  return player->inventory().itemCount(items[i].cls);
  }

NbSalesMenu::Layout NbSalesMenu::layout() const {
  // as the window of the plugin (UNION_ABI.DLL, AS_BACK_CRAFT_*.TGA): the list on the left third of the screen, the
  // chosen item on the right third, the middle is free (the hero at the table or the trader)
  const float s = Gothic::interfaceScale(this);
  Layout l;
  const int W = w(), H = h();
  const int pw = int(float(W)*0.34f);
  const int m  = std::max(6,int(10*s));
  // the title, or the tabs of the categories as in the plugin: two in a row over the list
  int top = m+int(44*s);
  if(!groups.empty()) {
    const int tabH = std::max(int(34*s), H/18);
    const int rows = int((groups.size()+1)/2);
    const int tw   = (pw-2*m-m/2)/2;
    for(size_t i=0; i<groups.size(); ++i) {
      const int c = int(i%2), r = int(i/2);
      l.tabs.emplace_back(m+c*(tw+m/2), m/2+r*(tabH+m/3), tw, tabH);
      }
    top = m/2+rows*(tabH+m/3)+m/2;
    }
  l.list  = Rect(m, top, pw-2*m, H-top-m);
  l.row   = std::max(int(48*s), l.list.h/7);
  l.panel = Rect(W-pw, 0, pw, H);
  const int bh = std::max(int(44*s), H/10);
  const int bw = bh;
  const int bottom = H-m-bh-m;
  l.sell  = Rect(l.panel.x+2*m, bottom, l.panel.w-4*m, bh);
  const int cy = bottom-m-bh;
  l.minus = Rect(l.panel.x+2*m, cy, bw, bh);
  l.max   = Rect(l.panel.x+l.panel.w-2*m-bw, cy, bw, bh);
  l.plus  = Rect(l.max.x-m/2-bw, cy, bw, bh);
  l.count = Rect(l.minus.x+bw+m/2, cy, l.plus.x-(l.minus.x+bw+m/2)-m/2, bh);
  const int cs = int(40*s);
  l.close = Rect(l.panel.x-cs-m, m, cs, cs);

  // the middle: the arrows of the list
  const int midX = pw, midW = l.panel.x-pw;
  const int aw = std::max(int(44*s), H/11);
  const int ah = int(float(aw)*1.6f);
  const int ax = midX+std::max(m,midW/14);
  l.up   = Rect(ax, H*35/100-ah/2, aw, ah);
  l.down = Rect(ax, H*68/100-ah/2, aw, ah);
  return l;
  }

void NbSalesMenu::clampScroll() {
  const auto l       = layout();
  const int  visible = std::max(1,l.list.h/std::max(1,l.row));
  const int  maxS    = std::max(0,int(items.size())-visible);
  scroll = std::clamp(scroll,0,maxS);
  if(int(sel)<scroll)
    scroll = int(sel);
  if(int(sel)>=scroll+visible)
    scroll = int(sel)-visible+1;
  }

void NbSalesMenu::select(size_t i) {
  if(i>=items.size())
    return;
  sel   = i;
  count = 1;
  message.clear();
  clampScroll();
  update();
  }

void NbSalesMenu::changeCount(int d) {
  const size_t have = owned(sel);
  if(have==0) {
    count = 1;
    return;
    }
  const int64_t c = int64_t(count)+d;
  count = size_t(std::clamp<int64_t>(c,1,int64_t(have)));
  update();
  }

void NbSalesMenu::sell() {
  if(player==nullptr || sel>=items.size())
    return;
  const size_t have = owned(sel);
  if(have==0)
    return;
  const size_t n     = std::min(count,have);
  const auto&  it    = items[sel];
  player->delItem(it.cls,uint32_t(n));
  if(it.price>0)
    player->addItem(player->world().script().goldId()->index(),size_t(it.price)*n);
  message = "Sprzedano: "+it.name+" x"+std::to_string(n)+"  (+"+std::to_string(int64_t(it.price)*int64_t(n))+")";
  count   = 1;
  update();
  }

void NbSalesMenu::mobileTap(float x, float y) {
  if(!active)
    return;
  const auto  l = layout();
  const Point p{int(x),int(y)};
  Log::i("NB sales window: tap ",int(x),",",int(y)," (window ",w(),"x",h(),", close ",l.close.x,",",l.close.y,")");
  // the X, with a margin for the finger
  const Rect closeHit(l.close.x-l.close.w/2,l.close.y-l.close.h/2,l.close.w*2,l.close.h*2);
  if(closeHit.contains(p)) {
    close();
    return;
    }
  for(size_t i=0; i<l.tabs.size(); ++i)
    if(l.tabs[i].contains(p)) {
      if(i!=group)
        setGroup(i);
      return;
      }
  if(l.up  .contains(p)) { scrollBy(-std::max(1,visibleRows()-1)); return; }
  if(l.down.contains(p)) { scrollBy(+std::max(1,visibleRows()-1)); return; }
  if(l.list.contains(p)) {
    const size_t i = size_t(scroll + (p.y-l.list.y)/std::max(1,l.row));
    select(i);
    return;
    }
  if(l.minus.contains(p)) { changeCount(-1); return; }
  if(l.plus .contains(p)) { changeCount(+1); return; }
  if(l.max  .contains(p)) { changeCount(int(owned(sel))); return; }
  if(l.sell .contains(p)) { sell(); return; }
  }

void NbSalesMenu::mobileScroll(float dy) {
  if(!active)
    return;
  const auto l = layout();
  int rows = int(dy/float(std::max(1,l.row)));
  if(rows==0 && std::abs(dy)>float(l.row)/3.f)
    rows = dy>0 ? 1 : -1;
  scrollBy(-rows);
  }

void NbSalesMenu::mouseDownEvent(MouseEvent& e) {
  if(!active) {
    e.ignore();
    return;
    }
  mobileTap(float(e.x),float(e.y));
  }

void NbSalesMenu::mouseWheelEvent(MouseEvent& e) {
  if(!active) {
    e.ignore();
    return;
    }
  mobileScroll(float(e.delta));
  }

void NbSalesMenu::keyDownEvent(KeyEvent& e) {
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
    case KeyEvent::K_Return: sell(); break;
    case KeyEvent::K_Tab:    if(!groups.empty()) setGroup((group+1)%groups.size()); break;
    default: break;
    }
  }

void NbSalesMenu::keyUpEvent(KeyEvent& e) {
  if(!active)
    e.ignore();
  }

void NbSalesMenu::paintEvent(PaintEvent& e) {
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

  static const char* const TexLeft   = "AS_BACK_CRAFT_LEFT.TGA";
  static const char* const TexRight  = "AS_BACK_CRAFT_RIGHT.TGA";
  static const char* const TexRecipe = "AS_BACK_CRAFT_RECIPE.TGA";
  static const char* const TexDesc   = "AS_BACK_CRAFT_RECIPE_DESC.TGA";
  static const char* const TexCreate = "AS_BACK_CRAFT_CREATE.TGA";
  static const char* const TexPlus   = "AS_BACK_CRAFT_PLUS.TGA";
  static const char* const TexMinus  = "AS_BACK_CRAFT_MINUS.TGA";
  static const char* const TexIngr   = "AS_BACK_CRAFT_INGRIDIENT.TGA";

  // the left panel: the title and the list; under the textures of the plugin a dark ground (they are half
  // transparent, in the plugin the screen under the window is darkened)
  const Rect leftR(0,0,l.list.x*2+l.list.w,h());
  bar(p,leftR.x,leftR.y,leftR.w,leftR.h,Color(0.02f,0.02f,0.02f,0.85f));
  bar(p,l.panel.x,l.panel.y,l.panel.w,l.panel.h,Color(0.02f,0.02f,0.02f,0.85f));
  image(tex(TexLeft),leftR,PanelFill);
  if(l.tabs.empty())
    big.drawText(p, l.list.x+int(8*s), l.list.y-int(12*s), "Sprzeda\xBF");   // cp1250: "Sprzedaż"
  // the tabs: the chosen one light (as in the plugin), the others dark
  for(size_t i=0; i<l.tabs.size(); ++i) {
    const Rect& r  = l.tabs[i];
    const bool  on = (i==group);
    frame(p,r,th,on ? Gold : PanelLine,on ? Color(0.55f,0.42f,0.18f,0.9f) : Color(0.06f,0.06f,0.06f,0.9f));
    textMid(on ? yel : fnt,r,groups[i]);
    }

  const auto* recipe = tex(TexRecipe);
  const auto* desc   = tex(TexDesc);
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
      bar(p,box.x,box.y,box.w,th*2,Gold);
      bar(p,box.x,box.y+box.h-th*2,box.w,th*2,Gold);
      }
    const size_t have = owned(i);
    const GthFont& f = have==0 ? dis : (i==sel ? yel : fnt);
    textMid(f,box,string_frm(items[i].name," (x",int(have),")"));
    // the picture of the item in the parchment (as the plugin), the price small at the bottom of it
    if(models!=nullptr) {
      if(auto* itm = icon(items[i].cls))
        models->drawItem(slot.x+slot.w/8, slot.y+slot.h/10, slot.w*3/4, slot.h*3/4, *itm);
      }
    {
      const GthFont& pf = have==0 ? dis : yel;
      const string_frm pt(int(items[i].price));
      const auto     sz = pf.textSize(pt);
      pf.drawText(p, slot.x+slot.w-sz.w-int(10*s), slot.y+slot.h-int(10*s), pt);
    }
    }
  // the middle: the arrows of the list (dim when there is nothing more there)
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


  // the right panel: the chosen item
  image(tex(TexRight),l.panel,PanelFill);
  {
    string_frm gold("Z\xB3oto: ",int(player->inventory().goldCount()));
    textMid(fnt,Rect(l.panel.x,int(10*s),l.panel.w,int(30*s)),gold);
  }
  if(sel<items.size()) {
    const auto&  it   = items[sel];
    const size_t have = owned(sel);
    const size_t n    = have>0 ? std::min(count,have) : 0;
    const int    card = std::min(l.panel.w/2,int(float(h())*0.24f));
    const Rect   cr(l.panel.x+(l.panel.w-card)/2, int(44*s), card, card);
    image(recipe,cr,RowSel);
    if(models!=nullptr) {
      if(auto* itm = icon(it.cls))
        models->drawItem(cr.x+cr.w/8, cr.y+cr.h/8, cr.w*3/4, cr.h*3/4, *itm);
      } else {
      textMid(big,cr,string_frm(int(it.price)));
      }
    const int lh = int(float(fnt.pixelSize())*1.6f);
    int y = cr.y+cr.h+int(6*s);
    textMid(yel,Rect(l.panel.x,y,l.panel.w,lh),it.name);                          y += lh;
    textMid(have>0 ? fnt : dis,Rect(l.panel.x,y,l.panel.w,lh),string_frm("Maksymalnie: ",int(have))); y += lh;
    // the place of the ingredients in the plugin: the price of one and the sum
    const int ib = std::min(int(float(l.panel.w)*0.36f),int(float(h())*0.16f));
    const Rect i1(l.panel.x+l.panel.w/2-ib-int(6*s), y, ib, ib);
    const Rect i2(l.panel.x+l.panel.w/2+int(6*s),    y, ib, ib);
    image(tex(TexIngr),i1,PanelFill);
    image(tex(TexIngr),i2,PanelFill);
    textMid(fnt,Rect(i1.x,i1.y,i1.w,i1.h/2),"Cena");
    textMid(yel,Rect(i1.x,i1.y+i1.h/2,i1.w,i1.h/2),string_frm(int(it.price)));
    textMid(fnt,Rect(i2.x,i2.y,i2.w,i2.h/2),"Razem");
    textMid(yel,Rect(i2.x,i2.y+i2.h/2,i2.w,i2.h/2),string_frm(int(int64_t(it.price)*int64_t(n))));
    y += ib+int(6*s);
    if(!message.empty())
      textMid(fnt,Rect(l.panel.x,y,l.panel.w,lh),message);

    // -  count  +  max
    const auto* minus = tex(TexMinus);
    const auto* plus  = tex(TexPlus);
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

    image(tex(TexCreate),l.sell,have>0 ? RowSel : PanelFill);
    textMid(have>0 ? big : dis,l.sell,"Sprzedaj");
    }

  // close
  frame(p,l.close,th,PanelLine,PanelFill);
  stroke(p,float(l.close.x+l.close.w/4),float(l.close.y+l.close.h/4),float(l.close.x+l.close.w*3/4),float(l.close.y+l.close.h*3/4),3*s,Cream);
  stroke(p,float(l.close.x+l.close.w*3/4),float(l.close.y+l.close.h/4),float(l.close.x+l.close.w/4),float(l.close.y+l.close.h*3/4),3*s,Cream);
  }
