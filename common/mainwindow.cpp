#include "mainwindow.h"
#include "game/nbhotbar.h"
#include "ui/cursorshape.h"

#include <Tempest/Except>
#include <Tempest/Painter>

#include <Tempest/Brush>
#include <Tempest/Pen>
#include <Tempest/Layout>
#include <Tempest/Application>
#include <Tempest/Log>
#include <set>

#include <algorithm>

#include "ui/dialogmenu.h"
#include "ui/menuroot.h"
#include "ui/stacklayout.h"
#include "ui/videowidget.h"

#include "utils/mouseutil.h"
#include "utils/string_frm.h"
#include "world/triggers/abstracttrigger.h"
#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "world/objects/interactive.h"
#include "game/originalsave.h"
#include "game/serialize.h"
#include "game/savegameheader.h"
#include "game/globaleffects.h"
#include "game/definitions/musicdefinitions.h"
#include "utils/gthfont.h"
#include "utils/haptics.h"
#include "game/savestore.h"
#include "game/combatfx.h"
#include "game/worldmarkers.h"
#include "game/nbbars.h"
#include "scripting/gameapi.h"
#include "utils/dbgpainter.h"

#include "commandline.h"
#include "gothic.h"

#ifdef __ANDROID__
#include <Tempest/AndroidApi>
#endif

using namespace Tempest;

// PROGRESS.TGA of Gothic II is 512 px wide; mods (New Balance) have HD textures 2x/4x bigger
static float progressTexScale(const Tempest::Texture2d* loadBox) {
  if(loadBox==nullptr || Gothic::inst().version().game==1 || loadBox->w()<=512)
    return 1.f;
  return float(loadBox->w())/512.f;
  }

// NB fix 40: the sprint of New Balance. In the original the Union plugin calls B_Hotkey_Sprint with the key ~ (the script
// switches the overlay Humans_MageSprint.MDS and SPRINT_ACTIVE, it checks the stamina and the potion of the speed).
static zenkit::DaedalusSymbol* nbSprintFn() {
  auto* w = Gothic::inst().world();
  return w==nullptr ? nullptr : w->script().getVm().find_symbol_by_name("B_HOTKEY_SPRINT");
  }

/// NB fix 41: the bar of the stamina of New Balance (shown by the scripts with Bar_*), nullptr when it is hidden.
/// NB fix 45: the numbers are taken from the variables of the scripts (ATR_STAMINA / ATR_STAMINA_MAX*10, as the
/// screen of the statistics and B_UpdateStaminaBar count them), so the bar never shows other numbers than the menu.
static const NbBars::Bar* nbStaminaBar(const Tempest::Texture2d*& tex, float& value) {
  auto* b = NbBars::at(NbBars::Stamina);
  auto* w = Gothic::inst().world();
  if(b==nullptr || !b->show || w==nullptr)
    return nullptr;
  auto& vm  = w->script().getVm();
  auto* cur = vm.find_symbol_by_name("ATR_STAMINA");
  auto* max = vm.find_symbol_by_name("ATR_STAMINA_MAX");
  if(cur!=nullptr && max!=nullptr && !cur->is_member() && !max->is_member() &&
     cur->type()==zenkit::DaedalusDataType::INT && max->type()==zenkit::DaedalusDataType::INT && max->get_int()>0) {
    b->value    = cur->get_int();
    b->maxRange = max->get_int()*10;
    }
  if(b->maxRange<=0)
    return nullptr;
  tex   = Resources::loadTexture(b->tex.empty() ? std::string("BAR_Stamina.tga") : b->tex);
  value = std::clamp(float(b->value)/float(b->maxRange),0.f,1.f);
  return tex!=nullptr ? b : nullptr;
  }

static bool nbToggleSprint() {
  auto* w  = Gothic::inst().world();
  auto* pl = Gothic::inst().player();
  auto* fn = nbSprintFn();
  if(fn==nullptr || pl==nullptr || pl->isDead() || pl->isDown() || Gothic::inst().isPause() || Gothic::inst().isInDialog())
    return false;
  w->script().getVm().call_function<void>(fn);
  return true;
  }

#ifdef OPENGOTHIC_TOUCH_UI
static bool nbSprintActive() {
  auto* w = Gothic::inst().world();
  if(w==nullptr)
    return false;
  auto* sym = w->script().getVm().find_symbol_by_name("SPRINT_ACTIVE");
  return sym!=nullptr && sym->type()==zenkit::DaedalusDataType::INT && sym->get_int()!=0;
  }
#endif

MainWindow::MainWindow(Device& device)
  : Window(CommandLine::inst().isWindowMode() ? Normal : Maximized),device(device),swapchain(device,hwnd()),
    atlas(device),rootMenu(keycodec),inventory(keycodec),
    dialogs(inventory),document(keycodec),
    console(*this),
    modMenu(atlas),
#if defined(__MOBILE_PLATFORM__) && !defined(OPENGOTHIC_TOUCH_UI)
    mobileUi(player),
#endif
    player(dialogs,inventory) {
  Gothic::inst().onSettingsChanged.bind(this,&MainWindow::onSettings);
  onSettings();

  if(Gothic::inst().version().game==2)
    setWindowTitle("Gothic II"); else
    setWindowTitle("Gothic");

  if(!CommandLine::inst().isWindowMode()) {
    setFullscreen(true);
    }

  //renderer.resetSwapchain();
  setupUi();

  barBack    = Resources::loadTexture("BAR_BACK.TGA");
  barHp      = Resources::loadTexture("BAR_HEALTH.TGA");
  barMisc    = Resources::loadTexture("BAR_MISC.TGA");
  barMana    = Resources::loadTexture("BAR_MANA.TGA");

  focusImg   = Resources::loadTexture("FOCUS_HIGHLIGHT.TGA");

  loadBox    = Resources::loadTexture("PROGRESS.TGA");
  loadVal    = Resources::loadTexture("PROGRESS_BAR.TGA");

  Gothic::inst().onStartGame   .bind(this,&MainWindow::startGame);
  Gothic::inst().onLoadGame    .bind(this,&MainWindow::loadGame);
  Gothic::inst().onSaveGame    .bind(this,&MainWindow::saveGame);

  Gothic::inst().onStartLoading.bind(this,&MainWindow::onStartLoading);
  Gothic::inst().onWorldLoaded .bind(this,&MainWindow::onWorldLoaded);
  Gothic::inst().onSessionExit .bind(this,&MainWindow::onSessionExit);

  Gothic::inst().onVideo       .bind(this,&MainWindow::onVideo);

  Gothic::inst().onBenchmarkFinished.bind(this,&MainWindow::onBenchmarkFinished);

  if(const std::string defSave = Gothic::inst().defaultSave(); !defSave.empty()){
    Gothic::inst().load(defSave);
    rootMenu.popMenu();
    }
  else if(!CommandLine::inst().doStartMenu()) {
    startGame(Gothic::inst().defaultWorld());
    rootMenu.popMenu();
    }
  else {
    rootMenu.processMusicTheme();
    }

  funcKey[2] = Shortcut(*this,Event::M_NoModifier,Event::K_F2);
  funcKey[2].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F2>);

  funcKey[3] = Shortcut(*this,Event::M_NoModifier,Event::K_F3);
  funcKey[3].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F3>);

  funcKey[4] = Shortcut(*this,Event::M_NoModifier,Event::K_F4);
  funcKey[4].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F4>);

  funcKey[5] = Shortcut(*this,Event::M_NoModifier,Event::K_F5);
  funcKey[5].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F5>);

  funcKey[6] = Shortcut(*this,Event::M_NoModifier,Event::K_F6);
  funcKey[6].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F6>);

  funcKey[7] = Shortcut(*this,Event::M_NoModifier,Event::K_F7);
  funcKey[7].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F7>);

  funcKey[9] = Shortcut(*this,Event::M_NoModifier,Event::K_F9);
  funcKey[9].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F9>);

  funcKey[10] = Shortcut(*this,Event::M_NoModifier,Event::K_F10);
  funcKey[10].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F10>);

  displayPos = Shortcut(*this,Event::M_Alt,Event::K_P);
  displayPos.onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_P>);
#ifdef OPENGOTHIC_TOUCH_UI
  setupMobileControls();
#endif
#ifdef __ANDROID__
  androidWindowGen = AndroidApi::nativeWindowGeneration();
  AndroidApi::setWindowLostHandler([](void* self) {
    reinterpret_cast<MainWindow*>(self)->androidWindowLost();
    }, this);
#endif
  Scripting::game::setup();
  }

MainWindow::~MainWindow() {
#ifdef __ANDROID__
  AndroidApi::setWindowLostHandler(nullptr,nullptr);
#endif
#ifdef OPENGOTHIC_TOUCH_UI
  androidControls.unbind();
#endif
  Scripting::ModHost::inst().stopAll();
  GameMusic::inst().stopMusic();
  Gothic::inst().cancelLoading();
  device.waitIdle();
  takeWidget(&dialogs);
  takeWidget(&inventory);
  takeWidget(&chapter);
  takeWidget(&document);
  takeWidget(&video);
  takeWidget(&nbSales);
  takeWidget(&nbCraft);
  takeWidget(&rootMenu);
  takeWidget(&modMenu);
  // every child added by setupUi() is a member: removeAllWidgets() calls `delete` on whatever is left in the list
  takeWidget(&errorScreen);
#if defined(__MOBILE_PLATFORM__) && !defined(OPENGOTHIC_TOUCH_UI)
  takeWidget(&mobileUi);
#endif
  removeAllWidgets();
#ifdef OPENGOTHIC_TOUCH_UI
  // the sprites of the touch controls are copied to the inventory: give them all back while the atlas is alive
  inventory.setTouchSprites(Tempest::Sprite(),Tempest::Sprite());
  androidControls.releaseIcons();
#endif
  // unload
  Gothic::inst().setGame(std::unique_ptr<GameSession>());
  }

float MainWindow::uiScale() const {
  return SystemApi::uiScale(hwnd());
  }

void MainWindow::setupUi() {
  setLayout(new StackLayout());
  addWidget(&document);
  addWidget(&dialogs);
  addWidget(&inventory);
  addWidget(&chapter);
  addWidget(&video);
  addWidget(&nbSales);
  addWidget(&nbCraft);
#if defined(__MOBILE_PLATFORM__) && !defined(OPENGOTHIC_TOUCH_UI)
  addWidget(&mobileUi);
#endif
  addWidget(&rootMenu);
  addWidget(&modMenu);
  addWidget(&errorScreen);

  rootMenu.setMainMenu();

  Gothic::inst().onDialogPipe  .bind(&dialogs,&DialogMenu::openPipe);
  Gothic::inst().onDialogGuest .bind(&dialogs,&DialogMenu::addGuest);
  Gothic::inst().onDialogCameraReset.bind(this,&MainWindow::requestDialogCamera);
  Gothic::inst().onStopAllSounds.bind(&dialogs,&DialogMenu::stopSounds);
  Gothic::inst().onOpenTrade   .bind(&inventory,&InventoryMenu::trade);
  Gothic::inst().onOpenPlayerSales.bind(&nbSales,&NbSalesMenu::open);
  Gothic::inst().onOpenCraft      .bind(&nbCraft,&NbCraftMenu::open);   // NB fix 70
  Gothic::inst().isNpcInDialogFn = std::bind(&DialogMenu::isNpcInDialog, &dialogs, std::placeholders::_1);

  Gothic::inst().onPrintScreen .bind(&dialogs,&DialogMenu::printScreen);
  Gothic::inst().onPrint       .bind(&dialogs,&DialogMenu::print);

  Gothic::inst().onIntroChapter.bind(&chapter, &ChapterScreen::show);
  Gothic::inst().onShowDocument.bind(this,&MainWindow::showDocument);
  }

/// A letter or a recipe that the player reads from the inventory. The inventory of CoM stays on the screen over the page, so
/// with the mods of Ikarus it is closed, as the original engine does it for the documents.
void MainWindow::showDocument(const DocumentMenu::Show& doc) {
  auto* world = Gothic::inst().world();
  if(world!=nullptr && world->script().hasDma() && inventory.isOpen()==InventoryMenu::State::Equip)
    inventory.close();
  document.show(doc);
  }

