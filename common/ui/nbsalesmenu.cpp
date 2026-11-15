#include "nbsalesmenu.h"

#include <Tempest/Painter>

#include <algorithm>

#include "world/objects/npc.h"
#include "world/world.h"
#include "game/gamescript.h"
#include "ui/touchdraw.h"
#include "utils/string_frm.h"
#include "resources.h"

using namespace Tempest;

NbSalesMenu::NbSalesMenu() {
  setFocusPolicy(NoFocus);
  }

void NbSalesMenu::open(Npc& pl, Npc& tr, const std::vector<Gothic::NbSaleItem>& list) {
  player  = &pl;
  trader  = &tr;
  items   = list;
  sel     = 0;
  count   = 1;
  scroll  = 0;
  message.clear();
  active  = !items.empty();
  // the first item that the player has
  for(size_t i=0; i<items.size(); ++i)
    if(owned(i)>0) {
      sel = i;
      break;
      }
  if(active)
    Gothic::inst().emitGlobalSound("TRADE_OPEN");
  update();
  }

void NbSalesMenu::close() {
  if(active)
    Gothic::inst().emitGlobalSound("TRADE_CLOSE");
  active = false;
  player = nullptr;
  trader = nullptr;
  items.clear();
  update();
  }

size_t NbSalesMenu::owned(size_t i) const {
  if(player==nullptr || i>=items.size())
    return 0;
  return player->inventory().itemCount(items[i].cls);
  }

NbSalesMenu::Layout NbSalesMenu::layout() const {
  const float s = Gothic::interfaceScale(this);
  Layout l;
  const int W = w(), H = h();
  const int m = std::max(8,int(12*s));
  // the list: the left 42% of the screen, the panel: the right 40%; the middle stays free (the hero, as in the plugin)
  l.list  = Rect(m, m+int(36*s), int(float(W)*0.42f), H-2*m-int(36*s));
  l.row   = std::max(int(44*s), l.list.h/8);
  l.panel = Rect(W-int(float(W)*0.40f)-m, m+int(36*s), int(float(W)*0.40f), H-2*m-int(36*s));
  const int bh = std::max(int(44*s), l.panel.h/9);
  const int bw = bh;
  const int by = l.panel.y+l.panel.h-2*bh-3*m;
  l.minus = Rect(l.panel.x+m,                 by, bw, bh);
  l.plus  = Rect(l.panel.x+l.panel.w-m-2*bw-m/2, by, bw, bh);
  l.max   = Rect(l.panel.x+l.panel.w-m-bw,    by, bw, bh);
  l.count = Rect(l.minus.x+bw+m/2, by, l.plus.x-(l.minus.x+bw+m/2)-m/2, bh);
  l.sell  = Rect(l.panel.x+m, by+bh+m, l.panel.w-2*m, bh+m);
  const int cs = int(40*s);
  l.close = Rect(W-m-cs, m-int(4*s), cs, cs);
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
  if(l.close.contains(p)) {
    close();
    return;
    }
  if(l.list.contains(p)) {
    const size_t i = size_t(scroll + (p.y-l.list.y)/std::max(1,l.row));
    select(i);
    return;
    }
  if(l.minus.contains(p)) { changeCount(-1); return; }
  if(l.plus .contains(p)) { changeCount(+1); return; }
  if(l.max  .contains(p)) { changeCount(int(owned(sel))); return; }
  if(l.sell .contains(p)) { sell(); return; }
  // outside of the list and the panel: closes, as a tap next to a menu
  if(!l.panel.contains(p))
    close();
  }