void MainWindow::paintEvent(PaintEvent& event) {
  // This frame's fence was waited before dispatchPaintEvent. Each in-flight
  // frame owns its preview meshes so resetting cannot free GPU-used buffers.
  dmaModels[cmdId].reset();
  nbSales.setModels(&dmaModels[cmdId]);   // NB fix 69: the pictures of the items of the sales window
  nbCraft.setModels(&dmaModels[cmdId]);   // NB fix 70: and of the crafting window
  Painter p(event);
  auto world = Gothic::inst().world();
  auto st    = Gothic::inst().checkLoading();

  if(!Gothic::inst().isInGame() && st==Gothic::LoadState::Idle && background.isEmpty()) {
    background = Resources::loadTextureUncached("STARTSCREEN.TGA");
    }

  if(world==nullptr && !background.isEmpty()) {
    p.setBrush(Color(0.0));
    p.drawRect(0,0,w(),h());

    if(st==Gothic::LoadState::Idle) {
      p.setBrush(Brush(background,Painter::NoBlend));
      p.drawRect(0,0,w(),h(),
                 0,0,background.w(),background.h());
      }
    }

  if(world!=nullptr) {
    world->globalFx()->scrBlend(p,Rect(0,0,w(),h()));
    }

  // NB fix 108: the screen of the death of New Balance (UNION_ABI.DLL 0x10222E00: bShowDeadScreen, YOU_DIE.TGA over the
  // whole screen while the hero is dead), it comes up in 2 s
  {
    static uint64_t deadSince = 0;
    auto* pl = Gothic::inst().player();
    if(world!=nullptr && pl!=nullptr && pl->isDead() && Gothic::settingsGetI("AST","bShowDeadScreen")!=0) {
      if(deadSince==0)
        deadSince = Application::tickCount();
      if(auto* tex = Resources::loadTexture("YOU_DIE.TGA")) {
        const float a = std::min(1.f,float(Application::tickCount()-deadSince)/2000.f);
        p.setBrush(Brush(*tex,Color(1.f,1.f,1.f,a)));
        p.drawRect(0,0,w(),h(),0,0,tex->w(),tex->h());
        }
      } else {
      deadSince = 0;
      }
  }

  if(st!=Gothic::LoadState::Idle && st!=Gothic::LoadState::Finalize) {
    if(st==Gothic::LoadState::Saving) {
      drawSaving(p);
      } else {
      auto back = Gothic::inst().loadingBanner();
      if(back!=nullptr && !back->isEmpty()) {
        p.setBrush(Brush(*back,Painter::NoBlend));
        p.drawRect(0,0,this->w(),this->h(),
                   0,0,back->w(),back->h());
        } else {
        // no picture: black, not a white rectangle of an empty texture
        p.setBrush(Brush(Color(0,0,0,1),Painter::NoBlend));
        p.drawRect(0,0,this->w(),this->h());
        }
      if(loadBox!=nullptr && !loadBox->isEmpty()) {
        if(Gothic::inst().version().game==1) {
          int lw = int(w()*0.5);
          int lh = int(h()*0.05);
          drawLoading(p,(w()-lw)/2, int(h()*0.75), lw, lh);
          } else {
          // the bar is made for PROGRESS.TGA of 512 px; a mod with a bigger (HD) texture, as New Balance
          // with 1024x128, is drawn at the same size, not 2x bigger (see progressTexScale)
          const float k  = progressTexScale(loadBox);
          const int   lw = int(float(loadBox->w())/k);
          const int   lh = int(float(loadBox->h())/k);
          drawLoading(p,int(w()*0.92)-lw, int(h()*0.12), lw, lh);
          }
        }
      }
    } else {
    if(world!=nullptr && world->view()){
      auto& camera = *Gothic::inst().camera();

      auto vp = camera.viewProj();
      p.setBrush(Color(1.0));

      drawMsg(p);
      drawDmaOverlay(p);

      auto focus = world->validateFocus(player.focus());
      if(auto* pl = Gothic::inst().player())
        world->script().dmaFocus(*pl,focus);
      world->script().nbSetFocusMob(focus.interactive);   // NB fix 79: the chest of Mob_SetBrakeChest
      world->script().nbSetFocusNpc(focus.npc);           // NB fix 149: GetFocusNpc
      // CoM hides the status of the player in a camera of a cutscene (OCGAME__UPDATEPLAYERSTATUS_REPLACECHECKINGGLOBALCUTSCENE)
      const bool inCamera = world->script().dmaCutsceneStatusHidden() || nbSales.isActive() || nbCraft.isActive();   // NB fix 68: hidden under the window
      if(!inCamera) {
        paintFocus(p,focus,vp);
        CombatFx::inst().paint(p,vp,w(),h(),Gothic::interfaceScale(this));
        WorldMarkers::inst().paint(p,vp,w(),h(),Gothic::interfaceScale(this));
        }

#ifdef OPENGOTHIC_TOUCH_UI
      constexpr bool screenBars = false;
#else
      const bool     screenBars = !Gothic::inst().isDesktop();
#endif
      if(auto pl = Gothic::inst().player(); pl!=nullptr && !inCamera && !pl->isDead()){
#ifdef OPENGOTHIC_TOUCH_UI
        drawTouchBars(p,*pl);
#endif
        if (screenBars) {
          auto& opt = Gothic::options();
          float hp  = float(pl->attribute(ATR_HITPOINTS))/float(pl->attribute(ATR_HITPOINTSMAX));
          float mp  = float(pl->attribute(ATR_MANA))     /float(pl->attribute(ATR_MANAMAX));

          bool showHealthBar = opt.showHealthBar;
          bool showManaBar   = (opt.showManaBar==2) || (opt.showManaBar==1 && (pl->weaponState()==WeaponState::Mage || inventory.isActive()));
          bool showSwimBar   = (opt.showSwimBar==2) || (opt.showSwimBar==1 && pl->isDive());

          if(showHealthBar)
            drawBar(p,barHp, 10, h()-10, hp, AlignLeft | AlignBottom);
          if(showManaBar)
            drawBar(p,barMana, w()-10, h()-10, mp, AlignRight | AlignBottom);
          bool swimDrawn = false;
          if(showSwimBar) {
            auto     v  = float(pl->diveTimeLimit())/1000.f;
            if(v>0) {
              auto t = float(pl->diveTime())/1000.f;
              drawBar(p,barMisc,w()/2,h()-10, (v-t)/(v), AlignHCenter | AlignBottom);
              swimDrawn = true;
              }
            }
          // NB fix 41: the stamina in the middle, above the bar of the air when it is there
          const Tempest::Texture2d* stTex = nullptr;
          float                     stVal = 0;
          if(nbStaminaBar(stTex,stVal)!=nullptr) {
            const float scale = Gothic::interfaceScale(this);
            const int   barW  = int(200.f*scale*float(std::min(w(),800))/800.f);
            const int   barH  = barBack==nullptr ? 0 : int(float(barBack->h())*float(barW)/float(std::max(barBack->w(),1)));
            drawBar(p,stTex,w()/2,h()-10-(swimDrawn ? barH+4 : 0),stVal,AlignHCenter | AlignBottom);
            }
          }
        }
      }
    }

  if(auto c = Gothic::inst().camera()) {
    DbgPainter dbg(p,c->viewProj(),w(),h());
    c->debugDraw(dbg);
    if(world!=nullptr) {
      world->marchPoints(dbg);
      world->marchInteractives(dbg);
      world->view()->dbgLights(dbg);
      world->marchCsCameras(dbg);
      }
    }

  renderer.dbgDraw(p);

  const float scale = Gothic::interfaceScale(this);
  if(Gothic::inst().doFrate() && !Gothic::inst().isDesktop()) {
    char fpsT[64]={};
    std::snprintf(fpsT,sizeof(fpsT),"fps = %.2f",fps.get());

    auto& fnt = Resources::font(scale);
    fnt.drawText(p,5,fnt.pixelSize()+5,fpsT);
    }

  if(!Gothic::inst().isDesktop() && world!=nullptr) {
    if(Gothic::inst().doClock()) {
      auto hour = world->time().hour();
      auto min  = world->time().minute();
      auto& fnt = Resources::font(scale);
      string_frm clockT(int(hour),":",int(min));
      fnt.drawText(p,w()-fnt.textSize(clockT).w-5,fnt.pixelSize()+5,clockT);
      }

    auto c = Gothic::inst().camera();
    if(Gothic::inst().doVobBox() && c!=nullptr) {
      DbgPainter dbg(p,c->viewProj(),w(),h());
      world->drawVobBoxNpcNear(dbg);
      }

    if(Gothic::inst().doVobRays() && c!=nullptr) {
      DbgPainter dbg(p,c->viewProj(),w(),h());
      player.drawVobRay(dbg);
      }
    }

  if(auto wx = Gothic::inst().worldView()) {
    wx->dbgClusters(p, Vec2(float(w()), float(h())));
    }
  }

#ifdef __ANDROID__
void MainWindow::androidWindowLost() {
  // NB fix 74: Android takes the surface when the game goes to the background - the swapchain leaves it now
  // (before: the old swapchain stayed and after the return the frames went to the dead surface - a black screen)
  for(auto& i:fence)
    i.wait();
  device.waitIdle();
  swapchain.reset();   // no native window: the swapchain and its surface are destroyed, nothing new is made
  androidWindowGen = AndroidApi::nativeWindowGeneration();
  }

bool MainWindow::androidWindowCheck() {
  // NB fix 74: after the background the window has the same size, so no resize event comes - the swapchain is made
  // here on the new surface. false: nothing to draw on
  const auto gen = AndroidApi::nativeWindowGeneration();
  if(gen!=androidWindowGen || swapchain.imageCount()==0) {
    for(auto& i:fence)
      i.wait();
    device.waitIdle();
    swapchain.reset();
    androidWindowGen = gen;
    if(swapchain.imageCount()==0)
      return false;
    Log::i("android: the swapchain is made anew (",swapchain.w(),"x",swapchain.h(),")");
    if(auto camera = Gothic::inst().camera())
      camera->setViewport(swapchain.w(),swapchain.h());
    }
  return true;
  }
#endif

void MainWindow::resizeEvent(SizeEvent&) {
  for(auto& i:fence)
    i.wait();
  swapchain.reset();
  if(auto camera = Gothic::inst().camera())
    camera->setViewport(swapchain.w(),swapchain.h());

  if(SystemApi::isFullscreen(hwnd())) {
    setCursorPosition(Point(w()/2,h()/2));
    }

  dMouse = Point();
  }

void MainWindow::mouseDownEvent(MouseEvent &event) {
#if defined(OPENGOTHIC_TOUCH_UI) && !defined(__ANDROID__)
  // The mouse emulates a touch screen: left button is a finger, right button is "back"
  if(event.button==Event::ButtonLeft)
    androidControls.pointerDown(0,float(event.x),float(event.y)); else
  if(event.button==Event::ButtonRight)
    androidControls.physical(Mobile::Key::Escape,true);
  return;
#endif
  if(event.button<sizeof(mouseP))
    mouseP[event.button]=true;
  if(auto* world = Gothic::inst().world(); world!=nullptr && !rootMenu.isActive() && !video.isActive() && !dialogs.wantsText()) {
    const auto code = KeyCodec::scriptKeyCode(event.button);
    if(code>0 && size_t(code)<dmaKeysDown.size() && !dmaKeysDown[size_t(code)]) {
      dmaKeysDown[size_t(code)] = true;
      world->script().dmaKeyState(code,true);
      world->script().dmaKey(code);
      }
    }
  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);
  player.onKeyPressed(act,KeyEvent::K_NoKey,mapping);
  }

void MainWindow::mouseUpEvent(MouseEvent &event) {
#if defined(OPENGOTHIC_TOUCH_UI) && !defined(__ANDROID__)
  if(event.button==Event::ButtonLeft)
    androidControls.pointerUp(0,float(event.x),float(event.y)); else
  if(event.button==Event::ButtonRight)
    androidControls.physical(Mobile::Key::Escape,false);
  return;
#endif
  const auto code = KeyCodec::scriptKeyCode(event.button);
  if(code>0 && size_t(code)<dmaKeysDown.size()) {
    dmaKeysDown[size_t(code)] = false;
    if(auto* world = Gothic::inst().world())
      world->script().dmaKeyState(code,false);
    }
  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);
  player.onKeyReleased(act,mapping);
  if(event.button<sizeof(mouseP))
    mouseP[event.button]=false;
  }

void MainWindow::mouseDragEvent(MouseEvent &event) {
#if defined(OPENGOTHIC_TOUCH_UI) && !defined(__ANDROID__)
  if(event.button==Event::ButtonLeft)
    androidControls.pointerMove(0,float(event.x),float(event.y));
  return;
#endif
  const bool fs = SystemApi::isFullscreen(hwnd());
  if(!mouseP[Event::ButtonLeft] && !fs)
    return;
  if(player.focus().npc && !fs)
    return;
  processMouse(event,true);
  }

void MainWindow::mouseMoveEvent(MouseEvent &event) {
#if defined(OPENGOTHIC_TOUCH_UI) && !defined(__ANDROID__)
  (void)event;
  return;
#endif
  processMouse(event,SystemApi::isFullscreen(hwnd()));
  }

void MainWindow::processMouse(MouseEvent& event, bool enable) {
  auto center = Point(w()/2,h()/2);
  if(enable && event.pos()!=center && hasFocus()) {
    dMouse += (event.pos()-center);
    setCursorPosition(center);
    }
  }

void MainWindow::tickMouse(uint64_t dt) {
  auto camera = Gothic::inst().camera();
  if(dialogs.hasContent() || Gothic::inst().isPause() || camera==nullptr || camera->isCutscene()) {
    dMouse = Point();
    return;
    }

  const bool enableMouse = Gothic::inst().settingsGetI("GAME","enableMouse");
  if(enableMouse==0) {
    dMouse = Point();
    return;
    }

  if(dMouse==Point())
    return;

  const bool  camLookaroundInverse = Gothic::inst().settingsGetI("GAME","camLookaroundInverse");
  const float mouseSensitivity     = Gothic::inst().settingsGetF("GAME","mouseSensitivity")/MouseUtil::mouseSysSpeed();
  PointF dpScaled = PointF(float(dMouse.x)*mouseSensitivity,float(dMouse.y)*mouseSensitivity);
  dpScaled.x/=float(w());
  dpScaled.y/=float(h());
  if(camLookaroundInverse)
    dpScaled.y *= -1.f;

  static float mul = 270.f;
  dpScaled *= mul;

  static float psMax = 720.f;
  const float  dtF   = float(dt)/1000.f;
  dpScaled.x = std::clamp(dpScaled.x, -(psMax*dtF), psMax*dtF);
  dpScaled.y = std::clamp(dpScaled.y, -(psMax*dtF), psMax*dtF);

  // Log::d("mouse dMouse   = ", dMouse.x,   ", ", dMouse.y);
  // Log::d("mouse dpScaled = ", dpScaled.x, ", ", dpScaled.y);

  camera->onRotateMouse(PointF(dpScaled.y,-dpScaled.x));
  if(!inventory.isActive()) {
    player.onRotateMouse(-dpScaled.x, -dpScaled.y);
    }

  dMouse = Point();
  }

void MainWindow::onSettings() {
  auto zMaxFps = Gothic::options().fpsLimit;
  if(zMaxFps<=0)
    zMaxFps = Gothic::inst().settingsGetI("ENGINE", "zMaxFps");
#ifdef OPENGOTHIC_TOUCH_UI
  zMaxFps = std::clamp(AndroidInput::preferences().fps,30,60);
#endif
  if(zMaxFps>0)
    maxFpsInv = 1000u/uint64_t(zMaxFps); else
    maxFpsInv = 0;
  }

void MainWindow::mouseWheelEvent(MouseEvent &event) {
  if(auto camera = Gothic::inst().camera())
    camera->changeZoom(event.delta);
  }

void MainWindow::keyDownEvent(KeyEvent &event) {
  if(errorScreen.isActive()) {
    event.accept();
    errorScreen.keyDownEvent(event);
    uiKeyUp=&errorScreen;
    return;
    }
  if(modMenu.isActive()) {
    event.accept();
    modMenu.keyDownEvent(event);
    uiKeyUp=&modMenu;
    return;
    }
  Scripting::game::keyEvent(event,true);

  // the mods of Ikarus and LeGo hook the keys of the game (oCGame::HandleEvent), once for a press
  if(auto* w = Gothic::inst().world(); w!=nullptr && !rootMenu.isActive() && !video.isActive() && !dialogs.wantsText()) {
    const int32_t code = KeyCodec::scriptKeyCode(event.key);
    if(code>0 && code<int32_t(dmaKeysDown.size()) && !dmaKeysDown[size_t(code)]) {
      dmaKeysDown[size_t(code)] = true;
      w->script().dmaKeyState(code,true);
      w->script().dmaKey(code);
      }
    }

  if(video.isActive()){
    event.accept();
    video.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&video;
      return;
      }
    }

  if(rootMenu.isActive()) {
    event.accept();
    rootMenu.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&rootMenu;
      return;
      }
    }

  if(chapter.isActive()){
    event.accept();
    chapter.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&chapter;
      return;
      }
    }

  if(nbSales.isActive()) {   // NB fix 68
    event.accept();
    nbSales.keyDownEvent(event);
    uiKeyUp = &nbSales;   // its key up must not reach the game (Escape would open the menu)
    return;
    }
  if(nbCraft.isActive()) {   // NB fix 70
    event.accept();
    nbCraft.keyDownEvent(event);
    uiKeyUp = &nbCraft;
    return;
    }

  if(document.isActive()){
    event.accept();
    document.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&document;
      return;
      }
    }

  if(dialogs.isActive()){
    event.accept();
    dialogs.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&dialogs;
      return;
      }
    }

  if(inventory.isActive()){
    event.accept();
    inventory.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&inventory;
      return;
      }
    }
  uiKeyUp=nullptr;

  // NB fix 143: G or R skip a scene of New Balance, as the plugin does
  if(event.key==Event::K_G || event.key==Event::K_R) {
    if(auto* w = Gothic::inst().world(); w!=nullptr && !dialogs.isActive() && w->script().nbCutsceneActive()) {
      w->script().nbSkipCutscene();
      event.accept();
      return;
      }
    }

  // NB fix 40: ~ switches the sprint of New Balance, as in the original
  if((event.code=='`' || event.code=='~') && nbToggleSprint()) {
    event.accept();
    return;
    }

  // NB fix 110: the keys 3..0 of a slot without a rune, with an item put there by the player: the item is used (a
  // weapon is equipped, a potion drunk, a plant eaten)
  if(auto* pl = Gothic::inst().player(); pl!=nullptr && !pl->isDead() && !dialogs.isActive() &&
     ((Event::K_3<=event.key && event.key<=Event::K_9) || event.key==Event::K_0)) {
    const int index = event.key==Event::K_0 ? 9 : int(event.key-Event::K_1);
    if(pl->inventory().currentSpell(uint8_t(index-2))==nullptr) {
      if(auto* it = NbHotbar::item(*pl,index)) {
        pl->useItem(it->clsId());
        event.accept();
        return;
        }
      }
    }

  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);
  player.onKeyPressed(act,event.key,mapping);

  if(event.key==Event::K_F11) {
    auto tex = renderer.screenshoot(cmdId, Size(int(swapchain.w()),int(swapchain.h())));
    auto pm  = device.readPixels(textureCast<const Texture2d&>(tex));
    pm.save("dbg.png");
    }
  event.accept();
  }

void MainWindow::keyRepeatEvent(KeyEvent& event) {
  if(modMenu.isActive()) {
    event.accept();
    modMenu.keyRepeatEvent(event);
    return;
    }
  if(uiKeyUp==&video){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&rootMenu){
    rootMenu.keyRepeatEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&chapter){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&document){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&dialogs){
    dialogs.keyRepeatEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&inventory){
    inventory.keyRepeatEvent(event);
    if(event.isAccepted())
      return;
    }
  }

void MainWindow::keyUpEvent(KeyEvent &event) {
  if(uiKeyUp==&errorScreen) {
    event.accept();
    errorScreen.keyUpEvent(event);
    uiKeyUp = nullptr;
    return;
    }
  Scripting::game::keyEvent(event,false);
  {
  const int32_t code = KeyCodec::scriptKeyCode(event.key);
  if(code>0 && code<int32_t(dmaKeysDown.size())) {
    dmaKeysDown[size_t(code)] = false;
    if(auto* w = Gothic::inst().world())
      w->script().dmaKeyState(code,false);
    }
  }
  if(uiKeyUp==&modMenu) {
    uiKeyUp = nullptr;
    modMenu.keyUpEvent(event);
    return;
    }
  if(uiKeyUp==&nbSales || uiKeyUp==&nbCraft) {   // NB fix 68, 70
    uiKeyUp = nullptr;
    event.accept();
    return;
    }
  if(uiKeyUp==&video){
    video.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&rootMenu){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&chapter){
    chapter.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&document){
    document.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&dialogs){
    dialogs.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&inventory){
    inventory.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }


  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);

  if(sceneLocksUi() && (act==KeyCodec::Escape || act==KeyCodec::Log || act==KeyCodec::Status || act==KeyCodec::Inventory)) {
    clearInput();
    return;
    }

  std::string_view menuEv;
  if(act==KeyCodec::Escape)
    menuEv = Gothic::inst().menuMain();
  else if(act==KeyCodec::Log)
    menuEv = "MENU_LOG";
  else if(act==KeyCodec::Status)
    menuEv = "MENU_STATUS";

  if(!menuEv.empty()) {
    rootMenu.setMenu(menuEv,act);
    rootMenu.showVersion(act==KeyCodec::Escape);
    if(auto pl = Gothic::inst().player())
      rootMenu.setPlayer(*pl);
    clearInput();
    }
  else if(act==KeyCodec::Inventory && !dialogs.isActive()) {
    if(inventory.isActive()) {
      inventory.close();
      } else {
      auto pl = Gothic::inst().player();
      if(pl!=nullptr)
        inventory.open(*pl);
      }
    clearInput();
    }
  player.onKeyReleased(act, mapping);
  }

void MainWindow::focusEvent(FocusEvent &event) {
#ifdef OPENGOTHIC_TOUCH_UI
  if(!event.in) {
    androidControls.cancel();
    clearInput();
    dMouse = Point();
    }
#endif
  if(!event.in) {
    clearInput();
    return;
    }
  dMouse = Point();

  if(SystemApi::isFullscreen(hwnd()))
    setCursorPosition(Point(w()/2,h()/2));
  }

/// NB fix 81: the color of the name in the focus. People and creatures: red - hostile, orange - angry, white - neutral,
/// green - friendly, turquoise - a companion or a summoned creature; the dead: white with loot, grey without. Chests and
/// doors: yellow - locked, green - something inside, grey - empty
Tempest::Color MainWindow::focusColor(const Focus& focus, const Npc& pl) {
  static const Tempest::Color white (1.f,  1.f,  1.f,  1.f);
  static const Tempest::Color grey  (0.62f,0.62f,0.62f,1.f);
  static const Tempest::Color red   (1.f,  0.32f,0.26f,1.f);
  static const Tempest::Color orange(1.f,  0.62f,0.22f,1.f);
  static const Tempest::Color green (0.45f,1.f,  0.42f,1.f);
  static const Tempest::Color cyan  (0.3f, 0.92f,0.9f, 1.f);
  static const Tempest::Color yellow(1.f,  0.86f,0.25f,1.f);
  auto* world = Gothic::inst().world();
  if(world==nullptr)
    return white;

  if(auto* npc = focus.npc) {
    if(npc->isDead())
      return npc->inventory().iterator(Inventory::T_Ransack).isValid() ? white : grey;
    constexpr size_t AIV_PARTYMEMBER = 15;
    if(npc->handle().aivar[AIV_PARTYMEMBER]!=0 || world->script().nbIsSummon(*npc))
      return cyan;
    if(world->script().nbIsTrainingDummy(*npc))   // NB fix 127: not "friendly" - a target for the training
      return white;
    switch(world->script().personAttitude(*npc,pl)) {
      case ATT_HOSTILE:  return red;
      case ATT_ANGRY:    return orange;
      case ATT_FRIENDLY: return green;
      default:           return white;
      }
    }

  if(auto* mob = focus.interactive) {
    if(mob->isLocked())
      return yellow;
    if(mob->isContainer())
      return mob->inventory().isEmpty() ? grey : green;
    // NB fix 138: a vein of iron, gold or magic ore - green while there is ore in it, grey when it is mined out
    if(const int32_t ore = world->script().nbOreStock(*mob); ore>=0)
      return ore>0 ? green : grey;
    }
  return white;
  }

void MainWindow::paintFocus(Painter& p, const Focus& focus, const Matrix4x4& vp) {
  if(!focus || dialogs.isActive())
    return;

  const float scale = Gothic::interfaceScale(this);
  auto        world = Gothic::inst().world();
  auto        pl    = world==nullptr ? nullptr : world->player();
  if(pl==nullptr)
    return;

  auto pw  = 1.f;
  auto pos = focus.displayPosition();
  vp.project(pos.x,pos.y,pos.z,pw);

  if(pw<=0.f)
    return;

  pos /= pw;

  int   ix  = int((0.5f*pos.x+0.5f)*float(w()));
  int   iy  = int((0.5f*pos.y+0.5f)*float(h()));
  auto& fnt = Resources::font(scale);

  auto tsize = fnt.textSize(focus.displayName());
  ix-=tsize.w/2;
  if(iy<tsize.h)
    iy = tsize.h;
  if(iy>h())
    iy = h();
  // NB fix 81: the name in the color of what it is (as zUtilities): the attitude of a person to the player, the state of a chest
  const auto color = focusColor(focus,*pl);
  const int  sh    = std::max(1,int(scale));
  fnt.tinted(Tempest::Color(0.f,0.f,0.f,0.8f)).drawText(p,ix+sh,iy+sh,focus.displayName());
  fnt.tinted(color).drawText(p,ix,iy,focus.displayName());

  if(focus.npc!=nullptr && !focus.npc->isDead() &&
     !(focus.npc->handle().aivar[15]!=0 && world->script().dmaHidesPartyFocus())) {   // CoM: no bar for the companions
    float hp = float(focus.npc->attribute(ATR_HITPOINTS))/float(focus.npc->attribute(ATR_HITPOINTSMAX));
    drawBar(p,barHp, w()/2,10, hp, AlignHCenter|AlignTop);

    // a mod asks for it (combat.configure): health of the enemy as a number, right under its bar
    const auto co = Scripting::ModHost::inst().combatOptions();
    if(co.active && co.showEnemyHp && barBack!=nullptr) {
      const int   barW = int(200.f*scale*float(std::min(w(),800))/800.f);
      const int   barH = int(float(barBack->h())*float(barW)/float(std::max(barBack->w(),1)));
      const std::string text = std::to_string(focus.npc->attribute(ATR_HITPOINTS))+"/"+
                               std::to_string(focus.npc->attribute(ATR_HITPOINTSMAX));
      const auto  ts   = fnt.textSize(text);
      const int   tx   = w()/2-ts.w/2;
      const int   ty   = 10+barH+ts.h;
      const uint32_t c = co.colorEnemyHp;
      const auto color = Tempest::Color(float((c>>16)&0xFF)/255.f,float((c>>8)&0xFF)/255.f,float(c&0xFF)/255.f,1.f);
      fnt.tinted(Tempest::Color(0.f,0.f,0.f,0.85f)).drawText(p,tx+1,ty+1,text);
      fnt.tinted(color).drawText(p,tx,ty,text);
      }
    }

  const int foc = Gothic::settingsGetI("GAME","highlightMeleeFocus");
  if(focus.npc!=nullptr  &&
     (foc==1 || foc==3) &&
     player.isPressed(KeyCodec::ActionGeneric) &&
     pl->weaponState()!=WeaponState::NoWeapon &&
     pl->weaponState()!=WeaponState::Fist) {
    auto tr = vp;
    tr.mul(focus.npc->transform());

    const auto b    = focus.npc->bounds();
    const auto bbox = b.bbox; //focus.npc->bBox();

    Vec3 bx[] = {
      {bbox[0].x, bbox[0].y, bbox[0].z},
      {bbox[1].x, bbox[0].y, bbox[0].z},
      {bbox[1].x, bbox[1].y, bbox[0].z},
      {bbox[0].x, bbox[1].y, bbox[0].z},
      {bbox[0].x, bbox[0].y, bbox[1].z},
      {bbox[1].x, bbox[0].y, bbox[1].z},
      {bbox[1].x, bbox[1].y, bbox[1].z},
      {bbox[0].x, bbox[1].y, bbox[1].z},
      };

    int min[2]={ix,iy-20}, max[2]={ix,iy-20};
    for(int i=0; i<8; ++i) {
      tr.project(bx[i]);
      int x = int((bx[i].x*0.5f+0.5f)*float(w()));
      int y = int((bx[i].y*0.5f+0.5f)*float(h()));
      min[0] = std::min(x,min[0]);
      min[1] = std::min(y,min[1]);
      max[0] = std::max(x,max[0]);
      max[1] = std::max(y,max[1]);
      }

    paintFocus(p,Rect(min[0],min[1],max[0]-min[0],max[1]-min[1]));
    }

  // focusImg
  /*
  if(auto pl = focus.interactive){
    pl->marchInteractives(p,vp,w(),h());
    }*/
  }

void MainWindow::paintFocus(Painter& p, Rect rect) {
  if(focusImg==nullptr)
    return;
  const int w2 = focusImg->w();
  const int h2 = focusImg->h();
  const int w  = w2/2;
  const int h  = h2/2;

  if(rect.w<w) {
    int dw = w-rect.w;
    rect.x -= dw/2;
    rect.w += dw;
    }
  if(rect.h<h) {
    int dh = h-rect.h;
    rect.y -= dh/2;
    rect.h += dh;
    }

  p.setBrush(Brush(*focusImg,Painter::Add));
  p.drawRect(rect.x,         rect.y,         w,h, 0,0, w, h);
  p.drawRect(rect.x+rect.w-w,rect.y,         w,h, w,0, w2,h);
  p.drawRect(rect.x,         rect.y+rect.h-h,w,h, 0,h, w, h2);
  p.drawRect(rect.x+rect.w-w,rect.y+rect.h-h,w,h, w,h, w2,h2);
  }

void MainWindow::drawBar(Painter &p, const Tempest::Texture2d* bar, int x, int y, float v, AlignFlag flg) {
  const float scale = Gothic::interfaceScale(this);
  drawBar(p,bar,x,y,int(200.f*scale*float(std::min(w(),800))/800.f),v,flg);
  }

void MainWindow::drawBar(Painter &p, const Tempest::Texture2d* bar, int x, int y, int width, float v, AlignFlag flg) {
  if(barBack==nullptr || bar==nullptr)
    return;
  const float destW   = float(width);
  const float k       = float(destW)/float(std::max(barBack->w(),1));
  const float destH   = float(barBack->h())*k;
  const float destHin = float(destH)*24.f/32.f;
  //const float destHin = 20;//float(destH)*24.f/32.f;

  v = std::max(0.f,std::min(v,1.f));
  if(flg & AlignRight)
    x-=int(destW);
  else if(flg & AlignHCenter)
    x-=int(destW)/2;
  if(flg & AlignBottom)
    y-=int(destH);

  p.setBrush(*barBack);
  p.drawRect(x,y,int(destW),int(destH), 0,0,barBack->w(),barBack->h());

  int   dy = int(0.5f*(destH-destHin));
  float pd = 9.f*k;
  p.setBrush(*bar);
  p.drawRect(x+int(pd),y+dy,int(float(destW-pd*2)*v),int(destHin),
             0,0,bar->w(),bar->h());
  }