void NbSalesMenu::mobileScroll(float dy) {
  if(!active)
    return;
  const auto l = layout();
  scroll -= int(dy/float(std::max(1,l.row)));
  const int visible = std::max(1,l.list.h/std::max(1,l.row));
  scroll = std::clamp(scroll,0,std::max(0,int(items.size())-visible));
  update();
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
  const int   th  = std::max(1,int(2*s));

  const auto textMid = [&](const GthFont& f, const Rect& r, std::string_view txt) {
    const auto sz = f.textSize(txt);
    f.drawText(p, r.x+(r.w-sz.w)/2, r.y+(r.h+sz.h)/2-int(4*s), txt);
    };

  bar(p,0,0,w(),h(),Dim);

  // the title and the gold
  {
    const std::string title = "Sprzeda\xBF";   // cp1250: "Sprzedaż"
    yel.drawText(p, l.list.x, l.list.y-int(8*s), title);
    string_frm gold("Z\xB3oto: ",int(player->inventory().goldCount()));
    const auto sz = fnt.textSize(gold);
    fnt.drawText(p, l.panel.x+l.panel.w-sz.w, l.panel.y-int(8*s), gold);
  }

  // the list
  frame(p,l.list,th,PanelLine,PanelFill);
  const int visible = std::max(1,l.list.h/std::max(1,l.row));
  for(int r=0; r<visible; ++r) {
    const size_t i = size_t(scroll+r);
    if(i>=items.size())
      break;
    const Rect row(l.list.x+th, l.list.y+th+r*l.row, l.list.w-2*th, l.row-th);
    if(i==sel)
      bar(p,row.x,row.y,row.w,row.h,RowSel);
    bar(p,row.x,row.y+row.h,row.w,1,Hairline);
    const size_t have = owned(i);
    string_frm   txt(items[i].name," (x",int(have),")");
    const GthFont& f = have==0 ? dis : (i==sel ? yel : fnt);
    const auto sz = f.textSize(txt);
    f.drawText(p, row.x+int(12*s), row.y+(row.h+sz.h)/2-int(4*s), txt);
    string_frm price(int(items[i].price));
    const auto ps = f.textSize(price);
    f.drawText(p, row.x+row.w-ps.w-int(12*s), row.y+(row.h+ps.h)/2-int(4*s), price);
    }
  if(scroll>0)
    chevron(p,float(l.list.x+l.list.w/2),float(l.list.y+int(10*s)),10*s,2*s,false,Gold);

  // the chosen item
  frame(p,l.panel,th,PanelLine,PanelFill);
  if(sel<items.size()) {
    const auto&  it   = items[sel];
    const size_t have = owned(sel);
    const int    lh   = int(float(yel.pixelSize())*1.5f);
    int          y    = l.panel.y+int(16*s)+lh;
    textMid(yel,Rect(l.panel.x,y-lh,l.panel.w,lh),it.name);
    y += lh;
    string_frm price("Cena: ",int(it.price));
    textMid(fnt,Rect(l.panel.x,y-lh,l.panel.w,lh),price);
    y += lh;
    string_frm max("Maksymalnie: ",int(have));
    textMid(have>0 ? fnt : dis,Rect(l.panel.x,y-lh,l.panel.w,lh),max);
    y += lh;
    string_frm total("Razem: ",int(int64_t(it.price)*int64_t(have>0 ? std::min(count,have) : 0)));
    textMid(yel,Rect(l.panel.x,y-lh,l.panel.w,lh),total);
    if(!message.empty()) {
      y += lh;
      textMid(fnt,Rect(l.panel.x,y-lh,l.panel.w,lh),message);
      }

    // -  count  +  max
    frame(p,l.minus,th,PanelLine,RowSel);
    stroke(p,float(l.minus.x+l.minus.w/4),float(l.minus.y+l.minus.h/2),float(l.minus.x+l.minus.w*3/4),float(l.minus.y+l.minus.h/2),3*s,Gold);
    frame(p,l.plus,th,PanelLine,RowSel);
    stroke(p,float(l.plus.x+l.plus.w/4),float(l.plus.y+l.plus.h/2),float(l.plus.x+l.plus.w*3/4),float(l.plus.y+l.plus.h/2),3*s,Gold);
    stroke(p,float(l.plus.x+l.plus.w/2),float(l.plus.y+l.plus.h/4),float(l.plus.x+l.plus.w/2),float(l.plus.y+l.plus.h*3/4),3*s,Gold);
    frame(p,l.max,th,PanelLine,RowSel);
    textMid(yel,l.max,"Max");
    frame(p,l.count,th,PanelLine,PanelFill);
    textMid(fnt,l.count,string_frm(int(have>0 ? std::min(count,have) : 0)));

    frame(p,l.sell,th,have>0 ? Gold : PanelLine,have>0 ? RowSel : PanelFill);
    textMid(have>0 ? yel : dis,l.sell,"Sprzedaj");
    }

  // close
  frame(p,l.close,th,PanelLine,PanelFill);
  stroke(p,float(l.close.x+l.close.w/4),float(l.close.y+l.close.h/4),float(l.close.x+l.close.w*3/4),float(l.close.y+l.close.h*3/4),3*s,Cream);
  stroke(p,float(l.close.x+l.close.w*3/4),float(l.close.y+l.close.h/4),float(l.close.x+l.close.w/4),float(l.close.y+l.close.h*3/4),3*s,Cream);
  }