#ifdef OPENGOTHIC_TOUCH_UI
/// The bars of the original interface: life on the left above the hotbar, mana on the right, oxygen under water
void MainWindow::drawTouchBars(Painter& p, Npc& pl) {
  const auto mode = mobileContext();
  const float hp  = float(pl.attribute(ATR_HITPOINTS))/float(std::max(1,pl.attribute(ATR_HITPOINTSMAX)));
  const float mp  = float(pl.attribute(ATR_MANA))/float(std::max(1,pl.attribute(ATR_MANAMAX)));

  // optional numbers (the settings of the touch interface): never inside of a bar, so the bars stay as they are
  const auto  pref      = AndroidInput::preferences();
  const bool  numbers   = pref.hudNumbers!=0;
  // NB fix 137: New Balance has a bar of the experience of its own (the option of the game); the old counter only without it
  const bool  xpBar     = Gothic::settingsGetI("NB_OG","bXpBar")!=0 && nbSprintFn()!=nullptr;
  const bool  xp        = pref.hudXp!=0 && !xpBar;
  androidControls.xpBar(xpBar);
  const auto  hpText    = std::to_string(pl.attribute(ATR_HITPOINTS))+"/"+
                          std::to_string(pl.attribute(ATR_HITPOINTSMAX));
  const auto  mpText    = std::to_string(pl.attribute(ATR_MANA))+"/"+std::to_string(pl.attribute(ATR_MANAMAX));
  const auto  xpText    = std::to_string(pl.experience())+"/"+std::to_string(pl.experienceNext());
  auto&       hudFont   = Resources::font(Gothic::interfaceScale(this));
  // the text is drawn twice, the dark one is a shadow: it can be read on any background
  auto drawText = [&](const std::string& text, int x, int baseline) {
    hudFont.tinted(Tempest::Color(0.f,0.f,0.f,0.85f)).drawText(p,x+1,baseline+1,text);
    hudFont.tinted(Tempest::Color(1.f,1.f,1.f,1.f)).drawText(p,x,baseline,text);
    };
  auto barHeight = [&](int width) {
    return barBack==nullptr ? 0 : int(float(barBack->h())*float(width)/float(std::max(barBack->w(),1)));
    };

  if(mode==Mobile::Mode::Inventory) {
    const int width = int(200.f*Gothic::interfaceScale(this)*float(std::min(w(),800))/800.f);
    const int yMana = 10+int(float(width)*0.125f)+2;
    drawBar(p,barHp,10,10,width,hp,AlignLeft);
    drawBar(p,barMana,10,yMana,width,mp,AlignLeft);
    // the numbers stand to the right of the bars, the experience under them
    const int bh = barHeight(width);
    const int th = hudFont.textSize(hpText).h;
    if(numbers) {
      drawText(hpText,10+width+8,10+(bh+th)/2);
      drawText(mpText,10+width+8,yMana+(bh+th)/2);
      }
    if(xp)
      drawText(xpText,10,yMana+bh+th+4);
    return;
    }
  if(mode!=Mobile::Mode::Game)
    return;

  // NB fix 42: the bars stand where the editor of the HUD has put them (by default over the ends of the hotbar, the
  // stamina of New Balance in the middle), in the size chosen there; a hidden bar is not drawn (in the editor it is)
  const auto& places  = androidControls.hudPlaces();
  const bool  editing = androidControls.editingHud();
  const float buttons = float(pref.buttons)/100.f;
  auto placeOf = [&](int i) {
    const auto it = places.find(Mobile::HudBarIds[i]);
    return it==places.end() ? Mobile::HudPlace() : it->second;
    };
  struct Drawn {
    bool visible = false;
    int  x = 0, top = 0, width = 0, bottom = 0;
    };
  Drawn drawn[Mobile::HudBarCount];
  auto drawHudBar = [&](int i, const Tempest::Texture2d* tex, float value) {
    const auto place = placeOf(i);
    if(place.hidden && !editing)
      return;
    const auto r     = Mobile::hudBarRect(i,float(w()),float(h()),buttons,place);
    const int  width = int(r.w);
    const int  bh    = barHeight(width);
    const int  bot   = int(r.y+r.h/2)+bh/2;
    drawBar(p,tex,int(r.x),bot,width,value,AlignLeft | AlignBottom);
    drawn[i] = {true,int(r.x),bot-bh,width,bot};
    };

  drawHudBar(0,barHp,hp);
  // NB fix 137: the experience of the level, as the bar of the plugin (UNION_XP_BAR): the part of the way to the next level.
  // The span of a level is the one of RX_CalcNextLevelExp of the scripts: 500, from the level 10 600, 20 650, 30 800, 40 900,
  // 50 1000, 60 1200 - times (level+1)
  if(xpBar) {
    const int32_t lvl  = pl.level();
    const int32_t k    = lvl>=60 ? 1200 : lvl>=50 ? 1000 : lvl>=40 ? 900 : lvl>=30 ? 800 : lvl>=20 ? 650 : lvl>=10 ? 600 : 500;
    const int64_t next = pl.experienceNext();
    const int64_t span = lvl<=0 ? 500 : int64_t(k)*(lvl+1);
    const int64_t prev = std::max<int64_t>(0,next-span);
    const int64_t have = std::clamp<int64_t>(int64_t(pl.experience())-prev,0,std::max<int64_t>(1,next-prev));
    const int64_t need = std::max<int64_t>(1,next-prev);
    drawHudBar(3,barMisc,float(have)/float(need));
    if(drawn[3].visible) {
      const auto& d    = drawn[3];
      const auto  text = "Poz. "+std::to_string(lvl)+"   "+std::to_string(have)+"/"+std::to_string(need)+
                         " ("+std::to_string(int(have*100/need))+"%)";
      const int   tw   = hudFont.textSize(text).w;
      const int   th0  = hudFont.textSize(text).h;
      drawText(text,d.x+(d.width-tw)/2,(d.top+d.bottom)/2+th0/2-2);
      }
    }
  if(pl.isDive()) {
    const float total = float(pl.diveTimeLimit())/1000.f;
    const float left  = total>0 ? (total-float(pl.diveTime())/1000.f)/total : 0.f;
    drawHudBar(2,barMisc,left);
    } else {
    drawHudBar(2,barMana,mp);
    }

  // the stamina of New Balance (filled by the scripts with Bar_*): in the editor it is shown full to be placed
  const Tempest::Texture2d* stTex = nullptr;
  float                     stVal = 0;
  const auto*               stBar = nbStaminaBar(stTex,stVal);
  androidControls.stamina(NbBars::at(NbBars::Stamina)->maxRange>0);
  if(stBar!=nullptr)
    drawHudBar(1,stTex,stVal); else
  if(editing && NbBars::at(NbBars::Stamina)->maxRange>0)
    drawHudBar(1,Resources::loadTexture("BAR_Stamina.tga"),1.f);

  const int th = hudFont.textSize(hpText).h;
  auto textOver = [&](const Drawn& d, const std::string& text, int line) {
    // centered over the frame (the text is above it, not in it); line 1 is one more line higher
    drawText(text,d.x+(d.width-hudFont.textSize(text).w)/2,d.top-2-line*(th+2));
    };
  if(numbers) {
    if(drawn[0].visible)
      textOver(drawn[0],hpText,0);
    if(drawn[2].visible && !pl.isDive())
      textOver(drawn[2],mpText,0);
    if(drawn[1].visible && stBar!=nullptr)
      textOver(drawn[1],std::to_string(stBar->value)+"/"+std::to_string(stBar->maxRange),0);
    }
  if(xp) {
    if(drawn[1].visible) {
      // above the stamina (and its numbers)
      textOver(drawn[1],xpText,numbers && stBar!=nullptr ? 1 : 0);
      } else {
      // between the bars of life and mana, on the level of the frames (the default place)
      const auto r = Mobile::hudBarRect(1,float(w()),float(h()),buttons,Mobile::HudPlace());
      drawText(xpText,w()/2-hudFont.textSize(xpText).w/2,int(r.y+r.h/2)+th/2);
      }
    }
  }
#endif

void MainWindow::drawMsg(Tempest::Painter& p) {
  if(barBack==nullptr)
    return;
  const float scale   = Gothic::interfaceScale(this);
  const float destW   = 200.f*scale*float(std::min(w(),800))/800.f;
  const float k       = float(destW)/float(std::max(barBack->w(),1));
  const float destH   = float(barBack->h())*k;

  const int y = 10 + int(destH) + 10;
  dialogs.drawMsg(p, y);
  }

/// The views and the texts that the scripts of Ikarus and LeGo put on the screen (bars, messages, windows of the mods)
void MainWindow::drawDmaOverlay(Tempest::Painter& p) {
  auto* world = Gothic::inst().world();
  if(world==nullptr || !world->script().hasDma())
    return;

  dmaOverlay.clear();
  world->script().dmaOverlay(dmaOverlay,w(),h());
  if(dmaOverlay.empty())
    return;

  const float sx    = float(w())/8192.f;
  const float sy    = float(h())/8192.f;
  const float scale = std::min(float(w())/800.f,float(h())/600.f);

  auto brushColor = [](uint32_t c) {
    return Color(float((c>>16)&0xFF)/255.f, float((c>>8)&0xFF)/255.f, float(c&0xFF)/255.f, float((c>>24)&0xFF)/255.f);
    };

  for(auto& it:dmaOverlay) {
    const auto color = brushColor(it.color);
    if(color.a()<=0.01f)
      continue;
    if(it.kind==GameScript::DmaOverlayItem::Model) {
      if(it.model!=nullptr && it.model->user_ptr!=nullptr) {
        const auto* item = reinterpret_cast<const Item*>(it.model->user_ptr);
        const auto width = int(it.w*sx), height = int(it.h*sy);
        if(width>0 && height>0)
          dmaModels[cmdId].drawItem(int(it.x*sx),int(it.y*sy),width,height,*item);
        }
      } else if(it.kind==GameScript::DmaOverlayItem::Triangle) {
      if(const auto* tex = Resources::loadTexture(it.name)) {
        const auto blend = it.blend==1 ? Painter::NoBlend : it.blend==3 ? Painter::Add : Painter::Alpha;
        p.setBrush(Brush(*tex,color,blend));
        const auto& a = it.vertices[0];
        const auto& b = it.vertices[1];
        const auto& c = it.vertices[2];
        const float tw = float(tex->w()), th = float(tex->h());
        p.drawTriangle(a.x*sx,a.y*sy,a.u*tw,a.v*th,
                       b.x*sx,b.y*sy,b.u*tw,b.v*th,
                       c.x*sx,c.y*sy,c.u*tw,c.v*th);
        }
      } else if(it.kind==GameScript::DmaOverlayItem::Image) {
      const int x = int(it.x*sx), y = int(it.y*sy), iw = int(it.w*sx), ih = int(it.h*sy);
      if(iw<=0 || ih<=0)
        continue;
      if(it.name=="DEFAULT.TGA") {
        p.setBrush(color);
        p.drawRect(x,y,iw,ih);
        continue;
        }
      if(const Tempest::Texture2d* tex = Resources::loadTexture(it.name)) {
        p.setBrush(Brush(*tex,color));
        p.drawRect(x,y,iw,ih,0,0,tex->w(),tex->h());
        }
      } else {
      const auto fnt = Resources::font(it.font,Resources::FontType::Normal,scale).tinted(color);
      fnt.drawText(p,int(it.x*sx),int(it.y*sy)+fnt.pixelSize(),it.name);
      }
    }
  p.setBrush(Color(1.0));
  }

void MainWindow::drawProgress(Painter &p, int x, int y, int w, int h, float v) {
  if(v<0.1f)
    v=0.1f;
  p.setBrush(*loadBox);
  p.drawRect(x,y,w,h, 0,0,loadBox->w(),loadBox->h());

  // the margins of the bar are in the pixels of the original 512 px texture: for an HD texture
  // of a mod they are scaled with it, else the bar is drawn over the frame
  const float k = progressTexScale(loadBox);
  int paddL = int((float(w)*75.f*k)/float(loadBox->w()));
  int paddT = int((float(h)*10.f*k)/float(loadBox->h()));

  if(Gothic::inst().version().game==1) {
    paddL = int((float(w)*30.f)/float(loadBox->w()));
    paddT = int((float(h)* 5.f)/float(loadBox->h()));
    }

  p.setBrush(*loadVal);
  p.drawRect(x+paddL,y+paddT,int(float(w-2*paddL)*v),h-2*paddT,
             0,0,loadVal->w(),loadVal->h());
  }

void MainWindow::drawLoading(Painter &p, int x, int y, int w, int h) {
  float v = float(Gothic::inst().loadingProgress())/100.f;
  drawProgress(p,x,y,w,h,v);
  }

void MainWindow::drawSaving(Painter &p) {
  if(auto back = Gothic::inst().loadingBanner()) {
    p.setBrush(Brush(*back,Painter::NoBlend));
    p.drawRect(0,0,this->w(),this->h(),
               0,0,back->w(),back->h());
    }

  if(saveback==nullptr)
    saveback = Resources::loadTexture("SAVING.TGA");
  if(saveback==nullptr)
    return;

  const float scale = Gothic::interfaceScale(this);
  int         szX   = Gothic::options().saveGameImageSize.w;
  int         szY   = Gothic::options().saveGameImageSize.h;

  if(szX<=460 || szY<=300) {
    // way too small otherwise
    szX = 460;
    szY = 300;
    }
  szX = int(float(szX)*scale);
  szY = int(float(szY)*scale);
  drawSaving(p,*saveback,szX,szY,scale);
  }

void MainWindow::drawSaving(Painter& p, const Tempest::Texture2d& back, int sw, int sh, float scale) {
  const int x = (w()-sw)/2, y = (h()-sh)/2;

  // SAVING.TGA is semi-transparent image with the idea to accomulate alpha over time
  // ... for loop for now
  p.setBrush(back);
  for(int i=0;i<10;++i) {
    p.drawRect(x,y,sw,sh, 0,0,back.w(),back.h());
    }

  float v = float(Gothic::inst().loadingProgress())/100.f;
  drawProgress(p, x+int(100.f*scale), y+sh-int(75.f*scale), sw-2*int(100.f*scale), int(40.f*scale), v);
  }

void MainWindow::isDialogClosed(bool& ret) {
  ret = !(dialogs.isActive() || document.isActive() || nbSales.isActive() || nbCraft.isActive());
  }

template<Tempest::KeyEvent::KeyType k>
void MainWindow::onMarvinKey() {
  switch(k) {
    case Event::K_F2:
      if(Gothic::inst().isMarvinEnabled()) {
        console.resize(w(),h());
        console.setFocus(true);
        console.exec();
        }
      break;
    case Event::K_F3:
      setFullscreen(!SystemApi::isFullscreen(hwnd()));
      break;
    case Event::K_F4:
      if(Gothic::inst().isMarvinEnabled()) {
        auto camera = Gothic::inst().camera();
        auto pl = Gothic::inst().player();
        if(camera!=nullptr && pl!=nullptr) {
          camera->setMarvinMode(Camera::M_Normal);
          camera->reset(pl);
          }
        }
      break;
    case Event::K_F5: {
      const bool useQuickSaveKeys = Gothic::settingsGetI("GAME", "useQuickSaveKeys")!=0;
#ifdef NDEBUG
      const bool debug = false;
#else
      const bool debug = true;
#endif
      if(!debug && Gothic::inst().isMarvinEnabled() && !dialogs.isActive()) {
        if(auto camera = Gothic::inst().camera()) {
          camera->setMarvinMode(Camera::M_Freeze);
          }
        }
      else if(Gothic::inst().isInGameAndAlive() && !Gothic::inst().isPause() && useQuickSaveKeys) {
        Gothic::inst().quickSave();
        }
      break;
      }

    case Event::K_F6:
      if(Gothic::inst().isMarvinEnabled() && !dialogs.isActive()) {
        auto camera = Gothic::inst().camera();
        auto pl     = Gothic::inst().player();
        auto inter  = pl!=nullptr ? pl->interactive() : nullptr;
        if(camera!=nullptr && inter==nullptr)
          camera->setMarvinMode(Camera::M_Free);
        }
      break;
    case Event::K_F7:
      if(Gothic::inst().isMarvinEnabled() && !dialogs.isActive()) {
        if(auto camera = Gothic::inst().camera()) {
          camera->setMarvinMode(Camera::M_Pinned);
          }
        }
      break;
    case Event::K_F8:
      //player.marvinF8();
      break;

    case Event::K_F9: {
      const bool useQuickSaveKeys = Gothic::settingsGetI("GAME", "useQuickSaveKeys")!=0;
      if(Gothic::inst().isMarvinEnabled()) {
        if(runtimeMode==R_Normal)
          runtimeMode = R_Suspended; else
          runtimeMode = R_Normal;
        }
      else if(!Gothic::inst().isPause() && useQuickSaveKeys) {
        Gothic::inst().quickLoad();
        }
      break;
      }
    case Event::K_F10:
      if(runtimeMode==R_Suspended)
        runtimeMode = R_Step; else
        modMenu.toggle();
      break;
    case Event::K_P:
      if(Gothic::inst().isMarvinEnabled()) {
        if(auto p = Gothic::inst().player()) {
          auto pos = p->position();
          string_frm buf("Position: ", pos.x,'/',pos.y,'/',pos.z);
          Gothic::inst().onPrint(buf);
          }
        }
      break;
    }
  }

uint64_t MainWindow::tick() {
  if(CommandLine::inst().doShowErrors())
    errorScreen.poll();
  if(errorScreen.isActive())
    clearInput();
  Scripting::game::tick();
  NbBars::inventoryOpen() = inventory.isActive();   // NB fix 49: Hlp_InventoryIsOpen
  if(auto* w = Gothic::inst().world()) {
    // a script (CoM, fistfight) closes the inventory of the player
    if(w->script().dmaTakeInventoryClose() && inventory.isActive())
      inventory.close();
    }
  modMenu.refresh();
#ifdef OPENGOTHIC_TOUCH_UI
  {
    // a field of text of the menu of the mods gets the keyboard of the screen when it gets the focus
    static bool typing = false;
    const bool  wants  = modMenu.wantsText() || dialogs.wantsText() || rootMenu.wantsText();
    androidControls.modMenu(modMenu.isActive());
    androidControls.bareMenu(nbSales.isActive() || nbCraft.isActive());   // NB fix 68: no buttons of the overlay over the sales window
    if(dialogs.takeKeyboardRequest()) {
      typing = true;
      androidControls.keyboard(true);
      }
    if(wants!=typing) {
      typing = wants;
      androidControls.keyboard(wants);
      }
  }
  updateMobileControls();
#endif
  auto time = Application::tickCount();
  auto dt   = time-lastTick;
  // NOTE: limit to ~200 FPS in game logic to avoid math issues
  if(dt<5)
    return 0;
  lastTick  = time;

  auto st = Gothic::inst().checkLoading();
  if(st==Gothic::LoadState::Finalize || st==Gothic::LoadState::FailedLoad || st==Gothic::LoadState::FailedSave) {
    const std::string failure = Gothic::inst().loadError();
    Gothic::inst().finishLoading();
    if(st==Gothic::LoadState::Finalize && OriginalSave::takeSaveName(importedSaveName))
      importedSaveDelay = 30; // a few frames, the save takes a picture of the screen
    if(st==Gothic::LoadState::FailedLoad) {
      // the game was dropped by the start of loading: nothing to continue, back to the menu with the reason on the screen
      rootMenu.setMainMenu();
      errorScreen.report("Unable to load the game",
                         failure.empty() ? std::string("unknown error") : failure);
      }
    if(st==Gothic::LoadState::FailedSave) {
      Gothic::inst().onPrint("unable to write savegame file");
      if(!failure.empty())
        errorScreen.report("Unable to write the save-game",failure);
      }
    return 0;
    }
  else if(st!=Gothic::LoadState::Idle) {
    if(st==Gothic::LoadState::Loading)
      GameMusic::inst().setMusic(GameMusic::SysLoading); else
      rootMenu.processMusicTheme();
    return 0;
    }

  video.tick();
  if(video.isActive())
    return 0;

  if(importedSaveDelay>0 && --importedSaveDelay==0 && Gothic::inst().isInGame()) {
    // an imported original save gets a slot of its own right away: it is on the list of the saves like any other
    Gothic::inst().save(SaveStore::newFile(),importedSaveName);
    importedSaveName.clear();
    }

  if(Gothic::inst().isPause()) {
    return 0;
    }

  if(dt>50)
    dt=50;

  if(runtimeMode==R_Step) {
    runtimeMode = R_Suspended;
    dt = 1000/60; //60 fps
    }
  else if(runtimeMode==R_Suspended) {
    auto camera = Gothic::inst().camera();
    if(camera!=nullptr && camera->isFree()) {
      tickMouse(dt);
      }
    update();
    return 0;
    }

  realTickDt = dt;
  dialogs.tick(dt);
  inventory.tick(dt);

  // fast forward: more game steps in a frame, none of them longer than 50 ms
  uint64_t total = dt;
#ifdef OPENGOTHIC_TOUCH_UI
  total = fastForwardDt(dt);
#endif
  if(dialogs.isActive())
    ;//clearInput();
  const uint64_t steps = std::max<uint64_t>(1,(total+49)/50);
  for(uint64_t i=0; i<steps; ++i) {
    const uint64_t step = total/steps + (i<total%steps ? 1 : 0);
    Gothic::inst().tick(step);
    player.tickFocus();
    if(document.isActive())
      clearInput();
    if(i==0)
      tickMouse(dt);
#ifdef OPENGOTHIC_TOUCH_UI
    player.setPadMove(androidControls.moveYaw(),androidControls.moveSide());
#endif
    player.tickMove(step);
    }
  update();
  return total;
  }

void MainWindow::updateAnimation(uint64_t dt) {
  Gothic::inst().updateAnimation(dt);
  }

/// The inventory, the menu, the log: a scene of a mod without a dialog does not let the player open them (for at most two minutes,
/// so that a scene that has hung does not hold him for ever)
bool MainWindow::sceneLocksUi() const {
  if(auto* world = Gothic::inst().world(); world!=nullptr && world->script().dmaCutsceneMenuBlocked())
    return true;
  return sceneLockTime>0 && sceneLockTime<120000;
  }

void MainWindow::requestDialogCamera() {
  dialogCameraReset = true;
  }

void MainWindow::tickCamera(uint64_t dt) {
  {
  auto* w = Gothic::inst().world();
  if(w!=nullptr && w->script().hasDma() && w->isCutsceneLock() && !dialogs.isActive())
    sceneLockTime += dt; else
    sceneLockTime = 0;
  }
  auto pcamera = Gothic::inst().camera();
  auto pl      = Gothic::inst().player();
  if(pcamera==nullptr)
    return;

  auto&      camera       = *pcamera;
  if(dialogCameraReset && !camera.isCutscene()) {
    dialogCameraReset = false;
    auto* world = Gothic::inst().world();
    if(dialogs.isActive() && !dialogs.isSelfDialog() && !camera.isFree() && world!=nullptr && !world->script().dmaDialogCameraOff()) {
      camera.setMode(Camera::Dialog);
      dialogs.dialogCamera(camera);
      }
    }
  const auto ws           = pl!=nullptr ? pl->weaponState() : WeaponState::NoWeapon;
  const bool meleeFocus   = (ws==WeaponState::Fist ||
                             ws==WeaponState::W1H  ||
                             ws==WeaponState::W2H);
  auto       pos          = pl!=nullptr ? pl->cameraBone(camera.isFirstPerson()) : Vec3();

  if(!camera.isCutscene() && !camera.isFree()) {
    const bool fs = SystemApi::isFullscreen(hwnd());
    if(!fs && mouseP[Event::ButtonLeft]) {
      camera.setSpin(camera.spin());
      camera.setTarget(pos);
      }
    else if(dialogs.isActive() && !dialogs.isMobsiDialog() && !dialogs.isSelfDialog()) {
      dialogs.dialogCamera(camera);
      }
    else if(inventory.isActive()) {
      camera.setTarget(pos);
      }
    else if(player.focus().npc!=nullptr && meleeFocus && pl!=nullptr) {
      auto spin = camera.spin();
      spin.y = pl->rotation();
      camera.setSpin(spin);
      camera.setTarget(pos);
      }
    else if(pl!=nullptr && !camera.isFree()) {
      auto spin = camera.spin();
      if(pl->interactive()==nullptr && !pl->isDown())
        spin.y = pl->rotation();
      if(pl->isDive() && !camera.isMarvin())
        spin.x = -pl->rotationY();
      camera.setSpin(spin);
      camera.setTarget(pos);
      }
    }

  if(dt==0)
    return;
  if(camera.isToggleEnabled() && !camera.isCutscene())
    camera.setMode(solveCameraMode());
  camera.tick(dt);
  }

Camera::Mode MainWindow::solveCameraMode() const {
  const auto camera = Gothic::inst().camera();
  if(camera!=nullptr && camera->isFree())
    return Camera::Normal;

  // NB fix 72: a menu of the hero with himself (the teleport rune of NB: "...do wieży Xardasa") has no one to look at: the
  // dialog camera looked from above on his head. As on the PC the camera stays behind the hero
  if(dialogs.isActive() && !dialogs.isSelfDialog()) {
    // CoM turns the camera of the dialog off in the scenes with cameras of their own
    auto* w = Gothic::inst().world();
    if(w==nullptr || !w->script().dmaDialogCameraOff())
      return Camera::Dialog;
    }

  if(camera!=nullptr && camera->isFirstPerson())
    return Camera::FirstPerson;

  if(inventory.isOpen()==InventoryMenu::State::Equip ||
     inventory.isOpen()==InventoryMenu::State::Ransack)
    return Camera::Inventory;

  if(auto pl=Gothic::inst().player()) {
    if(pl->interactive()!=nullptr)
      return Camera::Mobsi;
    }

  if(auto pl = Gothic::inst().player()) {
    if(pl->isDead())
      return Camera::Death;
    if(pl->isDive())
      return Camera::Dive;
    if(pl->isSwim())
      return Camera::Swim;
    if(pl->isFallingDeep())
      return Camera::Fall;
    bool g2 = Gothic::inst().version().game==2;
    switch(pl->weaponState()){
      case WeaponState::Fist:
      case WeaponState::W1H:
      case WeaponState::W2H:
        return Camera::Melee;
      case WeaponState::Bow:
      case WeaponState::CBow:
        return g2 ? Camera::Ranged : Camera::Normal;
      case WeaponState::Mage:
        return g2 ? Camera::Ranged : Camera::Melee;
      case WeaponState::NoWeapon:
        return Camera::Normal;
      }
    }

  return Camera::Normal;
  }

void MainWindow::startGame(std::string_view slot) {
  // before the old game is dropped: the scripts of the mod choose the picture
  const std::string banner = Gothic::loadingBannerName(Gothic::inst().world()!=nullptr ? &Gothic::inst().world()->script() : nullptr,"LOADING.TGA");
  // gothic.emitGlobalSound(gothic.loadSoundFx("NEWGAME"));

  if(Gothic::inst().checkLoading()==Gothic::LoadState::Idle){
    setGameImpl(nullptr);
    }

  Gothic::inst().startLoad(banner,[slot=std::string(slot)](std::unique_ptr<GameSession>&& game){
    game = nullptr; // clear world-memory now
    OriginalSave original;
    if(OriginalSave::pending(original)) {
      try {
        return std::unique_ptr<GameSession>(new GameSession(original));
        }
      catch(const std::exception& e) {
        // the next New game is a plain one; the reason is on the screen
        OriginalSave::retire(original,false,"");
        throw std::runtime_error(std::string("original save import: ")+e.what());
        }
      }
    std::unique_ptr<GameSession> w(new GameSession(slot));
    return w;
    });

  background = Texture2d();
  update();
  }

void MainWindow::loadGame(std::string_view slot) {
  // before the old game is dropped: the scripts of the mod choose the picture
  std::string banner = Gothic::loadingBannerName(Gothic::inst().world()!=nullptr ? &Gothic::inst().world()->script() : nullptr,"LOADING.TGA");
  if(auto* w = Gothic::inst().world(); w!=nullptr && w->script().dmaLoadDisabled()) {
    Log::i("The scripts of the mod do not allow loading now (ENABLELOADGAME)");
    return;
    }
  if(Gothic::inst().checkLoading()==Gothic::LoadState::Idle) {
    setGameImpl(nullptr);
    }

  Gothic::inst().setBenchmarkMode(Benchmark::None);
  // the loading picture of the world of the save (LOADING_NEWWORLD.TGA), as for a change of the world:
  // the mods on Union (New Balance) choose LOADING_<WORLD>_<number> by it; without the header the picture of the scripts stays
  try {
    Tempest::RFile fin{std::string(slot)};
    Serialize      reader(fin);
    SaveGameHeader hdr;
    reader.setEntry("header");
    reader.read(hdr);
    std::string w = hdr.world;
    const size_t beg = w.find_last_of("\\/");
    if(beg!=std::string::npos)
      w = w.substr(beg+1);
    const size_t end = w.rfind('.');
    if(end!=std::string::npos)
      w.resize(end);
    if(!w.empty())
      banner = "LOADING_" + w + ".TGA";
    }
  catch(...) {
    }
  Gothic::inst().startLoad(banner,[slot=std::string(slot)](std::unique_ptr<GameSession>&& game){
    game = nullptr; // clear world-memory now
    try {
      Tempest::RFile file(slot);
      Serialize      s(file);
      std::unique_ptr<GameSession> w(new GameSession(s));
      return w;
      }
    catch(const std::bad_alloc&) {
      throw;
      }
    catch(const std::exception& e) {
      // the name of the file is the first thing to know about a damaged save-game
      throw std::runtime_error("save-game '"+slot+"': "+e.what());
      }
    });

  background = Texture2d();
  update();
  }

void MainWindow::saveGame(std::string_view slot, std::string_view name) {
  if(dialogs.isActive())
    return;
  if(auto w = Gothic::inst().world(); w!=nullptr && w->currentCs()!=nullptr)
    return;
  if(auto w = Gothic::inst().world(); w!=nullptr && w->script().dmaSaveDisabled()) {
    // CoM does not let the player save in its scenes
    Log::i("The scripts of the mod do not allow saving now (ENABLESAVEGAME)");
    return;
    }

  auto tex  = renderer.screenshoot(cmdId, Size(int(swapchain.w()),int(swapchain.h())));
  auto lres = Attachment();

  static int32_t kThumbW = 800;
  const  int32_t kThumbH = tex.w()>0 ? int32_t((tex.h() * kThumbW) / tex.w()) : 0;
  if(kThumbW>0 && kThumbH>0 && kThumbW<tex.w() && kThumbH<tex.h()) {
    lres = device.attachment(Tempest::TextureFormat::RGBA8, uint32_t(kThumbW), uint32_t(kThumbH));
    }

  if(!lres.isEmpty()) {
    // reduce size of the save entry preview screenshot for faster save & load
    CommandBuffer cmd;
    {
    auto enc = cmd.startEncoding(device);
    enc.setDebugMarker("Downscale screenhoot");
    enc.setFramebuffer({{lres, Vec4(), Tempest::Preserve}});
    enc.setPushData(IVec2(lres.w(), lres.h()));
    enc.setBinding(0, tex, Sampler::nearest());
    enc.setPipeline(Shaders::inst().downscale);
    enc.draw(nullptr, 0, 3);
    }
    auto sync = device.submit(cmd);
    sync.wait();
    }

  auto& thumb = lres.isEmpty() ? tex : lres;
  auto  pm    = device.readPixels(textureCast<const Texture2d&>(thumb));

  Gothic::inst().startSave(std::move(textureCast<Texture2d&>(tex)),[slot=std::string(slot),name=std::string(name),pm](std::unique_ptr<GameSession>&& game){
    if(!game)
      return std::move(game);

    {
    Tempest::WFile  f(slot);
    Serialize      s(f);
    game->save(s,name,pm);
    }

    // no print yet, because threading
    // gothic.print("Game saved");
    return std::move(game);
    });

  update();
  }

void MainWindow::onVideo(std::string_view fname) {
  if(Gothic::inst().isBenchmarkMode())
    return;
  video.pushVideo(fname);
  }

void MainWindow::onStartLoading() {
  dialogCameraReset = false;
  player   .clearInput();
  player   .clearFocus();
#ifdef OPENGOTHIC_TOUCH_UI
  fastForward = false;
#endif
  inventory.onWorldChanged();
  dialogs  .onWorldChanged();
  }

void MainWindow::onWorldLoaded() {
  dMouse = Point();

  if(Gothic::inst().isBenchmarkMode()) {
    if(auto world = Gothic::inst().world()) {
      const TriggerEvent evt("TIMEDEMO","",world->tickCount(),TriggerEvent::T_Trigger);
      world->execTriggerEvent(evt);
      }
    benchmark.clear();
    }

  player   .clearInput();
  inventory.onWorldChanged();
  dialogs  .onWorldChanged();

  device.waitIdle();
  for(auto& c:commands)
    c = device.commandBuffer();

  if(auto c = Gothic::inst().camera())
    c->setViewport(uint32_t(w()),uint32_t(h()));

  renderer.onWorldChanged();

  if(auto pl = Gothic::inst().player())
    rootMenu.setPlayer(*pl);

  if(auto pl = Gothic::inst().player())
    pl->multSpeed(1.f);
  lastTick = Application::tickCount();
  player.clearFocus();
  }

void MainWindow::onSessionExit() {
  rootMenu.setMainMenu();
  }

void MainWindow::onBenchmarkFinished() {
  if(benchmark.numFrames==0)
    return;

  double fps  = benchmark.fpsSum/double(benchmark.numFrames);
  double low1 = 0;
  size_t num1 = 0;
  for(size_t i=0; i<benchmark.low1procent.size(); ++i) {
    auto v = benchmark.low1procent[i];
    if(v<=0)
      continue;
    low1 += 1000.0/double(v);
    num1 += 1;
    }
  low1 = num1>0 ? low1/double(num1) : 0.0;
  benchmark.clear();

  string_frm str("Benchmark: low 1% = ", low1, " fps = ", fps);
  Log::i(str.c_str());
  console.printLine(str);

  if(Gothic::inst().isBenchmarkModeCi()) {
    Log::i("Exiting benchmark");
    Tempest::SystemApi::exit();
    return;
    }

  console.setFocus(true);
  console.exec();
  }

void MainWindow::setGameImpl(std::unique_ptr<GameSession> &&w) {
  // clear pointers to outdated game-state
  inventory.onWorldChanged();
  dialogs  .onWorldChanged();
  Gothic::inst().setGame(std::move(w));
  }

void MainWindow::clearInput() {
  player.clearInput();
  std::memset(mouseP,0,sizeof(mouseP));
  if(auto* world = Gothic::inst().world()) {
    for(size_t code=1; code<dmaKeysDown.size(); ++code)
      if(dmaKeysDown[code])
        world->script().dmaKeyState(int(code),false);
    }
  dmaKeysDown.reset();
  }

void MainWindow::setFullscreen(bool fs) {
  SystemApi::setAsFullscreen(hwnd(),fs);
  setCursorShape(fs ? HiddenCursor : CursorShape::Arrow);
  }

void MainWindow::render(){
  GameScript::nbUiBusy = inventory.isActive() || dialogs.isActive() || document.isActive() ||
                         nbSales.isActive() || nbCraft.isActive();   // NB fix 136
  try {
#ifdef __ANDROID__
    if(!androidWindowCheck())
      return;
#endif
    static uint64_t time=Application::tickCount();

    static bool once=true;
    if(once) {
      // CoM supplies its own menu recording. The vanilla startup WAV is a
      // long musical cue on the SFX device, independent of the music player.
      if(!Gothic::musicDef().hasRecordedMusic())
        Gothic::inst().emitGlobalSoundWav("GAMESTART.WAV");
      once=false;
      }

    if(T_UNLIKELY(Gothic::inst().isBenchmarkModeCi())) {
      const auto st = Gothic::inst().checkLoading();
      if(st==Gothic::LoadState::Loading) {
        // skip loading frames in benchmark-ci mode, for sake of easier tooling
        return;
        }
      }

    /*
      Note: game update goes first
      once player position is updated, animation bones(cameraBone in particular) can be updated
      lastly - camera position
      */
    uint64_t dt = 0;
    try {
      dt = tick();
      updateAnimation(dt);
      tickCamera(dt==0 ? 0 : realTickDt);
      }
    catch(const Tempest::SwapchainSuboptimal&) {
      throw;
      }
    catch(const std::exception& e) {
      // an exception of the scripts or of the game: it is shown with the details, the game goes on (the same error is shown once)
      Log::e("exception in the game: ",e.what());
      errorScreen.report("Exception of the game",e.what());
      }

    auto& sync = fence[cmdId];
    if(!sync.wait(0)) {
      // GPU rendering is not done, pass to next frame
      std::this_thread::yield();
      return;
      }
    Resources::resetRecycled(cmdId);
    Resources::updateTextureQuality(Gothic::settingsGetF("INTERNAL","texDetailIndex"));

    if(video.isActive()) {
      video.paint(device,cmdId);
      uiLayer.clear();
      PaintEvent p(uiLayer,atlas,this->w(),this->h());
      video.paintEvent(p);
      }
    else if(needToUpdate() || Gothic::inst().checkLoading()!=Gothic::LoadState::Idle) {
      dispatchPaintEvent(uiLayer,atlas);

      numOverlay.clear();
      PaintEvent p(numOverlay,atlas,this->w(),this->h());
      inventory.paintNumOverlay(p);
      }
    uiMesh [cmdId].update(device,uiLayer);
    numMesh[cmdId].update(device,numOverlay);

#ifdef OPENGOTHIC_TOUCH_UI
    updateMobileOverlay();
#endif
    // Models of the inventory are drawn over everything else, hide them below a document
    inventory.setCovered(document.isActive() || chapter.isActive() || video.isActive());

    CommandBuffer& cmd = commands[cmdId];
    {
    auto enc = cmd.startEncoding(device);
    renderer.draw(swapchain[swapchain.currentImage()],enc,cmdId,uiMesh[cmdId],numMesh[cmdId],inventory,video);
    if(!dmaModels[cmdId].isEmpty() && !video.isActive() && !document.isActive() &&
       !chapter.isActive() && !rootMenu.isActive() && Gothic::inst().checkLoading()==Gothic::LoadState::Idle)
      renderer.drawUiItems(swapchain[swapchain.currentImage()],enc,dmaModels[cmdId]);
#ifdef OPENGOTHIC_TOUCH_UI
    // The last pass: controls stay above menus, video and 3D inventory
    enc.setViewport(0,0,w(),h());
    touchMesh[cmdId].draw(enc);
    if(mobileContext()==Mobile::Mode::Game && !androidControls.settingsVisible())
      renderer.drawTouchItems(swapchain[swapchain.currentImage()],enc,mobileItems[cmdId]);
#endif
    }
    sync = device.submit(cmd);
    device.present(swapchain);
    cmdId = (cmdId+1u)%Resources::MaxFramesInFlight;

    auto t = Application::tickCount();
    auto frameTime = maxFpsInv;
    if(!Gothic::inst().isInGame() && !video.isActive())
      frameTime = std::max<uint64_t>(frameTime,16);
    if(t-time<frameTime) {
      uint32_t delay = uint32_t(frameTime-(t-time));
      Application::sleep(delay);
      t += delay;
      }
    fps.push(t-time);
    if(Gothic::inst().isBenchmarkMode() && Gothic::inst().world()!=nullptr && Gothic::inst().world()->currentCs()!=nullptr)
      benchmark.push(t-time);
    time = t;
    }
  catch(const Tempest::SwapchainSuboptimal&) {
    Log::e("swapchain is outdated - reset renderer");
    device.waitIdle();
    swapchain.reset();
    }
  }

double MainWindow::Fps::get() const {
  uint64_t sum=0,num=0;
  for(auto& i:dt)
    if(i>0) {
      sum+=i;
      num++;
      }
  if(num==0 || sum==0)
    return 60;
  uint64_t fps = (1000*100*num)/sum;
  return double(fps)/100.0;
  }

void MainWindow::Fps::push(uint64_t t) {
  for(size_t i=9;i>0;--i)
    dt[i]=dt[i-1];
  dt[0]=t;
  }

void MainWindow::BenchmarkData::push(uint64_t t) {
  fpsSum += t>0 ? (1000.0/double((t))) : 60.0;
  numFrames++;
  auto at = std::lower_bound(low1procent.begin(), low1procent.end(), t, std::greater<uint64_t>());
  low1procent.insert(at, t);
  low1procent.resize(std::min(low1procent.size(), (numFrames+99)/100));
  }

void MainWindow::BenchmarkData::clear() {
  low1procent.reserve(128);
  low1procent.clear();
  numFrames = 0;
  fpsSum = 0;
  }

#ifdef OPENGOTHIC_TOUCH_UI
static_assert(Mobile::CategoryCount==InventoryMenu::CatCount, "touch categories do not match the inventory");

namespace {
// Hotbar slots: melee weapon, ranged weapon and the magic slots
const Item* hotbarItem(Npc& pl, int index) {
  if(index==0)
    return pl.currentMeleeWeapon();
  if(index==1)
    return pl.currentRangedWeapon();
  if(auto* spl = pl.inventory().currentSpell(uint8_t(index-2)))
    return spl;
  return NbHotbar::item(pl,index);   // NB fix 110: an item put on the hotbar by the player
  }

void cycleSpell(Npc& pl) {
  constexpr int count = 8; // magic slots 3..10
  const int slot  = int(pl.inventory().currentSpellSlot());
  const int start = (3<=slot && slot<=10) ? slot-3 : -1;
  for(int n=1; n<=count; ++n) {
    const int next = (start+n)%count;
    if(pl.inventory().currentSpell(uint8_t(next))) {
      pl.drawMage(uint8_t(next+3));
      break;
      }
    }
  }
}

Mobile::Mode MainWindow::mobileContext() const {
  if(errorScreen.isActive())
    return Mobile::Mode::Menu;
  if(modMenu.isActive())
    return Mobile::Mode::Menu;
  if(Gothic::inst().checkLoading()!=Gothic::LoadState::Idle)
    return Mobile::Mode::Loading;
  if(video.isActive() || chapter.isActive() || document.isActive())
    return Mobile::Mode::Video;
  if(nbSales.isActive() || nbCraft.isActive())
    return Mobile::Mode::Menu;   // NB fix 68: the sales window - the controls of the game are hidden
  if(rootMenu.isActive())
    return rootMenu.isTouchMenu() ? Mobile::Mode::Menu : Mobile::Mode::Dialog;
  if(inventory.isOpen()==InventoryMenu::State::LockPicking)
    return Mobile::Mode::Lockpick;
  if(inventory.isActive() && !inventory.nbAutoLooting())   // NB fix 170: the quick loot keeps the controls of the game
    return Mobile::Mode::Inventory;
  if(dialogs.isActive())
    return Mobile::Mode::Dialog;
  return Gothic::inst().isInGame() ? Mobile::Mode::Game : Mobile::Mode::Loading;
  }

void MainWindow::setupMobileControls() {
  androidControls.update(*this,mobileContext());

  androidControls.look = [this](float dx, float dy) {
    auto camera = Gothic::inst().camera();
    if(!hasFocus() || mobileContext()!=Mobile::Mode::Game || camera==nullptr ||
       camera->isCutscene() || Gothic::inst().isPause())
      return;
    if(Gothic::inst().settingsGetI("GAME","camLookaroundInverse"))
      dy = -dy;
    camera->onRotateMouse(PointF(dy,-dx));
    player.onRotateMouse(-dx,-dy);
    };
  androidControls.menuTouch = [this](float x, float y, int phase) {
    if(modMenu.isActive())
      modMenu.mobileTouch(x,y,phase); else
    if(rootMenu.isActive())
      rootMenu.mobileTouch(x,y,phase); else
    if(inventory.isActive())
      inventory.mobileTouch(x,y,phase); else
    if(dialogs.isActive())
      dialogs.mobileTouch(x,y,phase);
    };
  androidControls.menuTap = [this](float x, float y) {
    if(errorScreen.isActive())
      errorScreen.dismiss(); else
    if(modMenu.isActive())
      modMenu.mobileTap(x,y); else
    if(rootMenu.isActive())
      rootMenu.mobileTap(x,y); else
    if(nbSales.isActive())
      nbSales.mobileTap(x,y); else
    if(nbCraft.isActive())
      nbCraft.mobileTap(x,y); else
    if(inventory.isActive())
      inventory.mobileTap(x,y); else
    if(dialogs.isActive())
      dialogs.mobileTap(x,y);
    };
  androidControls.menuScroll = [this](float dy) {
    if(errorScreen.isActive())
      errorScreen.scroll(dy>0 ? 2 : -2); else
    if(rootMenu.isActive())
      rootMenu.mobileScroll(dy); else
    if(nbSales.isActive())
      nbSales.mobileScroll(dy); else
    if(nbCraft.isActive())
      nbCraft.mobileScroll(dy); else
    if(inventory.isActive())
      inventory.mobileScroll(dy);
    };
  androidControls.action = [this](int id) {
    onMobileAction(id);
    };
  // NB fix 159: the quick loot (the key of NB on PC: KEY_QUICKLOOT) - a long press of the action button
  androidControls.lootFocus = [this]() {
    nbLogNearNpcs();     // NB fix 163
    if(!player.focus())
      nbLogNearMobs();   // NB fix 159: the diagnostics of the places that cannot be used (the treasures)
    return nbQuickLootTarget()!=0;
    };
  androidControls.quickLoot = [this]() { return nbQuickLoot(); };
  // NB fix 111: a slot of the wheel: the item is used; an empty slot opens the inventory (to put something there);
  // a long press on a slot takes its item off the wheel
  androidControls.wheelPick = [this](int slot, bool held) {
    auto* pl = Gothic::inst().player();
    if(pl==nullptr || pl->isDead())
      return;
    auto* it = NbWheel::item(*pl,slot);
    if(it==nullptr) {
      if(NbWheel::name(slot).empty())
        inventory.open(*pl);
      return;
      }
    if(held) {
      NbWheel::set(slot,"");
      return;
      }
    pl->useItem(it->clsId());
    };

  androidControls.loadIcons(atlas);
  inventory.setTouchSprites(androidControls.icon(AndroidInput::IconInvSlot),
                            androidControls.icon(AndroidInput::IconInvSlotSelected));
  androidControls.bind();
  }

uint64_t MainWindow::fastForwardDt(uint64_t dt) {
  // the fast forward exists only while a mod asks for it (touch.fastForward), else the game is vanilla
  if(!Scripting::ModHost::inst().fastForwardWanted())
    fastForward = false;
  if(!fastForward)
    return dt;
  auto* world  = Gothic::inst().world();
  auto* camera = Gothic::inst().camera();
  if(world==nullptr || camera==nullptr || mobileContext()!=Mobile::Mode::Game)
    return dt;
  if(auto* pl = Gothic::inst().player(); pl!=nullptr && pl->isDead()) {
    fastForward = false;
    return dt;
    }
  if(dialogs.isActive() || inventory.isActive() || document.isActive() ||
     camera->isCutscene() || world->isCutsceneLock() || Gothic::inst().isInDialog())
    return dt;
  const int speed = std::clamp(AndroidInput::preferences().speed,100,300);
  return dt*uint64_t(speed)/100;
  }

void MainWindow::onMobileAction(int id) {
  if(id==Mobile::Special::Mods) {
    modMenu.open();
    return;
    }

  const int category = id-Mobile::Special::FirstCategory;
  if(0<=category && category<Mobile::CategoryCount) {
    if(mobileContext()==Mobile::Mode::Inventory)
      inventory.setCategory(category);
    return;
    }

  if(id==Mobile::Special::QuickSave) {
    auto* pl = Gothic::inst().player();
    auto* w  = Gothic::inst().world();
    if(mobileContext()==Mobile::Mode::Game && pl!=nullptr && w!=nullptr && !pl->isDown() &&
       !dialogs.isActive() && w->currentCs()==nullptr && !w->script().dmaSaveDisabled()) {
      const int n = Gothic::inst().quickSave();
      Gothic::inst().onPrint("Quick save "+std::to_string(n));
      }
    return;
    }

  if(id==Mobile::Special::LoadSave) {
    // the player is dead: the newest save, whatever slot it is in
    auto* pl = Gothic::inst().player();
    if(pl!=nullptr && pl->isDead()) {
      const auto saves = SaveStore::list();
      if(saves.empty()) {
        Gothic::inst().onPrint("No save to load");
        } else {
        Gothic::inst().load(saves.front().file);
        }
      }
    return;
    }

  if(id==Mobile::Special::Speed) {
    if(Scripting::ModHost::inst().fastForwardWanted())
      fastForward = !fastForward;
    return;
    }
  if(id==Mobile::Special::Sprint) {
    if(mobileContext()==Mobile::Mode::Game)
      nbToggleSprint();
    return;
    }
  if(mobileContext()!=Mobile::Mode::Game)
    return;
  if(id==Mobile::Special::QuickAttack)
    player.quickAttack();   // NB fix 98
  // NB fix 131: the summoned creatures - the button opens and closes its choices; a choice closes them
  if(id==Mobile::Special::Summons)
    summonMenuOpen = !summonMenuOpen;
  if(id==Mobile::Special::SkipScene) {   // NB fix 143
    if(auto* w = Gothic::inst().world(); w!=nullptr && w->script().nbCutsceneActive())
      w->script().nbSkipCutscene();
    }
  if(id==Mobile::Special::SummonAttack || id==Mobile::Special::SummonMode || id==Mobile::Special::SummonReset) {
    auto* w  = Gothic::inst().world();
    auto* pl = Gothic::inst().player();
    if(w!=nullptr && pl!=nullptr) {
      auto& vm = w->script();
      if(id==Mobile::Special::SummonAttack) {
        Npc* enemy = player.focus().npc;
        if(enemy==nullptr)
          enemy = pl->target();
        if(enemy==nullptr || enemy==pl || enemy->isDead() || vm.nbIsSummon(*enemy))
          Gothic::inst().onPrint("Przyzwańce: brak celu"); else
          vm.nbSummonsAttack(*enemy);
        }
      if(id==Mobile::Special::SummonMode)
        vm.nbSummonsToggleMode();
      if(id==Mobile::Special::SummonReset)
        vm.nbResetSummons(true,true);
      }
    if(id!=Mobile::Special::SummonMode)
      summonMenuOpen = false;
    }
  if(id==Mobile::Special::Target)
    player.cycleTarget();
  if(id==Mobile::Special::Spell) {
    if(auto pl = Gothic::inst().player())
      cycleSpell(*pl);
    }
  }

/// The option of the settings: a weapon in the hand locks the opponent by itself, as the lock button does
void MainWindow::updateAutoLock() {
  auto* pl = Gothic::inst().player();
  const bool drawn = pl!=nullptr && pl->weaponState()!=WeaponState::NoWeapon;
  const bool fresh = drawn && !weaponWasDrawn;
  weaponWasDrawn   = drawn;
  if(!fresh || AndroidInput::preferences().autoLock==0)
    return;
  if(mobileContext()!=Mobile::Mode::Game || pl->isDown() || pl->interactive()!=nullptr)
    return;
  player.autoLock();
  }

/// Damage is felt: the stronger the hit, the longer and stronger the vibration
void MainWindow::updateHaptics() {
  auto pl = Gothic::inst().player();
  if(pl==nullptr) {
    lastHitpoints = -1;
    return;
    }

  const int hp = pl->attribute(ATR_HITPOINTS);
  if(lastHitpoints>0 && hp<lastHitpoints && AndroidInput::preferences().vibration!=0) {
    const int lost = lastHitpoints-hp;
    if(hp<=0)
      Haptics::vibrate(450,255); else
      Haptics::vibrate(std::clamp(30+lost*2,30,160),std::clamp(90+lost*5,90,255));
    }
  lastHitpoints = hp;
  }

void MainWindow::updateMobileControls() {
  updateHaptics();
  updateAutoLock();
  updateMobileAvailability();
  androidControls.inventoryLists(inventory.mobileTwoLists());
  androidControls.gui(Gothic::interfaceScale(this));
  if(androidControls.update(*this,mobileContext())) {
    clearInput();
    dMouse = Point();
    }
#if defined(__ANDROID__)
  // NB fix 133: on the phone every key of the movement comes from the touch controls (the stick, the buttons, a keyboard or a
  // pad through them). A move the controls no longer hold, but the hero still has, lost its release somewhere on the way
  // (a window took the key up) - the hero ran forward by himself. It is let go here
  if(mobileContext()==Mobile::Mode::Game) {
    const bool fwd  = !androidControls.held(Mobile::Key::Up);
    const bool back = !androidControls.held(Mobile::Key::Down);
    if(player.nbReleaseStuckMove(fwd,back))
      Log::i("touch: a movement key without its release was let go (",
             uiKeyUp==nullptr ? "no window" : "a window took the key up",")");
    // NB fix 134: the hero moves more than 2 m in a second while the stick and the arrows are not held - what moves him
    static Tempest::Vec3 lastPos;
    static uint64_t      lastAt = 0;
    auto* pl = Gothic::inst().player();
    auto* w  = Gothic::inst().world();
    if(pl!=nullptr && w!=nullptr && w->tickCount()>=lastAt+1000) {
      const auto pos   = pl->position();
      const bool still = fwd && back && !androidControls.held(Mobile::Key::StrafeLeft) &&
                         !androidControls.held(Mobile::Key::StrafeRight) && !androidControls.stickHeld();
      if(lastAt!=0 && still && (pos-lastPos).length()>200.f && !pl->isInAir() && w->currentCs()==nullptr)
        Log::i("[hero-ai] the hero moves by himself (",int((pos-lastPos).length()/100.f)," m/s): ",pl->nbMoveDebug(),
               " | ",player.nbInputDebug());
      lastPos = pos;
      lastAt  = w->tickCount();
      }
    }
#endif
  }

/// Controls of actions that are not possible at the moment are hidden or disabled
void MainWindow::updateMobileAvailability() {
  using Mobile::Key;
  using Mobile::keyMask;

  uint64_t allowed = keyMask(Key::Escape);
  const auto mode  = mobileContext();

  // a dead player: no button of the speed, a button that loads the newest save instead
  {
  auto pl = Gothic::inst().player();
  androidControls.playerDead(mode==Mobile::Mode::Game && pl!=nullptr && pl->isDead());
  // NB fix 40: the button of the sprint, when the scripts have it
  androidControls.sprint(nbSprintFn()!=nullptr);
  // NB fix 131: the button of the summoned creatures, while the hero has some (the wisp and the pets are not counted)
  {
  auto* w    = Gothic::inst().world();
  const bool have = mode==Mobile::Mode::Game && w!=nullptr && w->script().nbHasSummons();
  if(!have)
    summonMenuOpen = false;
  androidControls.summons(have,summonMenuOpen);
  // NB fix 143: the button "POMIŃ" while a scene of New Balance can be skipped
  androidControls.skipScene(mode==Mobile::Mode::Game && w!=nullptr && w->script().nbCutsceneActive());
  }
  }

  if(mode==Mobile::Mode::Game) {
    bool target = false, spells = false, movement = false, quick = false;
    if(auto pl = Gothic::inst().player()) {
      const auto camera = Gothic::inst().camera();
      const bool alive  = !pl->isDead() && !pl->isDown();
      const bool active = !Gothic::inst().isPause() && camera!=nullptr && !camera->isCutscene();
      const bool free   = alive && pl->interactive()==nullptr && active;
      const bool ground = free && !pl->isSwim() && !pl->isDive();

      const auto ws     = pl->weaponState();
      const bool armed  = ws!=WeaponState::NoWeapon;
      const bool ranged = ws==WeaponState::Bow || ws==WeaponState::CBow;
      const bool melee  = armed && !ranged && ws!=WeaponState::Mage;

      movement = alive && active;
      if(alive)
        allowed |= keyMask(Key::Status) | keyMask(Key::Log) | keyMask(Key::Map);
      if(alive && pl->isTransformed())
        allowed |= keyMask(Key::Enter);   // the only way back to the human form: the touch screen has no keyboard
      if(ground) {
        allowed |= keyMask(Key::Inventory) | keyMask(Key::Weapon) | keyMask(Key::Walk);
        if(pl->canSneak())
          allowed |= keyMask(Key::Sneak);

        const bool canAttack = armed && (!ranged || pl->hasAmmunition()) &&
                               (ws!=WeaponState::Mage || pl->attribute(ATR_MANA)>0);
        if(canAttack || (!armed && bool(player.focus())))
          allowed |= keyMask(Key::Action);
        if(melee)
          allowed |= keyMask(Key::AttackLeft) | keyMask(Key::AttackRight) | keyMask(Key::Block);
        if(armed) {
          allowed |= keyMask(Key::StrafeLeft) | keyMask(Key::StrafeRight);
          // the arrows forward and back with the side steps (the back one is the jump back of the fight)
          allowed |= keyMask(Key::Up) | keyMask(Key::Down);
          }

        target = armed;

        int spellCount = 0;
        for(int i=0; i<10; ++i) {
          if(hotbarItem(*pl,i)==nullptr)
            continue;
          allowed |= keyMask(Key(int(Key::One)+i));
          if(i>=2)
            ++spellCount;
          }
        spells = ws==WeaponState::Mage && spellCount>1;
        quick  = melee || (ws==WeaponState::Mage && pl->attribute(ATR_MANA)>0);   // NB fix 103
        }
      if(free && !pl->isInAir() && !pl->isJump())
        allowed |= keyMask(Key::Jump);
      if(alive && pl->interactive()!=nullptr)
        allowed |= keyMask(Key::Action);
      }
    androidControls.availability(allowed,target,spells,movement,quick);
    return;
    }

  if(mode==Mobile::Mode::Inventory) {
    if(inventory.hasSelectedItem()) {
      allowed |= keyMask(Key::Enter) | keyMask(Key::Weapon) |
                 keyMask(Key::Up) | keyMask(Key::Down) | keyMask(Key::Left) | keyMask(Key::Right);
      if(inventory.isOpen()!=InventoryMenu::State::Equip)
        allowed |= keyMask(Key::Jump);
      if(inventory.mobileCanHotbar())
        allowed |= keyMask(Key::Sneak) | keyMask(Key::Walk);   // NB fix 110, 111
      }
    androidControls.availability(allowed,false,false,false);
    }
  }

/// Prepares the touch HUD for rendering: status bars, hotbar models and the controls themselves
void MainWindow::updateMobileOverlay() {
  struct HotbarItem {
    const Item*  item;
    Mobile::Rect rect;
    };
  std::vector<HotbarItem> hotbar;
  std::string             stamp;

  auto& hud = androidControls.hud;
  hud = {};
  hud.inventoryCategory = inventory.category();
  hud.inventoryNew      = inventory.nbNewCategories();   // NB fix 121
  hud.inventoryTransfer = inventory.isOpen()!=InventoryMenu::State::Equip;
  hud.targetLocked = player.hasLock();
  hud.fastForward  = fastForward;
  hud.sprint       = nbSprintActive();
  if(auto* w = Gothic::inst().world())
    hud.summonPassive = w->script().nbSummonsPassive();   // NB fix 131
  if(mobileContext()==Mobile::Mode::Inventory)
    hud.useLabel = std::string(inventory.mobileActionLabel());

  const auto mode = mobileContext();
  auto       pl   = Gothic::inst().player();
  if((mode==Mobile::Mode::Game || mode==Mobile::Mode::Inventory) && !androidControls.settingsVisible() && pl!=nullptr) {
    const auto ws = pl->weaponState();
    if(ws==WeaponState::NoWeapon) {
      const auto focus = player.focus();
      hud.actionIcon = focus.npc!=nullptr         ? AndroidInput::IconTalk    :
                       focus.interactive!=nullptr ? AndroidInput::IconContext : AndroidInput::IconInteract;
      } else
    if(ws==WeaponState::Mage)
      hud.actionIcon = AndroidInput::IconCast; else
    if(ws==WeaponState::Bow || ws==WeaponState::CBow)
      hud.actionIcon = AndroidInput::IconShoot; else
      hud.actionIcon = AndroidInput::IconAttack;

    hud.health      = float(pl->attribute(ATR_HITPOINTS))/float(std::max(1,pl->attribute(ATR_HITPOINTSMAX)));
    hud.mana        = float(pl->attribute(ATR_MANA))/float(std::max(1,pl->attribute(ATR_MANAMAX)));
    hud.hp          = pl->attribute(ATR_HITPOINTS);
    hud.hpMax       = pl->attribute(ATR_HITPOINTSMAX);
    hud.mp          = pl->attribute(ATR_MANA);
    hud.mpMax       = pl->attribute(ATR_MANAMAX);
    hud.diving      = pl->isDive();
    hud.manaVisible = ws==WeaponState::Mage || pl->attribute(ATR_MANA)<pl->attribute(ATR_MANAMAX);

    const auto diveTime = float(pl->diveTimeLimit())/1000.f;
    hud.oxygen = diveTime>0 ? 1.f-float(pl->diveTime())/(1000.f*diveTime) : 0.f;

    for(const auto& b:androidControls.buttons()) {
      if(!b.visible || b.key<Mobile::Key::One || b.key>Mobile::Key::Zero)
        continue;
      const int   index = int(b.key)-int(Mobile::Key::One);
      const Item* item  = hotbarItem(*pl,index);
      if(item==nullptr)
        continue;
      if(item==pl->activeWeapon())
        hud.active = index;
      hotbar.push_back({item,b.rect});
      stamp += std::string(item->handle().visual)+":"+std::to_string(index)+":"+
               std::to_string(b.rect.x)+":"+std::to_string(b.rect.y)+":"+
               std::to_string(b.rect.w)+":"+std::to_string(b.rect.h)+";";
      }
    }

  // NB fix 111: the wheel of the quick choice - its models in the slots, the names and the numbers for the controls
  if(auto* pl = Gothic::inst().player(); pl!=nullptr && androidControls.wheelOpen()) {
    auto utf8 = [](std::string_view s) {
      // the names of the game are in cp1250 (the fonts of Gothic), the controls draw UTF-8
      static const uint16_t hi[128] = {
        0x20AC,0,0x201A,0,0x201E,0x2026,0x2020,0x2021,0,0x2030,0x0160,0x2039,0x015A,0x0164,0x017D,0x0179,
        0,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,0,0x2122,0x0161,0x203A,0x015B,0x0165,0x017E,0x017A,
        0x00A0,0x02C7,0x02D8,0x0141,0x00A4,0x0104,0x00A6,0x00A7,0x00A8,0x00A9,0x015E,0x00AB,0x00AC,0x00AD,0x00AE,0x017B,
        0x00B0,0x00B1,0x02DB,0x0142,0x00B4,0x00B5,0x00B6,0x00B7,0x00B8,0x0105,0x015F,0x00BB,0x013D,0x02DD,0x013E,0x017C,
        0x0154,0x00C1,0x00C2,0x0102,0x00C4,0x0139,0x0106,0x00C7,0x010C,0x00C9,0x0118,0x00CB,0x011A,0x00CD,0x00CE,0x010E,
        0x0110,0x0143,0x0147,0x00D3,0x00D4,0x0150,0x00D6,0x00D7,0x0158,0x016E,0x00DA,0x0170,0x00DC,0x00DD,0x0162,0x00DF,
        0x0155,0x00E1,0x00E2,0x0103,0x00E4,0x013A,0x0107,0x00E7,0x010D,0x00E9,0x0119,0x00EB,0x011B,0x00ED,0x00EE,0x010F,
        0x0111,0x0144,0x0148,0x00F3,0x00F4,0x0151,0x00F6,0x00F7,0x0159,0x016F,0x00FA,0x0171,0x00FC,0x00FD,0x0163,0x02D9};
      std::string out;
      for(char sc:s) {
        const auto     ch = uint8_t(sc);
        const uint32_t c  = ch<0x80 ? ch : hi[ch-0x80];
        if(c==0)
          continue;
        if(c<0x80) {
          out += char(c);
          } else
        if(c<0x800) {
          out += char(0xC0u|(c>>6));
          out += char(0x80u|(c&0x3Fu));
          } else {
          out += char(0xE0u|(c>>12));
          out += char(0x80u|((c>>6)&0x3Fu));
          out += char(0x80u|(c&0x3Fu));
          }
        }
      return out;
      };
    for(int k=0; k<NbWheel::Count; ++k) {
      auto* it = NbWheel::item(*pl,k);
      hud.wheelName [size_t(k)] = it!=nullptr ? utf8(it->description()) : std::string();
      hud.wheelCount[size_t(k)] = it!=nullptr ? int(it->count()) : 0;
      if(it==nullptr)
        continue;
      const auto r = androidControls.wheelSlot(k);
      hotbar.push_back({it,r});
      stamp += "w"+std::string(it->handle().visual)+":"+std::to_string(k)+";";
      }
    }

  // Models are cached, and rebuilt only when the hotbar changes
  if(stamp!=mobileItemStamp[cmdId]) {
    mobileItems[cmdId].reset();
    for(const auto& slot:hotbar) {
      const auto& r = slot.rect;
      mobileItems[cmdId].drawItem(int(r.x+r.w*0.12f),int(r.y+r.h*0.22f),int(r.w*0.76f),int(r.h*0.66f),*slot.item);
      }
    mobileItemStamp[cmdId] = std::move(stamp);
    }

  touchLayer.clear();
  {
    PaintEvent p(touchLayer,atlas,w(),h());
    androidControls.paint(p);
  }
  touchMesh[cmdId].update(device,touchLayer);
  }
#endif

#ifdef OPENGOTHIC_TOUCH_UI
// NB fix 159: what the long press of the action button can loot: 1 - a dead body, 2 - a plain chest, 0 - nothing
int MainWindow::nbQuickLootTarget() const {
  auto* pl = Gothic::inst().player();
  if(pl==nullptr || pl->isDown() || pl->weaponState()!=WeaponState::NoWeapon || inventory.isActive())
    return 0;
  const auto focus = player.focus();
  if(focus.npc!=nullptr && focus.npc->isDead())
    return 1;
  // NB fix 167: every chest that is not locked (the chests with a script or an owner too: they are opened as usual)
  if(focus.interactive!=nullptr && focus.interactive->isContainer() && !focus.interactive->isLocked())
    return 2;
  return 0;
  }

// NB fix 163 (diagnostics): why a character near is not in the focus
void MainWindow::nbLogNearNpcs() {
  auto* pl = Gothic::inst().player();
  auto* w  = Gothic::inst().world();
  if(pl==nullptr || w==nullptr)
    return;
  // the focus and the characters up to 5 m, at the action button (once in 3 s)
  static uint64_t nextNpcLog = 0;
  if(Application::tickCount()>=nextNpcLog) {
    nextNpcLog = Application::tickCount()+3000;
    const auto f = player.focus();
    Tempest::Log::i("[ctx] focus: npc=\"",f.npc!=nullptr ? f.npc->displayName() : std::string_view("-"),
                    "\" mob=",int(f.interactive!=nullptr)," item=",int(f.item!=nullptr));
    w->detectNpc(pl->position(),500.f,[&](Npc& n) {
      if(&n==pl)
        return;
      const auto  d   = n.position()-pl->position();
      const float ang = std::atan2(d.z,d.x);
      const float dif = std::abs(std::remainder(pl->rotationRad()-ang,2.f*float(M_PI)))*180.f/float(M_PI);
      Tempest::Log::i("[ctx] npc near: \"",n.displayName(),"\" dist=",int(d.length()),
                      " angle=",int(dif)," noFocus=",int(n.handle().no_focus)," dead=",int(n.isDead()),
                      " down=",int(n.isDown())," party=",n.handle().aivar[15]," see=",int(pl->canSeeNpc(n,true)),
                      " ws=",int(pl->weaponState())," pos=",int(n.position().x)," ",int(n.position().y)," ",int(n.position().z));
      });
    }
  }

// NB fix 159 (diagnostics): the action button without a focus - the interactive objects up to 5 m, once each
void MainWindow::nbLogNearMobs() {
  auto* pl = Gothic::inst().player();
  auto* w  = Gothic::inst().world();
  if(pl==nullptr || w==nullptr)
    return;
  static const World*        seenWorld = nullptr;
  static std::set<uint32_t>  seen;
  if(seenWorld!=w) {
    seenWorld = w;
    seen.clear();
    }
  size_t near = 0;
  for(uint32_t i=0; auto* m = w->mobsiById(i); ++i) {
    const float d = (m->position()-pl->position()).length();
    if(d>500.f)
      continue;
    ++near;
    if(!seen.insert(i).second)
      continue;
    Tempest::Log::i("[ctx] mob near (no focus): \"",m->tag(),"\" focus=\"",m->focusName(),"\" name=\"",m->displayName(),
                    "\" scheme=",m->schemeName()," state=",m->nbOnStateFunc()," dist=",int(d),
                    " container=",int(m->isContainer()));
    }
  if(near==0 && seen.insert(uint32_t(-1)).second)
    Tempest::Log::i("[ctx] mob near (no focus): none in 5 m");
  // NB fix 165 (diagnostics): the places of digging of NB (onStateFunc HIDDENFIND) in this world - where they are, once
  if(seen.insert(uint32_t(-2)).second) {
    int count = 0;
    for(uint32_t i=0; auto* m = w->mobsiById(i); ++i) {
      std::string st(m->nbOnStateFunc());
      for(auto& c:st)
        c = char(std::toupper(uint8_t(c)));
      if(st.find("HIDDEN")==std::string::npos && st.find("DIG")==std::string::npos && st.find("GRAVE")==std::string::npos)
        continue;
      ++count;
      const auto p = m->position();
      Tempest::Log::i("[ctx] dig place: \"",m->tag(),"\" state=",m->nbOnStateFunc()," scheme=",m->schemeName(),
                      " at ",int(p.x)," ",int(p.y)," ",int(p.z)," dist=",int((p-pl->position()).length()/100.f)," m");
      }
    const auto hp = pl->position();
    Tempest::Log::i("[ctx] dig places in this world: ",count," (the hero at ",int(hp.x)," ",int(hp.y)," ",int(hp.z),")");
    }
  // items near (the sparkles of a treasure could be an item or an effect)
  w->detectItem(pl->position(),500.f,[&](Item& it) {
    if(seen.insert(uint32_t(0x80000000u)+uint32_t(it.clsId())).second)
      Tempest::Log::i("[ctx] item near (no focus): ",it.displayName()," dist=",int((it.position()-pl->position()).length()));
    });
  }

bool MainWindow::nbQuickLoot() {
  auto* pl = Gothic::inst().player();
  auto* w  = Gothic::inst().world();
  const int kind = nbQuickLootTarget();
  if(pl==nullptr || w==nullptr || kind==0)
    return false;
  const auto& fnt = Resources::font(1.f);
  // as in NB: not with enemies around (RX_Warn_QuickLoot_EnemiesNearby)
  bool enemies = false;
  w->detectNpc(pl->position(),1500.f,[&](Npc& n) {
    if(&n!=pl && !n.isDown() && n.target()==pl && n.isAttack())
      enemies = true;
    });
  if(enemies) {
    Gothic::inst().onPrintScreen("Wrogowie w pobli\xBF" "u!",-1,70,2,fnt);
    return false;
    }

  const auto  focus = player.focus();
  if(kind==2) {
    // NB fix 167: a chest is opened as by a tap (its scripts, its owner, the animation), and the inventory takes all
    // of it and closes at once
    if(focus.interactive->inventory().iterator(Inventory::T_Inventory).isValid()) {
      inventory.nbArmAutoLoot();
      return true;
      }
    Gothic::inst().onPrintScreen("Nic tu nie ma",-1,70,2,fnt);
    return false;
    }
  const Inventory& inv   = kind==1 ? focus.npc->inventory() : focus.interactive->inventory();
  struct Taken { size_t cls; size_t count; std::string name; };
  std::vector<Taken> taken;
  for(auto it = inv.iterator(kind==1 ? Inventory::T_Ransack : Inventory::T_Inventory); it.isValid(); ++it)
    taken.push_back({it->clsId(),it.count(),std::string(it->displayName())});
  if(taken.empty()) {
    Gothic::inst().onPrintScreen("Nic tu nie ma",-1,70,2,fnt);
    return false;
    }
  for(auto& t:taken) {
    if(kind==1)
      pl->addItem(t.cls,*focus.npc,t.count); else
      pl->addItem(t.cls,*focus.interactive,t.count);
    }
  std::vector<std::pair<std::string,size_t>> lines;
  for(auto& t:taken)
    lines.emplace_back(t.name,t.count);
  InventoryMenu::nbPrintTaken(lines);
  return false;
  }
#endif
