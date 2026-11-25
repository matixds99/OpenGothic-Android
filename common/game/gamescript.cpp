#include "gamescript.h"
#include "utils/fileext.h"

#include <set>

#include <cmath>
#include <Tempest/Application>
#include "nboptions.h"
#include "nbiteminfo.h"
#include "nbbars.h"
#include "nbtexts.h"

#include <Tempest/Log>
#include <Tempest/SoundEffect>

#include <algorithm>
#include <cstdlib>
#include <array>
#include <cctype>
#include <map>
#include <type_traits>
#include <unordered_map>
#include <cstring>
#include <unordered_set>

#include "game/compatibility/directmemory.h"
#include "game/definitions/spelldefinitions.h"
#include "game/serialize.h"
#include "game/savesymbols.h"
#include "utils/string_frm.h"
#include "world/objects/npc.h"
#include "game/fightalgo.h"
#include "utils/gthfont.h"
#include "world/objects/item.h"
#include "world/objects/interactive.h"
#include "world/triggers/movetrigger.h"
#include "physics/physicvbo.h"
#include "world/focus.h"
#include "world/triggers/abstracttrigger.h"
#include "graphics/visualfx.h"
#include "utils/fileutil.h"
#include "commandline.h"
#include "gothic.h"
#include "game/dialogtest.h"
#include "gamemusic.h"
#include "resources.h"

using namespace Tempest;

namespace {

// Returning New Balance (NB): the RX_* externals come from its own DLL and are not documented,
// so every binding checks the signature declared in GOTHIC.DAT before it is registered.
// An external whose signature differs is left to the default handler of the VM (it pops the
// parameters and returns 0/""/null), so a wrong guess can never break the stack.
template<class T> struct NbIsSharedPtr : std::false_type {};
template<class I> struct NbIsSharedPtr<std::shared_ptr<I>> : std::true_type {};

template<class T>
constexpr zenkit::DaedalusDataType nbType() {
  using U = std::decay_t<T>;
  if constexpr(std::is_same_v<U,int32_t>)
    return zenkit::DaedalusDataType::INT;
  else if constexpr(std::is_same_v<U,float>)
    return zenkit::DaedalusDataType::FLOAT;
  else if constexpr(std::is_same_v<U,std::string_view> || std::is_same_v<U,std::string>)
    return zenkit::DaedalusDataType::STRING;
  else {
    static_assert(NbIsSharedPtr<U>::value, "unsupported type of an NB external");
    return zenkit::DaedalusDataType::INSTANCE;
    }
  }

bool nbTypeMatches(zenkit::DaedalusDataType want, zenkit::DaedalusDataType got) {
  if(want==got)
    return true;
  // "var func" is passed as an int (the index of the symbol), as in Info_AddChoice
  return want==zenkit::DaedalusDataType::INT && got==zenkit::DaedalusDataType::FUNCTION;
  }

template<class F> struct NbSig : NbSig<decltype(&F::operator())> {};
template<class C, class R, class... A>
struct NbSig<R(C::*)(A...) const> {
  static bool matches(zenkit::DaedalusVm& vm, zenkit::DaedalusSymbol& s) {
    const auto params = vm.find_parameters_for_function(&s);
    const std::array<zenkit::DaedalusDataType, sizeof...(A)> want = {nbType<A>()...};
    if(params.size()!=want.size())
      return false;
    for(size_t i=0; i<want.size(); ++i)
      if(!nbTypeMatches(want[i],params[i].type()))
        return false;
    if constexpr(std::is_void_v<R>)
      return !s.has_return();
    else
      return s.has_return() && nbTypeMatches(nbType<R>(),s.rtype());
    }
  };

const char* nbTypeName(zenkit::DaedalusDataType t) {
  switch(t) {
    case zenkit::DaedalusDataType::VOID:     return "void";
    case zenkit::DaedalusDataType::FLOAT:    return "float";
    case zenkit::DaedalusDataType::INT:      return "int";
    case zenkit::DaedalusDataType::STRING:   return "string";
    case zenkit::DaedalusDataType::FUNCTION: return "func";
    case zenkit::DaedalusDataType::INSTANCE: return "instance";
    default:                                 return "?";
    }
  }

std::string nbSignature(zenkit::DaedalusVm& vm, zenkit::DaedalusSymbol& s) {
  std::string sig = std::string(s.has_return() ? nbTypeName(s.rtype()) : "void") + " " + std::string(s.name()) + "(";
  bool first = true;
  for(auto& p : vm.find_parameters_for_function(&s)) {
    if(!first)
      sig += ", ";
    sig += nbTypeName(p.type());
    first = false;
    }
  return sig + ")";
  }

// the diagnostic of the scripts of NB itself (RX_DEBUG_RETURN_SHOW + CMD_COLOR): "NB trace" in the log.
// false, once the AI of NB works
constexpr bool kNbScriptTrace = true;

// a delayed function of RX_CALLBACK*: at the time, with SELF = the character (if there is one)
struct NbCallback {
  uint64_t                      at         = 0;
  uint32_t                      func       = 0;
  std::shared_ptr<zenkit::INpc> npc;
  bool                          waitDialog = false;
  };

/// NB fix 138: 0 iron, 1 gold, 2 magic ore, -1 not a vein (the function of the state of the mob)
int32_t nbOreKind(const Interactive& mob) {
  std::string f(mob.nbOnStateFunc());
  for(auto& c:f)
    c = char(std::toupper(uint8_t(c)));
  if(f.find("IRONHACKEN")!=std::string::npos)
    return 0;
  if(f.find("GOLDHACKEN")!=std::string::npos)
    return 1;
  if(f.find("OREHACKEN")!=std::string::npos)
    return 2;
  return -1;
  }

/// the ore in the name of the vob, -1 when the vein was not touched yet (the name is not a number, or too big)
int32_t nbOreParse(const Interactive& mob, int32_t kind) {
  const auto n = mob.nbVobName();
  if(n.empty() || n.size()>6)
    return -1;
  for(char c:n)
    if(c<'0' || c>'9')
      return -1;
  const int32_t v = std::atoi(std::string(n).c_str());
  if((kind==1 && v>81) || (kind!=1 && v>=150))
    return -1;
  return v;
  }

// the state of NB of one GameScript (kept here, so gamescript.h stays as it is)
struct NbState {
  std::vector<NbCallback>  callbacks;
  zenkit::DaedalusSymbol*  cycle           = nullptr;
  uint64_t                 lastCycleSecond = uint64_t(-1);
  // the loops that the Union plugin calls by the name (UNION_ABI.DLL):
  // a timer of 500 ms: RX_Main_CycleTrigger, at the next tick b_gettime, and again; PerFrame every frame
  zenkit::DaedalusSymbol*  getTime         = nullptr;
  zenkit::DaedalusSymbol*  perFrame        = nullptr;
  zenkit::DaedalusSymbol*  checkMenu       = nullptr;
  zenkit::DaedalusSymbol*  gamePaused      = nullptr; // GamePaused: the HUD of the scripts, the plugin calls it every frame
  uint64_t                 nextHud         = 0;       // NB fix 62: GamePaused and the effects, every 250 ms here
  uint64_t                 nextSprintCheck = 0;       // NB fix 173
  zenkit::DaedalusSymbol*  frameTime       = nullptr;
  int                      unionCycle      = 0;
  uint64_t                 nextCycle       = 0;   // the next tick of the timer of the plugin (NB fix 57)
  uint64_t                 nextMenuCheck   = 0;
  uint64_t                 nextMoveset     = 0;   // the timer of 100 ms of fix 30
  std::vector<std::weak_ptr<zenkit::INpc>> regen;  // RX_RestoreNpcHealthSlow (NB fix 50)
  uint64_t                 nextRegen       = 0;
  // NB fix 52: the log of the missing externals - from where, with what, how many times
  struct Missing {
    uint64_t    count = 0;
    std::string from;
    std::string args;
    };
  std::map<std::string,Missing,std::less<>>   missing;
  uint64_t                                    missingTotal    = 0;
  uint64_t                                    missingReported = 0;
  uint64_t                                    nextMissingLog  = 0;
  std::vector<std::pair<uint32_t,uint32_t>>   fnAddr;    // the code of the functions: address, symbol (sorted)
  std::unordered_map<std::string,uint32_t>    writeLogSeen; // RX_WriteLog: the same line, not without end
  // NB fix 56: the parts of the world that the scripts insert (RX_InsertVobTree): the world, the file, the objects.
  // The objects are never deleted with the world (the world may be gone before GameScript): only RX_RemoveVobTree
  // deletes them, while their world lives. They are in the save ("nb/vobtrees") and inserted again after a load
  struct VobTree {
    std::string                          world;
    std::string                          file;
    std::vector<std::unique_ptr<Vob>>*   vobs = nullptr;
    };
  std::vector<VobTree>                        vobTrees;
  // NB fix 61: the item that the wisp of Vatras flies to (AI_GotoItem of B_GetWispDetectedItem)
  std::weak_ptr<zenkit::INpc>                 wispNpc;
  std::weak_ptr<zenkit::IItem>                wispItem;
  uint64_t                                    wispSince = 0;
  const zenkit::IItem*                        wispFound = nullptr; // "Coś znaleziono!" once for an item (fix 62)
  float                                       wispBest  = 1e9f;    // the closest the wisp came to it (log, fix 61)
  uint64_t                                    nextWispSearch = 0;  // NB fix 61: the search of the plugin, every 500 ms
  uint64_t                                    nextWispLog    = 0;  // the state of the search in the log, every 10 s
  // NB fix 64: the texts of the wisp as in the plugin (video of the PC): "Szukanie..." / "Coś znaleziono!" on the
  // right, and the list "Ognik: name xN" of the brought items on the left (the last 7, 10 s each)
  uint64_t                                    wispSeen       = 0;  // the wisp was near the hero (for the texts)
  bool                                        wispReturn     = false; // NB fix 64: flying back to the hero
  uint64_t                                    nextWispFarLog = 0;     // NB fix 154
  // NB fix 68: the window of the plugin to sell given items (RX_Craft_PlayerSales_*): the list and the trader. The
  // window opens after the dialog is closed (the scripts call AI_StopProcessInfos before it)
  std::vector<Gothic::NbSaleItem>             salesList;
  std::weak_ptr<zenkit::INpc>                 salesNpc;
  bool                                        salesPending = false;
  // NB fix 70: the crafting window of the plugin (RX_OpenCraft), opened at the next tick out of the dialog
  int32_t                                     craftPending = -1;
  // NB fix 79: the mob in the focus of the player (Mob_GetBreakNum, Mob_SetBrakeChest), set every frame by the window,
  // and the key of the plugin that HandleEvent sees pressed (RX_KeyPressed) while the button of the lock is handled
  Interactive*                                focusMob = nullptr;
  std::string                                 keyPressed;
  // NB fix 149: the npc in the focus of the player (GetFocusNpc, Npc_ValidFocusNpc), set every frame by the window
  std::weak_ptr<zenkit::INpc>                 focusNpc;
  // NB fix 82: the music of the boss fight - the last time the fight went on (RX_StartBossMusic, RX_UpdateBossMusicCoords)
  uint64_t                                    bossMusicSeen = 0;
  bool                                        musicMuteReset = false;   // NB fix 116
  zenkit::IInfo*                              curInfo = nullptr;        // NB fix 124: the info of the choice that runs
  // NB fix 125: RX_ToggleVobVisible - "WORLD|NAME" -> shown (in the save-game, put again on every new world), and the
  // visuals of the items that Ext_PutInSlot hangs on the bones (by the instance)
  std::unordered_map<std::string,bool>        vobVisual;
  uint32_t                                    vobVisualWorld = 0;
  std::unordered_map<size_t,std::string>      slotVisual;
  // NB fix 126: the damage of the spells by the intelligence (RX_DamageCalcRunes); the definitions of the spells are made
  // again at the next tick when the values changed; the values are counted again after a load
  // NB fix 128: the trolls throw stones (UNION_ABI.DLL 0x103889b0 and 0x10389560): the throw that is under way and
  // the pause after it, for every troll
  struct TrollThrow {
    std::weak_ptr<zenkit::INpc> troll, target;
    uint64_t                    nextTry   = 0;   // no throw before (the pause of the plugin, 7.5-16 s)
    uint64_t                    releaseAt = 0;   // the stone leaves the hand (0 - no throw now)
    uint64_t                    nextLog   = 0;   // NB fix 130: why it does not throw, in the log every 5 s
    };
  std::unordered_map<const zenkit::INpc*,TrollThrow> trolls;
  uint64_t                                    trollNextLook = 0;
  uint64_t                                    summonNextCount = 0;   // NB fix 129
  uint32_t                                    followWorld = 0;       // NB fix 131: the world and the place of the hero
  uint64_t                                    summonCallCd = 0;      // NB fix 176: ms left until the summons can be called again
  bool                                        trfWas       = false;  // NB fix 181: the hero was transformed (the plugin 0x1056aec0)
  uint64_t                                    trfOpenMs    = 0;      // NB fix 181: HeroTransOpen without a transformation, ms
  float                                       blessAcc[3]  = {};     // NB fix 184: RX_AST_Blessing - the fractions of hp, stamina, mana
  uint64_t                                    monsterStatsNext = 0;  // NB fix 185: RX_AST_UpdateMonsterStats every second
  uint64_t                                    blinkNext = 0;         // NB fix 189: the teleports of the mages, every 0.5 s
  std::unordered_map<const Npc*,std::pair<int32_t,int32_t>> blinkCount; //   seen / not seen counters
  // NB fix 190: the teleport goes on during an animation of the mage (the special attack 11 of the plugin)
  struct BlinkJob {
    std::weak_ptr<zenkit::INpc> npc, target;
    Tempest::Vec3               dst;
    uint64_t                    teleAt = 0, endAt = 0;
    bool                        moved  = false;
    };
  std::vector<BlinkJob>                       blinkJobs;
  // NB fix 192: the special attacks of Im'Arah (UNION_ABI.DLL 0x1007fe60 chooses, 0x1006f630 / 0x10070050 / 0x10070860 do)
  struct ImarahStrike {
    uint64_t      startAt = 0, flashAt = 0, waveAt = 0, until = 0;
    Tempest::Vec3 pos;
    int32_t       state = 0;   // 0 waits, 1 the animation, 2 the flash is there, 3 done
    };
  struct ImarahJob {
    std::weak_ptr<zenkit::INpc> npc, target;
    int32_t                     attack = 0;
    int32_t                     state  = 0;
    uint64_t                    start = 0, fireAt = 0, endAt = 0, animEnd = 0, until = 0;
    bool                        red    = false;
    std::vector<ImarahStrike>   strikes;
    };
  std::vector<ImarahJob>                      imarahJobs;
  uint64_t                                    imarahNext  = 0;    // the choice once a second
  int32_t                                     imarahEvery = 0;    // the seconds between the attacks (0x105701a0)
  std::unordered_map<const Npc*,uint64_t>     summonBossLog;      // NB fix 193: a summon at a boss, in the log once
  std::unordered_map<const Npc*,uint64_t>     summonSwitch;       // NB fix 222: the last change of the target of a summon (nbSummonHitBy)
  // NB fix 132: the second enemy of a character, to turn to (RX_SetReAttackNpc) - the slot AIVRX_ReAttack_Slot keeps the key
  std::unordered_map<int32_t,std::weak_ptr<zenkit::INpc>> reattack;
  std::unordered_map<const Npc*,uint64_t>     bodyEmptySince;        // NB fix 136: a dead body found empty, since when
  int32_t                                     reattackNext = 1;
  Tempest::Vec3                               followPos;             //   in the frame before, for his teleports
  bool                                        spellsDirty   = false;
  uint32_t                                    spellsWorld   = 0;
  uint32_t                                    enterLocWorld = 0;   // NB fix 139: RX_OnEnterLoc_* called for this world
  uint64_t                                    cutsceneNextCheck = 0;   // NB fix 143
  uint64_t                                    npcRangeNext      = 0;   // NB fix 146
  bool                                        npcRangeOn        = false;
  bool                                        cutsceneActive    = false;
  uint64_t                                    spellsNextCheck = 0;
  uint64_t                                    spellsHash    = 0;
  // NB fix 100: the fights of the bosses of the plugin (UNION_ABI.DLL 0x10054220, 0x100546A0): the characters with
  // AI_OnTick_<instance> that fight, their timer goes every second; built again after a load from aivar[95]
  std::vector<std::weak_ptr<zenkit::INpc>>    bossFight;
  uint64_t                                    nextBossTick  = 0;
  bool                                        bossFightLoaded = false;
  // NB fix 106: the damage over time of the plugin (RX_ApplyDotDamage): the burning, the poison, the magic
  struct Dot {
    std::weak_ptr<zenkit::INpc> target, attacker;
    int32_t  ticks    = 0;
    int32_t  damage   = 0;
    int32_t  type     = 0;      // DAM_FIRE 8, DAM_MAGIC 32, DAM_POISON 5 of NB...
    uint64_t interval = 1000;
    uint64_t next     = 0;
    bool     notKill  = false;
    };
  std::vector<Dot>                            dots;
  struct DotLine {
    std::string text;
    int32_t     type  = 0;
    uint64_t    until = 0;
    };
  std::vector<DotLine>                        dotLines;   // on the left of the screen, 3 s each
  // NB fix 65: the loot of the dead (skill 10 of the wisp of the plugin): the body it flies to, the bodies done
  std::weak_ptr<zenkit::INpc>                 wispBody;
  // NB fix 175: by the object and not by the address - a new character (a new world, a new monster) at the address of an
  // old body was left alone by the wisp for 10 min
  std::map<std::weak_ptr<zenkit::INpc>,uint64_t,std::owner_less<std::weak_ptr<zenkit::INpc>>> wispBodySkip;
  // NB fix 177: since when a body with something to take is seen dead (the wisp waits 1 s: ZS_Dead finishes it first)
  std::map<std::weak_ptr<zenkit::INpc>,uint64_t,std::owner_less<std::weak_ptr<zenkit::INpc>>> wispBodySeen;
  std::vector<std::pair<std::string,uint64_t>> wispLines;          // the brought items: text, until
  std::map<std::weak_ptr<zenkit::IItem>,uint64_t,std::owner_less<std::weak_ptr<zenkit::IItem>>> wispSkip; // not reached: not again for 60 s
  float                                       wispBestH = 0;
  const World*                                vobWorld = nullptr;
  // NB fix 76: AST_GetManaBonus - the parts of a point of mana that are left from the last call (as the plugin)
  float                                       manaBonusRest = 0;
  // NB fix 76: RX_ContainsRoutine - the routine of each character from Npc_ExchangeRoutine ("RTN_NAME_ID", upper case).
  // Not in the save: after a load the start routine of the character (daily_routine) counts
  struct Routine {
    const zenkit::INpc* handle = nullptr;
    std::string         name;
    };
  std::unordered_map<const Npc*,Routine>      routines;
  // NB fix 157: the projectiles of the monsters (Wld_PlayEffect_Vec with bIsProjectile): the effect flies from the
  // caster to the target, the damage and the effect of the hit come at its arrival
  struct Shot {
    Effect                        fx;
    const VisualFx*               vfx    = nullptr;
    std::weak_ptr<zenkit::INpc>   src, dst;
    Tempest::Vec3                 pos;
    Tempest::Vec3                 dir;            // NB fix 158: a straight flight, aimed at the start
    float                         left   = 0.f;   // the rest of its way, cm
    float                         speed  = 1.f;   // cm per ms
    int32_t                       damage = 0;
    int32_t                       type   = 0;
    uint64_t                      until  = 0;
    };
  std::vector<Shot>                           shots;
  uint64_t                                    nextVobRepair = 0;   // NB fix 165
  // NB fix 221: StartChangeWorldTime(n, f) of the plugin - the time of the game slows down to f and comes back, n*10 steps of 100 ms
  uint64_t                                    slowStart     = 0;
  int32_t                                     slowSteps     = 0;
  float                                       slowFactor    = 1.f;
  float                                       slowApplied   = 1.f;
  uint64_t                                    nextAuraCheck = 0;   // NB fix 227
  uint64_t                                    nextSummonFlags = 0; // NB fix 228
  // NB fix 229: the watch of the hero - what he has and does, and the variables of the scripts that changed
  uint64_t                                    heroWatchNext = 0;
  std::string                                 heroGuild, heroWeapon, heroArmor, heroMelee, heroRange, heroSummons, heroTrans;
  std::string                                 heroAnim;
  uint64_t                                    heroAnimWindow = 0;
  int                                         heroAnimLines  = 0;
  const void*                                 varWorld = nullptr;
  uint64_t                                    varLearnUntil = 0;
  uint64_t                                    varNext = 0;
  uint64_t                                    varCntStart = 0;
  std::vector<uint32_t>                       varIdx;    // the symbol
  std::vector<uint16_t>                       varElem;   // the element of an array
  std::vector<int32_t>                        varLast;
  std::vector<uint8_t>                        varCnt;
  std::vector<uint8_t>                        varTotal;   // NB fix 230: the changes since the world was loaded
  std::vector<uint8_t>                        varUp;      // NB fix 232: the steps up one after another
  std::vector<uint8_t>                        varMute;
  const void*                                 aivOwner = nullptr;
  std::vector<std::string>                    aivName;
  std::vector<int32_t>                        aivLast;
  std::vector<uint8_t>                        aivCnt;
  std::vector<uint8_t>                        aivTotal;
  std::vector<uint8_t>                        aivMute;
  // NB fix 161: the companions go with the hero when he teleports (a jump of his position in the same world)
  const World*                                partyWorld    = nullptr;
  Tempest::Vec3                               partyHeroPos;
  uint64_t                                    nextPartyFocus = 0;   // NB fix 163
  };

static bool nbEqualsNoCase(std::string_view a, std::string_view b) {
  if(a.size()!=b.size())
    return false;
  for(size_t i=0; i<a.size(); ++i)
    if(std::toupper(static_cast<unsigned char>(a[i]))!=std::toupper(static_cast<unsigned char>(b[i])))
      return false;
  return true;
  }

/// NB fix 77: whether the symbol is an instance of an item (C_ITEM through its prototypes). is_instance_of<IItem>() of
/// ZenKit is true only when the instance was made already (an item that was in the world or in an inventory): the
/// recipes of new items (the soups, the compotes) were lost by it
static bool nbIsItemSymbol(zenkit::DaedalusVm& vm, zenkit::DaedalusSymbol* sym) {
  if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::INSTANCE)
    return false;
  if(sym->is_instance_of<zenkit::IItem>())
    return true;
  auto* p = sym;
  for(int depth=0; depth<8 && p!=nullptr; ++depth) {
    if(p->type()==zenkit::DaedalusDataType::CLASS)
      return p->name()=="C_ITEM";
    if(p->parent()==uint32_t(-1))
      return false;
    p = vm.find_symbol_by_index(p->parent());
    }
  return false;
  }

// "[ctx]" in the log: what the player did, so the errors around it have a place in the game (NB fix 52)
template<class... Args>
void nbCtx(const Args&... args) {
  Log::i("[ctx] ",args...);
  }

/// the function of the script with the code at the address pc (for the log)
std::string nbFunctionAt(zenkit::DaedalusVm& vm, NbState& st, uint32_t pc) {
  if(st.fnAddr.empty()) {
    for(uint32_t i=0; i<vm.symbols().size(); ++i) {
      auto* s = vm.find_symbol_by_index(i);
      if(s==nullptr || s->is_external() || s->is_member() || !s->is_const())
        continue;
      const auto t = s->type();
      if(t==zenkit::DaedalusDataType::FUNCTION || t==zenkit::DaedalusDataType::INSTANCE || t==zenkit::DaedalusDataType::PROTOTYPE)
        st.fnAddr.emplace_back(s->address(),i);
      }
    std::sort(st.fnAddr.begin(),st.fnAddr.end());
    }
  auto it = std::upper_bound(st.fnAddr.begin(),st.fnAddr.end(),std::make_pair(pc,uint32_t(-1)));
  if(it==st.fnAddr.begin())
    return "?";
  --it;
  auto* s = vm.find_symbol_by_index(it->second);
  return s!=nullptr ? s->name() : "?";
  }

/// a text of the plugin for the screen: "\n" of the scripts is a new line, the tags [POS=..] [COL=..] [/] [RED]... go away
std::string nbPlainText(std::string_view in) {
  std::string out;
  out.reserve(in.size());
  for(size_t i=0; i<in.size(); ++i) {
    if(in[i]=='\\' && i+1<in.size() && (in[i+1]=='n' || in[i+1]=='N')) {
      out += '\n';
      ++i;
      continue;
      }
    if(in[i]=='[') {
      const size_t e = in.find(']',i);
      if(e!=std::string_view::npos && e-i<=24) {
        const auto tag = in.substr(i+1,e-i-1);
        if(tag=="/" || tag.rfind("POS=",0)==0 || tag.rfind("COL=",0)==0 ||
           tag=="RED" || tag=="YELLOW" || tag=="GREEN" || tag=="WHITE" || tag=="BLUE") {
          i = e;
          continue;
          }
        }
      }
    out += in[i];
    }
  return out;
  }

/// an external the scripts call and nobody implements: the first call with the place and the arguments
void nbMissingCall(NbState& st, zenkit::DaedalusVm& vm, std::string_view name, std::string args) {
  ++st.missingTotal;
  auto it = st.missing.find(name);
  if(it!=st.missing.end()) {
    ++it->second.count;
    return;
    }
  NbState::Missing m;
  m.count = 1;
  m.from  = nbFunctionAt(vm,st,vm.pc());
  m.args  = std::move(args);
  Log::e("not implemented call [",name,"] from ",m.from," (",m.args,")");
  st.missing.emplace(std::string(name),std::move(m));
  }

/// how many times every missing external was called: the most used first
void nbMissingReport(NbState& st, const char* why) {
  if(st.missingTotal==st.missingReported)
    return;
  st.missingReported = st.missingTotal;
  std::vector<std::pair<uint64_t,const std::string*>> v;
  for(auto& [n,m]:st.missing)
    v.emplace_back(m.count,&n);
  std::sort(v.begin(),v.end(),[](const auto& a, const auto& b) { return a.first>b.first; });
  std::string line;
  for(size_t i=0; i<v.size() && i<25; ++i) {
    line += *v[i].second + " x" + std::to_string(v[i].first);
    if(i+1<v.size() && i+1<25)
      line += ", ";
    }
  Log::i("NB missing (",why,"): ",int(st.missing.size())," functions, ",int(st.missingTotal)," calls: ",line);
  }

/// the name of the file without the folders (VOBTREE/X.ZEN -> X.ZEN): the files of the game are found by the name
std::string nbFileName(std::string_view path) {
  const size_t p = path.find_last_of("/\\");
  std::string ret(p==std::string_view::npos ? path : path.substr(p+1));
  for(auto& c:ret)
    c = char(std::toupper(static_cast<unsigned char>(c)));
  return ret;
  }

/// a .ZEN with objects put into the world. Startup: the first time (the items and the contents of the chests come with
/// it); None: again after a load - the items are in the save already, they would be there twice
std::vector<std::unique_ptr<Vob>>* nbLoadVobTree(World& world, const std::string& file, Vob::Flags flags) {
  auto* tree = Resources::loadVobBundle(file);
  if(tree==nullptr || tree->empty()) {
    Log::e("NB: RX_InsertVobTree: the file \"",file,"\" is not in the game or is empty");
    return nullptr;
    }
  auto* ret = new std::vector<std::unique_ptr<Vob>>();
  for(auto& v:*tree)
    if(v!=nullptr)
      if(auto p = Vob::load(nullptr,world,*v,flags))
        ret->emplace_back(std::move(p));
  return ret;
  }

/// NB fix 66: the skills of the wisp, as the scripts keep them (the dialog of the wisp, RX_WispLearnLoot_*):
/// WISPSKILL0 the loot of the dead, 1 weapons, 2 potions and stone tablets, 3 food and plants, 4 armors, 5 magic
/// items (rings, amulets), 6 runes and scrolls, 7 documents (known from the start), 8 a larger area, 9 speed
bool nbWispSkill(zenkit::DaedalusVm& vm, int n) {
  auto* s = vm.find_symbol_by_name("WISPSKILL"+std::to_string(n));
  return s!=nullptr && s->type()==zenkit::DaedalusDataType::INT && !s->is_member() && s->get_int()!=0;
  }

/// the skill that the wisp needs for this item (-1: none - the other items: pouches, gold, ore, trophies, keys of
/// the lockpicks... are taken always, NB fix 67)
int nbWispSkillFor(int32_t mainFlag, std::string_view name) {
  if(name.find("STONEPLATE")!=std::string_view::npos || name.find("SKILLTABLET")!=std::string_view::npos)
    return 2;
  if(mainFlag & (2|4|8))      return 1;   // ITEM_KAT_NF, FF, MUN
  if(mainFlag & 128)          return 2;   // ITEM_KAT_POTIONS
  if(mainFlag & 32)           return 3;   // ITEM_KAT_FOOD (the plants too)
  if(mainFlag & 16)           return 4;   // ITEM_KAT_ARMOR
  if(mainFlag & int32_t(1u<<31)) return 5; // ITEM_KAT_MAGIC
  if(mainFlag & 512)          return 6;   // ITEM_KAT_RUNE (and the scrolls)
  if(mainFlag & 64)           return 7;   // ITEM_KAT_DOCS
  return -1;
  }

/// NB fix 62: the list of the effects with their time in the upper right corner (UNION_ABI.DLL 0x10254b20): lines
/// "(mm:ss) name" at x 5200 and y 40+200*n of the virtual 8192 of ZenGin. The names: the texts of the plugin
/// (AB_Translation.lbr) or the strings of GOTHIC.DAT (as the plugin). Not here: the vampire, Full SnC+ and the karma
/// skills, the slowing of time (the plugin keeps them in itself)
void nbBuffPanel(zenkit::DaedalusVm& vm, int32_t summonCallSec) {
  const auto var = [&vm](std::string_view name) -> int32_t {
    auto* s = vm.find_symbol_by_name(name);
    return s!=nullptr && s->type()==zenkit::DaedalusDataType::INT && !s->is_member() ? s->get_int() : 0;
    };
  const auto str = [&vm](std::string_view name) -> std::string {
    auto* s = vm.find_symbol_by_name(name);
    return s!=nullptr && s->type()==zenkit::DaedalusDataType::STRING && !s->is_member() ? std::string(s->get_string()) : std::string();
    };
  const auto mmss = [](int32_t sec) {
    sec = std::max(sec,0);
    char buf[32] = {};
    std::snprintf(buf,sizeof(buf),"(%02d:%02d) ",int(sec/60),int(sec%60));
    return std::string(buf);
    };

  if(Gothic::settingsGetI("AST","bShowBuffInfo")==0 || var("MOREINFOONSCREEN")==0)
    return;
  if(var("BHEROISINCUTSCENE")!=0 || var("HEROISDEAD")!=0 || Gothic::inst().isInDialog())
    return;

  std::vector<std::string> lines;
  const auto timed = [&](int32_t sec, std::string_view label) {
    if(!label.empty())
      lines.push_back(mmss(sec)+std::string(label));
    };

  // the scroll of a transformation
  if(var("HEROTRANS")!=0 && var("HEROTRANSSCROLL")!=0 && var("HEROTRANSCOUNT")<=var("TEMPTRANSTIME"))
    timed(var("TEMPTRANSTIME")-var("HEROTRANSCOUNT"),str("RX_SCROLLTRANSTIME"));

  // the timers of the potions and the blessings: the seconds left in the variable
  static const std::pair<const char*,const char*> timers[] = {
    {"TEMPPALLIGHTISON",         "sHolyLight"},
    {"RX_POTIONHOLY",            "sEffectResistCurse"},
    {"RX_SPEEDPOTION",           "sEffectAcceleration"},
    {"RX_INVISIBLEPOTION",       "sEffectInvisible"},
    {"RX_STAMINAFASTPOTION",     "sEffectStamina"},
    {"RX_POTIONMANAREG",         "sEffectWeakMana"},
    {"RX_POTIONMANAREG2",        "sEffectMiddleMana"},
    {"RX_POTIONMANAREG3",        "sEffectStrongMana"},
    {"RX_POTIONANTIPOISON",      "sEffectResistPoison"},
    {"RX_NP_RESIST",             "sEffectResist"},
    {"RX_MAGICPOWERTIMER",       "sEffectMagicPower"},
    {"RX_STAMINAFASTPOTION2",    "sEffectStamina2"},
    {"RX_SERAFISBONUS",          "sEffectSerafis"},
    {"RX_FOODHEALTHBONUS",       "sEffectHealthRegen"},
    {"BUFFSTONEGOLEMHEARTCOUNT", "sBuffStoneGolemHeartCount"},
    {"BUFFFIREGOLEMHEARTCOUNT",  "sBuffFireGolemHeartCount"},
    {"BUFFICEGOLEMHEARTCOUNT",   "sBuffIceGolemHeartCount"},
    {"BUFFSWAMPGOLEMHEARTCOUNT", "sBuffSwampGolemHeartCount"},
    {"RX_ISGAROTHREGEN",         "sNextIsgarothBonus"},
    {"RX_ISGAROTHREGENSTA",      "sNextIsgarothBonusSta"},
    {"RX_ISGAROTHMANA",          "sNextIsgarothBonusMana"},
    };
  for(auto& [v,key]:timers) {
    const int32_t sec = var(v);
    if(sec<=0)
      continue;
    if(std::string_view(v)=="RX_INVISIBLEPOTION" && sec==1)
      continue; // as the plugin
    timed(sec,NbTexts::get(key));
    }

  if(const int32_t shield = var("RX_MAGICSHIELDACTIVE"); shield>0)
    lines.push_back(std::string(NbTexts::get("sMagicShieldAbsorbeLeft"))+std::to_string(shield));

  // the curse of the Nightmare (less healing) in some worlds
  if(var("RX_NIGHTMARE")>0 && var("RX_NP_CURSERESISTACTIVE")==0) {
    const int32_t lvl = var("CURRENTLEVEL");
    if(lvl==3 || lvl==0x21 || lvl==0x11 || lvl==0x26 || lvl==0x23 || (lvl==9 && var("BLKDRAGNISDEAD")==0)) {
      if(auto s = str("RX_STR_LESSHEAL"); !s.empty())
        lines.push_back(s);
      }
    }
  if(var("RX_NEXTHITIGNORE")!=0)
    lines.push_back(std::string(NbTexts::get("sNextHitPotion")));
  // NB fix 176: the summons can be called to the hero again in (UNION_ABI.DLL 0x10256c48)
  if(summonCallSec>0)
    timed(summonCallSec,NbTexts::get("RX_SummonTeleportCD_String"));

  if(var("FIRESHIELDISUP")!=0)
    timed(3600-var("TEMPFIRESHIELDTICK"),str("RX_BUFFFIRESHIELD"));
  if(var("ACIDISON")!=0)
    timed((var("ACIDISSCROLL")!=0 ? 1800 : 3600)-var("TEMPACIDISON"),str("RX_TIMER_SEVERESTINK"));
  if(const int32_t sec = var("RX_POTIONRESISTINGWEAKNESS"); sec>0)
    timed(sec,str("RX_POTIONRESISTINGWEAKNESS_STR"));
  if(const int32_t sec = var("RX_POTIONRESISTINGTOSICKNESS"); sec>0)
    timed(sec,str("RX_POTIONRESISTINGTOSICKNESS_STR"));
  if(const int32_t type = var("JOINTBONUSTYPE"); type==2 || type==3) {
    const int32_t cnt = var("TEMPJOINTBONUSCOUNT");
    if(cnt<=600) {
      const char* s = var("WARTRANSE")!=0 ? "RX_INFOWARTRANSE" : var("FIGHTMEDITATION")!=0 ? "RX_INFOFIGHTMEDITATION" : "RX_INFOMAGICMEDITATION";
      timed(600-cnt,str(s));
      }
    }
  if(var("SC_ISOBSESSED")>0)
    if(auto s = str("RX_STR_OBSESSIONEFFECT"); !s.empty())
      lines.push_back(s);
  static const std::pair<const char*,const char*> auras[] = {
    {"RX_NECRORUNEAURAACTIVE",    "RX_STR_NECROAURA_ON"},
    {"RX_NECRORUNEAURAACTIVE_02", "RX_STR_NECROAURA_02_ON"},
    {"RX_NECRORUNEAURAACTIVE_03", "RX_STR_NECROAURA_03_ON"},
    {"RX_NECRORUNEAURAACTIVE_04", "RX_STR_NECROAURA_04_ON"},
    {"RX_NECRORUNEAURAACTIVE_05", "RX_STR_NECROAURA_05_ON"},
    };
  for(auto& [v,s]:auras)
    if(var(v)!=0)
      if(auto txt = str(s); !txt.empty())
        lines.push_back(txt);
  if(const int32_t sec = var("RX_SCOUT_SKILL_DAMAGEDIMTIME"); sec!=0)
    timed(sec,str("RX_STR_SCOUTDAMAGEDIM"));
  if(const int32_t sec = var("RX_ISSNEAKINVISTIMER"); sec!=0)
    timed(sec,str("RX_STR_SCOUTINVISRESTORE"));
  if(var("RX_ISSNEAKINVIS")!=0)
    timed(var("HEROINVISIBLETIMERNOW")-var("HEROINVISIBLETIMER"),str("RX_STR_SCOUTINVISACTIVE"));
  if(const int32_t ms = var("RX_LIGHTSPELL_TIME"); ms!=0)
    timed(ms/1000,str("RX_COMMON_SPELL_LIGHTISON"));
  if(const int32_t sec = var("RX_STAMINAHEALRUNEBONUS"); sec>0)
    timed(sec,str("RX_STR_HEALRUNEREGENSTAMINABONUS"));
  if(const int32_t sec = var("RX_NP_CURSERESISTACTIVE"); sec>0)
    timed(sec,str("RX_STR_LEGCURSEPLACERESIST"));
  if(var("HEROTRANS")==0 && var("RX_OLDSTAMINA")==0) {
    if(var("RX_STATUSCOLD")>0)
      if(auto s = str("RX_BUFFINFO_COLD"); !s.empty())
        lines.push_back(s);
    if(var("RX_STATUSHEAT")>0)
      if(auto s = str("RX_BUFFINFO_HEAT"); !s.empty())
        lines.push_back(s);
    }
  if(const int32_t ms = var("RX_ITPO_STAMINA_SEC"); ms>0)
    timed(ms/1000,str("RX_BUFFINFO_STAMINA_POTION"));
  if(var("RX_OLDSTAMINA")==0)
    if(const int32_t ms = var("RX_STAMINAZEROSLOW"); ms!=0)
      timed(ms/1000+1,str("RX_BUFFINFO_ZEROSLOW"));

  // PrintScreen of OpenGothic: x, y in % of the screen; the same place replaces the text, it is gone after 1 s.
  // The step: 3% (the plugin: 200/8192 = 2.4%, too little for the larger letters of a phone); from 6%: under the karma
  // of the scripts (x 90 and 95, y 1 and 3), which the longer lines would cover with the letters of a phone
  auto& fnt = Resources::font("FONT_OLD_10_WHITE.TGA",Resources::FontType::Normal,1.f);
  for(size_t i=0; i<lines.size() && i<30; ++i)
    Gothic::inst().onPrintScreen(lines[i],63,6+int(i)*3,1,fnt);
  }

/// another world or a loaded save: the parts of this world again (the old objects stay with the old world)
void nbVobTreesSync(NbState& st, World& world) {
  if(st.vobWorld==&world)
    return;
  st.vobWorld = &world;
  for(auto& t:st.vobTrees) {
    t.vobs = nullptr;
    if(t.world==world.name()) {
      t.vobs = nbLoadVobTree(world,t.file,Vob::None);
      Log::i("NB: the part of the world \"",t.file,"\" inserted again");
      }
    }
  }

std::unordered_map<const GameScript*, NbState> nbStates;

}


// NB: the hit chances and the fight skills of the NPC (B_SetFightSkills -> RX_ApplyProperOverlaysForNpc, fixes 23 and 30)
namespace NbMoveset {

  // B_SetFightSkills -> RX_ApplyProperOverlaysForNpc (UNION_ABI.DLL 0x10198B20), see the binding
  static void applyFightSkills(Npc& npc, int32_t percent) {
    auto& h = npc.handle();
    for(int t=TALENT_1H; t<=TALENT_CROSSBOW; ++t)
      h.hitchance[t] = std::clamp(h.hitchance[t]+percent, 0, 100);

    static const char* all[] = {
      "HUMANS_1HST1.MDS","HUMANS_1HST2.MDS","HUMANS_1HST3.MDS",
      "HUMANS_2HST1.MDS","HUMANS_2HST2.MDS","HUMANS_2HST3.MDS",
      "HUMANS_BOWT1.MDS","HUMANS_BOWT2.MDS","HUMANS_CBOWT1.MDS","HUMANS_CBOWT2.MDS",
      "HUMANS_2X2ST3.MDS",
      };
    for(auto name:all)
      if(auto sk = Resources::loadSkeleton(name))
        npc.delOverlay(sk);

    const auto apply = [&npc](Talent t, int lvl, const char* mds) {
      npc.setTalentSkill(t,lvl);
      if(mds!=nullptr)
        if(auto sk = Resources::loadSkeleton(mds))
          npc.addOverlay(sk,0);
      };
    const auto melee = [&](Talent t, const char* st1, const char* st2, const char* st3) {
      const int hc = h.hitchance[t];
      if(hc>=90)      apply(t,3,st3);
      else if(hc>=60) apply(t,2,st2);
      else if(hc>=30) apply(t,1,st1);
      else            apply(t,0,nullptr);
      };
    const auto range = [&](Talent t, const char* t1, const char* t2) {
      const int hc = h.hitchance[t];
      if(hc>=60)      apply(t,2,t2);
      else if(hc>=30) apply(t,1,t1);
      else            apply(t,0,nullptr);
      };
    melee(TALENT_1H,"HUMANS_1HST1.MDS","HUMANS_1HST2.MDS","HUMANS_1HST3.MDS");
    melee(TALENT_2H,"HUMANS_2HST1.MDS","HUMANS_2HST2.MDS","HUMANS_2HST3.MDS");
    range(TALENT_BOW,     "HUMANS_BOWT1.MDS", "HUMANS_BOWT2.MDS");
    range(TALENT_CROSSBOW,"HUMANS_CBOWT1.MDS","HUMANS_CBOWT2.MDS");
    }

  // NB fix 30: a save made before fix 23 keeps the hit chances 0 of every NPC (B_SetFightSkills runs only when the
  // character is made). The scripts wrote the percents also into AIVAR 84..87 (1H, 2H, bow, crossbow): take them.
  static void restoreFightSkills(Npc& npc) {
    auto& h = npc.handle();
    if(h.hitchance[TALENT_1H]!=0 || h.hitchance[TALENT_2H]!=0 || h.hitchance[TALENT_BOW]!=0 || h.hitchance[TALENT_CROSSBOW]!=0)
      return;
    if(h.aivar[84]<=0 && h.aivar[85]<=0 && h.aivar[86]<=0 && h.aivar[87]<=0)
      return;
    h.hitchance[TALENT_1H]       = std::clamp(h.aivar[84],0,100);
    h.hitchance[TALENT_2H]       = std::clamp(h.aivar[85],0,100);
    h.hitchance[TALENT_BOW]      = std::clamp(h.aivar[86],0,100);
    h.hitchance[TALENT_CROSSBOW] = std::clamp(h.aivar[87],0,100);
    applyFightSkills(npc,0);
    }
  static const char* const allOverlays[] = {
    "humans_1hST1.mds","humans_1hST2.mds","humans_1hST3.mds",
    "Shield_ST1.MDS","Shield_ST2.MDS","Shield_ST3.MDS","Shield_ST4.MDS","Hum_Shield2.MDS","Shield.MDS","PRE_Hum_Shield2.MDS",
    "Humans_Pirate.mds","Humans_Rapier_ST3.mds","Humans_Rapier_ST2.mds","Humans_Rapier_ST1.mds","PRE_START.mds",
    "humans_2hST3.mds","humans_2hST2.mds","humans_2hST1.mds","HUMANS_AXEST1.MDS","HUMANS_AXEST2.MDS",
    "PRE_START_2X2.MDS","HUM_2X2.MDS","Humans_2x2ST3.MDS","Humans_1x2ST3.MDS",
    "PRE_Humans_O2Hl2.mds","Humans_O2Hl2.mds",
    "PRE_Humans_Stafx.mds","PRE_Humans_Stafw.mds","PRE_Humans_Stafd.mds","PRE_Humans_Stafg.mds","PRE_Humans_Stafdemon.mds",
    "Humans_Stafx.mds","Humans_Stafw.mds","Humans_Stafd.mds","Humans_Stafg.mds","Humans_Stafdemon.mds",
    "PRE_Humans_Speer.mds","Humans_Speer.mds","Humans_SPST1.mds","Humans_SPST2.mds","HUMANS_2h_b_st1.mds",
    "HUMANS_BOSS03.MDS","HUMANS_BOSS032.MDS","HUMANS_1HDEXST1.MDS","HUMANS_1HDEXST2.MDS","HUMANS_1HDEXST3.MDS",
    };

  // an equipped shield of NB: ITEM_KAT_NF with ITEM_SHIELD (0x40000), not a weapon of one hand
  static bool hasShield(const Npc& npc) {
    for(auto it = npc.inventory().iterator(Inventory::T_Inventory); it.isValid(); ++it) {
      if(!it.isEquipped())
        continue;
      auto& h = it->handle();
      const uint32_t fl = uint32_t(h.flags);
      if(h.main_flag==2 /*ITEM_KAT_NF*/ && (fl & 0x40000)!=0 && (fl & 0x40003)!=0x40003)
        return true;
      }
    return false;
    }

  // 1024AA90: a style of the weapons of dexterity is replaced by the usual one: always for an axe, else 80 of 101
  static std::string dexFix(const Npc& npc, std::string name, uint32_t rnd) {
    static const char* dex[3] = {"HUMANS_1HDEXST1.MDS","HUMANS_1HDEXST2.MDS","HUMANS_1HDEXST3.MDS"};
    static const char* usu[3] = {"HUMANS_1HST1.MDS","HUMANS_1HST2.MDS","HUMANS_1HST3.MDS"};
    for(int i=0; i<3; ++i) {
      if(name!=dex[i])
        continue;
      auto* w   = const_cast<Npc&>(npc).currentMeleeWeapon();
      bool  axe = w!=nullptr && (uint32_t(w->handle().flags) & 0x8000)!=0;
      if(axe || (rnd%101)<=80)
        return usu[i];
      }
    return name;
    }

  static bool isRapier(Npc& npc) {     // NB: BIT_ITEM_PIERCE_DAMAGE (2) in C_ITEM.HP
    auto* w = npc.currentMeleeWeapon();
    return w!=nullptr && (w->handle().hp & 2)!=0;
    }
  static bool isAxe(Npc& npc) {        // ITEM_AXE
    auto* w = npc.currentMeleeWeapon();
    return w!=nullptr && (uint32_t(w->handle().flags) & 0x8000)!=0;
    }
  // the name of the instance contains one of the names (zSTRING::Search in the plugin)
  static bool nameHas(GameScript& sc, Npc& npc, std::initializer_list<const char*> names) {
    auto* sym = sc.findSymbol(npc.handle().symbol_index());
    if(sym==nullptr)
      return false;
    for(auto n:names)
      if(sym->name().find(n)!=std::string::npos)
        return true;
    return false;
    }
  static const char* pick(GameScript& sc, std::initializer_list<const char*> t) {
    return t.begin()[sc.rand(uint32_t(t.size()))];
    }

  // 1024AD00: bandits, one hand
  static std::string bdt1h(GameScript& sc, Npc& npc) {
    const bool sh = hasShield(npc);
    if(nameHas(sc,npc,{"BDT_1460_NESTOR","BDT_1093_ADDON_FRANCO","BDT_1083_ADDON_ESTEBAN"})) {
      if(isAxe(npc))
        return sh ? pick(sc,{"SHIELD_ST2.MDS","SHIELD_ST3.MDS","SHIELD_ST1.MDS"})
                  : pick(sc,{"HUMANS_1HST1.MDS","HUMANS_1HST2.MDS","HUMANS_1HST3.MDS"});
      if(sh)
        return pick(sc,{"SHIELD_ST2.MDS","SHIELD_ST3.MDS","SHIELD_ST4.MDS"});
      if(isRapier(npc))
        return pick(sc,{"HUMANS_RAPIER_ST1.MDS","HUMANS_RAPIER_ST2.MDS","HUMANS_RAPIER_ST2.MDS","HUMANS_RAPIER_ST3.MDS"});
      return pick(sc,{"HUMANS_1HST1.MDS","HUMANS_1HST3.MDS","HUMANS_1HST2.MDS"});
      }
    if(nameHas(sc,npc,{"BDT_1060_DEXTER","BDT_10620_BANDIT_L","BDT_10621_BANDIT_L","BDT_10622_BANDIT_L",
                       "BDT_10623_BANDIT_L","BDT_HAART"})) {
      if(sh)
        return pick(sc,{"SHIELD_ST1.MDS","SHIELD_ST3.MDS","SHIELD_ST2.MDS"});
      if(isRapier(npc))
        return pick(sc,{"HUMANS_RAPIER_ST1.MDS","HUMANS_RAPIER_ST2.MDS","Humans_Pirate.MDS"});
      return pick(sc,{"HUMANS_1HST3.MDS","HUMANS_1HST2.MDS","HUMANS_1HST1.MDS",
                      "HUMANS_1HDEXST3.MDS","HUMANS_1HDEXST2.MDS","HUMANS_1HDEXST1.MDS"});
      }
    if(npc.handle().hitchance[TALENT_1H]>=60) {
      if(sh)
        return pick(sc,{"SHIELD_ST2.MDS","SHIELD_ST1.MDS","SHIELD_ST2.MDS"});
      if(isRapier(npc))
        return pick(sc,{"HUMANS_RAPIER_ST2.MDS","HUMANS_RAPIER_ST3.MDS","HUMANS_RAPIER_ST2.MDS"});
      return pick(sc,{"HUMANS_1HST3.MDS","HUMANS_1HST2.MDS","HUMANS_1HDEXST2.MDS","HUMANS_1HDEXST3.MDS"});
      }
    if(sh)
      return pick(sc,{"SHIELD_ST2.MDS","SHIELD_ST1.MDS","SHIELD_ST2.MDS","SHIELD_ST3.MDS"});
    if(isRapier(npc))
      return pick(sc,{"HUMANS_RAPIER_ST1.MDS","Humans_Pirate.MDS","HUMANS_RAPIER_ST1.MDS"});
    return pick(sc,{"HUMANS_1HST1.MDS","HUMANS_1HST2.MDS","HUMANS_1HDEXST1.MDS","HUMANS_1HDEXST2.MDS"});
    }

  // 1024B810: bandits, two hands
  static std::string bdt2h(GameScript& sc, Npc& npc) {
    if(nameHas(sc,npc,{"TPL_GOR_BOBA"}))
      return "HUMANS_2h_b_st1.MDS";
    const int hc = npc.handle().hitchance[TALENT_2H];
    if(hc>=85)
      return pick(sc,{"HUMANS_2HST3.MDS","HUMANS_AXEST2.MDS","HUMANS_SPST2.MDS"});
    if(hc>=60)
      return pick(sc,{"HUMANS_2HST2.MDS","HUMANS_AXEST2.MDS","HUMANS_2HST1.MDS"});
    return pick(sc,{"HUMANS_2HST1.MDS","HUMANS_2HST2.MDS"});
    }

  // 1024BA00: mercenaries, one hand (no table by the hit chance in the plugin)
  static std::string sld1h(GameScript& sc, Npc& npc) {
    if(nameHas(sc,npc,{"AL_Boss3"}))
      return sc.rand(2)==1 ? "HUMANS_BOSS03.MDS" : "HUMANS_BOSS032.MDS";
    if(nameHas(sc,npc,{"SLD_814_SENTENZA"})) {
      if(isRapier(npc))
        return pick(sc,{"HUMANS_RAPIER_ST2.MDS","HUMANS_RAPIER_ST3.MDS","HUMANS_RAPIER_ST2.MDS","HUMANS_RAPIER_ST1.MDS"});
      return pick(sc,{"HUMANS_1HST3.MDS","HUMANS_1HST3.MDS","HUMANS_1HST2.MDS","HUMANS_1HST1.MDS",
                      "HUMANS_1HDEXST2.MDS","HUMANS_1HDEXST1.MDS"});
      }
    if(nameHas(sc,npc,{"SLD_802_BUSTER","SLD_816_FESTER","SLD_807_BULLCO","SLD_822_RAOUL"})) {
      if(isRapier(npc))
        return pick(sc,{"Humans_Pirate.MDS","HUMANS_RAPIER_ST1.MDS",""});
      return pick(sc,{"HUMANS_1HST1.MDS","HUMANS_1HST2.MDS","HUMANS_1HDEXST1.MDS"});
      }
    if(hasShield(npc))
      return pick(sc,{"SHIELD_ST2.MDS","SHIELD_ST1.MDS","SHIELD_ST1.MDS","SHIELD_ST2.MDS"});
    if(isRapier(npc))
      return pick(sc,{"Humans_Pirate.MDS","HUMANS_RAPIER_ST1.MDS","HUMANS_RAPIER_ST2.MDS"});
    return pick(sc,{"HUMANS_1HST1.MDS","HUMANS_1HST2.MDS","HUMANS_1HST2.MDS","HUMANS_1HDEXST1.MDS"});
    }
  // 1024C080: mercenaries, two hands
  static std::string sld2h(GameScript& sc, Npc&) {
    return sc.rand(2)==1 ? "HUMANS_AXEST2.MDS" : "HUMANS_2HST2.MDS";
    }

  // 1024C1D0: militia, one hand (two hands: the common table)
  static std::string mil1h(GameScript& sc, Npc& npc) {
    if(nameHas(sc,npc,{"MIL_317_RUGA","MIL_319_PABLO","MIL_324_PECK","MIL_316_WAMBO","MIL_3120_WULFGAR","MIL_312_WULFGAR"}))
      return pick(sc,{"HUMANS_1HST3.MDS","HUMANS_1HST1.MDS","HUMANS_1HST2.MDS",
                      "HUMANS_1HDEXST1.MDS","HUMANS_1HDEXST2.MDS","HUMANS_1HDEXST3.MDS"});
    return pick(sc,{"HUMANS_1HST1.MDS","HUMANS_1HST2.MDS","HUMANS_1HDEXST1.MDS","HUMANS_1HDEXST2.MDS"});
    }

  // 1024C720 / 1024C8D0: paladins
  static std::string pal1h(GameScript& sc, Npc& npc) {
    if(hasShield(npc))
      return pick(sc,{"SHIELD_ST2.MDS","SHIELD_ST3.MDS","SHIELD_ST3.MDS","SHIELD_ST4.MDS"});
    return pick(sc,{"HUMANS_1HST3.MDS","HUMANS_1HST2.MDS","HUMANS_1HDEXST3.MDS","HUMANS_1HDEXST2.MDS"});
    }
  static std::string pal2h(GameScript& sc, Npc&) {
    return pick(sc,{"HUMANS_2HST2.MDS","HUMANS_2HST3.MDS","HUMANS_SPST2.MDS"});
    }

  // 1024CA20 / 1024CBD0: dragon hunters
  static std::string djg1h(GameScript& sc, Npc& npc) {
    if(hasShield(npc))
      return pick(sc,{"SHIELD_ST2.MDS","SHIELD_ST3.MDS","SHIELD_ST4.MDS","SHIELD_ST3.MDS"});
    return pick(sc,{"HUMANS_1HST3.MDS","HUMANS_1HST2.MDS","HUMANS_1HDEXST2.MDS","HUMANS_1HDEXST3.MDS"});
    }
  static std::string djg2h(GameScript& sc, Npc&) {
    return sc.rand(2)==1 ? "HUMANS_2HST3.MDS" : "HUMANS_2HST2.MDS";
    }

  // 1024CD20: guild 16 (GIL_TPL in NB), two hands
  static std::string g16_2h(GameScript& sc, Npc&) {
    return pick(sc,{"HUMANS_SPST2.MDS","HUMANS_2HST3.MDS","HUMANS_2HST2.MDS"});
    }

  // 1024CE70 / 1024D300: pirates
  static std::string pir1h(GameScript& sc, Npc& npc) {
    if(nameHas(sc,npc,{"PIR_1352_Addon_AlligatorJack"}))
      return "HUMANS_1HST2.MDS";
    const int  hc = npc.handle().hitchance[TALENT_1H];
    const bool sh = hasShield(npc);
    if(hc>=90) {
      if(sh)
        return pick(sc,{"SHIELD_ST3.MDS","SHIELD_ST4.MDS","SHIELD_ST2.MDS","SHIELD_ST3.MDS"});
      if(isRapier(npc))
        return pick(sc,{"HUMANS_RAPIER_ST2.MDS","HUMANS_RAPIER_ST1.MDS","HUMANS_RAPIER_ST3.MDS","HUMANS_RAPIER_ST3.MDS"});
      return pick(sc,{"HUMANS_1HST2.MDS","HUMANS_1HST3.MDS","HUMANS_1HST3.MDS","HUMANS_1HST3.MDS",
                      "HUMANS_1HDEXST3.MDS","HUMANS_1HDEXST2.MDS"});
      }
    if(hc>=60) {
      if(sh)
        return pick(sc,{"SHIELD_ST1.MDS","SHIELD_ST2.MDS","SHIELD_ST1.MDS","SHIELD_ST2.MDS"});
      if(isRapier(npc))
        return pick(sc,{"HUMANS_RAPIER_ST2.MDS","HUMANS_RAPIER_ST1.MDS","HUMANS_PIRATE.MDS","HUMANS_RAPIER_ST2.MDS"});
      return pick(sc,{"HUMANS_1HST1.MDS","HUMANS_1HST2.MDS","HUMANS_1HST2.MDS","HUMANS_1HST2.MDS",
                      "HUMANS_1HDEXST1.MDS","HUMANS_1HDEXST2.MDS"});
      }
    if(sh)
      return sc.rand(2)==1 ? "SHIELD_ST2.MDS" : "SHIELD_ST1.MDS";
    if(isRapier(npc))
      return pick(sc,{"HUMANS_PIRATE.MDS","HUMANS_RAPIER_ST1.MDS","HUMANS_RAPIER_ST1.MDS"});
    return pick(sc,{"HUMANS_1HST1.MDS","HUMANS_1HST1.MDS","HUMANS_1HDEXST1.MDS"});
    }
  static std::string pir2h(GameScript& sc, Npc&) {
    return pick(sc,{"HUMANS_2HST1.MDS","HUMANS_2HST2.MDS","HUMANS_SPST2.MDS"});
    }

  // 1024D450 / 1024D6D0: outlaws (GIL_OUT)
  static std::string out1h(GameScript& sc, Npc& npc) {
    const int  hc = npc.handle().hitchance[TALENT_1H];
    const bool sh = hasShield(npc);
    if(hc>=90) {
      if(sh)
        return pick(sc,{"SHIELD_ST2.MDS","SHIELD_ST1.MDS","SHIELD_ST2.MDS","SHIELD_ST3.MDS"});
      return pick(sc,{"","HUMANS_1HST1.MDS","HUMANS_1HST2.MDS","HUMANS_1HST3.MDS",
                      "HUMANS_1HDEXST1.MDS","HUMANS_1HDEXST2.MDS","HUMANS_1HDEXST3.MDS"});
      }
    if(hc>=60) {
      if(sh)
        return pick(sc,{"SHIELD_ST2.MDS","SHIELD_ST3.MDS","SHIELD_ST2.MDS","SHIELD_ST1.MDS"});
      return pick(sc,{"","HUMANS_1HST1.MDS","HUMANS_1HST2.MDS","HUMANS_1HDEXST1.MDS","HUMANS_1HDEXST2.MDS"});
      }
    if(sh)
      return sc.rand(2)==1 ? "SHIELD_ST1.MDS" : "SHIELD_ST2.MDS";
    return pick(sc,{"","HUMANS_1HST1.MDS","HUMANS_1HDEXST1.MDS"});
    }
  static std::string out2h(GameScript& sc, Npc&) {
    return pick(sc,{"HUMANS_AXEST2.MDS","HUMANS_2HST1.MDS","HUMANS_2HST3.MDS"});
    }

  // the style of the guild; "" = no table done yet for this guild (the styles of the hit chance stay)
  static bool styleFor(GameScript& sc, Npc& npc, int guild, bool oneHand, std::string& out) {
    switch(guild) {
      case 9:  out = oneHand ? bdt1h(sc,npc) : bdt2h(sc,npc); return true;
      case 7:  out = oneHand ? sld1h(sc,npc) : sld2h(sc,npc); return true;
      case 2:  if(!oneHand) return false; out = mil1h(sc,npc); return true;
      case 1:  out = oneHand ? pal1h(sc,npc) : pal2h(sc,npc); return true;
      case 6:  out = oneHand ? djg1h(sc,npc) : djg2h(sc,npc); return true;
      case 16: if(oneHand) return false; out = g16_2h(sc,npc); return true;
      case 13: out = oneHand ? pir1h(sc,npc) : pir2h(sc,npc); return true;
      case 10: out = oneHand ? out1h(sc,npc) : out2h(sc,npc); return true;
      // GIL_SKELETON (36): long tables by the names of the skeletons - not done yet
      default: return false;
      }
    }

  // UNION_ABI.DLL 0x1024A460, see the comment at fix 34 in the CHANGELOG
  static void movesetTick(GameScript& sc, Npc& npc, int sepHum) {
    const int g = int(npc.guild());
    if(g>=sepHum && g!=36)
      return;
    const auto ws = npc.weaponState();
    const bool w1 = (ws==WeaponState::W1H), w2 = (ws==WeaponState::W2H);
    if(!w1 && !w2)
      return;
    Npc* enemy = npc.target();
    if(enemy==nullptr || enemy->isDown())
      return;
    auto& h = npc.handle();
    if(h.attribute[ATR_HITPOINTS] <= h.attribute[ATR_HITPOINTSMAX]/15)
      return;

    int changed = npc.newBalanceGetNpcVar(9);
    int count   = npc.newBalanceGetNpcVar(151);
    if(changed!=0 && count==0 && npc.qDistTo(*enemy)>=500.f*500.f) {
      changed = 0;
      npc.newBalanceSetNpcVar(9,0);
      }
    if(count>0)
      npc.newBalanceSetNpcVar(151,count-1);
    if(changed!=0)
      return;

    std::string style;
    if(!styleFor(sc,npc,g,w1,style))
      return;
    for(auto name:allOverlays)
      if(auto sk = Resources::loadSkeleton(name))
        npc.delOverlay(sk);
    if(w1)
      style = dexFix(npc,style,sc.rand(101));
    if(!style.empty())
      if(auto sk = Resources::loadSkeleton(style))
        npc.addOverlay(sk,0);
    npc.newBalanceSetNpcVar(9,1);
    npc.newBalanceSetNpcVar(151,40+int(sc.rand(41)));
    }

  // every 100 ms: the NPC near the hero (old saves: fix 30)
  static void tick(GameScript& sc, World& world, int sepHum) {
    auto* pl = world.player();
    if(pl==nullptr)
      return;
    world.detectNpc(pl->position(), 1500.f, [&sc,pl,sepHum](Npc& npc) {
      if(&npc==pl || npc.isPlayer() || npc.isDown())
        return;
      restoreFightSkills(npc);
      movesetTick(sc,npc,sepHum);
      });
    }

  }

template <typename T>
struct ScopeVar final {
  ScopeVar(zenkit::DaedalusSymbol& sym, const std::shared_ptr<T>& h) : prev(sym.get_instance()), sym(sym) {
    sym.set_instance(h);
    }

  ScopeVar(const ScopeVar&)=delete;
  ~ScopeVar(){
    sym.set_instance(prev);
    }

  std::shared_ptr<zenkit::DaedalusInstance> prev;
  zenkit::DaedalusSymbol&                   sym;
  };

struct ScopeCtx final {
  ScopeCtx(GameScript& script, NpcProcessPolicy pp) : script(script) {
    prev = script.aiProcessPolicy;
    script.aiProcessPolicy = pp;
    }

  ~ScopeCtx() {
    script.aiProcessPolicy = prev;
    }

  GameScript&      script;
  NpcProcessPolicy prev = NpcProcessPolicy::AiNormal;
  };


bool GameScript::GlobalOutput::output(Npc& npc, std::string_view text) {
  return owner.aiOutput(npc,text,false);
  }

bool GameScript::GlobalOutput::outputSvm(Npc &npc, std::string_view text) {
  return owner.aiOutputSvm(npc,text,false);
  }

bool GameScript::GlobalOutput::outputOv(Npc &npc, std::string_view text) {
  return owner.aiOutputSvm(npc,text,true);
  }

bool GameScript::GlobalOutput::printScr(Npc& npc, int time, std::string_view msg, int x, int y, std::string_view font) {
  auto& f = Resources::font(font, Resources::FontType::Normal, 1.0);
  Gothic::inst().onPrintScreen(msg,x,y,time,f);
  return true;
  }

bool GameScript::GlobalOutput::isFinished() {
  return true;
  }

GameScript::PerDist::PerDist() {
  for(auto& i:range)
    i = -1;
  }

int GameScript::PerDist::at(PercType perc, int r) const {
  if(perc>=PERC_Count)
    return r;
  auto rr = range[perc];
  if(rr>0)
    return rr;
  return r;
  }


GameScript::GameScript(GameSession &owner)
    :owner(owner), vm(createVm(Gothic::inst())) {
  if (vm.global_self() == nullptr || vm.global_other() == nullptr || vm.global_item() == nullptr ||
      vm.global_victim() == nullptr || vm.global_hero() == nullptr)
    throw std::runtime_error("Cannot find script symbol SELF, OTHER, ITEM, VICTIM, or HERO! Cannot proceed!");

  vmLang = Gothic::inst().settingsGetI("GAME", "language");
  vm.register_exception_handler([](zenkit::DaedalusVm& v, const zenkit::DaedalusScriptError& exc, const zenkit::DaedalusInstruction& instr) {
    const auto ret = zenkit::lenient_vm_exception_handler(v,exc,instr);
    // where the script was: the first times that every error happens, so a log is not drowned
    static std::unordered_map<std::string,int> seen;
    if(seen.size()<200 && ++seen[exc.what()]<=2)
      v.print_stack_trace();
    return ret;
    });
  Gothic::inst().setupCommonScriptClasses(vm);
  Gothic::inst().setupVmCommonApi(vm);
  aiDefaultPipe.reset(new GlobalOutput(*this));
  initCommon();
  SaveSymbols::scriptsReport(vm);   // NB fix 212: an update of the scripts since the last start
  initSettings();
  Gothic::inst().onSettingsChanged.bind(this,&GameScript::initSettings);
  }

std::string GameScript::dmaReport() {
  return dma==nullptr ? std::string() : dma->report();
  }

void GameScript::dmaChangeLevel(bool begin) {
  if(dma!=nullptr) {
    if(begin)
      dma->clearMobs();
    dma->eventChangeLevel(begin);
    }
  }

void GameScript::dmaSave(Serialize& fout) {
  if(dma!=nullptr)
    dma->eventSaveGame(fout);
  }

void GameScript::dmaBeforeSave() {
  if(dma!=nullptr)
    dma->eventBeforeSaveGame();
  }

auto GameScript::dmaSaveWorldName(std::string_view fallback) -> std::string {
  return dma==nullptr ? std::string(fallback) : dma->saveWorldName(fallback);
  }

auto GameScript::dmaLoadingTexture() -> std::string {
  return dma==nullptr ? std::string() : dma->loadingTexture();
  }

void GameScript::dmaLoad(Serialize& fin) {
  if(dma!=nullptr)
    dma->eventLoadGame(fin);
  }

void GameScript::dmaItemTaken(Npc& owner, Item& item) {
  if(dma!=nullptr)
    dma->eventItemTaken(owner,item);
  }

int GameScript::dmaChangeAttribute(Npc& npc, int attribute, int change) {
  return dma==nullptr ? change : dma->eventChangeAttribute(npc,attribute,change);
  }

void GameScript::dmaKey(int key) {
  if(dma!=nullptr)
    dma->eventKey(key);
  }

void GameScript::dmaKeyState(int key, bool down) {
  if(dma!=nullptr)
    dma->eventKeyState(key,down);
  }

void GameScript::dmaZoneMusic(std::string_view name) {
  if(dma!=nullptr)
    dma->eventZoneMusic(name);
  }

void GameScript::dmaMobCanOpen(Interactive& mob) {
  if(dma!=nullptr)
    dma->eventMobCanOpen(mob);
  }

void GameScript::dmaInventoryOpen(Npc& npc) {
  if(dma!=nullptr)
    dma->eventInventoryOpen(npc);
  }

void GameScript::dmaItemInfo(const Item& item) {
  // NB: the description of weapons/armors is made by the Union plugin - see nbiteminfo.h
  NbItemInfo::apply(vm,const_cast<Item&>(item).handle());
  if(dma!=nullptr)
    dma->eventItemInfo(item);
  }

void GameScript::dmaDialogChoice(const DlgChoice* choice, bool subChoice, int selection) {
  if(dma!=nullptr) {
    ScopeVar self(*vm.global_self(),vm.global_self()->get_instance());
    ScopeVar other(*vm.global_other(),vm.global_other()->get_instance());
    ScopeVar item(*vm.global_item(),vm.global_item()->get_instance());
    dma->eventDialogChoice(choice,subChoice,selection);
    }
  }

bool GameScript::dmaTakeInventoryClose() {
  return dma!=nullptr && dma->takeInventoryClose();
  }

bool GameScript::dmaInFinalBoards() const {
  return dma!=nullptr && dma->inFinalBoards();
  }

bool GameScript::dmaHidesPartyFocus() const {
  return dma!=nullptr && dma->hidesPartyFocus();
  }

void GameScript::dmaSpellBegin(Npc& caster) {
  if(dma!=nullptr)
    dma->eventSpellBegin(caster);
  }

bool GameScript::dmaSpellKeepsItem(int spellId) {
  return dma!=nullptr && dma->spellKeepsItem(spellId);
  }

const std::vector<std::string>& GameScript::inventoryCategoryNames() {
  if(!invCategoryNamesInit) {
    invCategoryNamesInit = true;
    auto* sym = vm.find_symbol_by_name("TXT_INV_CAT");
    if(sym!=nullptr && sym->type()==zenkit::DaedalusDataType::STRING)
      for(uint32_t i=0; i<sym->count(); ++i)
        invCategoryNames.push_back(sym->get_string(uint16_t(i)));
    }
  return invCategoryNames;
  }

uint32_t GameScript::inventoryCategoryMask(int index) const {
  auto* groups = vm.find_symbol_by_name("INV_CAT_GROUPS");
  if(groups==nullptr || groups->type()!=zenkit::DaedalusDataType::INT || index<0 || uint32_t(index)>=groups->count())
    return 0;
  return uint32_t(groups->get_int(uint16_t(index)));
  }

int GameScript::inventoryScriptCategory() const {
  auto* category = vm.find_symbol_by_name("CATINV_ACTIVECATEGORY");
  return category!=nullptr && category->type()==zenkit::DaedalusDataType::INT ? category->get_int() : -1;
  }

void GameScript::setInventoryScriptCategory(int index) {
  auto* category = vm.find_symbol_by_name("CATINV_ACTIVECATEGORY");
  auto* groups = vm.find_symbol_by_name("INV_CAT_GROUPS");
  if(category==nullptr || groups==nullptr || groups->count()==0 ||
     category->type()!=zenkit::DaedalusDataType::INT || groups->type()!=zenkit::DaedalusDataType::INT)
    return;
  auto* g1 = vm.find_symbol_by_name("CATINV_G1MODE");
  const int first = g1!=nullptr && g1->get_int()!=0 ? std::min(1,int(groups->count())-1) : 0;
  category->set_int(std::clamp(index,first,int(groups->count())-1));
  }

size_t GameScript::dmaPickLockLength(Interactive& mob, size_t length) {
  return dma==nullptr ? length : dma->eventPickLockLength(mob,length);
  }

bool GameScript::dmaFinishingMove(Npc& npc, Npc& target) {
  return dma!=nullptr && dma->eventFinishingMove(npc,target);
  }

bool GameScript::dmaSellItem(Npc& npc, Item& item) {
  return dma!=nullptr && dma->eventSellItem(npc,item);
  }

std::string GameScript::dmaLastCallOther() const {
  return dma!=nullptr ? dma->lastCallOther() : std::string();
  }

bool GameScript::dmaInCamera() const {
  return dma!=nullptr && dma->inCamera();
  }

bool GameScript::dmaCutsceneMenuBlocked() const {
  return dma!=nullptr && dma->cutsceneMenuBlocked();
  }

bool GameScript::dmaCutsceneStatusHidden() const {
  return dma!=nullptr && dma->cutsceneStatusHidden();
  }

void GameScript::dmaDialogMessage(std::string& name, uint32_t& color, bool& bottom, bool playerLine) {
  if(dma!=nullptr)
    dma->eventDialogMessage(name,color,bottom,playerLine);
  }

bool GameScript::dmaSaveDisabled() const {
  return dma!=nullptr && dma->saveDisabled();
  }

bool GameScript::dmaLoadDisabled() const {
  return dma!=nullptr && dma->loadDisabled();
  }

bool GameScript::dmaDialogCameraOff() const {
  return dma!=nullptr && dma->dialogCameraDisabled();
  }

bool GameScript::dmaTrialogFinishPending() const {
  return dma!=nullptr && dma->trialogFinishPending();
  }

bool GameScript::dmaUnderwaterPickup() const {
  return dma!=nullptr && dma->underwaterPickup();
  }

bool GameScript::dmaDropUnconscious(Npc& npc) {
  return dma!=nullptr && dma->eventDropUnconscious(npc);
  }

void GameScript::dmaEquipItem(Npc& npc, Item& item, bool equip) {
  if(dma!=nullptr)
    dma->eventEquipItem(npc,item,equip);
  }

bool GameScript::dmaEquipBestWeapons(Npc& npc) {
  return dma!=nullptr && dma->eventEquipBestWeapons(npc);
  }

bool GameScript::dmaEnemyCandidate(Npc& npc, Npc& candidate) {
  return dma==nullptr || dma->eventEnemyCandidate(npc,candidate);
  }

bool GameScript::dmaDropVob(Npc& npc, Item& item) {
  return dma!=nullptr && dma->eventDropVob(npc,item);
  }

void GameScript::dmaCsCamera(bool trigger) {
  if(dma!=nullptr)
    dma->eventCsCamera(trigger);
  }

void GameScript::dmaStatusScreen(bool show) {
  if(dma!=nullptr)
    dma->eventStatusScreen(show);
  }

auto GameScript::dmaTakeMenuTexts() -> std::vector<std::pair<std::string,std::string>> {
  return dma==nullptr ? std::vector<std::pair<std::string,std::string>>() : dma->takeMenuTexts();
  }

void GameScript::dmaFocus(Npc& player, const Focus& focus) {
  if(dma!=nullptr)
    dma->eventFocus(player,focus);
  }

void GameScript::dmaSpellFocus(Npc& caster, int spellId) {
  if(dma!=nullptr)
    dma->eventSpellFocus(caster,spellId);
  }

bool GameScript::dmaSpellTarget(Interactive& mob, TargetType type) {
  return dma!=nullptr && dma->eventSpellTarget(mob,type);
  }

/// The focus of the magic: the mods of Ikarus change it while the game runs (the lock picking spell of CoM)
const zenkit::IFocus& GameScript::focusMage() const {
  if(dma!=nullptr)
    dma->syncFocusMage(const_cast<zenkit::IFocus&>(cFocusMage));
  return cFocusMage;
  }

void GameScript::dmaOverlay(std::vector<DmaOverlayItem>& out, int width, int height) {
  if(dma!=nullptr)
    dma->collectOverlay(out,width,height);
  }

auto GameScript::dmaDamageBegin(Npc& victim, Npc& attacker, bool isSpell, int spellId, bool ranged, bool dontKill,
                               const DamageCalculator::Damage& damage) -> DmaDamage {
  DmaDamage ret;
  ret.dontKill = dontKill;
  ret.damage = damage;
  if(dma!=nullptr)
    dma->eventDamageBegin(victim,attacker,isSpell,spellId,ranged,ret);
  return ret;
  }

void GameScript::dmaDamageEnd(Npc& victim, DmaDamage& d) {
  if(dma!=nullptr)
    dma->eventDamageEnd(victim,d);
  }

void GameScript::dmaFallDamage(Npc& npc) {
  if(dma!=nullptr)
    dma->eventFallDamage(npc);
  }

int32_t GameScript::guildSeparatorHum() const {
  if(sepHum<0) {
    const bool g2  = owner.version().game==2;
    auto*      sym = const_cast<zenkit::DaedalusVm&>(vm).find_symbol_by_name("GIL_SEPERATOR_HUM");
    sepHum = sym!=nullptr && sym->type()==zenkit::DaedalusDataType::INT && sym->get_int()>0 ? sym->get_int()
                                                                                          : int32_t(g2 ? GIL_SEPERATOR_HUM : GIL_G1_SEPERATOR_HUM);
    }
  return sepHum;
  }

int32_t GameScript::guildSeparatorOrc() const {
  if(sepOrc<0) {
    const bool g2  = owner.version().game==2;
    auto*      sym = const_cast<zenkit::DaedalusVm&>(vm).find_symbol_by_name("GIL_SEPERATOR_ORC");
    sepOrc = sym!=nullptr && sym->type()==zenkit::DaedalusDataType::INT && sym->get_int()>0 ? sym->get_int()
                                                                                          : int32_t(g2 ? GIL_SEPERATOR_ORC : GIL_G1_SEPERATOR_ORC);
    }
  return sepOrc;
  }

GameScript::~GameScript() {
  nbStates.erase(this);
  Gothic::inst().onSettingsChanged.ubind(this,&GameScript::initSettings);
  }

void GameScript::initCommon() {
  // Returning New Balance: the externals of its DLL (UNION_ABI.DLL), with the signatures of its GOTHIC.DAT.
  // The others stay in the default handler of the VM (0, "" or null), which the log lists at the start.
  if(vm.find_symbol_by_name("RX_ISSPACETNET")!=nullptr) {
    auto& nbState = nbStates[this];
    nbState.cycle = vm.find_symbol_by_name("RX_MAIN_CYCLETRIGGER");
    if(nbState.cycle!=nullptr && nbState.cycle->type()!=zenkit::DaedalusDataType::FUNCTION)
      nbState.cycle = nullptr;
    const auto nbFunc = [this](std::string_view name) -> zenkit::DaedalusSymbol* {
      auto* f = vm.find_symbol_by_name(name);
      return f!=nullptr && f->type()==zenkit::DaedalusDataType::FUNCTION && !f->is_external() &&
             vm.find_parameters_for_function(f).empty() ? f : nullptr;
      };
    nbState.getTime   = nbFunc("B_GETTIME");
    nbState.perFrame  = nbFunc("PERFRAME");
    nbState.checkMenu = nbFunc("B_CHECKMENUOPTION");
    nbState.gamePaused = nbFunc("GAMEPAUSED");
    if(auto* ft = vm.find_symbol_by_name("RX_FRAMETIME"); ft!=nullptr && ft->type()==zenkit::DaedalusDataType::INT && !ft->is_member())
      nbState.frameTime = ft;
    // NB fix 52: a missing external goes to the log with the function of the script that calls it and the
    // arguments of the first call; the numbers of the calls come every 5 minutes and at the save ("NB missing")
    vm.register_default_external_custom([this](zenkit::DaedalusVm& v, zenkit::DaedalusSymbol& sym) {
      auto params = v.find_parameters_for_function(&sym);
      std::string args;
      for(size_t i=params.size(); i>0; --i) {
        std::string a;
        try {
          switch(params[i-1].type()) {
            case zenkit::DaedalusDataType::INT:
              a = std::to_string(v.pop_int());
              break;
            case zenkit::DaedalusDataType::FLOAT:
              a = std::to_string(v.pop_float());
              break;
            case zenkit::DaedalusDataType::STRING:
              a = "\"" + std::string(v.pop_string()) + "\"";
              break;
            case zenkit::DaedalusDataType::INSTANCE: {
              auto inst = v.pop_instance();
              auto* is  = inst!=nullptr ? v.find_symbol_by_instance(*inst) : nullptr;
              a = is!=nullptr ? is->name() : "NULL";
              break;
              }
            default:
              break;
            }
          }
        catch(...) {
          a = "?";
          }
        args = args.empty() ? a : a + ", " + args;
        }
      if(sym.has_return()) {
        if(sym.rtype()==zenkit::DaedalusDataType::FLOAT)
          v.push_float(0.f); else
        if(sym.rtype()==zenkit::DaedalusDataType::INT)
          v.push_int(0); else
        if(sym.rtype()==zenkit::DaedalusDataType::STRING)
          v.push_string(""); else
        if(sym.rtype()==zenkit::DaedalusDataType::INSTANCE)
          v.push_instance(nullptr);
        }
      nbMissingCall(nbStates[this],v,sym.name(),std::move(args));
      });
    std::vector<std::string> nbBound;
    // registers only a declared external whose signature in GOTHIC.DAT is the one of the callback
    const auto bindNb = [this,&nbBound](std::string_view name, auto callback) {
      auto* s = vm.find_symbol_by_name(name);
      if(s==nullptr || !s->is_external())
        return;
      if(!NbSig<decltype(callback)>::matches(vm,*s)) {
        Log::e("NB: ",nbSignature(vm,*s)," has another signature than expected, it is left to the default handler");
        return;
        }
      vm.register_external(name, callback);
      nbBound.emplace_back(s->name());
      };
    // the global variables of the scripts, by the name: NB keeps its state in them
    const auto intVar = [this](std::string_view name) -> zenkit::DaedalusSymbol* {
      auto* s = vm.find_symbol_by_name(name);
      return s!=nullptr && s->type()==zenkit::DaedalusDataType::INT && !s->is_member() ? s : nullptr;
      };

    // the variables of NB on the character
    bindNb("RX_GETNPCVAR", [this](std::shared_ptr<zenkit::INpc> ref, int32_t index) -> int32_t {
      auto* npc = findNpcRaw(ref);
      return npc==nullptr ? 0 : npc->newBalanceGetNpcVar(index);
      });
    bindNb("RX_SETNPCVAR", [this](std::shared_ptr<zenkit::INpc> ref, int32_t index, int32_t value) {
      if(auto* npc = findNpcRaw(ref))
        npc->newBalanceSetNpcVar(index,value);
      });
    bindNb("RX_INCNPCVAR", [this](std::shared_ptr<zenkit::INpc> ref, int32_t index, int32_t value) {
      if(auto* npc = findNpcRaw(ref))
        npc->newBalanceSetNpcVar(index,npc->newBalanceGetNpcVar(index)+value);
      });
    // RX_*NPCVARMASK(npc, AIVRX_BITMASK_COMMON_0, AIVRX_BITMASK_...): the value of a mask, not the number of a bit
    bindNb("RX_GETNPCVARMASK", [this](std::shared_ptr<zenkit::INpc> ref, int32_t index, int32_t mask) -> int32_t {
      auto* npc = findNpcRaw(ref);
      return npc!=nullptr && (npc->newBalanceGetNpcVar(index) & mask)!=0 ? 1 : 0;
      });
    bindNb("RX_SETNPCVARMASKTRUE", [this](std::shared_ptr<zenkit::INpc> ref, int32_t index, int32_t mask) {
      if(auto* npc = findNpcRaw(ref))
        npc->newBalanceSetNpcVar(index,npc->newBalanceGetNpcVar(index) | mask);
      });
    bindNb("RX_SETNPCVARMASKFALSE", [this](std::shared_ptr<zenkit::INpc> ref, int32_t index, int32_t mask) {
      if(auto* npc = findNpcRaw(ref))
        npc->newBalanceSetNpcVar(index,npc->newBalanceGetNpcVar(index) & ~mask);
      });
    bindNb("RX_TOGGLENPCVARMASK", [this](std::shared_ptr<zenkit::INpc> ref, int32_t index, int32_t mask) {
      if(auto* npc = findNpcRaw(ref))
        npc->newBalanceSetNpcVar(index,npc->newBalanceGetNpcVar(index) ^ mask);
      });

    // RX_*INTMASKVALUE("RX_AW_NotesMask0", 22): the number of a bit in a global variable
    bindNb("RX_GETINTMASKVALUE", [intVar](std::string_view name, int32_t bit) -> int32_t {
      auto* s = intVar(name);
      return s!=nullptr && bit>=0 && bit<32 && ((uint32_t(s->get_int())>>bit) & 1u) ? 1 : 0;
      });
    bindNb("RX_SETINTMASKVALUE", [intVar](std::string_view name, int32_t bit, int32_t value) {
      auto* s = intVar(name);
      if(s==nullptr || bit<0 || bit>=32)
        return;
      uint32_t v = uint32_t(s->get_int());
      v = value!=0 ? (v | (1u<<bit)) : (v & ~(1u<<bit));
      s->set_int(int32_t(v));
      });
    bindNb("RX_TOGGLETMASKVALUE", [intVar](std::string_view name, int32_t bit) {
      auto* s = intVar(name);
      if(s!=nullptr && bit>=0 && bit<32)
        s->set_int(int32_t(uint32_t(s->get_int()) ^ (1u<<bit)));
      });
    bindNb("RX_GETCOUNTTRUEINMASKVALUE", [intVar](std::string_view name) -> int32_t {
      auto* s = intVar(name);
      int32_t n = 0;
      for(uint32_t v = s!=nullptr ? uint32_t(s->get_int()) : 0u; v!=0; v &= v-1)
        ++n;
      return n;
      });

    // the global variables by the name
    bindNb("RX_GETVARVALUE", [intVar](std::string_view name) -> int32_t {
      auto* s = intVar(name);
      return s!=nullptr ? s->get_int() : 0;
      });
    bindNb("RX_SETVARVALUE", [intVar](std::string_view name, int32_t value) {
      if(auto* s = intVar(name))
        s->set_int(value);
      });
    bindNb("RX_INCREASEVARVALUE", [intVar](std::string_view name, int32_t value) {
      if(auto* s = intVar(name))
        s->set_int(s->get_int()+value);
      });
    // RX_ChangeValueClamp("RX_MonsterProtRatio", delta, min, max)
    bindNb("RX_CHANGEVALUECLAMP", [intVar](std::string_view name, int32_t delta, int32_t lo, int32_t hi) {
      if(auto* s = intVar(name))
        s->set_int(std::clamp(s->get_int()+delta,std::min(lo,hi),std::max(lo,hi)));
      });
    bindNb("RX_GETSTRINGVALUE", [this](std::string_view name) -> std::string {
      auto* s = vm.find_symbol_by_name(name);
      return s!=nullptr && s->type()==zenkit::DaedalusDataType::STRING && !s->is_member() ? s->get_string() : std::string();
      });

    // strings
    bindNb("RX_CONCATSTRINGS", [](std::string_view a, std::string_view b, std::string_view c, std::string_view d) -> std::string {
      std::string ret;
      ret.reserve(a.size()+b.size()+c.size()+d.size());
      ret.append(a).append(b).append(c).append(d);
      return ret;
      });
    // RX_FormatStringPlaceholder("RX_BookManagerText", p0..p5): "{0}".."{5}" in the variable are replaced
    bindNb("RX_FORMATSTRINGPLACEHOLDER", [this](std::string_view name, std::string_view p0, std::string_view p1, std::string_view p2,
                                                std::string_view p3, std::string_view p4, std::string_view p5) {
      auto* s = vm.find_symbol_by_name(name);
      if(s==nullptr || s->type()!=zenkit::DaedalusDataType::STRING || s->is_member())
        return;
      const std::array<std::string_view,6> args = {p0,p1,p2,p3,p4,p5};
      std::string text = s->get_string();
      for(size_t i=0; i<args.size(); ++i) {
        const std::string key = "{" + std::to_string(i) + "}";
        for(size_t at = text.find(key); at!=std::string::npos; at = text.find(key,at+args[i].size()))
          text.replace(at,key.size(),args[i]);
        }
      s->set_string(text);
      });
    bindNb("RX_GETINSTANCENAME", [this](std::shared_ptr<zenkit::DaedalusInstance> ref) -> std::string {
      auto* s = ref!=nullptr ? vm.find_symbol_by_index(ref->symbol_index()) : nullptr;
      return s!=nullptr ? s->name() : std::string();
      });
    bindNb("RX_NPCINSTCONTAINS", [this](std::shared_ptr<zenkit::INpc> ref, std::string_view part) -> int32_t {
      auto* s = ref!=nullptr ? vm.find_symbol_by_index(ref->symbol_index()) : nullptr;
      if(s==nullptr)
        return 0;
      std::string p(part);
      for(auto& c:p)
        c = char(std::toupper(static_cast<unsigned char>(c)));
      return s->name().find(p)!=std::string::npos ? 1 : 0;
      });

    // random
    bindNb("RX_GETRANDVAL", [this](int32_t lo, int32_t hi) -> int32_t {
      if(hi<lo)
        std::swap(lo,hi);
    const uint64_t range = uint64_t(int64_t(hi)-lo)+1u;
    return int32_t(int64_t(lo) + int64_t(uint64_t(randGen()) % range));
      });
    bindNb("RX_GETBIGRANDOMINT", [this]() -> int32_t {
      return int32_t(randGen() & 0x7fffffff);
      });
    // RX_SelfPseudoRandom(chance, "RX_PSEUDORANDOM_VALUE_..."): a chance in percent
    bindNb("RX_SELFPSEUDORANDOM", [this](int32_t chance, std::string_view) -> int32_t {
      return int32_t(randGen()%100) < chance ? 1 : 0;
      });

    // SELF, OTHER, VICTIM and ITEM: saved before a callback of the scripts and restored after it
    auto parserVars = std::make_shared<std::vector<std::array<std::shared_ptr<zenkit::DaedalusInstance>,4>>>();
    bindNb("RX_SAVEPARSERVARS", [this,parserVars]() {
      parserVars->push_back({vm.global_self()->get_instance(),  vm.global_other()->get_instance(),
                             vm.global_victim()->get_instance(),vm.global_item()->get_instance()});
      });
    bindNb("RX_RESTOREPARSERVARS", [this,parserVars]() {
      if(parserVars->empty())
        return;
      auto& v = parserVars->back();
      vm.global_self()  ->set_instance(v[0]);
      vm.global_other() ->set_instance(v[1]);
      vm.global_victim()->set_instance(v[2]);
      vm.global_item()  ->set_instance(v[3]);
      parserVars->pop_back();
      });

    // RX_CallNpcFunc(self, other, "B_AssessDamage"): the function of this character, B_ASSESSDAMAGE_NONE_50003,
    // if the scripts have one. The scripts do "if(!RX_CallNpcFunc(...)) { return; }":
    // TRUE  = there is no own function, the common reaction goes on (B_MM_AssessDamage, B_AssessDamage)
    // FALSE = the own function handled it (it returns FALSE itself)
    bindNb("RX_CALLNPCFUNC", [this](std::shared_ptr<zenkit::INpc> slf, std::shared_ptr<zenkit::INpc> oth, std::string_view base) -> int32_t {
      auto* inst = slf!=nullptr ? vm.find_symbol_by_index(slf->symbol_index()) : nullptr;
      if(inst==nullptr)
        return 1;
      auto* fn = vm.find_symbol_by_name(std::string(base) + "_" + inst->name());
      if(fn==nullptr || fn->type()!=zenkit::DaedalusDataType::FUNCTION || fn->is_external())
        return 1;
      ScopeVar self (*vm.global_self(),  slf);
      ScopeVar other(*vm.global_other(), oth);
      if(fn->has_return())
        return vm.call_function<int>(fn);
      vm.call_function<void>(fn);
      return 0;
      });

    // the delayed functions: run in tick(), RX_CALLBACKDIALOG waits for the end of the dialog
    const auto addCallback = [this](std::shared_ptr<zenkit::INpc> npc, float sec, int32_t func, bool waitDialog) {
      auto* fn = vm.find_symbol_by_index(uint32_t(func));
      if(fn==nullptr || fn->type()!=zenkit::DaedalusDataType::FUNCTION)
        return;
      NbCallback c;
      c.at         = tickCount() + uint64_t(std::max(0.f,sec)*1000.f);
      c.func       = uint32_t(func);
      c.npc        = std::move(npc);
      c.waitDialog = waitDialog;
      nbStates[this].callbacks.push_back(std::move(c));
      };
    bindNb("RX_CALLBACK", [addCallback](float sec, int32_t func) {
      addCallback(nullptr,sec,func,false);
      });
    bindNb("RX_CALLBACKNPC", [addCallback](std::shared_ptr<zenkit::INpc> npc, float sec, int32_t func) {
      if(npc!=nullptr)
        addCallback(npc,sec,func,false);
      });
    bindNb("RX_CALLBACKINDIALOG", [addCallback](std::shared_ptr<zenkit::INpc> npc, float sec, int32_t func) {
      addCallback(npc,sec,func,false);
      });
    bindNb("RX_CALLBACKDIALOG", [addCallback](std::shared_ptr<zenkit::INpc> npc, float sec, int32_t func) {
      addCallback(npc,sec,func,true);
      });

    // dialogs: the text of a choice that the condition function writes, and the choices of NB
    bindNb("RX_SETDIALOGDESCRIPTION", [this](int32_t infoInstance, std::string_view text) {
      if(auto* info = findInfo(uint32_t(infoInstance)))
        info->description = text;
      });
    bindNb("RX_INFO_ADDCHOICE", [this](int32_t info, std::string_view text, int32_t func, int32_t) {
      info_addchoice(info,text,func);
      });

    // fight
    bindNb("RX_ISENEMY", [this](std::shared_ptr<zenkit::INpc> aRef, std::shared_ptr<zenkit::INpc> bRef) -> int32_t {
      auto* a = findNpc(aRef);
      auto* b = findNpc(bRef);
      return a!=nullptr && b!=nullptr && a!=b && a->isEnemy(*b) ? 1 : 0;
      });
    bindNb("RX_CLEARNPCFOCUS", [this](std::shared_ptr<zenkit::INpc> ref) {
      if(auto* npc = findNpc(ref))
        npc->setTarget(nullptr);
      });
    bindNb("RX_ISCASTINGSPELL", [this](std::shared_ptr<zenkit::INpc> ref) -> int32_t {
      auto* npc = findNpc(ref);
      return npc!=nullptr && npc->weaponState()==WeaponState::Mage ? 1 : 0;
      });
    // RX_GetEnemyRadius(self, 500): how many enemies are in the radius
    bindNb("RX_GETENEMYRADIUS", [this](std::shared_ptr<zenkit::INpc> ref, int32_t radius) -> int32_t {
      auto* npc = findNpc(ref);
      if(npc==nullptr || radius<=0)
        return 0;
      int32_t n = 0;
      world().detectNpc(npc->position(), float(radius), [npc,&n](Npc& oth) {
        if(&oth!=npc && !oth.isDead() && oth.isEnemy(*npc))
          ++n;
        });
      return n;
      });
    // RX_AgroAttack(self, 1000, 1): the nearest enemy in the radius is attacked; 1 = there was one
    bindNb("RX_AGROATTACK", [this](std::shared_ptr<zenkit::INpc> ref, int32_t radius, int32_t) -> int32_t {
      auto* npc = findNpc(ref);
      if(npc==nullptr || npc->isDead() || radius<=0)
        return 0;
      Npc*  target = nullptr;
      float dist   = float(radius)*float(radius);
      world().detectNpc(npc->position(), float(radius), [npc,&target,&dist](Npc& oth) {
        if(&oth==npc || oth.isDead() || !oth.isEnemy(*npc))
          return;
        const float d = oth.qDistTo(*npc);
        if(d<dist) {
          dist   = d;
          target = &oth;
          }
        });
      if(target==nullptr)
        return 0;
      npc->setTarget(target);
      vm.global_other()->set_instance(target->handlePtr());
      // below GIL_SEPERATOR_HUM are the humans (ZS_Attack), above the monsters (ZS_MM_Attack); 21 in NB
      auto* sepHum = vm.find_symbol_by_name("GIL_SEPERATOR_HUM");
      const int32_t sep = sepHum!=nullptr ? sepHum->get_int() : 16;
      const ScriptFn st = int32_t(npc->guild())<sep ? ZS_Attack : ZS_MM_Attack;
      if(st.isValid() && !npc->isInState(st)) {
        npc->clearAiQueue();
        npc->aiPush(AiQueue::aiStartState(st,0,target,nullptr,""));
        }
      return 1;
      });
    bindNb("RX_CANSENSEPLAYER", [this](std::shared_ptr<zenkit::INpc> ref) -> int32_t {
      auto* npc = findNpc(ref);
      auto* pl  = world().player();
      return npc!=nullptr && pl!=nullptr && npc->canSenseNpc(*pl,true)!=SensesBit::SENSE_NONE ? 1 : 0;
      });
    bindNb("RX_HASRANGEWEAPONANY", [this](std::shared_ptr<zenkit::INpc> ref) -> int32_t {
      auto* npc = findNpc(ref);
      return npc!=nullptr && npc->currentRangedWeapon()!=nullptr ? 1 : 0;
      });
    bindNb("RX_GETRANGEWEAPONANY", [this](std::shared_ptr<zenkit::INpc> ref) -> std::shared_ptr<zenkit::IItem> {
      auto* npc = findNpc(ref);
      return npc!=nullptr && npc->currentRangedWeapon()!=nullptr ? npc->currentRangedWeapon()->handlePtr() : nullptr;
      });
    bindNb("RX_AI_WAIT", [this](std::shared_ptr<zenkit::INpc> ref, float sec) {
      ai_wait(ref,sec);
      });

    // the characters in the world
    bindNb("RX_UNSTUCK", [this](std::shared_ptr<zenkit::INpc> ref) -> int32_t {
      auto* npc = findNpc(ref);
      if(npc==nullptr)
        return 0;
      fixNpcPosition(*npc,npc->rotation(),0);
      return 1;
      });
    bindNb("RX_REMOVENPCFORCE", [this](std::shared_ptr<zenkit::INpc> ref) {
      if(auto* npc = findNpcRaw(ref))
        world().removeNpc(*npc);
      });
    bindNb("RX_TELEPORTFORCE", [this](std::shared_ptr<zenkit::INpc> ref, std::string_view wp) {
      auto* npc = findNpc(ref);
      auto* pt  = world().findPoint(wp);
      if(npc==nullptr || pt==nullptr)
        return;
      npc->clearAiQueue();
      npc->aiPush(AiQueue::aiTeleport(*pt));
      });
    // RX_EquipArmor(npc, "ITAR_Bau_L", flag): "" takes the armor off
    bindNb("RX_EQUIPARMOR", [this](std::shared_ptr<zenkit::INpc> ref, std::string_view name, int32_t) {
      auto* npc = findNpc(ref);
      if(npc==nullptr)
        return;
      if(name.empty()) {
        if(auto* a = npc->currentArmor())
          npc->unequipItem(a->clsId());
        return;
        }
      auto* sym = vm.find_symbol_by_name(name);
      if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::INSTANCE)
        return;
      const uint32_t id = sym->index();
      if(npc->itemCount(id)==0)
        npc->addItem(id,1);
      npc->equipItem(id);
      });
    bindNb("RX_GETNPCADDR", [this](std::shared_ptr<zenkit::INpc> ref) -> int32_t {
      auto* npc = findNpcRaw(ref);
      return npc!=nullptr ? int32_t(world().npcId(npc))+1 : 0;
      });

    // The scripts of NB explain why the AI stops (B_MM_AssessEnemy, ZS_MM_Attack, B_AssessDamage...):
    // CMD_COLOR("ZS_MM_Attack ", "self AIVRX_NPC_IS_NEUTRAL"), when RX_DEBUG_RETURN_SHOW is 1.
    // Each message goes to the log a few times only, the perceptions run several times a second.
    if(kNbScriptTrace) {
      if(auto* s = vm.find_symbol_by_name("RX_DEBUG_RETURN_SHOW")) {
        try { s->set_int(1); } catch(...) { Log::e("NB: RX_DEBUG_RETURN_SHOW cannot be set"); }
        }
      }
    auto traceSeen = std::make_shared<std::unordered_map<std::string,int>>();
    bindNb("CMD_COLOR", [traceSeen](std::string_view where, std::string_view why) {
      std::string key = std::string(where) + std::string(why);
      if(traceSeen->size()<2000 && ++(*traceSeen)[key]<=3)
        Log::i("NB trace: ",where,"| ",why);
      });

    // the log, the profiler and the brackets of the code of NB
    // the same line of RX_WriteLog (e.g. "RX_Main_CycleTrigger {" every 2 seconds) only 3 times, then at 100, 1000...
    bindNb("RX_WRITELOG",            [this](std::string_view msg) {
      auto& seen = nbStates[this].writeLogSeen;
      if(seen.size()>5000)
        seen.clear();
      const uint32_t n = ++seen[std::string(msg)];
      if(n<=3)
        Log::d("NB: ",msg); else
      if(n==100 || n==1000 || n==10000)
        Log::d("NB: ",msg," (x",int(n)," so far)");
      });
    bindNb("RX_PRINTDEBUGINFOFUNC",  [](std::string_view)     {});
    bindNb("RX_PRINTPERF",           [](std::string_view, int32_t) {});
    bindNb("RX_PRINTP",              [this](std::string_view msg) { nbPrintLog(vm,msg); Gothic::inst().onPrint(std::string(msg).c_str()); });
    // the messages of NB on the screen (the plugin draws them; bound only if they are externals in GOTHIC.DAT)
    bindNb("AI_PRINT",               [this](std::string_view msg) { nbPrintLog(vm,msg); Gothic::inst().onPrint(std::string(msg).c_str()); });
    bindNb("AI_PRINTRED",            [this](std::string_view msg) { nbPrintLog(vm,msg); Gothic::inst().onPrint(std::string(msg).c_str()); });
    bindNb("AI_PRINTBONUS",          [this](std::string_view msg) { nbPrintLog(vm,msg); Gothic::inst().onPrint(std::string(msg).c_str()); });
    bindNb("AI_FASTPRINTSKILLCRAFT", [this](std::string_view msg) { nbPrintLog(vm,msg); Gothic::inst().onPrint(std::string(msg).c_str()); });
    bindNb("AI_NOTICEPRINT",         [this](int32_t, int32_t, std::string_view msg) { nbPrintLog(vm,msg); Gothic::inst().onPrint(std::string(msg).c_str()); });
    // NB fix 45: the other messages of the plugin. Without them a refusal was silent, e.g. at a teacher
    // (B_TeachStamina: AI_PrintItemInfo / AI_PrintClr "not enough learning points / gold", "improve by +1"...)
    const auto nbPrint = [this](std::string_view msg) {
      nbPrintLog(vm,msg);   // NB fix 229
      if(!msg.empty())
        Gothic::inst().onPrint(std::string(msg).c_str());
      };
    bindNb("AI_PRINTITEMINFO",  [nbPrint](std::string_view /*title*/, std::string_view msg, int32_t, int32_t) { nbPrint(msg); });
    bindNb("AI_PRINTCLR",       [nbPrint](std::string_view msg, int32_t, int32_t, int32_t) { nbPrint(msg); });
    bindNb("AI_PRINTCRAFT",     [nbPrint](std::string_view msg, int32_t, int32_t, int32_t) { nbPrint(msg); });
    bindNb("AI_FASTPRINT_1",    [nbPrint](std::string_view msg, int32_t, int32_t) { nbPrint(msg); });
    bindNb("AI_FASTPRINT_2",    [nbPrint](std::string_view msg, int32_t, int32_t) { nbPrint(msg); });
    bindNb("AI_PRINTEXP",       [nbPrint](std::string_view msg) { nbPrint(msg); });
    bindNb("AI_PRINTMIDDLE",    [nbPrint](std::string_view msg) { nbPrint(msg); });
    bindNb("AI_PRINTBOTTOMRED", [nbPrint](std::string_view msg) { nbPrint(msg); });
    bindNb("AI_PRINTBONUSITEM", [nbPrint](std::string_view msg) { nbPrint(msg); });
    bindNb("AI_PRINTBONUSLONG", [nbPrint](std::string_view msg) { nbPrint(msg); });
    bindNb("PRINTWINC",         [nbPrint](std::string_view msg) { nbPrint(msg); });
    bindNb("AI_PRINTPOS",       [nbPrint](std::string_view msg, int32_t, int32_t, std::string_view, int32_t, int32_t) { nbPrint(msg); });
    bindNb("AI_PRINTPOS_CLEAR", []() {});
    bindNb("AI_PRINTCLRSCREEN", []() {});
    bindNb("PRINTSCREENCOLOR",  [this](std::string_view msg, int32_t x, int32_t y, std::string_view font, int32_t time, std::string_view color) {
      nbPrintLog(vm,msg);   // NB fix 229
      // NB fix 62: the colors of OpenGothic fonts: red (the karma without the prayer of the day: "#FF0000"), yellow
      auto type = Resources::FontType::Normal;
      if(color.size()==7 && color[0]=='#') {
        const auto hex = [&](size_t i) { return int(std::strtol(std::string(color.substr(i,2)).c_str(),nullptr,16)); };
        const int r = hex(1), g = hex(3), b = hex(5);
        if(r>=200 && g<100 && b<100)
          type = Resources::FontType::Red;
        else if(r>=200 && g>=150 && b<100)
          type = Resources::FontType::Yellow;
        }
      Gothic::inst().onPrintScreen(msg,x,y,time,Resources::font(font,type,1.0));
      });
    // B_LogEntry: "Nowy wpis w dzienniku!" + the name of the topic. As the Union plugin draws it: on the left,
    // at 29% of the height, the title in RX_QuestTitleFont (new: gold, failed: red, finished: green in the
    // plugin - here: yellow / red / white, the colors of OpenGothic fonts) and the topic in white below.
    const auto printQuest = [this](std::string_view title, std::string_view topic, Resources::FontType color) {
      std::string titleFont = "FONT_OLD_20_WHITE.TGA";
      if(auto* f = vm.find_symbol_by_name("RX_QUESTTITLEFONT"); f!=nullptr && f->type()==zenkit::DaedalusDataType::STRING) {
        if(!f->get_string().empty() && Resources::hasFile(f->get_string()))
          titleFont = f->get_string();
        }
      Gothic::inst().onPrintScreen(title,2,27,5,Resources::font(titleFont,color,1));
      Gothic::inst().onPrintScreen(topic,2,31,5,Resources::font("FONT_OLD_20_WHITE.TGA",Resources::FontType::Normal,1));
      };
    bindNb("AI_PRINTQUEST",          [printQuest](std::string_view t, std::string_view q) { printQuest(t,q,Resources::FontType::Yellow); });
    bindNb("AI_PRINTQUESTCOMPLETE",  [printQuest](std::string_view t, std::string_view q) { printQuest(t,q,Resources::FontType::Normal); });
    bindNb("AI_PRINTQUESTFAIL",      [printQuest](std::string_view t, std::string_view q) { printQuest(t,q,Resources::FontType::Red);    });
    bindNb("AI_FASTPRINTCLEAR",      []() {});
    bindNb("RX_BEGIN",               [](int32_t) {});
    bindNb("RX_END",                 [](int32_t) {});
    bindNb("RX_TOGGLEPLAYERCONTROLS",[](int32_t) {});

    // Menu_ReadInt / Menu_WriteInt: the options of NB in Gothic.ini ([AST] bSkipLogo, bCanSaveGame, bOptionHealth...).
    // They come from Union, without them every option is 0 and nothing is saved.
    // the default values of the options of the plugin, as Union writes them into Gothic.ini at the start
    NbOptions::ensureDefaults();
    bindNb("MENU_READINT", [](std::string_view sec, std::string_view key) -> int32_t {
      return Gothic::settingsGetI(sec,key);
      });
    bindNb("MENU_WRITEINT", [](std::string_view sec, std::string_view key, int32_t value) {
      Gothic::settingsSetI(sec,key,value);
      });

    // the tables of crafting (alchemy, runes, scrolls, anvil, ...): NB opens the window of its DLL here.
    // The scripts of NB still have the old menus of dialogs (PC_Health_Start, PC_Circle, PC_..._End),
    // so this does what AI_ProcessInfos(hero) does in Gothic 2: the table opens the menu of the recipes.
    // RX_SetWeightValue(item, RX_W_*, value) in the instances of the items: the plugin keeps it per the
    // instance; used for the weight in the description (nbiteminfo.h) and by the scripts (Hero_Poisoned)
    bindNb("RX_SETWEIGHTVALUEEXT", [](std::shared_ptr<zenkit::IItem> itm, int32_t index, int32_t value) {
      if(itm!=nullptr)
        NbItemInfo::setWeight(*itm,index,value);
      });
    bindNb("RX_GETWEIGHTVALUE", [](std::shared_ptr<zenkit::IItem> itm, int32_t index) -> int32_t {
      return itm!=nullptr ? NbItemInfo::weight(itm->symbol_index(),index) : 0;
      });
    // NB fix 148: Npc_ChangeAttributeExtra(npc, attr, value) (UNION_ABI.DLL 0x10159C00) - Npc_ChangeAttribute that
    // ignores the flag of immortality (the flags are 0 for the change and come back if the npc lives). Used by
    // B_MagicHurtNpc_Immortal: the Hammer of Adanos against the stone guardians (B_MM_AssessDamage, 10000 damage),
    // the Holy Hammer against the magic golem. Without it these special hits did nothing.
    bindNb("NPC_CHANGEATTRIBUTEEXTRA", [this](std::shared_ptr<zenkit::INpc> npcRef, int32_t atr, int32_t val) {
      auto npc = findNpc(npcRef);
      if(npc==nullptr || atr<0)
        return;
      auto& h     = npc->handle();
      auto  flags = h.flags;
      h.flags = zenkit::NpcFlag(0);
      npc->changeAttribute(Attribute(atr),val,false);
      if(!npc->isDead())
        h.flags = flags;
      nbCtx("ChangeAttributeExtra '",npc->displayName(),"' attr=",atr," value=",val,
            " hp=",npc->attribute(ATR_HITPOINTS),npc->isDead() ? " (dead)" : "");
      });
    // B_SetFightSkills(npc, percent) for NPCs: the plugin adds percent to the hit chances (1H, 2H, bow,
    // crossbow; 0..100) and gives the matching fighting skill and overlay - ST3 from 90%, ST2 from 60%,
    // ST1 from 30% (bow/crossbow: T2 from 60%, T1 from 30%). Without it every NPC of NB stayed at 0%:
    // the untrained animations of a beginner and no critical hits (UNION_ABI.DLL 0x10198B20)
    bindNb("RX_APPLYPROPEROVERLAYSFORNPC", [this](std::shared_ptr<zenkit::INpc> npcRef, int32_t percent) {
      if(auto npc = findNpc(npcRef))
        NbMoveset::applyFightSkills(*npc,percent);
      });
    // re-reads the animation ids of the model after the overlays changed; OpenGothic looks the
    // animations up by name each time, so there is nothing to do
    bindNb("RX_INITANIMATIONS", [](std::shared_ptr<zenkit::INpc>) {});
    // B_SelectWeapon(slf, oth) of NB ends with RX_SelectWeaponNpc(slf, oth): the plugin draws the weapon for the fight.
    // Without it no NPC drew a weapon in a fight (fix 25). NB fix 31: the rules of the plugin (UNION_ABI.DLL 0x1017F840):
    //  - with a bow/crossbow and arrows the NPC shoots, unless the enemy is closer than 600 (and AIVRX_NPC_RANGE_ONLY is 0);
    //    bloodflies and harpies are always shot; an enemy immune to arrows (PROT_POINT -1, golems, trolls, guardians)
    //    only by a monster archer (AIVRX_IS_MONSTER_ARCHER);
    //  - an NPC that already shoots does not change to the sword for the animals (wolves, scavengers, rats, gobbos...);
    //  - after a change of the weapon the next one waits 6 s (AIVRX_NPC_CHANGEWEAPON_TIMER), with arrows
    // That is why Lares shoots the wolves with his bow.
    bindNb("RX_SELECTWEAPONNPC", [this](std::shared_ptr<zenkit::INpc> slfRef, std::shared_ptr<zenkit::INpc> othRef) {
      static std::unordered_map<const Npc*,uint64_t> changeTime;
      auto* slf = findNpcRaw(slfRef);
      auto* oth = findNpcRaw(othRef);
      if(slf==nullptr || slf->isDown())
        return;
      // ZS_Attack_Loop asks every frame: the weapon is drawn once, not once per frame
      if(!slf->isAiQueueEmpty())
        return;
      const auto  ws      = slf->weaponState();
      const bool  inRange = (ws==WeaponState::Bow || ws==WeaponState::CBow);
      const bool  melee   = slf->currentMeleeWeapon()!=nullptr;
      // (Npc::hasAmmunition looks only at the drawn weapon: with the sword in the hands it said "no arrows")
      bool ammo = false;
      if(auto* rw = slf->currentRangedWeapon()) {
        const int32_t mun = rw->handle().munition;
        ammo = mun>0 && slf->inventory().itemCount(size_t(mun))>0;
        }
      const float dist    = oth!=nullptr ? std::sqrt(slf->qDistTo(*oth)) : 0.f;
      const int   og      = oth!=nullptr ? int(oth->guild()) : 0;

      bool wantRanged = false, wantMelee = false, decided = false;
      if(ammo) {
        const bool close = slf->newBalanceGetNpcVar(55)==0 && dist<600.f && og!=30 && og!=43;
        if(!close) {
          const bool immune = oth!=nullptr && (oth->handle().protection[zenkit::DamageType::POINT]==-1 || og==44 || og==50 || og==56);
          if(!immune || slf->newBalanceGetNpcVar(136)!=0) {
            wantRanged = !inRange;
            decided    = true;
            }
          }
        }
      if(!decided) {
        if(ws!=WeaponState::Fist && !melee)
          wantMelee = true;                       // fists
        else if(melee && ws!=WeaponState::W1H && ws!=WeaponState::W2H)
          wantMelee = true;
        }
      static const int animals[] = {32,24,27,29,30,31,34,35,40,41,57,43,50,51,53,54,28,22,23};
      if(inRange && ammo && wantMelee && std::find(std::begin(animals),std::end(animals),og)!=std::end(animals))
        wantMelee = false;

      const uint64_t now   = tickCount();
      auto           it    = changeTime.find(slf);
      const bool     timer = (it!=changeTime.end() && now<it->second+6000);
      if(changeTime.size()>4096)
        changeTime.clear();
      if(wantRanged) {
        if(timer)
          return;
        slf->aiPush(AiQueue::aiRemoveWeapon());
        slf->aiPush(AiQueue::aiReadyRangedWeapon());
        changeTime[slf] = now;
        return;
        }
      if(wantMelee) {
        if(timer && ammo)
          return;
        if(ws!=WeaponState::NoWeapon)
          slf->aiPush(AiQueue::aiRemoveWeapon());
        slf->aiPush(AiQueue::aiReadyMeleeWeapon());
        changeTime[slf] = now;
        }
      });

    // B_CheckHealth of NB runs at the start of every state of a human (AIV_TAPOSITION = NOTINPOS) and calls
    // B_Npc_Desynchronize(npc, nCycleChar): AI_WaitMS with a global counter 250, 500 ... 3000 ms, so that the
    // characters of a place do not all move at the same moment. In ZS_Guide_Player / ZS_Follow_Player the
    // position is never reached, so a companion waited up to 3 s after every fight, dialog and stop before it
    // went on. For the members of the party (AIV_PARTYMEMBER) the wait is skipped, the others wait as in NB.
    // NB fix 62: RX_PrintCutscene (in GamePaused) writes "Pomiń, wciśnij G lub R" over the scenes of NB; the keys
    // come through RX_KeyPressed of the plugin, which is not here - the scene cannot be skipped, so no text either
    if(auto* pc = vm.find_symbol_by_name("RX_PRINTCUTSCENE"); pc!=nullptr && !pc->is_external() &&
       pc->type()==zenkit::DaedalusDataType::FUNCTION && vm.find_parameters_for_function(pc).empty() && !pc->has_return()) {
      vm.override_function("RX_PRINTCUTSCENE", []() {});
      }
    if(auto* ds = vm.find_symbol_by_name("B_NPC_DESYNCHRONIZE"); ds!=nullptr && !ds->is_external()) {
      vm.override_function("B_NPC_DESYNCHRONIZE", [this](std::shared_ptr<zenkit::INpc> slf, int32_t ncycle) {
        auto* npc = findNpcRaw(slf);
        if(npc==nullptr || ncycle<=0)
          return;
        if(owner.version().game==2 && npc->handle().aivar[15]!=0)
          return;
        npc->aiPush(AiQueue::aiWait(uint64_t(ncycle)));
        });
      }

    // Npc_GetGuildAttitude(npc, other): the attitude of the guilds (the table of Wld_SetGuildAttitude);
    // the scripts of NB ask it in B_CheckHealth and the reactions to the hero. Without it: 0 = ATT_HOSTILE
    bindNb("NPC_GETGUILDATTITUDE", [this](std::shared_ptr<zenkit::INpc> aRef, std::shared_ptr<zenkit::INpc> bRef) -> int32_t {
      auto* a = findNpcRaw(aRef);
      auto* b = findNpcRaw(bRef);
      return a!=nullptr && b!=nullptr ? int32_t(guildAttitude(*a,*b)) : int32_t(ATT_NEUTRAL);
      });
    // NB fix 32: the overlays of the weapons of the hero: RX_ResetAllOverlays (RX_ApplyAnim_Weapons, at every change of
    // the weapon) takes ~60 overlays away with Mdl_RemoveOverlayMds_NoUpdate - an external of the plugin, unknown here.
    // Nothing was taken away: once a rapier was in the hands, Humans_Rapier_ST* stayed for ever and every one-handed
    // weapon (a knife, a club) fought like a rapier. NoUpdate = without the re-init of the model (OpenGothic needs none).
    bindNb("MDL_REMOVEOVERLAYMDS_NOUPDATE", [this](std::shared_ptr<zenkit::INpc> npcRef, std::string_view name) {
      if(auto npc = findNpc(npcRef))
        npc->delOverlay(name);   // NB fix 172: by the name, nothing is loaded
      });
    bindNb("MDL_APPLYOVERLAYMDS_NOUPDATE", [this](std::shared_ptr<zenkit::INpc> npcRef, std::string_view name) {
      if(auto npc = findNpc(npcRef))
        if(auto sk = Resources::loadSkeleton(name))
          npc->addOverlay(sk,0);
      });
    bindNb("MDL_HASOVERLAYMDS", [this](std::shared_ptr<zenkit::INpc> npcRef, std::string_view name) -> int32_t {
      auto npc = findNpc(npcRef);
      if(npc==nullptr)
        return 0;
      return npc->hasOverlay(name) ? 1 : 0;   // NB fix 172: by the name
      });
    bindNb("RX_OPENCRAFT", [this](int32_t craftType) {
      // NB fix 70, 71: every table in the window of the plugin (the recipes from UNION_ABI.DLL); the old menus of the
      // scripts (fix 2) only for an unknown table
      if(craftType>=0 && craftType<=12 && !NbCraft::recipes(craftType).empty() && owner.player()!=nullptr) {
        nbStates[this].craftPending = craftType;
        nbCtx("craft window: ",craftType);
        return;
        }
      if(auto pl = owner.player()) {
        aiOutOrderId = 0;
        pl->aiPush(AiQueue::aiProcessInfo(*pl));
        }
      });

    // NB fix 41: the stamina. The scripts take it while sprinting only when the run is played (RX_IsRunAniActive), while
    // diving by the water level, and they show it in a bar of the plugin (Bar_*)
    const auto aniActive = [](Npc& npc, std::string_view name) {
      std::string up(name);
      for(auto& c:up)
        c = char(std::toupper(static_cast<unsigned char>(c)));
      for(auto& l:npc.animationLayers())
        if(l.name==up)
          return true;
      return false;
      };
    bindNb("RX_ISANIACTIVE", [this,aniActive](std::shared_ptr<zenkit::INpc> npcRef, std::string_view name) -> int32_t {
      auto npc = findNpc(npcRef);
      return npc!=nullptr && aniActive(*npc,name) ? 1 : 0;
      });
    bindNb("NPC_ISANIMATIONACTIVE", [this,aniActive](std::shared_ptr<zenkit::INpc> npcRef, std::string_view name) -> int32_t {
      auto npc = findNpc(npcRef);
      return npc!=nullptr && aniActive(*npc,name) ? 1 : 0;
      });
    bindNb("RX_ISRUNANIACTIVE", [this](std::shared_ptr<zenkit::INpc> npcRef) -> int32_t {
      // the loop of the run in any weapon mode: S_RUNL, S_1HRUNL, S_FISTRUNL, S_BOWRUNL, S_MAGRUNL...
      auto npc = findNpc(npcRef);
      if(npc==nullptr)
        return 0;
      for(auto& l:npc->animationLayers())
        if(l.name.rfind("S_",0)==0 && l.name.find("RUNL")!=std::string::npos)
          return 1;
      return 0;
      });
    bindNb("NPC_GETWATERLEVEL", [this](std::shared_ptr<zenkit::INpc> npcRef) -> int32_t {
      // WATER_NONE 0, WATER_FEET 1, WATER_DIVE 2. NB fix 173: as the engine (oCNpc water level): 1 is the water to the
      // knees (wading), 2 the deep water - swimming and diving. The swim (S_SWIMF) of the hero takes the stamina in
      // Hero_Poisoned by WATER_DIVE: with the swim as 1 the stamina never went down in the water
      auto npc = findNpc(npcRef);
      if(npc==nullptr)
        return 0;
      if(npc->isDive() || npc->isSwim())
        return 2;
      return npc->isInWater() ? 1 : 0;
      });
    NbBars::reset();
    bindNb("BAR_SETSHOW", [](int32_t bar, int32_t show) {
      if(auto b = NbBars::at(bar))
        b->show = show!=0;
      });
    bindNb("BAR_GETSHOW", [](int32_t bar) -> int32_t {
      auto b = NbBars::at(bar);
      return b!=nullptr && b->show ? 1 : 0;
      });
    bindNb("BAR_SETBARTEX", [](int32_t bar, std::string_view tex) {
      if(auto b = NbBars::at(bar))
        b->tex = std::string(tex);
      });
    bindNb("BAR_SETVALUE", [](int32_t bar, int32_t value) {
      if(auto b = NbBars::at(bar))
        b->value = value;
      });
    bindNb("BAR_SETMAXRANGE", [](int32_t bar, int32_t range) {
      if(auto b = NbBars::at(bar))
        b->maxRange = range;
      });

    // NB fix 49: the first batch of the externals that the scripts of NB call most and OpenGothic did not have.
    // Monsters cast with it (spiders: web, ice lance, fireball, acid): the effect flies from the caster to the target
    // and the damage (less the protection against its type, as a spell) hits the target, who reacts to it.
    bindNb("WLD_PLAYEFFECT_VEC", [this](std::string_view fx, std::shared_ptr<zenkit::INpc> srcRef, std::shared_ptr<zenkit::INpc> dstRef,
                                        int32_t /*level*/, int32_t damage, int32_t damageType, int32_t projectile) {
      auto src = findNpc(srcRef);
      auto dst = findNpc(dstRef);
      if(src==nullptr || dst==nullptr)
        return;
      const VisualFx* vfx = Gothic::inst().loadVisualFx(fx);
      if(vfx!=nullptr && projectile!=0 && src!=dst) {
        // NB fix 157: the effect of the projectile was put at once on the target (its trajectory TARGET): the swarm of
        // the swamp golem, of the zombies and the mummies, the acid of the swamp drones were not seen
        nbShoot(*src,*dst,*vfx,damage,damageType);
        return;
        }
      if(vfx!=nullptr)
        src->startEffect(*dst,*vfx);
      nbVecDamage(*src,*dst,damage,damageType);
      });

    // Npc_ClearInv: as Npc_ClearInventory
    bindNb("NPC_CLEARINV", [this](std::shared_ptr<zenkit::INpc> npcRef) {
      npc_clearinventory(npcRef);
      });
    // Vob_GetDistToVob(npc, npc, "FP_..."): the distance from the character to a way point or a free point
    bindNb("VOB_GETDISTTOVOB", [this](std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc>, std::string_view point) -> int32_t {
      auto npc = findNpc(npcRef);
      auto wp  = npc!=nullptr ? world().findPoint(point,false) : nullptr;
      if(wp==nullptr && npc!=nullptr) {
        // NB fix 142: not a point - an object of this name (a mover, a trigger)
        int32_t ret = 999999;
        world().forEachTrigger([&](AbstractTrigger& t) {
          if(ret==999999 && t.name()==point)
            ret = int32_t((npc->position()-t.position()).length());
          });
        return ret;
        }
      if(wp==nullptr)
        return 999999;
      const auto d = npc->position()-wp->position();
      return int32_t(d.length());
      });
    bindNb("WLD_GETTIMEHOUR", [this]() -> int32_t { return int32_t(world().time().hour());   });
    bindNb("WLD_GETTIMEMIN",  [this]() -> int32_t { return int32_t(world().time().minute()); });
    // Hlp_GetNpcByInstInRadius(npc, "Ghost_liker", 3500): the nearest character of this instance around the character
    bindNb("HLP_GETNPCBYINSTINRADIUS", [this](std::shared_ptr<zenkit::INpc> npcRef, std::string_view inst, int32_t radius) -> std::shared_ptr<zenkit::INpc> {
      auto npc = findNpc(npcRef);
      auto* sym = vm.find_symbol_by_name(inst);
      if(npc==nullptr || sym==nullptr)
        return nullptr;
      Npc*  best  = nullptr;
      float bestD = float(radius)*float(radius);
      const auto pos = npc->position();
      world().detectNpc(pos,float(radius),[&](Npc& n) {
        if(&n==npc || n.isDead() || n.handle().symbol_index()!=sym->index())
          return;
        const float d = (n.position()-pos).quadLength();
        if(d<=bestD) {
          bestD = d;
          best  = &n;
          }
        });
      return best!=nullptr ? best->handlePtr() : nullptr;
      });
    // Npc_GetItemById(npc, instance): the item of this instance in the inventory
    bindNb("NPC_GETITEMBYID", [this](std::shared_ptr<zenkit::INpc> npcRef, int32_t id) -> std::shared_ptr<zenkit::IItem> {
      auto npc = findNpc(npcRef);
      auto itm = (npc!=nullptr && id>0) ? npc->getItem(size_t(id)) : nullptr;
      return itm!=nullptr ? itm->handlePtr() : nullptr;
      });
    bindNb("HLP_INVENTORYISOPEN", []() -> int32_t { return NbBars::inventoryOpen() ? 1 : 0; });
    // StrCmp of the plugin is TRUE when the strings are the same, without the case: !StrCmp("WALk", GetWalkModeString(self))
    // -> AI_SetWalkMode(self, NPC_WALK) (NB fix 50: the first version returned 0 for equal strings, like C)
    bindNb("STRCMP", [](std::string_view a, std::string_view b) -> int32_t {
      if(a.size()!=b.size())
        return 0;
      for(size_t i=0; i<a.size(); ++i)
        if(std::toupper(static_cast<unsigned char>(a[i]))!=std::toupper(static_cast<unsigned char>(b[i])))
          return 0;
      return 1;
      });
    // NB fix 210: Hlp_GetIntArrayValue(name, index) / Hlp_SetIntArrayValue(name, index, value) of the plugin (0x101e7540):
    // an element of the int variable of the scripts with that name; a name that the scripts do not have gives -1 (the stamina
    // and the spirits of the karma, "karma_spirit_count": not in the scripts of this version of NB - -1 on the PC too)
    bindNb("HLP_GETINTARRAYVALUE", [this](std::string_view name, int32_t index) -> int32_t {
      auto* s = vm.find_symbol_by_name(name);
      if(s==nullptr)
        return -1;
      if(s->type()!=zenkit::DaedalusDataType::INT || index<0 || uint32_t(index)>=s->count())
        return 0;
      return s->get_int(uint16_t(index));
      });
    bindNb("HLP_SETINTARRAYVALUE", [this](std::string_view name, int32_t index, int32_t value) {
      auto* s = vm.find_symbol_by_name(name);
      if(s==nullptr || s->type()!=zenkit::DaedalusDataType::INT || s->is_const() || index<0 || uint32_t(index)>=s->count())
        return;
      s->set_int(value,uint16_t(index));
      });
    // NB fix 214: Patrol_Set / Patrol_SetPath(npc, "WP1|WP2|...", hour, minute, wait, state) of the plugin (0x104567b0,
    // 0x10455fa0): the patrol becomes the entries of the daily routine of the character, the whole day from hour:minute.
    //  - Patrol_Set: from the first way point to every other one and back (WP2, WP1, WP3, WP1...);
    //  - Patrol_SetPath: along the path and back (WP2..WPn, WPn-1..WP1).
    // An entry lasts the length of the way / 833.33 cm a minute (rounded) + wait minutes, 2..1440 minutes. The round repeats
    // 1440 / its length times; the rest of the day goes to the entries evenly when wait > 0, else to the last one.
    const auto nbPatrol = [this](std::shared_ptr<zenkit::INpc> npcRef, std::string_view list, int32_t hour, int32_t minute,
                                 int32_t wait, int32_t func, bool path) {
      auto npc = findNpc(npcRef);
      if(npc==nullptr)
        return;
      std::vector<const WayPoint*> wps;
      size_t at = 0;
      while(at<=list.size()) {
        size_t end = list.find('|',at);
        if(end==std::string_view::npos)
          end = list.size();
        auto name = list.substr(at,end-at);
        while(!name.empty() && name.front()==' ')
          name.remove_prefix(1);
        while(!name.empty() && name.back()==' ')
          name.remove_suffix(1);
        if(!name.empty()) {
          if(auto* wp = world().findPoint(name,false))
            wps.push_back(wp); else
            Log::e("NB: Patrol: unknown waypoint ",name," (",npc->displayName(),")");
          }
        at = end+1;
        }
      if(wps.size()<2 || func<=0) {
        Log::e("NB: Patrol_Set failed: ",list);
        return;
        }
      std::vector<const WayPoint*> seq;
      if(path) {
        for(size_t i=1; i<wps.size(); ++i)
          seq.push_back(wps[i]);
        for(size_t i=wps.size()-1; i-->0;)
          seq.push_back(wps[i]);
        } else {
        for(size_t i=1; i<wps.size(); ++i) {
          seq.push_back(wps[i]);
          seq.push_back(wps[0]);
          }
        }
      std::vector<int32_t> dur;
      int32_t total = 0;
      const WayPoint* prev = wps[0];
      for(auto* wp:seq) {
        float len = world().nbWayLength(*prev,*wp);
        if(len<0)
          len = -len-1.f;   // no way: the line between them, as the plugin
        int32_t m = int32_t(std::round(len/833.3333f)) + wait;
        m = std::clamp(m,2,1440);
        dur.push_back(m);
        total += m;
        prev = wp;
        }
      const int32_t reps = std::max(1,1440/std::max(total,1));
      const int32_t rest = std::max(0,1440-reps*total);
      std::vector<int32_t> all;
      for(int32_t r=0; r<reps; ++r)
        all.insert(all.end(),dur.begin(),dur.end());
      const int32_t n = int32_t(all.size());
      if(wait>0) {
        for(int32_t i=0; i<n; ++i)
          all[size_t(i)] += rest/n + (i<rest%n ? 1 : 0);
        } else if(n>0) {
        all.back() += rest;
        }
      int32_t t = ((hour*60+minute)%1440+1440)%1440;
      const int32_t stop = t+1440;
      for(int32_t i=0; i<n && t<stop; ++i) {
        const int32_t e = std::min(t+all[size_t(i)],stop);
        npc->addRoutine(gtime((t/60)%24,t%60),gtime((e/60)%24,e%60),uint32_t(func),seq[size_t(i)%seq.size()]->name);
        t = e;
        }
      nbCtx("patrol: ",npc->displayName(),", ",seq.size()," way points, a round ",total," min, ",n," entries");
      };
    bindNb("PATROL_SET", [nbPatrol](std::shared_ptr<zenkit::INpc> npc, std::string_view list, int32_t h, int32_t m,
                                    int32_t wait, int32_t func) { nbPatrol(npc,list,h,m,wait,func,false); });
    bindNb("PATROL_SETPATH", [nbPatrol](std::shared_ptr<zenkit::INpc> npc, std::string_view list, int32_t h, int32_t m,
                                        int32_t wait, int32_t func) { nbPatrol(npc,list,h,m,wait,func,true); });
    bindNb("PATROL_NEXTWP", [this](std::shared_ptr<zenkit::INpc> npcRef) -> std::string {
      auto npc = findNpc(npcRef);
      return npc!=nullptr ? npc->nbPatrolNextPoint() : std::string();
      });
    // NB fix 214: Npc_SetEffect(npc, n) (0x100e23d0): the effect of the character - 1 red, 2 green, 3 white eyes, else none
    bindNb("NPC_SETEFFECT", [](std::shared_ptr<zenkit::INpc> npcRef, int32_t n) {
      if(npcRef==nullptr)
        return;
      npcRef->effect = n==1 ? "SPELLFX_REDEYES" : n==2 ? "SPELLFX_GREENEYES" : n==3 ? "SPELLFX_WHITEEYES" : "";
      });
    // NB fix 214: the new karma of the plugin works only with varNewKarmaEnabled >= 0 (0x100ff7b0); these scripts do not have
    // it (IsNewKarmaEnabled is FALSE), so on the PC these functions do nothing
    bindNb("UPDATEKARMA",           [](int32_t, int32_t) {});
    bindNb("UPDATEKARMATROPHY",     [](int32_t) {});
    bindNb("KARMA_UPDATE_QUEST",    [](int32_t, int32_t) {});
    bindNb("KARMA_PLAYATTACKSOUND", [](std::shared_ptr<zenkit::INpc>, std::shared_ptr<zenkit::INpc>, int32_t) {});
    bindNb("KARMA_FANATICPICKWEAPON", [](std::shared_ptr<zenkit::INpc>, std::string_view, int32_t) -> int32_t { return 0; });
    // Karma_GetItemMunition(item) (0x10152f90): the munition of the item, -1 without it
    bindNb("KARMA_GETITEMMUNITION", [this](int32_t cls) -> int32_t {
      auto* sym = vm.find_symbol_by_index(uint32_t(cls));
      if(cls<0 || !nbIsItemSymbol(vm,sym))
        return -1;
      auto it = vm.init_instance<zenkit::IItem>(sym);
      return it!=nullptr ? it->munition : -1;
      });
    // NB fix 214: Npc_HasMessage(npc, type, subtype) (0x1008b890): a message of the kind in the queue of the character; the
    // scripts ask only (2, 2) - a walk to a character or an item (EV_GOTOVOB), ZS_MM_Attack_Loop
    bindNb("NPC_HASMESSAGE", [this](std::shared_ptr<zenkit::INpc> npcRef, int32_t type, int32_t sub) -> int32_t {
      auto npc = findNpc(npcRef);
      if(npc==nullptr)
        return 0;
      if(type==2 && sub==2)
        return npc->nbHasGotoVob() ? 1 : 0;
      return 0;
      });
    // NB fix 214: Npc_HasSprintMoveset(npc) (0x101cb900): the sprint animations (HUMANS_SPRINT, HUMANS_MAGESPRINT) are on
    bindNb("NPC_HASSPRINTMOVESET", [this](std::shared_ptr<zenkit::INpc> npcRef) -> int32_t {
      auto npc = findNpc(npcRef);
      if(npc==nullptr)
        return 0;
      return npc->hasOverlay("HUMANS_SPRINT.MDS") || npc->hasOverlay("HUMANS_MAGESPRINT.MDS") ? 1 : 0;
      });
    // NB fix 215: Log_GetTopicStatus(topic) (0x101a8280): the status of the topic of the journal (1 running, 2 success,
    // 3 failed, 4 obsolete), 0 without it. B_CloseTopic_Now closes the quests by it ("Zadanie zakończone!")
    bindNb("LOG_GETTOPICSTATUS", [this](std::string_view topic) -> int32_t {
      for(size_t i=0; i<quests.questCount(); ++i)
        if(quests.quest(i).name==topic)
          return int32_t(quests.quest(i).status);
      return 0;
      });
    // NB fix 215: Npc_IsDoingRoutine(npc, name) (0x101cddf0): the daily routine of the character is RTN_<name>_<id>
    bindNb("NPC_ISDOINGROUTINE", [this](std::shared_ptr<zenkit::INpc> npcRef, std::string_view name) -> int32_t {
      if(npcRef==nullptr || npcRef->daily_routine<=0)
        return 0;
      auto* rtn = vm.find_symbol_by_index(uint32_t(npcRef->daily_routine));
      if(rtn==nullptr)
        return 0;
      std::string want = "RTN_"+std::string(name)+"_"+std::to_string(npcRef->id);
      for(auto& c:want)
        c = char(std::toupper(uint8_t(c)));
      return rtn->name()==want ? 1 : 0;
      });
    // NB fix 215: Itm_SetFlagActive(item, on) (0x101bc230): the flag ITEM_ACTIVE (1<<30) of the item - the trophies of the
    // scripts (USE_ItAt_*: the claw at the belt) are shown as worn in the inventory
    bindNb("ITM_SETFLAGACTIVE", [](std::shared_ptr<zenkit::IItem> itm, int32_t on) {
      if(itm==nullptr)
        return;
      auto fl = uint32_t(itm->flags);
      fl = on!=0 ? (fl | (1u<<30)) : (fl & ~(1u<<30));
      itm->flags = zenkit::ItemFlag(fl);
      });
    // NB fix 215: the scrolls of the characters (0x10088950: a spell item that stacks, not ITRU_*)
    const auto nbIsScroll = [](const Item& it) {
      auto& h = it.handle();
      if((ItmFlags(h.main_flag) & ITM_CAT_RUNE)==0 || (ItmFlags(h.flags) & ITM_MULTI)==0)
        return false;
      return true;
      };
    const auto nbScrolls = [this,nbIsScroll](Npc& npc, std::vector<const Item*>* out) {
      bool any = false;
      for(auto it = npc.inventory().iterator(Inventory::T_Inventory); it.isValid(); ++it) {
        if(!nbIsScroll(*it))
          continue;
        auto* sym = vm.find_symbol_by_index(uint32_t(it->clsId()));
        if(sym!=nullptr && sym->name().rfind("ITRU_",0)==0)
          continue;
        any = true;
        if(out!=nullptr)
          out->push_back(&*it);
        }
      return any;
      };
    bindNb("NPC_HASSCROLL", [this,nbScrolls](std::shared_ptr<zenkit::INpc> npcRef) -> int32_t {
      auto npc = findNpc(npcRef);
      return npc!=nullptr && nbScrolls(*npc,nullptr) ? 1 : 0;
      });
    // Npc_GetScroll(npc) (0x101cfa70): the spell of a random scroll of the character, 0 without one
    bindNb("NPC_GETSCROLL", [this,nbScrolls](std::shared_ptr<zenkit::INpc> npcRef) -> int32_t {
      auto npc = findNpc(npcRef);
      std::vector<const Item*> list;
      if(npc==nullptr || !nbScrolls(*npc,&list) || list.empty())
        return 0;
      return list[size_t(std::rand())%list.size()]->handle().spell;
      });
    // Npc_HasScrollExp(npc) (0x101d4370): only on the Road of Trials (RX_RoadOfTrialsActiveValue == 938491): a scroll, not
    // with the variable 222 of the character, not with the scroll of the transformation into a meatbug
    bindNb("NPC_HASSCROLLEXP", [this,nbScrolls,intVar](std::shared_ptr<zenkit::INpc> npcRef) -> int32_t {
      auto* road = intVar("RX_ROADOFTRIALSACTIVEVALUE");
      if(road==nullptr || road->get_int()!=938491)
        return 0;
      auto npc = findNpc(npcRef);
      if(npc==nullptr || !nbScrolls(*npc,nullptr))
        return 0;
      if(npc->newBalanceGetNpcVar(222)!=0)
        return 0;
      if(auto* meatbug = vm.find_symbol_by_name("ITSC_TRFMEATBUG"); meatbug!=nullptr && npc->itemCount(meatbug->index())>0)
        return 0;
      return 1;
      });
    // NB fix 215: Wld_OpenMoverIfClosed / Wld_CloseMoverIfOpened(name) (0x101cb450, 0x101cb2e0): the mover of this name gets
    // the trigger only when it is closed / open (not while it moves)
    const auto nbMoverIf = [this](std::string_view name, int32_t wantState) {
      bool hit = false;
      world().forEachTrigger([&](AbstractTrigger& t) {
        if(hit || t.name()!=name)
          return;
        if(auto* mv = dynamic_cast<MoveTrigger*>(&t)) {
          hit = true;
          if(mv->moverState()==wantState) {
            const TriggerEvent evt(std::string(name),"",world().tickCount(),TriggerEvent::T_Trigger);
            world().triggerEvent(evt);
            }
          }
        });
      if(!hit)
        Log::e("NB: no mover ",name);
      };
    bindNb("WLD_OPENMOVERIFCLOSED",  [nbMoverIf](std::string_view name) { nbMoverIf(name,2); });
    bindNb("WLD_CLOSEMOVERIFOPENED", [nbMoverIf](std::string_view name) { nbMoverIf(name,0); });
    // NB fix 215: Npc_CanSeePlayerInRadius(radius) (0x101c7dd0): a living character near the hero (in the radius, cm) sees
    // him (ORCCHESTOPEN_S1: the orc chest is opened unseen)
    bindNb("NPC_CANSEEPLAYERINRADIUS", [this](int32_t radius) -> int32_t {
      auto* hero = world().player();
      if(hero==nullptr)
        return 0;
      int32_t seen = 0;
      world().detectNpc(hero->position(),float(radius),[&](Npc& n) {
        if(seen!=0 || &n==hero || n.isDead())
          return;
        if(n.canSeeNpc(*hero,false) || n.canSeeNpc(*hero,true))
          seen = 1;
        });
      return seen;
      });
    // NB fix 219: the searches of the wisp in its dialog (0x101aafe0 FindPlants, 0x101ab7a0 FindPlantsRare, 0x101aa3a0
    // PlantsInLoc, 0x101aa660 PlantsRareInLoc, 0x101ba8b0 FindPlates, 0x101b8d80 OldWisp_FindStealTargets). The items of the
    // world: ITPL_* are the plants, the six rare ones by their names; the plates are ITWR_* STONEPLATE, DRUIDPLATE, DEMONPLATE.
    // The plugin skips an item at the origin of the world (closer than 2.5 m to 0,0,0).
    const auto nbFindKind = [this](Item& it) -> int {
      if(it.position().length()<=250.f)
        return 0;
      const bool food = (uint32_t(it.mainFlag()) & 32u)!=0;   // the plants of the plugin: mainflag ITEM_KAT_FOOD
      auto* sym = vm.find_symbol_by_index(uint32_t(it.clsId()));
      if(sym==nullptr)
        return 0;
      std::string n(sym->name());
      for(auto& c:n)
        c = char(std::toupper(static_cast<unsigned char>(c)));
      if(n.find("ITPL_")!=std::string::npos && food) {
        for(const char* r:{"ITPL_DEX_HERB_01","ITPL_STRENGTH_HERB_01","ITPL_PERM_HERB","ITPL_XAGITTA_HERB_MIS","ITPL_SUPER_HERB","ITPL_CACTUSFLOWER"})
          if(n.find(r)!=std::string::npos)
            return 2;
        return 1;
        }
      if(n.find("ITWR_")!=std::string::npos &&
         (n.find("STONEPLATE")!=std::string::npos || n.find("DRUIDPLATE")!=std::string::npos || n.find("DEMONPLATE")!=std::string::npos))
        return 3;
      return 0;
      };
    const auto nbFindItems = [this,nbFindKind](uint32_t kinds, float radius) {
      std::vector<Item*> ret;
      auto* hero = world().player();
      for(uint32_t i=0; auto* it = world().itmById(i); ++i) {
        const int k = nbFindKind(*it);
        if(k==0 || (kinds & (1u<<k))==0)
          continue;
        if(radius>0.f && (hero==nullptr || (it->position()-hero->position()).length()>radius))
          continue;
        ret.push_back(it);
        }
      return ret;
      };
    // the items go to the hero; the wisp tells what it brought (RX_WispFoundInLocation of AB_Translation.lbr, "name xN")
    const auto nbFindTake = [this](const std::vector<Item*>& items, size_t max) -> int32_t {
      auto* hero = world().player();
      if(hero==nullptr)
        return 0;
      std::vector<std::pair<std::string,size_t>> got;
      int32_t n = 0;
      for(auto* itm:items) {
        if(size_t(n)>=max)
          break;
        const std::string name(itm->displayName());
        const size_t      cnt = std::max<size_t>(itm->count(),1);
        if(auto ptr = world().takeItem(*itm)) {
          hero->addItem(std::move(ptr));
          ++n;
          auto i = std::find_if(got.begin(),got.end(),[&](auto& g){ return g.first==name; });
          if(i==got.end())
            got.emplace_back(name,cnt); else
            i->second += cnt;
          }
        }
      auto& st = nbStates[this];
      for(auto& g:got)
        st.wispLines.emplace_back(std::string(NbTexts::get("RX_WispFoundInLocation"))+g.first+" x"+std::to_string(g.second),tickCount()+10000);
      nbCtx("wisp: ",n," items of the location to the hero");
      return n;
      };
    bindNb("FINDPLANTS", [this,nbFindItems,nbFindTake](int32_t) -> int32_t {
      // up to 30 at a time, 20 on the Ancient Island (currentLevel 47)
      auto* lvl = vm.find_symbol_by_name("CURRENTLEVEL");
      const size_t max = (lvl!=nullptr && lvl->type()==zenkit::DaedalusDataType::INT && lvl->get_int()==47) ? 20 : 30;
      return nbFindTake(nbFindItems(1u<<1,0.f),max);
      });
    bindNb("FINDPLANTSRARE", [nbFindItems,nbFindTake](int32_t radius) -> int32_t {
      // "Znalezione rośliny i tabliczki": all the plants and the plates in the radius of the hero
      return nbFindTake(nbFindItems((1u<<1)|(1u<<2)|(1u<<3),float(std::max(radius,1))),size_t(-1));
      });
    bindNb("PLANTSINLOC",     [nbFindItems]() -> int32_t { return int32_t(nbFindItems(1u<<1,0.f).size()); });
    bindNb("PLANTSRAREINLOC", [nbFindItems]() -> int32_t { return int32_t(nbFindItems(1u<<2,0.f).size()); });
    bindNb("FINDPLATES",      [nbFindItems](int32_t) -> int32_t { return int32_t(nbFindItems(1u<<3,0.f).size()); });
    // the characters of the location that the hero has not robbed yet (aivar[AIV_PlayerHasPickedMyPocket] 0): living people,
    // not the friends (npcType 2, 3) and not the guild 19, as 0x10055ea0 of the plugin
    bindNb("OLDWISP_FINDSTEALTARGETS", [this]() -> int32_t {
      int32_t n = 0;
      for(uint32_t i=0; i<world().npcCount(); ++i) {
        auto* npc = world().npcById(i);
        if(npc==nullptr || npc->isPlayer() || npc->isDead() || npc->position().length()<=250.f)
          continue;
        auto& h = npc->handle();
        if(h.guild>=21 || h.guild==19 || int(h.type)==2 || int(h.type)==3)   // GIL_SEPERATOR_HUM of NB: 21
          continue;
        if(h.aivar[6]==0)
          ++n;
        }
      return n;
      });
    // NB fix 220: OpenPocket(bag, flags) (0x101a8d90): the bag shows the inventory of the hero with its kinds of items only
    bindNb("OPENPOCKET", [this](std::shared_ptr<zenkit::IItem>, int32_t mask) {
      auto* hero = world().player();
      if(hero!=nullptr && mask!=0)
        Gothic::inst().onOpenPocket(*hero,uint32_t(mask));
      });
    // NB fix 217: AI_PlayEffect(npc, fx, target, damage, type) (0x101c7c50 -> 0x10057e00): a message in the queue of the
    // character - the effect flies at the target when the turn and the animation before it are done (Npc::nextAiAction)
    bindNb("AI_PLAYEFFECT", [this](std::shared_ptr<zenkit::INpc> npcRef, std::string_view fx, std::shared_ptr<zenkit::INpc> tgRef,
                                   int32_t damage, int32_t damageType) {
      auto* npc = findNpc(npcRef);
      if(npc==nullptr)
        return;
      npc->aiPush(AiQueue::aiPlayEffect(fx,findNpc(tgRef),damage,damageType));
      });
    // NB fix 217: Npc_SetFightRange(npc, r) (0x101cf9a0): the fight ranges of this character (FightAlgo)
    bindNb("NPC_SETFIGHTRANGE", [this](std::shared_ptr<zenkit::INpc> npcRef, int32_t range) {
      if(auto* npc = findNpc(npcRef))
        npc->nbFightRange = std::max(range,0);
      });
    // NB fix 217: AST_FreezeNpc(npc, seconds, effect) (0x101b80c0): the magic arrows and bolts, the frost weapons freeze the
    // victim. With the effect: the ice of SPELLFX_ICESPELL_SENDPERCEPTION_MagicArrow, and not again within 3 s. Not frozen
    // (0x10055b80): the trolls, the dragons, the demons, the ice golems, the high priest of the skeletons, a stunned one
    auto freezeCd = std::make_shared<std::unordered_map<const Npc*,uint64_t>>();
    bindNb("AST_FREEZENPC", [this,freezeCd](std::shared_ptr<zenkit::INpc> npcRef, int32_t seconds, int32_t effect) {
      auto* npc = findNpc(npcRef);
      if(npc==nullptr || npc->isDead() || npc->isPlayer() || seconds<=0)
        return;
      const int32_t g = npc->handle().guild;
      if(g==46 || g==48 || g==50 || g==52)   // the guilds of NB: GIL_ICEGOLEM, GIL_DEMON, GIL_TROLL, GIL_DRAGON
        return;
      if(auto* sym = vm.find_symbol_by_index(uint32_t(npc->handle().symbol_index())); sym!=nullptr && sym->name()=="SKELETON_MI_HIGH_PRIEST")
        return;
      if(npc->nbStateName()=="ZS_STUN")
        return;
      const uint64_t now = tickCount();
      if(effect!=0) {
        if(freezeCd->size()>4096)
          freezeCd->clear();
        auto& cd = (*freezeCd)[npc];
        if(now<cd)
          return;
        cd = now+3000;
        if(auto* vfx = Gothic::inst().loadVisualFx("SPELLFX_ICESPELL_SENDPERCEPTION_MagicArrow"))
          npc->startEffect(*npc,*vfx);
        }
      npc->nbFreezeUntil = std::max(npc->nbFreezeUntil,now+uint64_t(seconds)*1000);
      nbCtx("freeze: ",npc->displayName()," for ",seconds," s");
      });
    // NB fix 221: StartChangeWorldTime(n, factor) (0x101b9fe0 -> 0x10160b40): the joints of the templars slow the game down to
    // the factor and back over n*10 steps of 100 ms (35 -> 35 s); not while it runs already
    bindNb("STARTCHANGEWORLDTIME", [this](int32_t n, float factor) {
      auto& st = nbStates[this];
      if(n<=0 || !(factor>0.f) || factor>1.f || st.slowSteps>0)
        return;
      st.slowStart  = Application::tickCount();
      st.slowSteps  = n*10;
      st.slowFactor = factor;
      nbCtx("slow time: to ",factor," over ",n," s");
      });
    // NB fix 221: Ext_DarkScreen(fade, hold, ?) (0x101d1a30): the screen goes black in fade ms, stays black hold ms and comes
    // back in fade ms (the vampire scenes)
    bindNb("EXT_DARKSCREEN", [](int32_t fade, int32_t hold, int32_t) {
      auto& d = NbBars::darkScreen();
      d.start = Application::tickCount();
      d.fade  = std::max(fade,1);
      d.hold  = std::max(hold,0);
      });
    // NB fix 221: InitFullSnc() (0x101d1890) reads the limits of the Full SnC+ mode into the plugin itself (max1hStr, maxHp...);
    // the port does not have that mode - nothing to do. Cmd(text): a line of the console of the plugin - into the log
    bindNb("INITFULLSNC", []() {});
    bindNb("CMD", [traceSeen](std::string_view text) {
      if(traceSeen->size()<2000 && ++(*traceSeen)[std::string("cmd|")+std::string(text)]<=3)
        Log::i("NB cmd: ",text);
      });
    // the music of the plugin (bosses, scenes): off / on / switched, the setting of the player stays as it is
    // NB fix 116: RX_MusicOff mutes the music of the game until RX_MusicOn (on the PC it writes musicEnabled into Gothic.ini).
    // Not by setEnabled: every option written by the scripts (Menu_WriteInt of bCanSaveGame when a fight starts) loaded
    // the settings again and the music of the game came back over the music of the boss
    bindNb("RX_MUSICOFF",    []() { GameMusic::inst().setScriptMute(true);  });
    bindNb("RX_MUSICON",     []() { GameMusic::inst().setScriptMute(false); });
    bindNb("RX_TOGGLEMUSIC", []() { auto& m = GameMusic::inst(); m.setEnabled(!m.isEnabled()); });
    // NB fix 50 (from log.txt): the walk mode as text, the state of the hero, the regeneration of an NPC left behind
    bindNb("GETWALKMODESTRING", [this](std::shared_ptr<zenkit::INpc> npcRef) -> std::string {
      auto npc = findNpc(npcRef);
      if(npc==nullptr)
        return "";
      const auto wm = npc->walkMode();
      if((wm & WalkBit::WM_Dive)!=WalkBit::WM_Run)  return "DIVE";
      if((wm & WalkBit::WM_Swim)!=WalkBit::WM_Run)  return "SWIM";
      if((wm & WalkBit::WM_Sneak)!=WalkBit::WM_Run) return "SNEAK";
      if((wm & WalkBit::WM_Walk)!=WalkBit::WM_Run)  return "WALK";
      return "RUN";
      });
    // HERO_NORMAL 0, HERO_THREATENED 1, HERO_FIGHTING 2: as the music of the fight - someone has the hero as the target
    bindNb("HLP_GETHEROSTATUS", [this]() -> int32_t {
      auto pl = world().player();
      if(pl==nullptr || !world().isTargeted(*pl))
        return 0;
      return pl->weaponState()==WeaponState::NoWeapon ? 1 : 2;
      });
    // ZS_Attack: the hero ran away (more than 2000) - the NPC puts the weapon away and gets the life back slowly;
    // B_MM_AssessDamage stops it. Here: 2% of the maximum every second, see NbState::regen in the tick.
    bindNb("RX_RESTORENPCHEALTHSLOW", [this](std::shared_ptr<zenkit::INpc> npcRef) {
      if(npcRef==nullptr)
        return;
      auto& r = nbStates[this].regen;
      for(auto& w:r)
        if(w.lock()==npcRef)
          return;
      r.push_back(npcRef);
      });
    bindNb("RX_RESTORENPCHEALTHSTOP", [this](std::shared_ptr<zenkit::INpc> npcRef) {
      auto& r = nbStates[this].regen;
      r.erase(std::remove_if(r.begin(),r.end(),[&](const std::weak_ptr<zenkit::INpc>& w) {
        auto p = w.lock();
        return p==nullptr || p==npcRef;
        }),r.end());
      });
    // the features of other versions of the mod (Legacy, the multiplayer, the road of trials...) - off, and quiet in the log
    bindNb("ISNEWLEGFEATURES",        []() -> int32_t { return 0; });
    bindNb("RX_ISLEGAST",             []() -> int32_t { return 0; });
    bindNb("RX_ISMULTACTIVE",         []() -> int32_t { return 0; });
    bindNb("RX_ROADOFTRIALSACTIVE",   []() -> int32_t { return 0; });
    bindNb("RX_ISHEROINSIDEOTHERNPC", []() -> int32_t { return 0; });
    // NB fix 76: the externals of UNION_ABI.DLL that the scripts call all the time (log of 17.11: "NB missing")
    // Hlp_GetSlotItem / Hlp_HasInSlot (oCNpc::GetSlotItem): the item in a slot of the model - HERO_POISONED looks
    // every tick for the burning torch in the left hand (FAKELISON)
    const auto slotItem = [this](std::shared_ptr<zenkit::INpc> npcRef, std::string_view slot) -> std::shared_ptr<zenkit::IItem> {
      auto* npc = findNpc(npcRef);
      if(npc==nullptr)
        return nullptr;
      if(nbEqualsNoCase(slot,"ZS_LEFTHAND") && npc->isUsingTorch())
        return npc->torchItem();
      if(auto* it = npc->itemInSlot(slot))
        return it->handlePtr();
      return nullptr;
      };
    bindNb("HLP_GETSLOTITEM", slotItem);
    bindNb("HLP_HASINSLOT", [slotItem](std::shared_ptr<zenkit::INpc> npcRef, std::string_view slot) -> int32_t {
      return slotItem(std::move(npcRef),slot)!=nullptr ? 1 : 0;
      });
    // Ext_RemoveFromSlot (oCNpc::RemoveFromSlot, without a drop): the burning torch goes out (the scripts give the hero
    // the unlit torch themselves, HERO_POISONED in the water); the decorations of the bones go away
    bindNb("EXT_REMOVEFROMSLOT", [this](std::shared_ptr<zenkit::INpc> npcRef, std::string_view slot) {
      auto* npc = findNpc(npcRef);
      if(npc==nullptr)
        return;
      if(nbEqualsNoCase(slot,"ZS_LEFTHAND") && npc->isUsingTorch()) {
        npc->setTorch(false);
        return;
        }
      if(!nbEqualsNoCase(slot,"ZS_LEFTHAND") && !nbEqualsNoCase(slot,"ZS_RIGHTHAND"))
        npc->clearDecor(slot);
      });
    // AST_GetManaBonus (0x1016a920): the regeneration of mana of the hero (RX_RegenCommonFunc) - the maximal mana / 105
    // per call (/ 210 with the rage of Beliar), the parts of a point add up. Before: 0, the mana came back too slowly
    bindNb("AST_GETMANABONUS", [this]() -> int32_t {
      auto* pl = owner.player();
      if(pl==nullptr)
        return 0;
      auto& st      = nbStates[this];
      auto*         rage = vm.find_symbol_by_name("RX_BELIARRAGEACTIVEVALUE");
      const bool    rageOn = rage!=nullptr && rage->type()==zenkit::DaedalusDataType::INT && !rage->is_member() &&
                             rage->get_int()==0xd4a7a;
      const int32_t div  = rageOn ? 210 : 105;
      const float   v   = float(pl->attribute(ATR_MANAMAX))/float(div) + st.manaBonusRest;
      const float   n   = std::floor(v);
      st.manaBonusRest = v-n;
      return int32_t(n);
      });
    // RX_ContainsRoutine(npc, text) (0x101837e0): whether the name of the routine of the character has the text
    // (Pablo with Bartok on "GuideMitte", the arena fighters on "ArenaPlace")
    bindNb("RX_CONTAINSROUTINE", [this](std::shared_ptr<zenkit::INpc> npcRef, std::string_view text) -> int32_t {
      auto* npc = findNpc(npcRef);
      if(npc==nullptr || text.empty())
        return 0;
      std::string name;
      auto& rt = nbStates[this].routines;
      if(auto it = rt.find(npc); it!=rt.end() && it->second.handle==npcRef.get()) {
        name = it->second.name;
        } else {
        if(auto* d = vm.find_symbol_by_index(uint32_t(npcRef->daily_routine)); d!=nullptr && npcRef->daily_routine!=0)
          name = d->name();
        }
      std::string up(text);
      for(auto& c:up)
        c = char(std::toupper(static_cast<unsigned char>(c)));
      for(auto& c:name)
        c = char(std::toupper(static_cast<unsigned char>(c)));
      return name.find(up)!=std::string::npos ? 1 : 0;
      });
    // NB fix 79: the locks of the plugin (UNION_ABI.DLL). The key Y calls HandleEvent: the rune (ItRu_Unlock, 75 of mana),
    // or the strength (RX_ForcePickLock, from Karl), Shift+Y the lock picks (100 of dexterity) - the scripts check, play
    // the animation and ask the plugin to open the chest in the focus
    const auto focusMob = [this]() -> Interactive* {
      auto& st = nbStates[this];
      auto* w  = owner.world();
      if(st.focusMob==nullptr || w==nullptr)
        return nullptr;
      st.focusMob = w->validateFocus(Focus(*st.focusMob)).interactive;
      return st.focusMob;
      };
    // Mob_GetBreakNum (0x1015a240): the turns of the lock of the chest in the focus (the length of the code of the lock)
    bindNb("MOB_GETBREAKNUM", [focusMob]() -> int32_t {
      auto* mob = focusMob();
      if(mob==nullptr || !mob->isContainer())
        return 0;
      return int32_t(mob->pickLockCode().size());
      });
    // the chest (or the door) in the focus is opened: CantUnlock for a lock only with a key, else open and the function
    // of the script (UnlockBreakDone / UnlockDone)
    const auto unlockFocus = [this,focusMob](std::string_view done, bool scroll) {
      auto* mob = focusMob();
      if(mob==nullptr || !(mob->isContainer() || mob->isDoor()) || !mob->isLocked())
        return;
      auto call = [this](std::string_view name) {
        auto* fn = vm.find_symbol_by_name(name);
        if(fn==nullptr || fn->type()!=zenkit::DaedalusDataType::FUNCTION)
          return;
        try {
          vm.call_function<void>(fn);
          }
        catch(const std::exception& e) {
          Log::e("NB: ",name," failed: ",e.what());
          }
        };
      if(!mob->keyName().empty() && mob->pickLockCode().empty()) {
        call("CANTUNLOCK");
        return;
        }
      if(scroll) {
        auto* pl = owner.player();
        auto* sc = vm.find_symbol_by_name("ITSC_UNLOCK");
        if(pl!=nullptr && sc!=nullptr && pl->itemCount(sc->index())>0)
          pl->delItem(sc->index(),1);
        }
      mob->setAsCracked(true);
      nbCtx("lock opened: ",mob->tag().empty() ? std::string_view("(chest)") : mob->tag()," (",done,")");
      call(done);
      };
    bindNb("MOB_SETBRAKECHEST",    [unlockFocus]() { unlockFocus("UNLOCKBREAKDONE",false); });
    bindNb("MOB_SETUNLOCKED",      [unlockFocus]() { unlockFocus("UNLOCKDONE",true);       });
    bindNb("MOB_SETUNLOCKED_RUNE", [unlockFocus]() { unlockFocus("UNLOCKDONE",false);      });
    // RX_KeyPressed(name, ...): the key of the options of the plugin. On the touch screen the only one is the button of the
    // lock while it is handled
    bindNb("RX_KEYPRESSED", [this](std::string_view name, int32_t, int32_t) -> int32_t {
      auto& k = nbStates[this].keyPressed;
      return !k.empty() && nbEqualsNoCase(name,k) ? 1 : 0;
      });
    // NB fix 82: the music of the bosses (RX_CheckMusicBoss): a sound of SFX.DAT (BATTLE_*.wav), the music of the game is off
    // meanwhile (RX_MusicOff); RX_StopBossMusic (ZS_Dead of the boss) stops it by RX_StopAllSound and RX_MusicOn
    bindNb("RX_STARTBOSSMUSIC", [this](std::string_view name) {
      nbStates[this].bossMusicSeen = Application::tickCount();
      if(auto* last = vm.find_symbol_by_name("RX_LASTBOSSMUSIC"); last!=nullptr && last->type()==zenkit::DaedalusDataType::STRING)
        last->set_string(std::string(name));
      nbCtx("boss music: ",name);
      if(Gothic::settingsGetI("SOUND","musicEnabled")!=0)
        Gothic::inst().playBossMusic(name);
      // NB fix 116: no sound of this music - the music of the game is not left muted
      if(!Gothic::inst().isBossMusic())
        GameMusic::inst().setScriptMute(false);
      });
    bindNb("RX_UPDATEBOSSMUSICCOORDS", [this]() {
      nbStates[this].bossMusicSeen = Application::tickCount();
      // a save from a boss fight (RX_MusicBoss is TRUE): the music goes on after the load
      if(!Gothic::inst().isBossMusic() && Gothic::settingsGetI("SOUND","musicEnabled")!=0)
        if(auto* last = vm.find_symbol_by_name("RX_LASTBOSSMUSIC"); last!=nullptr && last->type()==zenkit::DaedalusDataType::STRING &&
           !last->get_string().empty()) {
          GameMusic::inst().setScriptMute(true);   // NB fix 116
          Gothic::inst().playBossMusic(last->get_string());
          }
      });
    bindNb("RX_STOPALLSOUND", []() {
      Gothic::inst().stopBossMusic();
      });
    // RX_TerminateDialog (oCInformationManager::Exit) and RX_ClearInput (zCInput::ClearKeyBuffer) after the teleport
    // of the rune: the scripts call AI_StopProcessInfos before, the dialog closes by it - nothing more here
    bindNb("RX_TERMINATEDIALOG", []() {});

    // NB fix 125: the package 5 of the functions of the plugin (log of 20.11, "NB missing")
    // Npc_HasActivePerc(npc, perc) (UNION_ABI.DLL 0x1018bc60): the perception is in the list of the character
    bindNb("NPC_HASACTIVEPERC", [this](std::shared_ptr<zenkit::INpc> ref, int32_t perc) -> int32_t {
      auto* npc = findNpcRaw(ref);
      if(npc==nullptr || perc<0 || perc>=int32_t(PERC_Count))
        return 0;
      return npc->hasPerc(PercType(perc)) ? 1 : 0;
      });
    // Ext_PutInSlot(npc, slot, item) (0x1015b090): the item made from the instance hangs on the bone - the pouch of gold
    // (ITSL_GOLDPOCKET on BIP01 SPINE1), the rod of the fishers (ITSE_FANGEL in BIP01 R HAND). Only the look: a decoration
    // of the model, Ext_RemoveFromSlot takes it away. ActivateZSSlot (0x1015b1d0, oCNpc::CreateInvSlot) is not needed:
    // the decorations hang on any bone of the model
    bindNb("EXT_PUTINSLOT", [this](std::shared_ptr<zenkit::INpc> ref, std::string_view slot, int32_t itemInst) {
      auto* npc = findNpcRaw(ref);
      if(npc==nullptr || itemInst<=0 || slot.empty())
        return;
      auto& cache = nbStates[this].slotVisual;
      auto  vis   = cache.find(size_t(itemInst));
      if(vis==cache.end()) {
        std::string name;
        auto* sym = vm.find_symbol_by_index(uint32_t(itemInst));
        if(nbIsItemSymbol(vm,sym))
          if(auto inst = vm.init_instance<zenkit::IItem>(sym); inst!=nullptr)
            name = inst->visual;
        vis = cache.emplace(size_t(itemInst),name).first;
        }
      if(vis->second.empty())
        return;
      npc->setDecor(npc->world().addView(vis->second),slot);
      });
    bindNb("ACTIVATEZSSLOT", [](std::shared_ptr<zenkit::INpc>, std::string_view) {});
    // RX_CallQuestLoop(name) (0x10198730): while the quest is running (the variable of the name is LOG_RUNNING), the loop
    // of the quest RX_QuestLoop_<name> is called
    bindNb("RX_CALLQUESTLOOP", [this](std::string_view name) {
      std::string up(name);
      for(auto& c:up)
        c = char(std::toupper(static_cast<unsigned char>(c)));
      auto* var = vm.find_symbol_by_name(up);
      if(var==nullptr || var->type()!=zenkit::DaedalusDataType::INT || var->get_int()!=1)
        return;
      auto* fn = vm.find_symbol_by_name("RX_QUESTLOOP_"+up);
      if(fn==nullptr || fn->type()!=zenkit::DaedalusDataType::FUNCTION)
        return;
      vm.call_function<void>(fn);
      });
    // RX_IsNpcIgnoreTarget(npc, target) (0x1017ff50): the list of the targets that a character ignores - it is filled by
    // the plugin only (the multiplayer), empty here
    bindNb("RX_ISNPCIGNORETARGET", [](std::shared_ptr<zenkit::INpc>, std::shared_ptr<zenkit::INpc>) -> int32_t { return 0; });
    // Mob_ScrambleContainers() (0x1015be10, HERO_POISONED after every load): new combinations of the locks of the chests
    // - every letter of a combination longer than one is L or R again, at random
    bindNb("MOB_SCRAMBLECONTAINERS", [this]() {
      size_t done = 0;
      for(uint32_t i=0; ; ++i) {
        auto* mob = world().mobsiById(i);
        if(mob==nullptr)
          break;
        if(!mob->isContainer())
          continue;
        std::string code(mob->pickLockCode());
        if(code.size()<=1)
          continue;
        for(auto& c:code)
          c = (std::rand()%100)<=50 ? 'R' : 'L';
        mob->setPickLockCode(code);
        ++done;
        }
      nbCtx("lock combinations scrambled: ",done);
      });
    // Npc_ModifyBodyState(npc, add, remove) (0x10159db0, oCNpc::ModifyBodyState): the modifiers of the state of the body
    // (BS_MOD_CONTROLLED in the concerts of In Extremo) - OpenGothic has no such modifiers, nothing changes.
    // NB fix 149: the scripts declare it with the int result - with void it was left to the default handler
    bindNb("NPC_MODIFYBODYSTATE", [](std::shared_ptr<zenkit::INpc>, int32_t, int32_t) -> int32_t { return 0; });
    // NB fix 149: GetFocusNpc(npc) (0x1015b660, oCNpc::GetFocusNpc) and Npc_ValidFocusNpc(npc) (0x1015b6c0): the npc in the
    // focus - of the player the one the window shows, of the others their target. Used by HandleEvent and G_OpenSteal
    // (the keys of the plugin: theft, the locks, the summons)
    const auto focusNpcOf = [this](std::shared_ptr<zenkit::INpc> ref) -> Npc* {
      auto npc = findNpc(ref);
      if(npc==nullptr)
        return nullptr;
      if(!npc->isPlayer())
        return npc->target();
      auto it = nbStates.find(this);
      if(it==nbStates.end())
        return nullptr;
      return findNpc(it->second.focusNpc.lock());
      };
    bindNb("GETFOCUSNPC", [focusNpcOf](std::shared_ptr<zenkit::INpc> ref) -> std::shared_ptr<zenkit::INpc> {
      auto* f = focusNpcOf(ref);
      return f!=nullptr ? f->handlePtr() : nullptr;
      });
    bindNb("NPC_VALIDFOCUSNPC", [focusNpcOf](std::shared_ptr<zenkit::INpc> ref) -> int32_t {
      return focusNpcOf(ref)!=nullptr ? 1 : 0;
      });
    // NB fix 151: RX_ApplyVisualGallahad(type) (0x101843a0) and RX_RemoveVisualGallahad(type) (0x10184670): Gallahad
    // enchants the look of the weapon of the hero (0 the melee weapon, 1 the ranged one): the particles
    // RX_Gallahad_Visual_Type (NB_VIS_MELEE_FIRE_USUAL, for the ranged weapon _RANGE_), the color
    // RX_Gallahad_Visual_Color (ITEMEFFECTMANAGER_*) and the intensity RX_Gallahad_Visual_PPS (0..4). The plugin keeps it
    // with the item; here it goes to C_Item.effect of the item ("NB_VIS_...:color:intensity"), saved with it
    const auto nbGallahadItem = [this](int32_t type) -> std::pair<Npc*,Item*> {
      auto* w  = owner.world();
      auto* pl = w!=nullptr ? w->player() : nullptr;
      if(pl==nullptr)
        return {nullptr,nullptr};
      return {pl, type==1 ? pl->currentRangedWeapon() : pl->currentMeleeWeapon()};
      };
    bindNb("RX_APPLYVISUALGALLAHAD", [this,nbGallahadItem](int32_t type) {
      auto [pl,itm] = nbGallahadItem(type);
      if(itm==nullptr)
        return;
      auto str = [this](std::string_view n) -> std::string {
        auto* s = vm.find_symbol_by_name(n);
        return s!=nullptr && s->type()==zenkit::DaedalusDataType::STRING ? std::string(s->get_string()) : std::string();
        };
      auto num = [this](std::string_view n) -> int32_t {
        auto* s = vm.find_symbol_by_name(n);
        return s!=nullptr && s->type()==zenkit::DaedalusDataType::INT ? s->get_int() : 0;
        };
      std::string vis = str("RX_GALLAHAD_VISUAL_TYPE");
      if(vis.empty())
        return;
      if(type==1)
        if(auto p = vis.find("MELEE"); p!=std::string::npos)
          vis.replace(p,5,"RANGE");
      const int32_t color = num("RX_GALLAHAD_VISUAL_COLOR");
      const int32_t pps   = num("RX_GALLAHAD_VISUAL_PPS");
      itm->handle().effect = vis+":"+std::to_string(color)+":"+std::to_string(pps);
      pl->setItemFx(type==1 ? 1 : 0, itm->handle().effect);
      nbCtx("Gallahad: ",itm->displayName()," <- ",itm->handle().effect);
      });
    bindNb("RX_REMOVEVISUALGALLAHAD", [nbGallahadItem](int32_t type) {
      auto [pl,itm] = nbGallahadItem(type);
      if(itm==nullptr || itm->handle().effect.rfind("NB_VIS_",0)!=0)
        return;
      itm->handle().effect.clear();
      pl->setItemFx(type==1 ? 1 : 0, "");
      });
    // NB fix 149: RX_CopyAllDialogsNpc(from, to) (0x1017a4f0): the dialogs (C_Info.npc) of the instance "from" go to the
    // instance "to" - Fregyal on the West Coast (ZS_Talk: NONE_1192_Fregeal -> NONE_1192_Fregeal_WC). The plugin moves
    // them in the info manager, so the old npc has them no more
    bindNb("RX_COPYALLDIALOGSNPC", [this](std::string_view from, std::string_view to) {
      auto* a = vm.find_symbol_by_name(from);
      auto* b = vm.find_symbol_by_name(to);
      if(a==nullptr || b==nullptr)
        return;
      const int32_t ia = int32_t(a->index()), ib = int32_t(b->index());
      int moved = 0;
      for(auto& info : dialogsInfo)
        if(info!=nullptr && info->npc==ia) {
          info->npc = ib;
          ++moved;
          }
      if(moved>0)
        nbCtx("dialogs: ",moved," moved from ",from," to ",to);
      });
    // NB fix 149: RX_CreateSpellRune(npc, spell) (0x1016ab90): before RX_ReadySpell / RX_ChangeSpell the npc gets the rune
    // of the spell, if it has none - without it AI_ReadySpell finds nothing to draw and the mages of NB (the druid Kormak:
    // the water fist) fought with their hands. The table of the runes is the one of the plugin
    bindNb("RX_CREATESPELLRUNE", [this](std::shared_ptr<zenkit::INpc> ref, int32_t spell) {
      static const std::pair<int32_t,const char*> runes[] = {
        {128,"ITRU_FIRELANCE"}, {119,"ITRU_ELECTROWAVE"}, {1,"ITRU_PALLIGHTHEAL"}, {62,"ITRU_DEATHBALL"}, {113,"ITRU_EXTRICATE"},
        {110,"ITRU_QUAKE"}, {61,"ITRU_DEATHBOLT"}, {112,"ITRU_LACERATE"}, {85,"ITRU_BELIARSRAGE"}, {114,"ITRU_EXPLOSION"},
        {34,"ITRU_THUNDERBALL"}, {23,"ITRU_INSTANTFIREBALL"}, {42,"ITRU_FIRERAIN"}, {75,"ITRU_GEYSER"}, {72,"ITRU_WATERFIST"},
        {117,"ITRU_ACID"}, {87,"ITRU_SKULL"}, {127,"ITRU_GREENSTRIKE"}, {31,"ITRU_SUMSKEL"}, {121,"ITRU_BLOODRAIN"},
        {124,"ITRU_ABYSSSPELL"}, {143,"ITRU_DARKBALL"}, {145,"ITRU_BIGDARKBALL"}, {144,"ITRU_DARKSPEAR"}, {120,"ITRU_FIREWAVE"},
        {26,"ITRU_WINDFIST"}, {71,"ITRU_WHIRLWIND"}, {38,"ITRU_FIRESTORM"}, {19,"ITRU_FIREBOLT"},
        };
      auto npc = findNpc(ref);
      if(npc==nullptr || spell<=0)
        return;
      for(auto& r : runes) {
        if(r.first!=spell)
          continue;
        auto* sym = vm.find_symbol_by_name(r.second);
        if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::INSTANCE)
          return;
        if(npc->itemCount(sym->index())==0) {
          npc->addItem(sym->index(),1);
          nbCtx("rune for the spell ",spell,": ",r.second," to ",npc->displayName());
          }
        return;
        }
      });
    // NB fix 184: RX_SelectSpell(npc, spell) (0x101711e0, RX_ChangeSpell of the bosses: Im'Arah...): the plugin puts the rune
    // of the spell into the magic book of the npc and makes it the chosen spell. In OpenGothic the next call of
    // RX_ChangeSpell, AI_ReadySpell, chooses the rune by the spell (Inventory::switchActiveSpell); RX_CreateSpellRune
    // before it gave the rune. Here only the check that the rune is there, for the log
    bindNb("RX_SELECTSPELL", [this](std::shared_ptr<zenkit::INpc> ref, int32_t spell) {
      auto npc = findNpc(ref);
      if(npc==nullptr || spell<=0)
        return;
      const auto* inv = &npc->inventory();
      for(auto it=inv->iterator(Inventory::T_Inventory); it.isValid(); ++it) {
        if(it->isSpellOrRune() && it->spellId()==spell)
          return;
        }
      nbCtx("select spell: ",npc->displayName()," has no rune of the spell ",spell);
      });
    // NB fix 184: RX_AST_Blessing(kind) (0x10170160), the blessings of Isgaroth in RX_RegenCommonFunc (every second): the
    // regeneration of the hero, the fractions add up (the plugin 0x10052430 / 0x10052600 / 0x10052570):
    //   0 - hp: max(hp max, 400) / 360 a second (/ 800 when XARDAS_KNOWSSACTANOME is set), up to the max;
    //   1 - stamina (only with RX_OldStamina): max(ATR_STAMINA_MAX*10, 360) / 360, up to ATR_STAMINA_MAX*10;
    //   2 - mana: max(mana max, 133.3) / 400, up to the max
    bindNb("RX_AST_BLESSING", [this](int32_t kind) {
      auto* hero = world().player();
      if(hero==nullptr || kind<0 || kind>2)
        return;
      auto& acc = nbStates[this].blessAcc[kind];
      auto& h   = hero->handle();
      if(kind==0 || kind==2) {
        const auto cur = kind==0 ? ATR_HITPOINTS    : ATR_MANA;
        const auto max = kind==0 ? ATR_HITPOINTSMAX : ATR_MANAMAX;
        if(h.attribute[cur]>=h.attribute[max])
          return;
        if(kind==0) {
          const int32_t knows = nbIntVar("XARDAS_KNOWSSACTANOME");
          acc += float(std::max(h.attribute[max],400))/(knows!=0 ? 800.f : 360.f);
          } else {
          acc += std::max(float(h.attribute[max]),400.f/3.f)/400.f;
          }
        if(acc<1.f)
          return;
        const int32_t add = int32_t(acc);
        acc -= float(add);
        h.attribute[cur] = std::min(h.attribute[cur]+add,h.attribute[max]);
        return;
        }
      if(nbIntVar("RX_OLDSTAMINA")<=0)
        return;
      auto* sta = vm.find_symbol_by_name("ATR_STAMINA");
      auto* stm = vm.find_symbol_by_name("ATR_STAMINA_MAX");
      if(sta==nullptr || stm==nullptr || sta->type()!=zenkit::DaedalusDataType::INT || stm->type()!=zenkit::DaedalusDataType::INT)
        return;
      const int32_t top = stm->get_int()*10;
      if(sta->get_int()>=top)
        return;
      acc += float(std::max(top,360))/360.f;
      if(acc<1.f)
        return;
      const int32_t add = int32_t(acc);
      acc -= float(add);
      sta->set_int(std::clamp(sta->get_int()+add,0,top));
      });
    // RX_ToggleVobVisible(name, show) (0x1016f9b0): the vob of the name and its children are hidden or shown (the secrets
    // of the quests, EVT_XARDAS_SECRET_02). The collision follows the visual. Kept in the save-game
    bindNb("RX_TOGGLEVOBVISIBLE", [this](std::string_view name, int32_t show) {
      std::string key(world().name());
      key += '|';
      key += name;
      for(auto& c:key)
        c = char(std::toupper(static_cast<unsigned char>(c)));
      nbStates[this].vobVisual[key] = show!=0;
      const size_t n = world().setVobVisualTree(name,show!=0,show!=0);
      nbCtx("vob ",name,show!=0 ? " shown" : " hidden"," (",n,")");
      });
    // RX_ResetSummons() (0x1016b020, PerFrame when the hero dies, the change of a pet): the summoned creatures go away,
    // the pets stay (NIXDOG, PET_JINA)
    // NB fix 132: the change of the target in a fight (UNION_ABI.DLL 0x10183dc0..0x10184050). B_MM_AssessDamage and
    // B_AssessDamage: a character hit again by another enemy than its target turns to it at once (RX_CanReAttackNow) or
    // keeps it for later (RX_SetReAttackNpc); ZS_Attack_Loop / ZS_MM_Attack_Loop turn to the kept one. Without them the
    // default 0 kept every monster on its first enemy - a troll beaten by a golem chased only the hero
    {
    const auto slotIndex = [this]() -> int32_t {
      auto* s = vm.find_symbol_by_name("AIVRX_REATTACK_SLOT");
      return s!=nullptr && s->type()==zenkit::DaedalusDataType::INT ? s->get_int() : -1;
      };
    const auto keptNpc = [this](int32_t key) -> Npc* {
      auto& st = nbStates[this];
      auto  it = st.reattack.find(key);
      if(key<=0 || it==st.reattack.end())
        return nullptr;
      auto h = it->second.lock();
      if(h==nullptr) {
        st.reattack.erase(it);
        return nullptr;
        }
      return findNpc(h);
      };
    // NB fix 133: never for the player - the engine of the PC runs no states on the hero (he is steered by the player);
    // here ZS_Attack started on him, and the hero ran by himself at the one who hit him
    bindNb("RX_CANREATTACK", [this,slotIndex,keptNpc](std::shared_ptr<zenkit::INpc> ref) -> int32_t {
      auto* npc = findNpcRaw(ref);
      const int32_t slot = slotIndex();
      if(npc!=nullptr && npc->isPlayer())
        return 0;
      if(npc==nullptr || slot<0)
        return 1;
      auto* kept = keptNpc(npc->newBalanceGetNpcVar(slot));
      return kept==nullptr ? 1 : 0;
      });
    // at once, unless the character is in the middle of its own blow
    bindNb("RX_CANREATTACKNOW", [this](std::shared_ptr<zenkit::INpc> ref) -> int32_t {
      auto* npc = findNpcRaw(ref);
      if(npc!=nullptr && npc->isPlayer())
        return 0;   // NB fix 133
      return npc!=nullptr && npc->isAttackAnim() ? 0 : 1;
      });
    bindNb("RX_HASREATTACKNPC", [this,slotIndex,keptNpc](std::shared_ptr<zenkit::INpc> ref) -> int32_t {
      auto* npc = findNpcRaw(ref);
      const int32_t slot = slotIndex();
      if(npc==nullptr || slot<0 || npc->isPlayer())   // NB fix 133
        return 0;
      auto* kept = keptNpc(npc->newBalanceGetNpcVar(slot));
      return kept!=nullptr && !kept->isDead() ? 1 : 0;
      });
    bindNb("RX_SETREATTACKNPC", [this,slotIndex](std::shared_ptr<zenkit::INpc> ref, std::shared_ptr<zenkit::INpc> otherRef) {
      auto* npc = findNpcRaw(ref);
      auto* oth = findNpcRaw(otherRef);
      const int32_t slot = slotIndex();
      if(npc==nullptr || oth==nullptr || slot<0 || npc->isPlayer())   // NB fix 133
        return;
      auto& st  = nbStates[this];
      int32_t key = npc->newBalanceGetNpcVar(slot);
      if(key<=0) {
        key = st.reattackNext++;
        npc->newBalanceSetNpcVar(slot,key);
        }
      st.reattack[key] = oth->handlePtr();
      nbCtx("reattack: ",npc->displayName()," keeps ",oth->displayName());
      });
    bindNb("RX_GETREATTACKNPC", [keptNpc](int32_t key) -> std::shared_ptr<zenkit::INpc> {
      auto* kept = keptNpc(key);
      if(kept!=nullptr)
        nbCtx("reattack: turns to ",kept->displayName());
      return kept!=nullptr ? kept->handlePtr() : nullptr;
      });
    bindNb("RX_CLEARREATTACKNPC", [this](int32_t key) {
      nbStates[this].reattack.erase(key);
      });
    }
    // NB fix 138: the mining of New Balance (UNION_ABI.DLL 0x1016f620, 0x1016f4f0, 0x100b0d40). A vein keeps its ore in
    // the name of its vob: at the first blow the plugin gives it 95-105 (iron), 79-81 (gold) or 9-11 (magic ore) - a name
    // that is not a number, or too big, is given anew. RX_InteractMobOre(n) takes n from it (no more than is left) and
    // gives back what was taken; the scripts (PC_*Hacken_Addon_Hour_Info) give the hero the ore by it
    bindNb("RX_INTERACTMOBORE", [this](int32_t count) -> int32_t {
      auto* hero = world().player();
      auto* mob  = hero!=nullptr ? hero->interactive() : nullptr;
      if(mob==nullptr)
        return 0;
      const int32_t kind = nbOreKind(*mob);
      if(kind<0)
        return 0;
      int32_t stock = nbOreParse(*mob,kind);
      if(stock<0) {
        if(kind==0)
          stock = 95+int32_t(std::rand()%11); else
        if(kind==1)
          stock = 79+int32_t(std::rand()%3); else
          stock = 9+int32_t(std::rand()%3);
        }
      const int32_t taken = std::max(0,std::min(count,stock));
      mob->nbSetVobName(std::to_string(stock-taken));
      nbCtx("ore: ",mob->nbOnStateFunc()," ",stock," - ",taken," = ",stock-taken);
      return taken;
      });
    bindNb("RX_INTERACTMOBOREGETCOUNT", [this]() -> int32_t {
      auto* hero = world().player();
      auto* mob  = hero!=nullptr ? hero->interactive() : nullptr;
      return mob!=nullptr ? std::max(0,nbOreStock(*mob)) : 0;
      });
    // NB fix 139: RX_SetMaterialCollision(material, on) (0x10180ba0): the material of the world (and of the objects) stops
    // or starts to collide - the plugin sets the bit noCollDet of the zCMaterial; e.g. NB_MAT_WC_BOAT_ALPHA, the invisible
    // shell of the boat of the druid, that lets the hero on board only when the journey to the West Coast is agreed
    bindNb("RX_SETMATERIALCOLLISION", [](std::string_view material, int32_t on) {
      const bool changed = PhysicVbo::setMaterialCollision(material,on!=0);
      nbCtx("material ",material," collision ",on!=0 ? "on" : "off",changed ? "" : " (no mesh has it now)");
      });
    // NB fix 139: RX_MoverIsActive(name) (0x1008de00): the mover of this name is moving (opening or closing)
    bindNb("RX_MOVERISACTIVE", [this](std::string_view name) -> int32_t {
      int32_t ret = 0;
      world().forEachTrigger([&](AbstractTrigger& t) {
        if(ret!=0 || t.name()!=name)
          return;
        if(auto* mv = dynamic_cast<MoveTrigger*>(&t)) {
          const int32_t s = mv->moverState();
          ret = (s==1 || s==3) ? 1 : 0;
          }
        });
      return ret;
      });
    // NB fix 140: RX_CheckNpcStandsOnVob(npc, name) (0x1017f360 -> 0x10054b40): the plugin looks down from the character
    // and compares the name of the object under it - here: the character stands over the box of the object (a mover, a
    // trigger) of this name. The dialog "Pora odpływać" of Fregyal asks it for the hero and for him (EVT_DRUIDBOAT)
    bindNb("RX_CHECKNPCSTANDSONVOB", [this](std::shared_ptr<zenkit::INpc> ref, std::string_view name) -> int32_t {
      auto* npc = findNpc(ref);
      if(npc==nullptr || name.empty())
        return 0;
      std::string want(name);
      for(auto& ch:want)
        ch = char(std::toupper(uint8_t(ch)));
      int32_t ret = 0;
      bool    found = false;
      const auto pos = npc->position();
      world().forEachTrigger([&](AbstractTrigger& t) {
        if(ret!=0)
          return;
        std::string n(t.name());
        for(auto& ch:n)
          ch = char(std::toupper(uint8_t(ch)));
        if(n.find(want)==std::string::npos)
          return;
        found = true;
        if(t.nbStandsOn(pos))
          ret = 1;
        });
      nbCtx("stands on ",want,": ",npc->displayName()," ",ret!=0 ? "yes" : "no",found ? "" : " (no object of this name)");
      return ret;
      });
    bindNb("RX_RESETSUMMONS", [this]() {
      nbResetSummons(false,false);
      });
    // RX_SetFXNpc(npc) (0x1017ab90): only remembers the character for the effects of the plugin
    bindNb("RX_SETFXNPC", [](std::shared_ptr<zenkit::INpc>) {});
    // NB fix 126: RX_DamageCalcRunes() (0x1017bf70 -> 0x10083400): the damage and the cost of the spells by the
    // intelligence of the hero (RX_ChangeInt, the curses, the heal spells of the paladins)
    bindNb("RX_DAMAGECALCRUNES", [this]() {
      auto& st = nbStates[this];
      const uint64_t h = nbDamageCalcRunes();
      if(h!=0 && h!=st.spellsHash) {
        st.spellsHash  = h;
        st.spellsDirty = true;   // the spells are made again at the next tick, not inside the script
        }
      });
    // NB fix 124: RX_ClearCurrentDialog (RX_ClearDialog - the choice "...(Wróć)" of the menus of NB: the shrines, the
    // tables): the choices of the info that runs are cleared, the dialog goes back to its list
    bindNb("RX_CLEARCURRENTDIALOG", [this]() {
      if(auto* info = nbStates[this].curInfo)
        info->choices.clear();
      });
    bindNb("RX_CLEARINPUT",      []() {});
    bindNb("RX_MULT_SETPARTYNPC",     [](std::shared_ptr<zenkit::INpc>) {});
    bindNb("MDL_SETHEROANIFPS",       [](std::shared_ptr<zenkit::INpc>, std::string_view, float) {});
    bindNb("INFOSTATADD",             [](std::shared_ptr<zenkit::INpc>, int32_t, int32_t) {});
    // NB fix 51: the notes and the books of the plugin. The scripts put the text and the texture in global variables
    // (RX_SetNoteType, RX_SetBookType) and open the window: here a document of OpenGothic with the texture of NB
    const auto nbStr = [this](std::string_view name) -> std::string {
      auto* s = vm.find_symbol_by_name(name);
      return s!=nullptr && s->type()==zenkit::DaedalusDataType::STRING ? std::string(s->get_string()) : std::string();
      };
    const auto nbInt = [this](std::string_view name) -> int32_t {
      auto* s = vm.find_symbol_by_name(name);
      return s!=nullptr && s->type()==zenkit::DaedalusDataType::INT ? s->get_int() : 0;
      };
    bindNb("RX_OPENNOTE", [nbStr,nbInt]() {
      DocumentMenu::Show doc;
      doc.nb.img  = nbStr("RX_SCROLLTEXTTEXTURE");
      doc.nb.text = nbPlainText(nbStr("RX_SCROLLTEXT"));
      doc.nb.w    = float(std::clamp(nbInt("RX_SCROLLSIZEX"),5,100))/100.f;
      doc.nb.h    = float(std::clamp(nbInt("RX_SCROLLSIZEY"),5,100))/100.f;
      // the plugin has no place of the text for the notes: the middle of the sheet
      doc.nb.columns.push_back({0.14f,0.11f,0.86f,0.89f});
      if(doc.nb.img.empty())
        return;
      nbCtx("note: ",doc.nb.img);
      Gothic::inst().onShowDocument(doc);
      });
    const auto nbBook = [nbStr,nbInt](int32_t sizeX, int32_t sizeY) {
      DocumentMenu::Show doc;
      sizeX = std::clamp(sizeX,5,100);
      sizeY = std::clamp(sizeY,5,100);
      doc.nb.img  = nbStr("RX_BOOKMANAGERTEXTURE");
      doc.nb.text = nbPlainText(nbStr("RX_BOOKMANAGERTEXT"));
      doc.nb.w    = float(sizeX)/100.f;
      doc.nb.h    = float(sizeY)/100.f;
      // the places of the pages are in the virtual units of the screen (8192), from the corner of the book
      const float vw = 8192.f*float(sizeX)/100.f;
      const float vh = 8192.f*float(sizeY)/100.f;
      const float lx0 = float(nbInt("RX_BOOKMANAGER_LP_STARTX"));
      const float lx1 = float(nbInt("RX_BOOKMANAGER_LP_ENDX"));
      const float ly0 = float(nbInt("RX_BOOKMANAGER_LP_STARTY"));
      const float ly1 = float(nbInt("RX_BOOKMANAGER_LP_ENDY"));
      const float rx0 = float(nbInt("RX_BOOKMANAGER_RP_STARTX"));
      if(lx1>lx0 && ly1>ly0 && rx0>=lx1) {
        doc.nb.columns.push_back({lx0/vw,ly0/vh,lx1/vw,ly1/vh});
        doc.nb.columns.push_back({rx0/vw,ly0/vh,(rx0+lx1-lx0)/vw,ly1/vh});
        } else {
        doc.nb.columns.push_back({0.08f,0.12f,0.46f,0.88f});
        doc.nb.columns.push_back({0.54f,0.12f,0.92f,0.88f});
        }
      if(doc.nb.img.empty())
        return;
      nbCtx("book: ",doc.nb.img);
      Gothic::inst().onShowDocument(doc);
      };
    bindNb("RX_TOGGLEBOOKMANAGERWITHSIZE", [nbBook](int32_t sizeX, int32_t sizeY) { nbBook(sizeX,sizeY); });
    bindNb("RX_TOGGLEBOOKMANAGER",         [nbBook]() { nbBook(80,68); });
    // NB fix 51: the death of a character - the function of the death of its instance (AI_OnDead_<instance>, 310 of them
    // in the scripts: the quests, the bosses) and AI_OnDead_Common, as UNION_ABI.DLL 0x100511F0 does it.
    // The bit 0x400 of the variable 71 of NB (BIT_KILL_FUNC): the function of the instance ran already.
    bindNb("RX_CALLONDEAD", [this](std::shared_ptr<zenkit::INpc> otherRef, std::shared_ptr<zenkit::INpc> selfRef) {
      auto* self = findNpcRaw(selfRef);
      if(self==nullptr || self->attribute(ATR_HITPOINTS)>0)
        return;
      const int32_t flags = self->newBalanceGetNpcVar(71);
      if((flags & 0x400)!=0)
        return;
      auto call = [this](zenkit::DaedalusSymbol* fn) {
        if(fn==nullptr || fn->type()!=zenkit::DaedalusDataType::FUNCTION)
          return false;
        try {
          if(fn->has_return())
            (void)vm.call_function<int>(fn); else
            vm.call_function<void>(fn);
          }
        catch(const std::exception& e) {
          Log::e("NB: ",fn->name()," failed: ",e.what());
          }
        return true;
        };
      ScopeVar sv(*vm.global_self(), selfRef);
      ScopeVar ov(*vm.global_other(), otherRef);
      if(self->newBalanceGetNpcVar(158)!=0) {
        self->newBalanceSetNpcVar(122,0);
        call(vm.find_symbol_by_name("RX_AST_REMOVETRADEGOODS"));
        }
      auto* inst = vm.find_symbol_by_index(selfRef->symbol_index());
      auto* own  = inst!=nullptr ? vm.find_symbol_by_name("AI_ONDEAD_"+inst->name()) : nullptr;
      if(call(own))
        self->newBalanceSetNpcVar(71,self->newBalanceGetNpcVar(71) | 0x400);
      call(vm.find_symbol_by_name("AI_ONDEAD_COMMON"));
      auto* killer = findNpcRaw(otherRef);
      nbCtx("death: ",self->displayName(),(own!=nullptr ? " (" : ""),(own!=nullptr ? own->name() : std::string()),
            (own!=nullptr ? ")" : "")," killer: ",(killer==nullptr ? std::string_view("-") : killer->isPlayer() ? std::string_view("hero") : killer->displayName()));
      });
    // NB fix 51: an information (a dialog) that goes to another NPC (UNION_ABI.DLL 0x1017A3E0: oCInfo::npc = the instance);
    // ZS_Talk gives so the dialogs of the poker, Xardas the mask of the sleeper...
    bindNb("RX_SETDIALOGNPC", [this](std::shared_ptr<zenkit::INpc> npcRef, std::string_view infoName) {
      auto* sym = vm.find_symbol_by_name(infoName);
      if(npcRef==nullptr || sym==nullptr)
        return;
      for(auto& info:dialogsInfo)
        if(info!=nullptr && info->symbol_index()==sym->index())
          info->npc = int32_t(npcRef->symbol_index());
      });
    // a weapon of dexterity (rapier, sabre): the bit BIT_ITEM_DEX_SWORD (1<<26) of C_Item.hp, see SetItemVarTrue
    bindNb("HLP_ISDEXSWORD", [](std::shared_ptr<zenkit::IItem> itm) -> int32_t {
      return itm!=nullptr && ((uint32_t(itm->hp)>>26) & 1u)!=0 ? 1 : 0;
      });
    bindNb("GAME_PLAY3DSOUND", [this](std::shared_ptr<zenkit::INpc> npcRef, std::string_view file) { snd_play3d(npcRef,file); });
    // NB fix 56 (batch 4): the world. RX_InsertVobTree puts the objects of a .ZEN into the world (137 calls: beds, chests,
    // decorations, items of the quests), RX_RemoveVobTree takes them away again
    bindNb("RX_INSERTVOBTREE", [this](std::string_view path) { nbInsertVobTree(path); });
    bindNb("RX_REMOVEVOBTREE", [this](std::string_view path) {
      auto& st   = nbStates[this];
      auto& wrld = world();
      nbVobTreesSync(st,wrld);
      const auto file = nbFileName(path);
      for(size_t i=0; i<st.vobTrees.size(); ++i) {
        auto& t = st.vobTrees[i];
        if(t.world!=wrld.name() || t.file!=file)
          continue;
        delete t.vobs;   // its world lives: the objects leave it
        st.vobTrees.erase(st.vobTrees.begin()+int(i));
        nbCtx("part of the world removed: ",file);
        return;
        }
      // NB fix 153: a name of an object of the world (EVT_SMOKE_00/01: the red smoke of the West Coast, a mover with
      // particles) - the plugin takes it away from the world; here it is hidden with its children, as RX_ToggleVobVisible
      // does, and stays hidden in the save-game
      if(!FileExt::hasExt(path,"ZEN")) {
        std::string key(wrld.name());
        key += '|';
        key += path;
        for(auto& c:key)
          c = char(std::toupper(static_cast<unsigned char>(c)));
        const size_t n = wrld.setVobVisualTree(path,false,false);
        if(n>0) {
          st.vobVisual[key] = false;
          nbCtx("object of the world removed (hidden): ",path," (",n,")");
          return;
          }
        }
      // a part of the .ZEN of the world itself: OpenGothic cannot take it away
      Log::i("NB: RX_RemoveVobTree: \"",file,"\" was not inserted by the scripts - skipped");
      });
    // the chests (the containers) of the world by their name
    const auto nbMobs = [this](std::string_view tag, auto fn) {
      for(uint32_t i=0; auto* mob = world().mobsiById(i); ++i) {
        const auto name = mob->tag();
        const bool same = name.size()==tag.size() && std::equal(name.begin(),name.end(),tag.begin(),[](char a, char b) {
          return std::toupper(static_cast<unsigned char>(a))==std::toupper(static_cast<unsigned char>(b));
          });
        if(same)
          fn(*mob);
        }
      };
    bindNb("MOB_REMOVEALLITEMS", [this,nbMobs](std::string_view tag) {
      nbMobs(tag,[this](Interactive& mob) {
        if(mob.isContainer())
          mob.inventory().clear(*this,mob,true);
        });
      });
    // NB fix 218: Mob_SetLockedByName(name, locked) (0x101cb140): oCMobLockable::SetLocked of the chests or the doors of this
    // name (the three chests of the temple of Ashtar: the chosen one opens, the two others are emptied and opened)
    bindNb("MOB_SETLOCKEDBYNAME", [nbMobs](std::string_view tag, int32_t locked) {
      nbMobs(tag,[locked](Interactive& mob) {
        if(mob.isContainer() || mob.isDoor())
          mob.nbSetLocked(locked!=0);
        });
      });
    // NB fix 218: Mob_RemoveItem_AB(name, item, count) (0x101a78e0): the items of this kind leave the chest (the bet of the
    // race of Orlan: the gold of Rukhar)
    bindNb("MOB_REMOVEITEM_AB", [this,nbMobs](std::string_view tag, int32_t item, int32_t count) {
      if(item<=0 || count<=0)
        return;
      bool done = false;
      nbMobs(tag,[&](Interactive& mob) {
        if(done || !mob.isContainer() || mob.inventory().itemCount(size_t(item))==0)
          return;
        Inventory gone;
        Inventory::transfer(gone,mob.inventory(),nullptr,size_t(item),size_t(count),world());
        done = true;
        });
      });
    // NB fix 218: Wld_SetCondFunc(name, func) (0x101bcaa0): the condition function of the mobs of this name (the chests of
    // the temple of Ashtar: CondFunc_Thief_Chest...) - Interactive asks it before the use
    bindNb("WLD_SETCONDFUNC", [nbMobs](std::string_view tag, std::string_view fn) {
      std::string up(fn);
      for(auto& c:up)
        c = char(std::toupper(static_cast<unsigned char>(c)));
      nbMobs(tag,[&up](Interactive& mob) { mob.nbSetConditionFunc(up); });
      });
    // NB fix 218: Wld_ChangePortal(room, guild) (0x101c7a90): the owner guild of the portal room, as Wld_AssignRoomToGuild
    // (the towers of the city become public for a scout of the militia)
    bindNb("WLD_CHANGEPORTAL", [this](std::string_view room, int32_t guild) {
      world().assignRoomToGuild(room,guild);
      });
    // NB fix 218: AI_TurnToVob(npc, name, ?) (0x101e1a30): in the queue - the character turns to the vob of this name (a way
    // point, a free point, a mover or a mob: the wall of the hidden room of Nefarius)
    bindNb("AI_TURNTOVOB", [this,nbMobs](std::shared_ptr<zenkit::INpc> npcRef, std::string_view name, int32_t) {
      auto* npc = findNpc(npcRef);
      if(npc==nullptr)
        return;
      bool          found = false;
      Tempest::Vec3 pos;
      if(auto* wp = world().findPoint(name,false)) {
        pos   = wp->position();
        found = true;
        }
      if(!found) {
        world().forEachTrigger([&](AbstractTrigger& t) {
          if(!found && t.name()==name) {
            pos   = t.position();
            found = true;
            }
          });
        }
      if(!found) {
        nbMobs(name,[&](Interactive& mob) {
          if(!found) {
            pos   = mob.position();
            found = true;
            }
          });
        }
      if(!found) {
        Log::e("NB: AI_TurnToVob - no vob ",name);
        return;
        }
      npc->aiPush(AiQueue::aiTurnToPos(int(pos.x),int(pos.z)));
      });
    // a locked chest/door opened by the scripts (0) or closed again (1): only what the world has made with a lock
    const auto nbLock = [nbMobs](std::string_view tag, int32_t lock) {
      nbMobs(tag,[lock](Interactive& mob) { mob.setAsCracked(lock==0); });
      };
    bindNb("RX_LOCKCHEST", [nbLock](std::string_view tag, int32_t lock) { nbLock(tag,lock); });
    bindNb("RX_LOCKDOOR",  [nbLock](std::string_view tag, int32_t lock) { nbLock(tag,lock); });
    // a trigger on (1) or off (0), e.g. the change of the level to the secret island after the key of Hagen
    bindNb("RX_ENABLETRIGGER", [this](std::string_view name, int32_t on) {
      TriggerEvent e(std::string(name),"",on!=0 ? TriggerEvent::T_Enable : TriggerEvent::T_Disable);
      world().triggerEvent(e);
      });
    // the goods of a trader (C_Item.hp & BIT_ITEM_REMOVE_TRADE, 1<<25) leave his body; not the bows and crossbows
    // (UNION_ABI.DLL 0x1017F1C0)
    bindNb("RX_REMOVEITEMSONNPCBEAT", [this](std::shared_ptr<zenkit::INpc> npcRef) {
      auto* npc = findNpc(npcRef);
      if(npc==nullptr)
        return;
      std::vector<std::pair<size_t,size_t>> del;
      for(auto it = npc->inventory().iterator(Inventory::T_Inventory); it.isValid(); ++it) {
        auto& itm = *it;
        const int32_t fl = int32_t(itm.handle().flags);
        if((uint32_t(fl) & uint32_t(ITM_BOW | ITM_CROSSBOW))!=0)
          continue;
        if((uint32_t(itm.handle().hp) & (1u<<25))!=0)
          del.emplace_back(itm.clsId(),it.count());
        }
      for(auto& [cls,cnt]:del)
        npc->delItem(cls,uint32_t(cnt));
      });
    // the crafting of the plugin: the item just made (CRAFT_ITEM)
    bindNb("RX_ISITEMCRAFT", [this](int32_t id) -> int32_t {
      auto* s = vm.find_symbol_by_name("CRAFT_ITEM");
      return s!=nullptr && s->type()==zenkit::DaedalusDataType::INT && s->get_int()==id ? 1 : 0;
      });
    // a character that steps aside in the fight (the monsters attack it then, B_MM_AssessEnemy)
    bindNb("RX_NPCISSTRAFING", [this](std::shared_ptr<zenkit::INpc> npcRef) -> int32_t {
      auto* npc = findNpc(npcRef);
      if(npc==nullptr)
        return 0;
      for(auto& l:npc->animationLayers())
        if(l.name.find("STRAFE")!=std::string::npos)
          return 1;
      return 0;
      });
    // the waits of the plugin (its own queue of the AI) - OpenGothic has none of them
    // NB fix 101: the inactive phases of the bosses (RX_SetInactive: AI_Wait for 500 s, RX_SetActive ends it)
    bindNb("RX_ISWAITING", [this](std::shared_ptr<zenkit::INpc> ref) -> int32_t {
      auto* npc = findNpcRaw(ref);
      return npc!=nullptr && npc->nbIsWaiting() ? 1 : 0;
      });
    bindNb("RX_STOP_AI_WAIT", [this](std::shared_ptr<zenkit::INpc> ref) {
      if(auto* npc = findNpcRaw(ref))
        npc->nbStopWait();
      });
    bindNb("UPDATESPEED", [this](std::shared_ptr<zenkit::INpc> ref, float speed) {
      if(auto* npc = findNpcRaw(ref))
        npc->nbSpeed = (std::isfinite(speed) && speed>0.f) ? std::clamp(speed,0.2f,3.f) : 1.f;
      });
    // the summoned creatures of the hero: they stop, while a boss cannot be hurt (RX_SetInactive)
    const auto forSummons = [this](auto fn) {
      auto* hero = world().player();
      if(hero==nullptr)
        return;
      world().detectNpc(hero->position(),5000.f,[&](Npc& n) {
        if(!n.isDead() && !n.isPlayer() && nbIsSummon(n))
          fn(n);
        });
      };
    bindNb("AI_CLEARAIQUEUE_SUMMONED", [forSummons]() { forSummons([](Npc& n) { n.clearAiQueue(); }); });
    bindNb("AI_WAIT_SUMMONED", [forSummons](int32_t sec) {
      forSummons([sec](Npc& n) { if(sec>0) n.aiPush(AiQueue::aiWait(uint64_t(sec)*1000)); });
      });
    bindNb("AI_STANDUP_SUMMONED", [forSummons]() { forSummons([](Npc& n) { n.aiPush(AiQueue::aiStandup()); }); });

    // NB fix 227: the summons of the necromancer (the plugin 0x103f63d0: the summoned creatures of the hero, aivar[91] ==
    // RX_SUMMON_ID; the undead ones: GIL_SUMMONED_SKELETON 37, GIL_SUMMONED_GOBBO_SKELETON 26, guild 55)
    const auto forUndeadSummons = [forSummons](auto fn) {
      forSummons([&](Npc& n) {
        const int32_t g = n.handle().guild;
        if(g==37 || g==26 || g==55)
          fn(n);
        });
      };
    // RX_SummonHeal (0x101cfd20 -> 0x103f70f0): the spell gives the life of the hero to his undead: it costs 150 points of
    // life for each circle of magic, every hurt one gets five times as much back (SPELLFX_DARKAURAEFFECT). Not enough life:
    // RX_NecroSummonerNoHealth and the price
    bindNb("RX_SUMMONHEAL", [this,forUndeadSummons]() {
      auto* hero = world().player();
      if(hero==nullptr)
        return;
      std::vector<Npc*> list;
      forUndeadSummons([&](Npc& n) { list.push_back(&n); });
      if(list.empty())
        return;
      const int32_t cost = hero->talentSkill(TALENT_MAGE)*150;
      const int32_t heal = cost*5;
      if(hero->attribute(ATR_HITPOINTS)<=cost) {
        Gothic::inst().onPrint(std::string(NbTexts::get("RX_NecroSummonerNoHealth"))+std::to_string(cost));
        return;
        }
      hero->changeAttribute(ATR_HITPOINTS,-cost,false);
      auto* vfx = Gothic::inst().loadVisualFx("SPELLFX_DARKAURAEFFECT");
      int healed = 0;
      for(auto* n:list) {
        if(n->attribute(ATR_HITPOINTS)>=n->attribute(ATR_HITPOINTSMAX))
          continue;
        n->changeAttribute(ATR_HITPOINTS,heal,false);
        if(vfx!=nullptr)
          n->startEffect(*n,*vfx);
        ++healed;
        }
      nbCtx("summon heal: ",cost," life of the hero, +",heal," to ",healed," of ",list.size());
      });
    // RX_ApplyBuffToSummons(mode) (0x101cfce0 -> 0x103f6820), the auras of the necromancer:
    //  0 - RX_NecroRuneAuraActive_03 and RX_NecroRuneAura_2: the undead summons are faster, 100% + 5% for each circle
    //      (AIVRX_NPC_SPEED), SPELLFX_DARKAURAEFFECT; off - back to 100%
    //  1 - RX_NecroRuneAuraActive_04 and RX_NecroRuneAura_3: the others around (not the summons, the friends, the party, the
    //      bosses, a frozen one) are slower by countLearnSpell/2 + 5 % for an hour (AIVRX_NPC_SLOW, _SLOWTIME),
    //      SPELLFX_SLOWDOWN_VIS; off - the slowness ends
    bindNb("RX_APPLYBUFFTOSUMMONS", [this,forUndeadSummons](int32_t mode) {
      auto* hero = world().player();
      if(hero==nullptr)
        return;
      if(mode==0) {
        const bool on = nbIntVar("RX_NECRORUNEAURAACTIVE_03")!=0 && nbIntVar("RX_NECRORUNEAURA_2")!=0;
        const float k = on ? float(100+5*hero->talentSkill(TALENT_MAGE))/100.f : 1.f;
        auto* vfx = on ? Gothic::inst().loadVisualFx("SPELLFX_DARKAURAEFFECT") : nullptr;
        int cnt = 0;
        forUndeadSummons([&](Npc& n) {
          n.nbAuraSpeed = k;
          n.nbAuraUntil = 0;
          if(vfx!=nullptr)
            n.startEffect(n,*vfx);
          ++cnt;
          });
        nbCtx("aura of the summons: speed ",int(k*100),"% for ",cnt);
        }
      else if(mode==1) {
        const bool on = nbIntVar("RX_NECRORUNEAURAACTIVE_04")!=0 && nbIntVar("RX_NECRORUNEAURA_3")!=0;
        const int32_t slow = nbIntVar("COUNTLEARNSPELL")/2+5;
        auto* vfx = on ? Gothic::inst().loadVisualFx("SPELLFX_SLOWDOWN_VIS") : nullptr;
        int cnt = 0;
        world().detectNpc(hero->position(),2000.f,[&](Npc& n) {
          if(n.isPlayer() || n.isDead() || n.isUnconscious() || nbIsSummon(n))
            return;
          auto& h = n.handle();
          if(int(h.type)==2 || h.aivar[15]!=0 || h.aivar[90]!=0 || personAttitude(n,*hero)==ATT_FRIENDLY)
            return;   // a friend, a member of the party, a boss
          if(n.nbStateName()=="ZS_MAGICFREEZE")
            return;
          if(on) {
            n.nbAuraSpeed = std::max(0.2f,1.f-float(slow)/100.f);
            n.nbAuraUntil = tickCount()+3600000;
            if(vfx!=nullptr)
              n.startEffect(n,*vfx);
            } else {
            n.nbAuraSpeed = 1.f;
            n.nbAuraUntil = 0;
            }
          ++cnt;
          });
        nbCtx("aura of slowness: ",on ? slow : 0,"% for ",cnt);
        }
      });
    // RX_IsValidTargetToSummon / RX_SummonNpcToTarget(instance, time) (0x101b8930, 0x101b8950): with KEY_NONTARGET_ACTION
    // the plugin shows a mark on the ground under the sight and summons the creature there (oCSpawnManager::SummonNpc).
    // The port has no such mark: the target is the character in the focus of the hero, else 5 m before him
    bindNb("RX_ISVALIDTARGETTOSUMMON", [this]() -> int32_t {
      auto it = nbStates.find(this);
      auto* f = it!=nbStates.end() ? findNpc(it->second.focusNpc.lock()) : nullptr;
      return (f!=nullptr && !f->isDead()) ? 1 : 0;
      });
    bindNb("RX_SUMMONNPCTOTARGET", [this](int32_t instance, float) {
      auto* hero = world().player();
      if(hero==nullptr || instance<=0)
        return;
      auto it = nbStates.find(this);
      auto* f = it!=nbStates.end() ? findNpc(it->second.focusNpc.lock()) : nullptr;
      Tempest::Vec3 at;
      if(f!=nullptr && !f->isDead()) {
        at = f->position();
        } else {
        const float a = hero->rotationRad();
        at = hero->position()+Tempest::Vec3(std::cos(a)*500.f,0.f,std::sin(a)*500.f);
        }
      auto* npc = world().addNpc(size_t(instance),at);
      if(npc==nullptr)
        return;
      fixNpcPositionNear(*npc,at,hero->rotation()+180.f,150.f);
      nbCtx("summon at the target: ",npc->displayName(),f!=nullptr ? std::string(" at ")+std::string(f->displayName()) : std::string(" before the hero"));
      });
    // RX_GetArenaSummonCount (0x101d06b0): the summons of the hero in the arena (a counter of the plugin) - those alive
    bindNb("RX_GETARENASUMMONCOUNT", [forSummons]() -> int32_t {
      int32_t n = 0;
      forSummons([&](Npc&) { ++n; });
      return n;
      });
    // RX_AgroNpcOnSummon(self, other) (0x101d8620): only with ISNEWLEGFEATURES (the "realmode" of the plugin - off in the
    // port): the monsters around the attacked summon join the fight against it. RX_AgroNpcOnSummon_Clear ends it
    bindNb("RX_AGRONPCONSUMMON",       [](std::shared_ptr<zenkit::INpc>, std::shared_ptr<zenkit::INpc>) {});
    bindNb("RX_AGRONPCONSUMMON_CLEAR", [](std::shared_ptr<zenkit::INpc>) {});

    // the plugin refreshes the texts of its window of the inventory; the summons of the multiplayer
    bindNb("UPDATEITEMSINFOININV", []() {});
    // NB fix 59: the change of the world by the scripts (teleport stones and runes of NB, the transfer items, the
    // temple of Azgalor, the valley of Adanos): Wld_ChangeLevel_AB(zen, waypoint) - UNION_ABI.DLL 0x10159E20 calls
    // oCGame::TriggerChangeLevel, as the triggers of the passes do. Without it the teleport did nothing
    bindNb("WLD_CHANGELEVEL_AB", [this](std::string_view zen, std::string_view wp) {
      if(zen.empty())
        return;
      nbCtx("change of the world: ",zen," -> ",wp);
      world().triggerChangeWorld(zen,wp);
      });
    bindNb("RX_ISREMOTESUMMON",    [](std::shared_ptr<zenkit::INpc>) -> int32_t { return 0; });
    // NB fix 68: the window to sell items for given prices (Hanna - jewels and caskets, Kardif - raw meat, Halvor -
    // pearls, shells and valuables, Kreol, Xardas as a necromancer...): UNION_ABI.DLL 0x1017EC40 - the craft window
    // of the plugin in the mode 13. Here: the trade window with only these items of the player and these prices
    bindNb("RX_CRAFT_PLAYERSALES_CLEARPRODUCTLIST", [this]() {
      nbStates[this].salesList.clear();
      });
    bindNb("RX_CRAFT_PLAYERSALES_ADDTOPRODUCTLIST", [this](std::string_view name, int32_t price) {
      std::string up(name);
      for(auto& c:up)
        c = char(std::toupper(uint8_t(c)));
      auto* sym = vm.find_symbol_by_name(up);
      if(!nbIsItemSymbol(vm,sym)) {
        Log::e("NB: RX_Craft_PlayerSales_AddToProductList: no item \"",name,"\"");
        return;
        }
      Gothic::NbSaleItem it;
      it.cls   = sym->index();
      it.price = price;
      // the name of the item for the window (the instance is made once, as for ITMI_GOLD)
      if(auto inst = vm.init_instance<zenkit::IItem>(sym); inst!=nullptr)
        it.name = inst->name;
      if(it.name.empty())
        it.name = up;
      nbStates[this].salesList.push_back(std::move(it));
      });
    bindNb("RX_OPENCRAFT_PLAYERSALES", [this](std::shared_ptr<zenkit::INpc> npc) {
      auto& st = nbStates[this];
      st.salesNpc     = npc;
      st.salesPending = (npc!=nullptr);
      nbCtx("sales window: ",st.salesList.size()," items");
      });
    // RX_Form_Const_SellItems / RX_Form_Alfred_SellItems: the same window, the list is fixed in the plugin
    // (UNION_ABI.DLL, tables at 0x105692c8 and 0x10568b38); SELF is the trader (RX_CallbackDialog sets it)
    struct NbFixedSale { const char* name; int32_t price; int group; };
    const auto fixedSales = [this](const char* who, std::initializer_list<const char*> groups,
                                   std::initializer_list<NbFixedSale> items, bool ownedOnlyCrabAnt) {
      auto& st = nbStates[this];
      st.salesList.clear();
      auto* hero = world().player();
      for(auto& fs:items) {
        std::string up(fs.name);
        for(auto& c:up)
          c = char(std::toupper(uint8_t(c)));
        auto* sym = vm.find_symbol_by_name(up);
        if(!nbIsItemSymbol(vm,sym)) {
          Log::e("NB: ",who," sales: no item \"",fs.name,"\"");
          continue;
          }
        // the plugin offers the crab meat and the ant mandibles only to who has them
        if(ownedOnlyCrabAnt && (up=="ITFO_CRABRAW" || up=="ITAT_ANTMANDIBULES") &&
           (hero==nullptr || hero->itemCount(sym->index())==0))
          continue;
        Gothic::NbSaleItem it;
        it.cls   = sym->index();
        it.price = fs.price;
        if(fs.group>=0 && size_t(fs.group)<groups.size())
          it.group = *(groups.begin()+fs.group);
        if(auto inst = vm.init_instance<zenkit::IItem>(sym); inst!=nullptr)
          it.name = inst->name;
        if(it.name.empty())
          it.name = up;
        st.salesList.push_back(std::move(it));
        }
      auto self = std::dynamic_pointer_cast<zenkit::INpc>(vm.global_self()->get_instance());
      st.salesNpc     = self;
      st.salesPending = (self!=nullptr);
      nbCtx(who," sales window: ",st.salesList.size()," items",self==nullptr ? " (no SELF)" : "");
      };
    // the categories: the numbers of the tables of the plugin, the names (cp1250) as the groups of its window
    bindNb("RX_FORM_CONST_SELLITEMS", [fixedSales]() {
      fixedSales("Constantino",{"Mikstury","Rzadkie ro\x9Cliny","Ro\x9Cliny","Trofea","Serca golem\xF3w"},{
        {"ITPO_PERM_STR",800,0},{"ITPO_PERM_DEX",800,0},{"ITPO_PERM_HEALTH",1000,0},{"ITPO_PERM_HEALTH_CRAFT",1200,0},
        {"ITPO_PERM_MANA",800,0},{"ITPO_PERM_MANA_CRAFT",1000,0},{"ITPO_PERM_STAMINA",800,0},
        {"ITPL_MANA_HERB_03",20,1},{"ITPL_HEALTH_HERB_03",30,1},{"ITPL_TEMP_HERB",15,1},{"ITPL_SPEED_HERB_01",50,1},
        {"ITPL_DEX_HERB_01",300,1},{"ITPL_STRENGTH_HERB_01",300,1},{"ITPL_PERM_HERB",2000,1},{"ITPL_SUPER_HERB",200,1},
        {"ITPL_HEALTH_HERB_01",10,2},{"ITPL_HEALTH_HERB_02",20,2},{"ITPL_MANA_HERB_01",7,2},{"ITPL_MANA_HERB_02",15,2},
        {"ITPL_MUSHROOM_01",5,2},{"ITPL_MUSHROOM_02",10,2},
        {"ITAT_BUGMANDIBLES",20,3},{"ITAT_CRAWLERMANDIBLES",30,3},{"ITAT_SPIDERMANDIBLES",120,3},{"ITFO_CRABRAW",30,3},
        {"ITAT_ANTMANDIBULES",25,3},{"ITAT_OREBUGCLAW",50,3},{"ITAT_WING",15,3},{"ITAT_STING",25,3},{"ITAT_ADDON_BCKOPF",40,3},
        {"ITAT_FIREGOLEMHEART",200,4},{"ITAT_ICEGOLEMHEART",200,4},{"ITAT_SWAMPGOLEMHEART",200,4},{"ITAT_STONEGOLEMHEART",200,4},
        },true);
      });
    bindNb("RX_FORM_ALFRED_SELLITEMS", [fixedSales]() {
      fixedSales("Alfred",{"Sk\xF3ry","Z\xEA""by i pazury","Rogi"},{
        {"ItAt_SheepFur",5,0},{"itat_rabbitfur",3,0},{"ItAt_WolfFur",8,0},{"ItAt_WargFur",30,0},{"ItAt_Addon_KeilerFur",15,0},
        {"ItAt_ShadowFur",90,0},{"ItAt_TrollBlackFur",1000,0},{"ItAt_TrollFur",175,0},{"itat_LurkerSkin",12,0},
        {"ItAt_SharkSkin",40,0},{"ItAt_IceWolfFur",30,0},{"ItAt_OrcDogFur",35,0},{"itat_pumafur",100,0},{"ItAt_WhitePuma",100,0},
        {"ItAt_Teeth",8,1},{"ItAt_Claw",5,1},{"ItAt_TrollTooth",60,1},{"ItAt_LurkerClaw",12,1},{"ItAt_CrawlerPlate",25,1},
        {"ItAt_SharkTeeth",25,1},{"ItAt_DesertSharkTeeth",50,1},{"ItAt_DrgSnapperHorn",50,2},{"ItAt_ShadowHorn",75,2},
        },false);
      });
    // the items of the scripts by name, equipping, the kind of a weapon
    bindNb("CREATEINVITEMSBYSTRING", [this](std::shared_ptr<zenkit::INpc> npcRef, std::string_view inst, int32_t count) {
      if(auto* sym = vm.find_symbol_by_name(inst); sym!=nullptr && count>0)
        createinvitems(npcRef,int(sym->index()),count);
      });
    bindNb("NPC_EQUIPITEM", [this](std::shared_ptr<zenkit::INpc> npcRef, int32_t id) {
      if(auto npc = findNpc(npcRef); npc!=nullptr && id>0)
        npc->equipItem(size_t(id));
      });
    bindNb("NPC_UNEQUIPITEM", [this](std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::IItem> itm) {
      if(auto npc = findNpc(npcRef); npc!=nullptr && itm!=nullptr)
        npc->unequipItem(itm->symbol_index());
      });
    bindNb("ITM_ONEHANDED", [](std::shared_ptr<zenkit::IItem> itm) -> int32_t {
      // ITEM_SWD (1<<14) or ITEM_AXE (1<<15) of the melee weapons of one hand
      return itm!=nullptr && (uint32_t(itm->flags) & ((1u<<14)|(1u<<15)))!=0 ? 1 : 0;
      });
    // NB fix 106: the damage over time - (target, attacker, ticks, damage of a tick, DAM_*, interval in ms, TRUE: renew
    // the same one instead of a second one); NotKill leaves 1 point of health
    const auto addDot = [this](std::shared_ptr<zenkit::INpc> target, std::shared_ptr<zenkit::INpc> attacker, int32_t ticks,
                               int32_t damage, int32_t type, int32_t interval, int32_t renew, bool notKill) {
      auto nb = nbStates.find(this);
      if(nb==nbStates.end() || target==nullptr || ticks<=0 || damage<=0)
        return;
      auto& dots = nb->second.dots;
      NbState::Dot d;
      d.target   = target;
      d.attacker = attacker;
      d.ticks    = ticks;
      d.damage   = damage;
      d.type     = type;
      d.interval = uint64_t(std::clamp(interval,100,60000));
      d.next     = tickCount()+d.interval;
      d.notKill  = notKill;
      if(renew!=0) {
        for(auto& o:dots) {
          if(o.target.lock()==target && o.attacker.lock()==attacker && o.type==type) {
            o = d;
            return;
            }
          }
        }
      if(dots.size()<200)
        dots.push_back(d);
      };
    bindNb("RX_APPLYDOTDAMAGE", [addDot](std::shared_ptr<zenkit::INpc> t, std::shared_ptr<zenkit::INpc> a, int32_t ticks,
                                         int32_t dmg, int32_t type, int32_t interval, int32_t renew) {
      addDot(t,a,ticks,dmg,type,interval,renew,false);
      });
    bindNb("RX_APPLYDOTDAMAGENOTKILL", [addDot](std::shared_ptr<zenkit::INpc> t, std::shared_ptr<zenkit::INpc> a, int32_t ticks,
                                                int32_t dmg, int32_t type, int32_t interval, int32_t renew) {
      addDot(t,a,ticks,dmg,type,interval,renew,true);
      });
    bindNb("RX_CLEARALLDOTS", [this]() {
      auto nb = nbStates.find(this);
      if(nb!=nbStates.end())
        nb->second.dots.clear();
      });
    // NB fix 196: StopPlayerBleeding (UNION_ABI.DLL 0x1016d610 -> 0x1013bdb0), called by the potions of healing: the damage
    // over time of the hero of the type 4 (DAM_EDGE, the bleeding) ends, PlayerIsBleeding = 0
    bindNb("STOPPLAYERBLEEDING", [this]() {
      auto  nb   = nbStates.find(this);
      auto* hero = world().player();
      size_t gone = 0;
      if(nb!=nbStates.end() && hero!=nullptr) {
        auto& dots = nb->second.dots;
        const auto me = hero->handlePtr();
        for(size_t i=0; i<dots.size(); ) {
          if(dots[i].type==4 && dots[i].target.lock()==me) {
            dots.erase(dots.begin()+ptrdiff_t(i));
            ++gone;
            continue;
            }
          ++i;
          }
        }
      if(auto* v = vm.find_symbol_by_name("PLAYERISBLEEDING"); v!=nullptr && v->type()==zenkit::DaedalusDataType::INT)
        v->set_int(0);
      if(gone>0)
        nbCtx("bleeding stopped: ",gone);
      });

    // NB fix 100: the bosses call their helpers - the character at the point, and it attacks the hero at once
    bindNb("WLD_INSERTNPCANDATTACKENEMY", [this](int32_t instance, std::string_view point) {
      if(instance<=0)
        return;
      auto* npc  = world().addNpc(size_t(instance),point);
      auto* hero = world().player();
      if(npc==nullptr)
        return;
      fixNpcPosition(*npc,0,150);
      if(hero==nullptr || hero->isDead())
        return;
      npc->setTarget(hero);
      const auto constInt = [this](std::string_view name, int32_t def) {
        auto* sym = vm.find_symbol_by_name(name);
        return sym!=nullptr && sym->type()==zenkit::DaedalusDataType::INT ? sym->get_int() : def;
        };
      const int32_t sepHum = constInt("GIL_SEPERATOR_HUM",16);
      try {
        if(npc->handle().guild<sepHum) {
          auto* fn = vm.find_symbol_by_name("B_ATTACK");
          if(fn!=nullptr && fn->type()==zenkit::DaedalusDataType::FUNCTION)
            vm.call_function<void>(fn,npc->handlePtr(),hero->handlePtr(),constInt("AR_KILL",19),int32_t(0));
          } else {
          auto* zs = vm.find_symbol_by_name("ZS_MM_ATTACK");
          if(zs!=nullptr)
            ai_startstate(npc->handlePtr(),int(zs->index()),0,"");
          }
        }
      catch(const std::exception& e) {
        Log::e("NB: Wld_InsertNpcAndAttackEnemy failed: ",e.what());
        }
      nbCtx("boss helper: ",npc->displayName()," at ",point," attacks the hero");
      });
    bindNb("NPC_CANSEENPC2", [this](std::shared_ptr<zenkit::INpc> aRef, std::shared_ptr<zenkit::INpc> bRef, int32_t freeLos) -> int32_t {
      auto a = findNpc(aRef);
      auto b = findNpc(bRef);
      return a!=nullptr && b!=nullptr && a->canSeeNpc(*b,freeLos!=0) ? 1 : 0;
      });

    {
    std::string list;
    for(auto& n : nbBound)
      list += n + " ";
    Log::i("NB: bound ",int(nbBound.size())," externals: ",list);   // NB fix 204: not an error (the summary)
    }
    // what is still missing, with the signature from GOTHIC.DAT
    for(uint32_t i=0; i<vm.symbols().size(); ++i) {
      auto* s = vm.find_symbol_by_index(i);
      if(!s->is_external() || s->name().rfind("RX",0)!=0)
        continue;
      if(std::find(nbBound.begin(),nbBound.end(),s->name())!=nbBound.end())
        continue;
      Log::i("NB: not implemented ",nbSignature(vm,*s));
      }
    }
  bindExternal("hlp_random",                     &GameScript::hlp_random);
  bindExternal("hlp_isvalidnpc",                 &GameScript::hlp_isvalidnpc);
  bindExternal("hlp_isvaliditem",                &GameScript::hlp_isvaliditem);
  bindExternal("hlp_isitem",                     &GameScript::hlp_isitem);
  bindExternal("hlp_getnpc",                     &GameScript::hlp_getnpc);
  bindExternal("hlp_getinstanceid",              &GameScript::hlp_getinstanceid);

  bindExternal("wld_insertnpc",                  &GameScript::wld_insertnpc);
  bindExternal("wld_removenpc",                  &GameScript::wld_removenpc);
  bindExternal("wld_insertitem",                 &GameScript::wld_insertitem);
  bindExternal("wld_insertobject",               &GameScript::wld_insertobject);
  bindExternal("wld_removeitem",                 &GameScript::wld_removeitem);
  bindExternal("wld_settime",                    &GameScript::wld_settime);
  bindExternal("wld_getday",                     &GameScript::wld_getday);
  bindExternal("wld_playeffect",                 &GameScript::wld_playeffect);
  bindExternal("wld_stopeffect",                 &GameScript::wld_stopeffect);
  bindExternal("wld_getplayerportalguild",       &GameScript::wld_getplayerportalguild);
  bindExternal("wld_getformerplayerportalguild", &GameScript::wld_getformerplayerportalguild);
  bindExternal("wld_setguildattitude",           &GameScript::wld_setguildattitude);
  bindExternal("wld_getguildattitude",           &GameScript::wld_getguildattitude);
  bindExternal("wld_exchangeguildattitudes",     &GameScript::wld_exchangeguildattitudes);
  bindExternal("wld_istime",                     &GameScript::wld_istime);
  bindExternal("wld_isfpavailable",              &GameScript::wld_isfpavailable);
  bindExternal("wld_isnextfpavailable",          &GameScript::wld_isnextfpavailable);
  bindExternal("wld_ismobavailable",             &GameScript::wld_ismobavailable);
  bindExternal("wld_setmobroutine",              &GameScript::wld_setmobroutine);
  bindExternal("wld_setobjectroutine",           &GameScript::wld_setobjectroutine);
  bindExternal("wld_getmobstate",                &GameScript::wld_getmobstate);
  bindExternal("wld_assignroomtoguild",          &GameScript::wld_assignroomtoguild);
  bindExternal("wld_detectnpc",                  &GameScript::wld_detectnpc);
  bindExternal("wld_detectnpcex",                &GameScript::wld_detectnpcex);
  bindExternal("wld_detectitem",                 &GameScript::wld_detectitem);
  bindExternal("wld_spawnnpcrange",              &GameScript::wld_spawnnpcrange);
  bindExternal("wld_sendtrigger",                &GameScript::wld_sendtrigger);
  bindExternal("wld_senduntrigger",              &GameScript::wld_senduntrigger);
  bindExternal("wld_israining",                  &GameScript::wld_israining);

  bindExternal("mdl_setvisual",                  &GameScript::mdl_setvisual);
  bindExternal("mdl_setvisualbody",              &GameScript::mdl_setvisualbody);
  bindExternal("mdl_setmodelfatness",            &GameScript::mdl_setmodelfatness);
  bindExternal("mdl_applyoverlaymds",            &GameScript::mdl_applyoverlaymds);
  bindExternal("mdl_applyoverlaymdstimed",       &GameScript::mdl_applyoverlaymdstimed);
  bindExternal("mdl_removeoverlaymds",           &GameScript::mdl_removeoverlaymds);
  bindExternal("mdl_setmodelscale",              &GameScript::mdl_setmodelscale);
  bindExternal("mdl_startfaceani",               &GameScript::mdl_startfaceani);
  bindExternal("mdl_applyrandomani",             &GameScript::mdl_applyrandomani);
  bindExternal("mdl_applyrandomanifreq",         &GameScript::mdl_applyrandomanifreq);
  bindExternal("mdl_applyrandomfaceani",         &GameScript::mdl_applyrandomfaceani);

  bindExternal("npc_settofightmode",             &GameScript::npc_settofightmode);
  bindExternal("npc_settofistmode",              &GameScript::npc_settofistmode);
  bindExternal("npc_isinstate",                  &GameScript::npc_isinstate);
  bindExternal("npc_isinroutine",                &GameScript::npc_isinroutine);
  bindExternal("npc_wasinstate",                 &GameScript::npc_wasinstate);
  bindExternal("npc_getdisttowp",                &GameScript::npc_getdisttowp);
  bindExternal("npc_exchangeroutine",            &GameScript::npc_exchangeroutine);
  bindExternal("npc_isdead",                     &GameScript::npc_isdead);
  bindExternal("npc_knowsinfo",                  &GameScript::npc_knowsinfo);
  bindExternal("npc_settalentskill",             &GameScript::npc_settalentskill);
  bindExternal("npc_gettalentskill",             &GameScript::npc_gettalentskill);
  bindExternal("npc_settalentvalue",             &GameScript::npc_settalentvalue);
  bindExternal("npc_gettalentvalue",             &GameScript::npc_gettalentvalue);
  bindExternal("npc_setrefusetalk",              &GameScript::npc_setrefusetalk);
  bindExternal("npc_refusetalk",                 &GameScript::npc_refusetalk);
  bindExternal("npc_hasitems",                   &GameScript::npc_hasitems);
  bindExternal("npc_hasspell",                   &GameScript::npc_hasspell);
  bindExternal("npc_getinvitem",                 &GameScript::npc_getinvitem);
  bindExternal("npc_getinvitembyslot",           &GameScript::npc_getinvitembyslot);
  bindExternal("npc_removeinvitem",              &GameScript::npc_removeinvitem);
  bindExternal("npc_removeinvitems",             &GameScript::npc_removeinvitems);
  bindExternal("npc_getbodystate",               &GameScript::npc_getbodystate);
  bindExternal("npc_getlookattarget",            &GameScript::npc_getlookattarget);
  bindExternal("npc_getdisttonpc",               &GameScript::npc_getdisttonpc);
  bindExternal("npc_hasequippedarmor",           &GameScript::npc_hasequippedarmor);
  bindExternal("npc_setperctime",                &GameScript::npc_setperctime);
  bindExternal("npc_percenable",                 &GameScript::npc_percenable);
  bindExternal("npc_percdisable",                &GameScript::npc_percdisable);
  bindExternal("npc_getnearestwp",               &GameScript::npc_getnearestwp);
  bindExternal("npc_getnextwp",                  &GameScript::npc_getnextwp);
  bindExternal("npc_clearaiqueue",               &GameScript::npc_clearaiqueue);
  bindExternal("npc_isplayer",                   &GameScript::npc_isplayer);
  bindExternal("npc_getstatetime",               &GameScript::npc_getstatetime);
  bindExternal("npc_setstatetime",               &GameScript::npc_setstatetime);
  bindExternal("npc_changeattribute",            &GameScript::npc_changeattribute);
  bindExternal("npc_isonfp",                     &GameScript::npc_isonfp);
  bindExternal("npc_getheighttonpc",             &GameScript::npc_getheighttonpc);
  bindExternal("npc_getequippedmeleeweapon",     &GameScript::npc_getequippedmeleeweapon);
  bindExternal("npc_getequippedrangedweapon",    &GameScript::npc_getequippedrangedweapon);
  bindExternal("npc_getequippedarmor",           &GameScript::npc_getequippedarmor);
  bindExternal("npc_canseenpc",                  &GameScript::npc_canseenpc);
  bindExternal("npc_canseenpcfreelos",           &GameScript::npc_canseenpcfreelos);
  bindExternal("npc_canseeitem",                 &GameScript::npc_canseeitem);
  bindExternal("npc_hasequippedweapon",          &GameScript::npc_hasequippedweapon);
  bindExternal("npc_hasequippedmeleeweapon",     &GameScript::npc_hasequippedmeleeweapon);
  bindExternal("npc_hasequippedrangedweapon",    &GameScript::npc_hasequippedrangedweapon);
  bindExternal("npc_getactivespell",             &GameScript::npc_getactivespell);
  bindExternal("npc_getactivespellisscroll",     &GameScript::npc_getactivespellisscroll);
  bindExternal("npc_getactivespellcat",          &GameScript::npc_getactivespellcat);
  bindExternal("npc_setactivespellinfo",         &GameScript::npc_setactivespellinfo);
  bindExternal("npc_getactivespelllevel",        &GameScript::npc_getactivespelllevel);
  bindExternal("npc_isinfightmode",              &GameScript::npc_isinfightmode);
  bindExternal("npc_settarget",                  &GameScript::npc_settarget);
  bindExternal("npc_gettarget",                  &GameScript::npc_gettarget);
  bindExternal("npc_getnexttarget",              &GameScript::npc_getnexttarget);
  bindExternal("npc_sendpassiveperc",            &GameScript::npc_sendpassiveperc);
  bindExternal("npc_sendsingleperc",             &GameScript::npc_sendsingleperc);
  bindExternal("npc_checkinfo",                  &GameScript::npc_checkinfo);
  bindExternal("npc_getportalguild",             &GameScript::npc_getportalguild);
  bindExternal("npc_isinplayersroom",            &GameScript::npc_isinplayersroom);
  bindExternal("npc_getreadiedweapon",           &GameScript::npc_getreadiedweapon);
  bindExternal("npc_hasreadiedweapon",           &GameScript::npc_hasreadiedweapon);
  bindExternal("npc_hasreadiedmeleeweapon",      &GameScript::npc_hasreadiedmeleeweapon);
  bindExternal("npc_hasreadiedrangedweapon",     &GameScript::npc_hasreadiedrangedweapon);
  bindExternal("npc_hasrangedweaponwithammo",    &GameScript::npc_hasrangedweaponwithammo);
  bindExternal("npc_isdrawingspell",             &GameScript::npc_isdrawingspell);
  bindExternal("npc_isdrawingweapon",            &GameScript::npc_isdrawingweapon);
  bindExternal("npc_perceiveall",                &GameScript::npc_perceiveall);
  bindExternal("npc_stopani",                    &GameScript::npc_stopani);
  bindExternal("npc_settrueguild",               &GameScript::npc_settrueguild);
  bindExternal("npc_gettrueguild",               &GameScript::npc_gettrueguild);
  bindExternal("npc_clearinventory",             &GameScript::npc_clearinventory);
  bindExternal("npc_getattitude",                &GameScript::npc_getattitude);
  bindExternal("npc_getpermattitude",            &GameScript::npc_getpermattitude);
  bindExternal("npc_setattitude",                &GameScript::npc_setattitude);
  bindExternal("npc_settempattitude",            &GameScript::npc_settempattitude);
  bindExternal("npc_hasbodyflag",                &GameScript::npc_hasbodyflag);
  bindExternal("npc_getlasthitspellid",          &GameScript::npc_getlasthitspellid);
  bindExternal("npc_getlasthitspellcat",         &GameScript::npc_getlasthitspellcat);
  bindExternal("npc_playani",                    &GameScript::npc_playani);
  bindExternal("npc_isdetectedmobownedbynpc",    &GameScript::npc_isdetectedmobownedbynpc);
  bindExternal("npc_getdetectedmob",             &GameScript::npc_getdetectedmob);
  bindExternal("npc_isdetectedmobownedbyguild",  &GameScript::npc_isdetectedmobownedbyguild);
  bindExternal("npc_ownedbynpc",                 &GameScript::npc_ownedbynpc);
  bindExternal("npc_canseesource",               &GameScript::npc_canseesource);
  bindExternal("npc_isincutscene",               &GameScript::npc_isincutscene);
  bindExternal("npc_getdisttoitem",              &GameScript::npc_getdisttoitem);
  bindExternal("npc_getheighttoitem",            &GameScript::npc_getheighttoitem);
  bindExternal("npc_getdisttoplayer",            &GameScript::npc_getdisttoplayer);

  bindExternal("ai_output",                      &GameScript::ai_output);
  bindExternal("ai_stopprocessinfos",            &GameScript::ai_stopprocessinfos);
  bindExternal("ai_processinfos",                &GameScript::ai_processinfos);
  bindExternal("ai_standup",                     &GameScript::ai_standup);
  bindExternal("ai_standupquick",                &GameScript::ai_standupquick);
  bindExternal("ai_continueroutine",             &GameScript::ai_continueroutine);
  bindExternal("ai_stoplookat",                  &GameScript::ai_stoplookat);
  bindExternal("ai_lookat",                      &GameScript::ai_lookat);
  bindExternal("ai_lookatnpc",                   &GameScript::ai_lookatnpc);
  bindExternal("ai_removeweapon",                &GameScript::ai_removeweapon);
  bindExternal("ai_unreadyspell",                &GameScript::ai_unreadyspell);
  bindExternal("ai_turnaway",                    &GameScript::ai_turnaway);
  bindExternal("ai_turntonpc",                   &GameScript::ai_turntonpc);
  bindExternal("ai_whirlaround",                 &GameScript::ai_whirlaround);
  bindExternal("ai_outputsvm",                   &GameScript::ai_outputsvm);
  bindExternal("ai_outputsvm_overlay",           &GameScript::ai_outputsvm_overlay);
  bindExternal("ai_startstate",                  &GameScript::ai_startstate);
  bindExternal("ai_playani",                     &GameScript::ai_playani);
  bindExternal("ai_setwalkmode",                 &GameScript::ai_setwalkmode);
  bindExternal("ai_wait",                        &GameScript::ai_wait);
  bindExternal("ai_waitms",                      &GameScript::ai_waitms);
  bindExternal("ai_waittillend",                 &GameScript::ai_waittillend);
  bindExternal("ai_aimat",                       &GameScript::ai_aimat);
  bindExternal("ai_shootat",                     &GameScript::ai_shootat);
  bindExternal("ai_stopaim",                     &GameScript::ai_stopaim);
  bindExternal("ai_dropitem",                    &GameScript::ai_dropitem);
  bindExternal("ai_snd_play",                    &GameScript::ai_snd_play);
  bindExternal("ai_stopfx",                      &GameScript::ai_stopfx);
  bindExternal("ai_aligntowp",                   &GameScript::ai_aligntowp);
  bindExternal("ai_gotowp",                      &GameScript::ai_gotowp);
  bindExternal("ai_gotofp",                      &GameScript::ai_gotofp);
  bindExternal("ai_playanibs",                   &GameScript::ai_playanibs);
  bindExternal("ai_equiparmor",                  &GameScript::ai_equiparmor);
  bindExternal("ai_equipbestarmor",              &GameScript::ai_equipbestarmor);
  bindExternal("ai_equipbestmeleeweapon",        &GameScript::ai_equipbestmeleeweapon);
  bindExternal("ai_equipbestrangedweapon",       &GameScript::ai_equipbestrangedweapon);
  bindExternal("ai_usemob",                      &GameScript::ai_usemob);
  bindExternal("ai_teleport",                    &GameScript::ai_teleport);
  bindExternal("ai_stoppointat",                 &GameScript::ai_stoppointat);
  bindExternal("ai_drawweapon",                  &GameScript::ai_drawweapon);
  bindExternal("ai_readymeleeweapon",            &GameScript::ai_readymeleeweapon);
  bindExternal("ai_readyrangedweapon",           &GameScript::ai_readyrangedweapon);
  bindExternal("ai_readyspell",                  &GameScript::ai_readyspell);
  bindExternal("ai_attack",                      &GameScript::ai_attack);
  bindExternal("ai_flee",                        &GameScript::ai_flee);
  bindExternal("ai_dodge",                       &GameScript::ai_dodge);
  bindExternal("ai_unequipweapons",              &GameScript::ai_unequipweapons);
  bindExternal("ai_unequiparmor",                &GameScript::ai_unequiparmor);
  bindExternal("ai_gotonpc",                     &GameScript::ai_gotonpc);
  bindExternal("ai_gotonextfp",                  &GameScript::ai_gotonextfp);
  bindExternal("ai_aligntofp",                   &GameScript::ai_aligntofp);
  bindExternal("ai_useitem",                     &GameScript::ai_useitem);
  bindExternal("ai_useitemtostate",              &GameScript::ai_useitemtostate);
  bindExternal("ai_setnpcstostate",              &GameScript::ai_setnpcstostate);
  bindExternal("ai_finishingmove",               &GameScript::ai_finishingmove);
  bindExternal("ai_takeitem",                    &GameScript::ai_takeitem);
  bindExternal("ai_gotoitem",                    &GameScript::ai_gotoitem);
  bindExternal("ai_pointat",                     &GameScript::ai_pointat);
  bindExternal("ai_pointatnpc",                  &GameScript::ai_pointatnpc);

  bindExternal("mob_hasitems",                   &GameScript::mob_hasitems);
  bindExternal("mob_createitems",                &GameScript::mob_createitems);
  bindExternal("ai_printscreen",                 &GameScript::ai_printscreen);

  bindExternal("ta_min",                         &GameScript::ta_min);

  bindExternal("log_createtopic",                &GameScript::log_createtopic);
  bindExternal("log_settopicstatus",             &GameScript::log_settopicstatus);
  bindExternal("log_addentry",                   &GameScript::log_addentry);

  bindExternal("equipitem",                      &GameScript::equipitem);
  bindExternal("createinvitem",                  &GameScript::createinvitem);
  bindExternal("createinvitems",                 &GameScript::createinvitems);

  bindExternal("perc_setrange",                  &GameScript::perc_setrange);

  bindExternal("info_addchoice",                 &GameScript::info_addchoice);
  bindExternal("info_clearchoices",              &GameScript::info_clearchoices);
  bindExternal("infomanager_hasfinished",        &GameScript::infomanager_hasfinished);

  bindExternal("snd_play",                       &GameScript::snd_play);
  bindExternal("snd_play3d",                     &GameScript::snd_play3d);

  bindExternal("game_initgerman",                &GameScript::game_initgerman);
  bindExternal("game_initenglish",               &GameScript::game_initenglish);

  bindExternal("exitsession",                    &GameScript::exitsession);

  // vm.validateExternals();

  spells               = std::make_unique<SpellDefinitions>(vm);
  svm                  = std::make_unique<SvmDefinitions>(vm);

  cFocusNorm           = findFocus("Focus_Normal");
  cFocusMelee          = findFocus("Focus_Melee");
  cFocusRange          = findFocus("Focus_Ranged");
  cFocusMage           = findFocus("Focus_Magic");

  ZS_Dead              = aiState(findSymbolIndex("ZS_Dead")).funcIni;
  ZS_Unconscious       = aiState(findSymbolIndex("ZS_Unconscious")).funcIni;
  ZS_Talk              = aiState(findSymbolIndex("ZS_Talk")).funcIni;
  ZS_Attack            = aiState(findSymbolIndex("ZS_Attack")).funcIni;
  ZS_MM_Attack         = aiState(findSymbolIndex("ZS_MM_Attack")).funcIni;

  spellFxInstanceNames = vm.find_symbol_by_name("spellFxInstanceNames");
  spellFxAniLetters    = vm.find_symbol_by_name("spellFxAniLetters");

  if(spellFxInstanceNames==nullptr || spellFxAniLetters==nullptr) {
    throw std::runtime_error("spellFxInstanceNames and/or spellFxAniLetters not found");
    }

  if(owner.version().game==2) {
    auto* currency = vm.find_symbol_by_name("TRADE_CURRENCY_INSTANCE");
    itMi_Gold      = currency!=nullptr ? vm.find_symbol_by_name(currency->get_string()) : nullptr;
    if(itMi_Gold!=nullptr){ // FIXME
      auto item = vm.init_instance<zenkit::IItem>(itMi_Gold);
      goldTxt = item->name;
      }
    auto* tradeMul = vm.find_symbol_by_name("TRADE_VALUE_MULTIPLIER");
    tradeValMult   = tradeMul != nullptr ? tradeMul->get_float() : 1.0f;

    auto* vtime     = vm.find_symbol_by_name("VIEW_TIME_PER_CHAR");
    viewTimePerChar = vtime != nullptr ? vtime->get_float() : 550.f;
    if(viewTimePerChar<=0.f)
      viewTimePerChar = 550.f;

    ItKE_lockpick     = vm.find_symbol_by_name("ItKE_lockpick");
    B_RefreshAtInsert = vm.find_symbol_by_name("B_RefreshAtInsert");
    } else {
    itMi_Gold      = vm.find_symbol_by_name("ItMiNugget");
    if(itMi_Gold!=nullptr) { // FIXME
      auto item = vm.init_instance<zenkit::IItem>(itMi_Gold);
      goldTxt = item->name;
      }
    //
    tradeValMult    = 0.5f;   // Gothic 1: a trader pays the half (see Item::sellCost)
    viewTimePerChar = 550.f;
    ItKE_lockpick   = vm.find_symbol_by_name("itkelockpick");
    }

  if(auto v = vm.find_symbol_by_name("DAM_CRITICAL_MULTIPLIER")) {
    damCriticalMultiplier = v->get_int();
    }

  auto* gilMax = vm.find_symbol_by_name("GIL_MAX");
  gilCount = gilMax!=nullptr ? size_t(gilMax->get_int()) : 0;

  auto* tblSz = vm.find_symbol_by_name("TAB_ANZAHL");
  gilTblSize = tblSz!=nullptr ? size_t(std::sqrt(tblSz->get_int())) : 0;
  gilAttitudes.resize(gilCount*gilCount,ATT_HOSTILE);
  wld_exchangeguildattitudes("GIL_ATTITUDES");

  auto id = vm.find_symbol_by_name("Gil_Values");
  if(id!=nullptr){
    cGuildVal = vm.init_instance<zenkit::IGuildValues>(id);
    // the human guilds move and fight like GIL_HUMAN: every guild below GIL_SEPERATOR_HUM of the scripts. That is 16 in the
    // original game; New Balance has 21, its guilds 16..20 are humans too (19 is the guild of the hero there - with the
    // values of a goblin he slid down every slope)
    const size_t humEnd = std::min(size_t(std::max(guildSeparatorHum(),0)), std::size(cGuildVal->slide_angle));
    for(size_t i=0;i<humEnd;++i){
      if(i==Guild::GIL_PUBLIC)
        continue;   // the portal rooms, not a guild of characters
      cGuildVal->water_depth_knee   [i]=cGuildVal->water_depth_knee   [Guild::GIL_HUMAN];
      cGuildVal->water_depth_chest  [i]=cGuildVal->water_depth_chest  [Guild::GIL_HUMAN];
      cGuildVal->jumpup_height      [i]=cGuildVal->jumpup_height      [Guild::GIL_HUMAN];
      cGuildVal->swim_time          [i]=cGuildVal->swim_time          [Guild::GIL_HUMAN];
      cGuildVal->dive_time          [i]=cGuildVal->dive_time          [Guild::GIL_HUMAN];
      cGuildVal->step_height        [i]=cGuildVal->step_height        [Guild::GIL_HUMAN];
      cGuildVal->jumplow_height     [i]=cGuildVal->jumplow_height     [Guild::GIL_HUMAN];
      cGuildVal->jumpmid_height     [i]=cGuildVal->jumpmid_height     [Guild::GIL_HUMAN];
      cGuildVal->slide_angle        [i]=cGuildVal->slide_angle        [Guild::GIL_HUMAN];
      cGuildVal->slide_angle2       [i]=cGuildVal->slide_angle2       [Guild::GIL_HUMAN];
      cGuildVal->disable_autoroll   [i]=cGuildVal->disable_autoroll   [Guild::GIL_HUMAN];
      cGuildVal->surface_align      [i]=cGuildVal->surface_align      [Guild::GIL_HUMAN];
      cGuildVal->climb_heading_angle[i]=cGuildVal->climb_heading_angle[Guild::GIL_HUMAN];
      cGuildVal->climb_horiz_angle  [i]=cGuildVal->climb_horiz_angle  [Guild::GIL_HUMAN];
      cGuildVal->climb_ground_angle [i]=cGuildVal->climb_ground_angle [Guild::GIL_HUMAN];
      cGuildVal->fight_range_base   [i]=cGuildVal->fight_range_base   [Guild::GIL_HUMAN];
      cGuildVal->fight_range_fist   [i]=cGuildVal->fight_range_fist   [Guild::GIL_HUMAN];
      cGuildVal->fight_range_g      [i]=cGuildVal->fight_range_g      [Guild::GIL_HUMAN];
      cGuildVal->fight_range_1hs    [i]=cGuildVal->fight_range_1hs    [Guild::GIL_HUMAN];
      cGuildVal->fight_range_1ha    [i]=cGuildVal->fight_range_1ha    [Guild::GIL_HUMAN];
      cGuildVal->fight_range_2hs    [i]=cGuildVal->fight_range_2hs    [Guild::GIL_HUMAN];
      cGuildVal->fight_range_2ha    [i]=cGuildVal->fight_range_2ha    [Guild::GIL_HUMAN];
      cGuildVal->falldown_height    [i]=cGuildVal->falldown_height    [Guild::GIL_HUMAN];
      cGuildVal->falldown_damage    [i]=cGuildVal->falldown_damage    [Guild::GIL_HUMAN];
      cGuildVal->blood_disabled     [i]=cGuildVal->blood_disabled     [Guild::GIL_HUMAN];
      cGuildVal->blood_max_distance [i]=cGuildVal->blood_max_distance [Guild::GIL_HUMAN];
      cGuildVal->blood_amount       [i]=cGuildVal->blood_amount       [Guild::GIL_HUMAN];
      cGuildVal->blood_flow         [i]=cGuildVal->blood_flow         [Guild::GIL_HUMAN];
      cGuildVal->blood_emitter      [i]=cGuildVal->blood_emitter      [Guild::GIL_HUMAN];
      cGuildVal->blood_texture      [i]=cGuildVal->blood_texture      [Guild::GIL_HUMAN];
      cGuildVal->turn_speed         [i]=cGuildVal->turn_speed         [Guild::GIL_HUMAN];
      }
    }

  if(DirectMemory::isRequired(vm))
    dma.reset(new DirectMemory(*this, vm));
  }

void GameScript::initSettings() {
  auto lang = Gothic::inst().settingsGetI("GAME", "language");
  if(vmLang!=lang) {
    vmLang = lang;
    //vm     = createVm(Gothic::inst());
    initDialogs();
    }
  }

void GameScript::initDialogs() {
  loadDialogOU();

  dialogsInfo.clear();
  vm.enumerate_instances_by_class_name("C_INFO", [this](zenkit::DaedalusSymbol& sym){
    dialogsInfo.push_back(vm.init_instance<zenkit::IInfo>(&sym));
    });
  }

void GameScript::loadDialogOU() {
  // Based on https://github.com/auronen/Gothic-2-localization mod,
  // at least in G2 .BIN should have a priority
  const std::string prefix = std::string(Gothic::inst().defaultOutputUnits());
  const std::array<std::string,2> names = {prefix + ".BIN", prefix + ".DAT"};

  const auto version = Gothic::inst().version().game==1 ? zenkit::GameVersion::GOTHIC_1
                                                        : zenkit::GameVersion::GOTHIC_2;

  // A damaged or unusual file of a mod (the texts of the speech) must not stop the whole game: the failure is logged with the name of the file
  // and the next candidate is tried, as it is done for the files of the disk below.
  auto readFromFiles = [&](const std::string& name) {
    try {
      std::unique_ptr<zenkit::Read> read;
      auto zen = Resources::openReader(name, read);
      auto lib = zen->read_object<zenkit::CutsceneLibrary>(version);
      if(lib==nullptr)
        throw std::runtime_error("no library of the output units in the file");
      dialogs = std::move(*lib);
      return true;
      }
    catch(const std::exception& e) {
      Log::e("unable to read the output units \"",name,"\": ",e.what());
      }
    return false;
    };

  for(auto& OU:names) {
    if(Resources::hasFile(OU) && readFromFiles(OU))
      return;

    const size_t segment = OU.find_last_of("\\/");
    if(segment!=std::string::npos && Resources::hasFile(OU.substr(segment+1)) && readFromFiles(OU.substr(segment+1)))
      return;

    char16_t str16[256] = {};
    for(size_t i=0; OU[i] && i<255; ++i)
      str16[i] = char16_t(OU[i]);

    auto gcutscene = CommandLine::inst().cutscenePath();
    auto full      = FileUtil::caseInsensitiveSegment(gcutscene,str16,Dir::FT_File);
    if(!FileUtil::exists(std::u16string(full)) && vmLang>=0) {
      gcutscene = CommandLine::inst().cutscenePath(ScriptLang(vmLang));
      full      = FileUtil::caseInsensitiveSegment(gcutscene,str16,Dir::FT_File);
      }

    try {
      auto buf = zenkit::Read::from(full);
      if(buf==nullptr)
        continue;
      auto zen = zenkit::ReadArchive::from(buf.get());
      if(zen==nullptr)
        continue;
      // auto zen = Resources::openReader(full);
      dialogs = std::move(*zen->read_object<zenkit::CutsceneLibrary>(version));
      return;
      }
    catch(...){
      // loop to next possible path
      }
    }
  Log::e("none of Zen-files for OU could be loaded");
  }

void GameScript::initializeInstanceNpc(const std::shared_ptr<zenkit::INpc>& npc, size_t instance) {
  auto sym = vm.find_symbol_by_index(uint32_t(instance));

  if(sym == nullptr) {
    Tempest::Log::e("Cannot initialize NPC ", instance, ": Symbol not found.");
    return;
    }

  vm.init_instance(npc, sym);

  if(npc->daily_routine!=0) {
    ScopeVar self(*vm.global_self(), npc);
    auto* daily_routine = vm.find_symbol_by_index(uint32_t(npc->daily_routine));

    if(daily_routine != nullptr) {
      vm.call_function(daily_routine);
      }
    }
  }

void GameScript::initializeInstanceItem(const std::shared_ptr<zenkit::IItem>& item, size_t instance) {
  auto sym = vm.find_symbol_by_index(uint32_t(instance));

  if(sym == nullptr) {
    Tempest::Log::e("Cannot initialize item ", instance, ": Symbol not found.");
    return;
    }

  vm.init_instance(item, sym);
  }

void GameScript::saveQuests(Serialize &fout) {
  if(auto it = nbStates.find(this); it!=nbStates.end()) {
    nbCtx("save of the game");
    nbMissingReport(it->second,"save");
    }
  quests.save(fout);
  fout.write(uint32_t(dlgKnownInfos.size()));
  for(auto& i:dlgKnownInfos)
    fout.write(uint32_t(i.first),uint32_t(i.second));

  fout.write(gilAttitudes);

  if(auto it = nbStates.find(this); it!=nbStates.end()) {
    // NB fix 56: the parts of the world inserted by the scripts (the caller sets its next entry after this)
    fout.setEntry("nb/vobtrees");
    auto& vt = it->second.vobTrees;
    fout.write(uint32_t(vt.size()));
    for(auto& t:vt)
      fout.write(t.world,t.file);
    // NB fix 125: the vobs that the scripts have hidden or shown
    fout.setEntry("nb/vobvisual");
    auto& vv = it->second.vobVisual;
    fout.write(uint32_t(vv.size()));
    for(auto& [key,show]:vv)
      fout.write(key,uint8_t(show ? 1 : 0));
    }
  }

void GameScript::loadQuests(Serialize& fin) {
  quests.load(fin);
  uint32_t sz=0, lostInfos=0;
  fin.read(sz);
  for(size_t i=0;i<sz;++i){
    uint32_t f=0,s=0;
    fin.read(f,s);
    // NB fix 209: the character and the dialog are symbols of the scripts of the save-game
    f = fin.mapSymbol(f);
    s = fin.mapSymbol(s);
    if(f==uint32_t(-1) || s==uint32_t(-1)) {
      ++lostInfos;
      continue;
      }
    dlgKnownInfos.insert(std::make_pair(f,s));
    }

  if(lostInfos>0)
    Log::e("[save] the scripts are not the scripts of the save-game: ",lostInfos," of ",sz," known dialogs are lost (they may come again)");

  fin.read(gilAttitudes);

  if(fin.setEntry("nb/vobtrees")) {
    auto& st = nbStates[this];
    uint32_t cnt = 0;
    fin.read(cnt);
    st.vobTrees.clear();
    for(uint32_t i=0; i<cnt; ++i) {
      NbState::VobTree t;
      fin.read(t.world,t.file);
      st.vobTrees.push_back(std::move(t));
      }
    st.vobWorld = nullptr;   // inserted again at the next tick, into the loaded world
    }
  if(fin.setEntry("nb/vobvisual")) {
    auto& st = nbStates[this];
    uint32_t cnt = 0;
    fin.read(cnt);
    st.vobVisual.clear();
    for(uint32_t i=0; i<cnt; ++i) {
      std::string key;
      uint8_t     show = 1;
      fin.read(key,show);
      st.vobVisual[key] = show!=0;
      }
    st.vobVisualWorld = 0;   // put again at the next tick
    }
  }

void GameScript::saveVar(Serialize &fout) {
  auto& dat = vm.symbols();
  fout.write(uint32_t(dat.size()));
  for(unsigned i = 0; i < dat.size(); ++i){
    auto* sym = vm.find_symbol_by_index(i); // never returns nullptr
    saveSym(fout,*sym);
    }
  }

void GameScript::loadVar(Serialize &fin) {
  std::string name;
  uint32_t sz=0;
  fin.read(sz);
  for(size_t i=0;i<sz;++i){
    auto t = uint32_t(zenkit::DaedalusDataType::VOID);
    fin.read(t);
    switch(zenkit::DaedalusDataType(t)) {
      case zenkit::DaedalusDataType::INT:{
        fin.read(name);
        auto* s = findSymbol(name);

        uint32_t size;
        fin.read(size);

        int v = 0;
        for(unsigned j = 0; j < size; ++j) {
          fin.read(v);
          if (s != nullptr && !s->is_member() && !s->is_const()) {
            s->set_int(v, uint16_t(j));
            }
          }

        break;
        }
      case zenkit::DaedalusDataType::FLOAT:{
        fin.read(name);
        auto* s = findSymbol(name);

        uint32_t size;
        fin.read(size);

        float v = 0;
        for (unsigned j = 0; j < size; ++j) {
          fin.read(v);
          if (s != nullptr && !s->is_member() && !s->is_const()) {
            s->set_float(v, uint16_t(j));
            }
          }

        break;
        }
      case zenkit::DaedalusDataType::STRING:{
        fin.read(name);
        auto* s = findSymbol(name);

        uint32_t size;
        fin.read(size);

        std::string v;
        for (unsigned j = 0; j < size; ++j) {
          fin.read(v);
          if (s != nullptr && !s->is_member() && !s->is_const()) {
            s->set_string(v, uint16_t(j));
            }
          }

        break;
        }
      case zenkit::DaedalusDataType::INSTANCE:{
        uint8_t dataClass=0;
        fin.read(dataClass);
        if(dataClass>0){
          uint32_t id=0;
          fin.read(name,id);
          auto* s = findSymbol(name);
          if (s == nullptr)
            break;
          if(dataClass==1) {
            auto npc = world().npcById(id);
            s->set_instance(npc ? npc->handlePtr() : nullptr);
            }
          else if(dataClass==2) {
            auto itm = world().itmById(id);
            s->set_instance(itm != nullptr ? itm->handlePtr() : nullptr);
            }
          else if(dataClass==3) {
            uint32_t itmClass=0;
            fin.read(itmClass);
            itmClass = fin.mapSymbol(itmClass);   // NB fix 209
            if(auto npc = world().npcById(id)) {
              auto itm = npc->getItem(itmClass);
              s->set_instance(itm ? itm->handlePtr() : nullptr);
              }
            }
          }
        break;
        }
      default:
        break;
      }
    }
  }

void GameScript::savePerc(Serialize& fout) {
  fout.write(uint32_t(PERC_Count));
  for(size_t i=0; i<PERC_Count; ++i) {
    fout.write(perceptionRanges.range[i]);
    }
  }

void GameScript::loadPerc(Serialize& fin) {
  uint32_t count = 0;
  fin.read(count);
  if(count!=PERC_Count) {
    if(hasSymbolName("initPerceptions"))
      vm.call_function("initPerceptions");
    return;
    }
  for(size_t i=0; i<PERC_Count; ++i)
    fin.read(perceptionRanges.range[i]);
  }

void GameScript::resetVarPointers() {
  for(uint32_t i=0;i<vm.symbols().size();++i){
    auto* s = vm.find_symbol_by_index(i); // never returns nullptr
    if(s->is_instance_of<zenkit::INpc>() || s->is_instance_of<zenkit::IItem>()){
      s->set_instance(nullptr);
      }
    }
  }

const QuestLog& GameScript::questLog() const {
  return quests;
  }

int GameScript::chapter() const {
  if(auto* sym = vm.find_symbol_by_name("KAPITEL"))
    return sym->get_int();
  return 0;
  }

void GameScript::saveSym(Serialize &fout, zenkit::DaedalusSymbol& i) {
  auto& w = world();
  switch(i.type()) {
    case zenkit::DaedalusDataType::INT:
      if(i.count()>0 && !i.is_member() && !i.is_const()){
        fout.write(i.type(), i.name(), i.count());

        for (unsigned j = 0; j < i.count(); ++j)
          fout.write(i.get_int(uint16_t(j)));
        return;
        }
      break;
    case zenkit::DaedalusDataType::FLOAT:
      if(i.count()>0 && !i.is_member() && !i.is_const()){
        fout.write(i.type(), i.name(), i.count());

        for (unsigned j = 0; j < i.count(); ++j)
          fout.write(i.get_float(uint16_t(j)));
        return;
        }
      break;
    case zenkit::DaedalusDataType::STRING:
      if(i.count()>0 && !i.is_member() && !i.is_const()){
        fout.write(i.type(), i.name(), i.count());

        for (unsigned j = 0; j < i.count(); ++j)
          fout.write(i.get_string(uint16_t(j)));
        return;
        }
      break;
    case zenkit::DaedalusDataType::INSTANCE:
      fout.write(i.type());

      if(i.is_instance_of<zenkit::INpc>()){
        auto hnpc = reinterpret_cast<const zenkit::INpc*>(i.get_instance().get());
        auto npc  = reinterpret_cast<const Npc*>(hnpc==nullptr ? nullptr : hnpc->user_ptr);
        fout.write(uint8_t(1),i.name(),world().npcId(npc));
        }
      else if(i.is_instance_of<zenkit::IItem>()){
        auto     item = reinterpret_cast<const zenkit::IItem*>(i.get_instance().get());
        uint32_t id   = w.itmId(item);
        if(id!=uint32_t(-1) || item==nullptr) {
          fout.write(uint8_t(2),i.name(),id);
          } else {
          uint32_t idNpc = uint32_t(-1);
          for(uint32_t r=0; r<w.npcCount(); ++r) {
            auto& n = *w.npcById(r);
            if(n.itemCount(item->symbol_index())>0) {
              idNpc = r;
              fout.write(uint8_t(3),i.name(),idNpc,uint32_t(item->symbol_index()));
              break;
              }
            }
          if(idNpc==uint32_t(-1))
            fout.write(uint8_t(2),i.name(),uint32_t(-1));
          }
        }
      else if(i.is_instance_of<zenkit::IFocus>() ||
              i.is_instance_of<zenkit::IGuildValues>() ||
              i.is_instance_of<zenkit::IInfo>()) {
        fout.write(uint8_t(0));
        }
      else {
        fout.write(uint8_t(0));
        }
      return;
    default:
      break;
    }
  fout.write(uint32_t(zenkit::DaedalusDataType::VOID));
  }

/// The collision of a static object (a pillar, a beam) is a hollow triangle mesh: the npc inside of it does not touch any triangle.
/// The rays ignore the back faces, so the rays from the outside, towards the point, hit the surface only if the point is inside of a solid object.
static bool isInsideSolid(const DynamicWorld& dyn, const Vec3& p) {
  const float range = 120;
  int         hit   = 0;
  for(int i=0; i<8; ++i) {
    const float a = float(i)*float(M_PI)/4.f;
    const auto  s = p+Vec3(std::cos(a)*range, 100, std::sin(a)*range);
    if(dyn.ray(s, p+Vec3(0,100,0)).hasCol)
      ++hit;
    }
  return hit>=7;
  }

void GameScript::fixNpcPosition(Npc& npc, float angle0, float distBias) {
  auto& dyn  = *world().physic();
  auto  pos0 = npc.position();

  if(!npc.hasCollision()) {
    auto ray = dyn.ray(pos0+Vec3(0,100,0), pos0+Vec3(0,-200,0));
    if(ray.hasCol && std::abs(pos0.y-ray.v.y) < 10.f) {
      npc.updateTransform();
      return;
      }
    }

  // The ground under the point: a way-point stays a bit over the floor, so the ray of a shifted point, started 100 over the way-point,
  // can land on the top of a cupboard or of a table (Hilda in the farm house stood on the cupboard and could not go to her bed).
  float ground0 = pos0.y;
  if(auto g = dyn.ray(pos0+Vec3(0,100,0), pos0+Vec3(0,-1000,0)); g.hasCol)
    ground0 = g.v.y;

  // the first pass stays on the level of the point, the second one takes any ground
  for(int pass = 0; pass<2; ++pass) {
    for(int r = 0; r<=800; r+=20) {
      for(int ang = 0; ang<360; ang+=30) {
        float a = float((float(ang)+angle0)*M_PI/180.0);
        float d = float(r)+distBias;
        auto  p = pos0+Vec3(std::cos(a)*d, 0, std::sin(a)*d);

        auto ray = dyn.ray(p+Vec3(0,100,0), p+Vec3(0,-1000,0));
        if(!ray.hasCol)
          continue;
        p.y = ray.v.y;
        if(pass==0 && std::abs(p.y-ground0)>60.f)
          continue;
        if(isInsideSolid(dyn,p))
          continue; // two npc at the same way-point: the shifted one ended up inside of a pillar (Oric, OC hall)
        npc.setPosition(p);
        if(!npc.hasCollision()) {
          npc.updateTransform();
          return;
          }
        if(d==0) {
          // no need to loop multiple angles, with R of zero
          break;
          }
        }
      }
    }

  // npc.setPosition(pos0);
  }

/// NB fix 223: a character placed next to another one (the summons of the spells, the call PRZYWOŁAJ of the panel, the
/// summons going with the hero through a teleport): on the floor of that one and not behind a wall from it. fixNpcPosition
/// takes the ground under the given point as its level: 2.5 m behind the hero on the second floor of the tower of Xardas
/// was the stairwell, the ground under it the first floor, 9 m lower - the golems stood there and could not come up.
/// Here: the ground under the anchor is the level, a point is taken when it is on that level (60 cm, then 2 m for the
/// stairs), it can be seen from the anchor at the height of the knees and the character fits there; else the first point
/// of the level and the sight; else the anchor itself
void GameScript::fixNpcPositionNear(Npc& npc, const Tempest::Vec3& anchor, float angle0, float distBias) {
  auto& dyn     = *world().physic();
  float groundA = anchor.y;
  if(auto g = dyn.ray(anchor+Vec3(0,50,0), anchor+Vec3(0,-300,0)); g.hasCol)
    groundA = g.v.y;
  const Vec3 eye = Vec3(anchor.x,groundA+80.f,anchor.z);

  bool hasAlt = false;
  Vec3 alt    = Vec3(anchor.x,groundA,anchor.z);
  for(float maxDy:{60.f,200.f}) {
    for(int r = 0; r<=600; r+=20) {
      for(int ang = 0; ang<360; ang+=30) {
        const float a = float((float(ang)+angle0)*M_PI/180.0);
        const float d = float(r)+distBias;
        Vec3 p = Vec3(anchor.x+std::cos(a)*d, groundA, anchor.z+std::sin(a)*d);
        auto ray = dyn.ray(p+Vec3(0,120,0), p+Vec3(0,-400,0));
        if(!ray.hasCol || std::abs(ray.v.y-groundA)>maxDy)
          continue;
        p.y = ray.v.y;
        if(dyn.ray(eye, p+Vec3(0,80,0)).hasCol)
          continue;   // a wall, a railing or the edge of the floor between
        if(isInsideSolid(dyn,p))
          continue;
        if(!hasAlt) {
          hasAlt = true;
          alt    = p;
          }
        npc.setPosition(p);
        if(!npc.hasCollision()) {
          npc.updateTransform();
          return;
          }
        if(d==0)
          break;
        }
      }
    }
  npc.setPosition(alt);
  npc.updateTransform();
  }

void GameScript::eventPlayAni(Npc& npc, std::string_view ani) {
  if(dma!=nullptr)
    dma->eventPlayAni(npc,ani);
  }

const World &GameScript::world() const {
  return *owner.world();
  }

World &GameScript::world() {
  return *owner.world();
  }

zenkit::IFocus GameScript::findFocus(std::string_view name) {
  auto id = vm.find_symbol_by_name(name);
  if(id==nullptr)
    return {};
  try {
    return *vm.init_instance<zenkit::IFocus>(id);
    }
  catch(const zenkit::DaedalusScriptError&) {
    return {};
    }
  }

void GameScript::storeItem(Item *itm) {
  auto* s = vm.global_item();
  if(itm!=nullptr) {
    s->set_instance(itm->handlePtr());
    } else {
    s->set_instance(nullptr);
    }
  }

zenkit::DaedalusSymbol* GameScript::findSymbol(std::string_view s) {
  return vm.find_symbol_by_name(s);
  }

zenkit::DaedalusSymbol* GameScript::findSymbol(const size_t s) {
  return vm.find_symbol_by_index(uint32_t(s));
  }

size_t GameScript::findSymbolIndex(std::string_view name) {
  auto sym = vm.find_symbol_by_name(name);
  return sym == nullptr ? size_t(-1) : sym->index();
  }

size_t GameScript::symbolsCount() const {
  return vm.symbols().size();
  }

const AiState& GameScript::aiState(ScriptFn id) {
  auto it = aiStates.find(id.ptr);
  if(it!=aiStates.end())
    return it->second;
  auto ins = aiStates.emplace(id.ptr,AiState(*this,id.ptr));
  return ins.first->second;
  }

const zenkit::ISpell& GameScript::spellDesc(int32_t splId) {
  auto& tag = spellFxInstanceNames->get_string(uint16_t(splId));
  return spells->find(tag);
  }

const VisualFx* GameScript::spellVfx(int32_t splId) {
  auto& tag = spellFxInstanceNames->get_string(uint16_t(splId));
  string_frm name("spellFX_",tag);
  return Gothic::inst().loadVisualFx(name);
  }

std::vector<GameScript::DlgChoice> GameScript::dialogChoices(std::shared_ptr<zenkit::INpc> player,
                                                             std::shared_ptr<zenkit::INpc> hnpc,
                                                             const std::vector<uint32_t>& except,
                                                             bool includeImp) {
  ScopeVar self (*vm.global_self(),  hnpc);
  ScopeVar other(*vm.global_other(), player);
  std::vector<zenkit::IInfo*> hDialog;
  for(auto& info : dialogsInfo) {
    if(info->npc==static_cast<int>(hnpc->symbol_index())) {
      hDialog.push_back(info.get());
      }
    }

  std::vector<DlgChoice> choice;
  for(int important=includeImp ? 1 : 0;important>=0;--important){
    for(auto& i:hDialog) {
      const zenkit::IInfo& info = *i;
      if(info.important!=important)
        continue;
      bool npcKnowsInfo = doesNpcKnowInfo(*player,vm.find_symbol_by_instance(info)->index());
      if(npcKnowsInfo && !info.permanent)
        continue;

      if(info.important && info.permanent){
        bool skip=false;
        for(auto i:except)
          if(static_cast<int>(i)==info.information){
            skip=true;
            break;
            }
        if(skip)
          continue;
        }

      bool valid=true;
      if(info.condition) {
        auto* conditionSymbol = vm.find_symbol_by_index(uint32_t(info.condition));
        if (conditionSymbol != nullptr) {
          valid = vm.call_function<int>(conditionSymbol) != 0;
          }
        }
      if(!valid)
        continue;

      DlgChoice ch;
      ch.title    = info.description;
      ch.scriptFn = uint32_t(info.information);
      ch.handle   = i;
      ch.isTrade  = info.trade!=0;
      ch.sort     = info.nr;
      choice.emplace_back(std::move(ch));
      }
    if(!choice.empty()){
      sort(choice);
      return choice;
      }
    }
  sort(choice);
  return choice;
  }

std::vector<GameScript::DlgChoice> GameScript::updateDialog(const GameScript::DlgChoice &dlg, Npc& player,Npc& npc) {
  if(dlg.handle==nullptr)
    return {};
  const zenkit::IInfo&               info = *dlg.handle;
  std::vector<GameScript::DlgChoice> ret;

  ScopeVar self (*vm.global_self(), npc.handlePtr());
  ScopeVar other(*vm.global_other(), player.handlePtr());

  for(size_t i=0;i<info.choices.size();++i){
    auto& sub = info.choices[i];
    GameScript::DlgChoice ch;
    ch.title    = sub.text;
    ch.scriptFn = uint32_t(sub.function);
    ch.handle   = dlg.handle;
    ch.isTrade  = false;
    ch.isSubChoice = true;
    // only where the scripts have the variable of the answer (the original Gothic 1 and 2 do not)
    if(!ch.title.empty() && ch.title[0]=='?' && dma!=nullptr && vm.find_symbol_by_name("INFOMANAGERANSWER")!=nullptr) {
      // Chronicles of Myrtana: "?Name" is an answer that is typed (INFOMANAGERANSWER of its scripts)
      ch.isInput = true;
      ch.title.erase(0,1);
      }
    ch.sort     = int(i);
    ret.push_back(ch);
    }

  sort(ret);
  return ret;
  }

/// The text that the player typed for a choice of the dialog. CoM reads it from the variable INFOMANAGERANSWER
void GameScript::setDialogAnswer(std::string_view text) {
  if(auto* s = vm.find_symbol_by_name("INFOMANAGERANSWER"))
    s->set_string(std::string(text));
  }

void GameScript::exec(const GameScript::DlgChoice &dlg, Npc& player, Npc& npc) {
  ScopeVar self (*vm.global_self(), npc.handlePtr());
  ScopeVar other(*vm.global_other(), player.handlePtr());

  zenkit::IInfo& info = *dlg.handle;

  if(&player!=&npc)
    player.stopAnim("");
  auto& pl = player.handle();

  if(info.information==int(dlg.scriptFn)) {
    setNpcInfoKnown(pl,info);
    } else {
    for(size_t i=0;i<info.choices.size();){
      if(info.choices[i].function==int(dlg.scriptFn))
        info.choices.erase(info.choices.begin()+int(i)); else
        ++i;
      }
    }

  if(auto* fn = vm.find_symbol_by_index(dlg.scriptFn); fn!=nullptr && nbStates.count(this))
    nbCtx("dialog: ",npc.displayName()," -> ",fn->name());
  // NB fix 124: RX_ClearCurrentDialog (the choice "Wróć" of NB) clears the choices of this info
  auto* nb       = nbStates.count(this) ? &nbStates[this] : nullptr;
  auto* prevInfo = nb!=nullptr ? nb->curInfo : nullptr;
  if(nb!=nullptr)
    nb->curInfo = &info;
  try {
    vm.call_function(vm.find_symbol_by_index(dlg.scriptFn));
    }
  catch(...) {
    if(nb!=nullptr)
      nb->curInfo = prevInfo;
    throw;
    }
  if(nb!=nullptr)
    nb->curInfo = prevInfo;
  }

void GameScript::printCannotUseError(Npc& npc, int32_t atr, int32_t nValue) {
  auto id = vm.find_symbol_by_name("G_CanNotUse");
  if(id==nullptr)
    return;

  ScopeVar self(*vm.global_self(), npc.handlePtr());
  vm.call_function<void>(id, npc.isPlayer(), atr, nValue);
  }

void GameScript::printCannotCastError(Npc &npc, int32_t plM, int32_t itM) {
  auto id = vm.find_symbol_by_name("G_CanNotCast");
  if(id==nullptr)
    return;

  ScopeVar self(*vm.global_self(), npc.handlePtr());
  vm.call_function<void>(id, npc.isPlayer(), itM, plM);
  }

void GameScript::printCannotBuyError(Npc &npc) {
  auto id = vm.find_symbol_by_name("player_trade_not_enough_gold");
  if(id==nullptr)
    return;
  ScopeVar self(*vm.global_self(), npc.handlePtr());
  vm.call_function<void>(id);
  }

void GameScript::printMobMissingItem(Npc &npc) {
  auto id = vm.find_symbol_by_name("player_mob_missing_item");
  if(id==nullptr) {
    if(owner.version().game==1)
      owner.player()->playAnimByName("T_DONTKNOW", BS_NONE);
    return;
    }
  ScopeVar self(*vm.global_self(), npc.handlePtr());
  vm.call_function<void>(id);
  }

void GameScript::printMobMissingKey(Npc& npc) {
  auto id = vm.find_symbol_by_name("player_mob_missing_key");
  if(id==nullptr) {
    if(owner.version().game==1)
      owner.player()->playAnimByName("T_DONTKNOW", BS_NONE);
    return;
    }
  ScopeVar self(*vm.global_self(), npc.handlePtr());
  vm.call_function<void>(id);
  }

void GameScript::printMobAnotherIsUsing(Npc &npc) {
  auto id = vm.find_symbol_by_name("player_mob_another_is_using");
  if(id==nullptr) {
    if(owner.version().game==1)
      owner.player()->playAnimByName("T_DONTKNOW", BS_NONE);
    return;
    }
  ScopeVar self(*vm.global_self(), npc.handlePtr());
  vm.call_function<void>(id);
  }

void GameScript::printMobMissingKeyOrLockpick(Npc& npc) {
  auto id = vm.find_symbol_by_name("player_mob_missing_key_or_lockpick");
  if(id==nullptr) {
    if(owner.version().game==1)
      owner.player()->playAnimByName("T_DONTKNOW", BS_NONE);
    return;
    }
  ScopeVar self(*vm.global_self(), npc.handlePtr());
  vm.call_function<void>(id);
  }

void GameScript::printMobMissingLockpick(Npc& npc) {
  auto id = vm.find_symbol_by_name("player_mob_missing_lockpick");
  if(id==nullptr) {
    if(owner.version().game==1)
      owner.player()->playAnimByName("T_DONTKNOW", BS_NONE);
    return;
    }
  ScopeVar self(*vm.global_self(), npc.handlePtr());
  vm.call_function<void>(id);
  }

void GameScript::printMobTooFar(Npc& npc) {
  auto id = vm.find_symbol_by_name("player_mob_too_far_away");
  if(id==nullptr) {
    owner.player()->playAnimByName("T_DONTKNOW", BS_NONE);
    return;
    }
  ScopeVar self(*vm.global_self(), npc.handlePtr());
  vm.call_function<void>(id);
  }

void GameScript::invokeState(const std::shared_ptr<zenkit::INpc>& hnpc, const std::shared_ptr<zenkit::INpc>& oth, const char *name) {
  auto id = vm.find_symbol_by_name(name);
  if(id==nullptr)
    return;

  ScopeVar self (*vm.global_self(),  hnpc);
  ScopeVar other(*vm.global_other(), oth);
  vm.call_function<void>(id);
  }

int GameScript::invokeState(Npc* npc, Npc* oth, Npc* vic, ScriptFn fn) {
  if(!fn.isValid())
    return 0;
  if(oth==nullptr){
    // oth=npc; //FIXME: PC_Levelinspektor?
    }
  if(vic==nullptr){
    // vic=owner.player();
    }

  if(fn==ZS_Talk){
    if(oth==nullptr || !oth->isPlayer()) {
      Log::e("unxepected perc acton");
      return 0;
      }
    }

  ScopeCtx ctx   (*this, npc!=nullptr ? npc->processPolicy() : NpcProcessPolicy::AiNormal);
  ScopeVar self  (*vm.global_self(),   npc != nullptr ? npc->handlePtr() : nullptr);
  ScopeVar other (*vm.global_other(),  oth != nullptr ? oth->handlePtr() : nullptr);
  ScopeVar victim(*vm.global_victim(), vic != nullptr ? vic->handlePtr() : nullptr);

  auto* sym = vm.find_symbol_by_index(uint32_t(fn.ptr));
  int   ret = 0;
  if(sym!=nullptr && sym->rtype() == zenkit::DaedalusDataType::INT) {
    ret = vm.call_function<int>(sym);
    }
  else if(sym!=nullptr) {
    vm.call_function<void>(sym);
    ret = 0;
    }

  if(vm.global_other()->is_instance_of<zenkit::INpc>()){
    auto oth2 = reinterpret_cast<zenkit::INpc*>(vm.global_other()->get_instance().get());
    if(oth!=nullptr && oth2!=&oth->handle()) {
      Npc* other = findNpc(oth2);
      npc->setOther(other);
      }
    }
  return ret;
  }

/// The function of an item (on_state, on_equip, on_unequip) runs with self = the character and item = the item, as in the
/// original engine: CoM reads ITEM.HP in the functions that read the recipes.
void GameScript::invokeItem(Npc *npc, ScriptFn fn, Item* itm) {
  if(fn==size_t(-1) || fn == 0)
    return;
  auto functionSymbol = vm.find_symbol_by_index(uint32_t(fn.ptr));

  if (functionSymbol == nullptr)
    return;

  if(npc->isPlayer() && nbStates.count(this))
    nbCtx("item of the hero: ",functionSymbol->name(),itm!=nullptr ? " (" : "",itm!=nullptr ? itm->displayName() : "",itm!=nullptr ? ")" : "",
          // NB fix 60: the last function of the script that ran - who took the armor off (if a script did it)
          functionSymbol->name().rfind("UNEQUIP",0)==0 ? " last script: " : "",
          functionSymbol->name().rfind("UNEQUIP",0)==0 ? nbFunctionAt(vm,nbStates[this],vm.pc()) : std::string());
  ScopeVar self(*vm.global_self(), npc->handlePtr());
  if(itm!=nullptr) {
    ScopeVar item(*vm.global_item(), itm->handlePtr());
    vm.call_function<void>(functionSymbol);
    return;
    }
  vm.call_function<void>(functionSymbol);
  }

int GameScript::invokeMana(Npc &npc, Npc* target, int mana) {
  auto fn = vm.find_symbol_by_name("Spell_ProcessMana");
  if(fn==nullptr)
    return SpellCode::SPL_SENDSTOP;

  ScopeVar self (*vm.global_self(),  npc.handlePtr());
  ScopeVar other(*vm.global_other(), target != nullptr ? target->handlePtr() : nullptr);

  return vm.call_function<int>(fn,mana);
  }

int GameScript::invokeManaRelease(Npc &npc, Npc* target, int mana) {
  auto fn = vm.find_symbol_by_name("Spell_ProcessMana_Release");
  if(fn==nullptr)
    return SpellCode::SPL_SENDSTOP;

  ScopeVar self (*vm.global_self(),  npc.handlePtr());
  ScopeVar other(*vm.global_other(), target != nullptr ? target->handlePtr() : nullptr);

  return vm.call_function<int>(fn,mana);
  }

void GameScript::invokeSpell(Npc &npc, Npc* target, Item &it) {
  auto&      tag = spellFxInstanceNames->get_string(uint16_t(it.spellId()));
  string_frm name("Spell_Cast_",tag);
  auto       fn = vm.find_symbol_by_name(name);
  if(fn==nullptr)
    return;

  const int32_t splLevel = npc.activeSpellLevel();
  ScopeVar self (*vm.global_self(),  npc.handlePtr());
  ScopeVar other(*vm.global_other(), target != nullptr ? target->handlePtr() : nullptr);
  try {
    if(fn->has_return() || fn->count()>1) {
      Log::d("unsupported spell-script signature: ",name);
      return;
      }
    if(fn->count()==1) {
      // this is a leveled spell
      const auto params = vm.find_parameters_for_function(fn);
      if(params.size()!=1 || params[0].type()!=zenkit::DaedalusDataType::INT) {
        Log::d("unsupported spell-level parameter: ",name);
        return;
        }
      vm.push_int(splLevel);
      }
    // Native spell adapters (CoM Telekinesis) must be honored here too.
    vm.invoke_function(fn);
    }
  catch(...) {
    Log::d("unable to call spell-script: \"",name,"\'");
    }
  }

int GameScript::debugInfoCondition(Npc& npc, Npc& player, std::string_view infoName, bool& known, std::string& detail) {
  string_frm cond(infoName,"_CONDITION");
  auto* fn   = vm.find_symbol_by_name(cond);
  auto* info = vm.find_symbol_by_name(infoName);
  if(fn==nullptr || info==nullptr)
    return -1;
  known = doesNpcKnowInfo(npc.handle(),info->index()) || doesNpcKnowInfo(player.handle(),info->index());

  // the variables that the condition reads and the infos that it asks about: what makes it false
  {
  uint32_t end = fn->address();
  for(uint32_t pc=fn->address(), n=0; n<400; ++n) {
    auto ins = vm.instruction_at(pc);
    if(ins.op==zenkit::DaedalusOpcode::B || ins.op==zenkit::DaedalusOpcode::BZ)
      end = std::max(end,uint32_t(ins.address));
    if(ins.op==zenkit::DaedalusOpcode::RSR && pc>=end)
      break;
    if(ins.op==zenkit::DaedalusOpcode::PUSHV || ins.op==zenkit::DaedalusOpcode::PUSHVI) {
      if(auto* s = vm.find_symbol_by_index(ins.symbol)) {
        if(s->type()==zenkit::DaedalusDataType::INT && !s->is_member() && !s->is_const() && s->count()==1)
          detail += std::string(s->name()) + "=" + std::to_string(s->get_int()) + " ";
        }
      }
    if(ins.op==zenkit::DaedalusOpcode::PUSHI && ins.immediate>10000 && ins.immediate<200000) {
      if(auto* s = vm.find_symbol_by_index(uint32_t(ins.immediate)))
        if(s->type()==zenkit::DaedalusDataType::INSTANCE && !s->is_member() && s->name().find("DIA_")==0)
          detail += "[asks " + std::string(s->name()) + (doesNpcKnowInfo(player.handle(),s->index()) ? ": known] " : ": NOT known] ");
      }
    pc += ins.size;
    }
  }
  ScopeVar self (*vm.global_self(),  npc.handlePtr());
  ScopeVar other(*vm.global_other(), player.handlePtr());
  return vm.call_function<int>(fn);
  }

int GameScript::invokeCond(Npc& npc, std::string_view func) {
  auto fn = vm.find_symbol_by_name(func);
  if(fn==nullptr) {
    Gothic::inst().onPrint("MOBSI::conditionFunc is not invalid");
    return 1;
    }
  ScopeVar self(*vm.global_self(), npc.handlePtr());
  return vm.call_function<int>(fn);
  }

void GameScript::invokePickLock(Npc& npc, int bSuccess, int bBrokenOpen) {
  auto fn   = vm.find_symbol_by_name("G_PickLock");
  if(fn==nullptr)
    return;
  ScopeVar self(*vm.global_self(), npc.handlePtr());
  vm.call_function<void>(fn, bSuccess, bBrokenOpen);
  }

void GameScript::invokeRefreshAtInsert(Npc& npc) {
  if(B_RefreshAtInsert==nullptr || owner.version().game!=2)
    return;
  ScopeVar self(*vm.global_self(), npc.handlePtr());
  vm.call_function<void>(B_RefreshAtInsert);
  }

CollideMask GameScript::canNpcCollideWithSpell(Npc& npc, Npc* shooter, int32_t spellId) {
  if(owner.version().game==1) {
    auto& spl = spellDesc(spellId);
    if(npc.isTargetableBySpell(TargetType(spl.target_collect_type)))
      return COLL_DOEVERYTHING; else
      return COLL_DONOTHING;
    }

  auto fn   = vm.find_symbol_by_name("C_CanNpcCollideWithSpell");
  if(fn==nullptr)
    return COLL_DOEVERYTHING;

  ScopeVar self (*vm.global_self(),  npc.handlePtr());
  ScopeVar other(*vm.global_other(), shooter->handlePtr());
  return CollideMask(vm.call_function<int>(fn, spellId));
  }

// Gothic 1 only differentiates between the two worldmap types with and
// without the orc addition and does this inside the code, not the script
int GameScript::playerHotKeyScreenMap_G1(Npc& pl) {
  size_t map = findSymbolIndex("itwrworldmap_orc");
  if(map==size_t(-1) || pl.itemCount(map)<1)
    map = findSymbolIndex("itwrworldmap");

  if(map==size_t(-1) || pl.itemCount(map)<1)
    return -1;

  pl.useItem(map);

  return int(map);
  }

int GameScript::playerHotKeyScreenMap(Npc& pl) {
  auto fn   = vm.find_symbol_by_name("player_hotkey_screen_map");
  if(fn==nullptr) {
    if(owner.version().game==1)
      return playerHotKeyScreenMap_G1(pl);
    return -1;
    }

  ScopeVar self(*vm.global_self(), pl.handlePtr());
  int map = vm.call_function<int>(fn);
  if(map>=0)
    pl.useItem(size_t(map));
  return map;
  }

void GameScript::playerHotLamePotion(Npc& pl) {
  auto opt = Gothic::inst().settingsGetI("GAME", "usePotionKeys");
  if(opt==0)
    return;

  auto fn   = vm.find_symbol_by_name("player_hotkey_lame_potion");
  if(fn==nullptr)
    return;

  ScopeVar self(*vm.global_self(), pl.handlePtr());
  vm.call_function<void>(fn);
  }

void GameScript::playerHotLameHeal(Npc& pl) {
  auto opt = Gothic::inst().settingsGetI("GAME", "usePotionKeys");
  if(opt==0)
    return;

  auto fn   = vm.find_symbol_by_name("player_hotkey_lame_heal");
  if(fn==nullptr)
    return;

  ScopeVar self(*vm.global_self(), pl.handlePtr());
  vm.call_function<void>(fn);
  }

std::string_view GameScript::spellCastAnim(Npc&, Item &it) {
  if(spellFxAniLetters==nullptr)
    return "FIB";
  return spellFxAniLetters->get_string(uint16_t(it.spellId()));
  }

bool GameScript::aiOutput(Npc &npc, std::string_view outputname, bool overlay) {
  string_frm name(outputname,".WAV");
  uint64_t   dt = 0;

  world().addDlgSound(name, npc.mapHeadBone(), WorldSound::talkRange, dt);
  npc.setAiOutputBarrier(messageTime(outputname),overlay);

  return true;
  }

bool GameScript::aiOutputSvm(Npc &npc, std::string_view outputname, bool overlay) {
  if(overlay) {
    if(tickCount()<svmBarrier)
      return true;
    svmBarrier = tickCount()+messageTime(outputname);
    }

  if(!outputname.empty())
    return aiOutput(npc,outputname,overlay);
  return true;
  }

bool GameScript::isDead(const Npc &pl) {
  return pl.isInState(ZS_Dead);
  }

bool GameScript::isUnconscious(const Npc &pl) {
  return pl.isInState(ZS_Unconscious);
  }

bool GameScript::isTalk(const Npc &pl) {
  return pl.isInState(ZS_Talk);
  }

bool GameScript::isAttack(const Npc& pl) const {
  return pl.isInState(ZS_Attack) || pl.isInState(ZS_MM_Attack);
  }

bool GameScript::isMonsterAttack(const Npc& pl) const {
  return ZS_MM_Attack!=0 && pl.isInState(ZS_MM_Attack);
  }

std::string_view GameScript::messageFromSvm(std::string_view id, int voice) const {
  return svm->find(id,voice);
  }

std::string_view GameScript::messageByName(std::string_view id) const {
  auto blk = dialogs.block_by_name(id);
  if(blk == nullptr)
    return "";
  auto msg = blk->get_message();
  if(msg == nullptr)
    return "";
  return msg->text;
  }

uint32_t GameScript::messageTime(std::string_view id) const {
  static std::string tmp;
  tmp.assign(id);

  uint32_t& time = msgTimings[tmp];
  if(time>0)
    return time;

  string_frm name(id,".WAV");
  auto s = Resources::loadSoundBuffer(name);
  if(s.timeLength()>0) {
    time = uint32_t(s.timeLength());
    } else {
    auto txt = messageByName(id);
    time = uint32_t(float(txt.length())*viewTimePerChar);
    time = std::min(time, 16000u);
    }
  return time;
  }

void GameScript::printNothingToGet() {
  auto id = vm.find_symbol_by_name("player_plunder_is_empty");
  if(id==nullptr) {
    if(owner.version().game==1)
      owner.player()->playAnimByName("T_DONTKNOW", BS_NONE);
    return;
    }
  ScopeVar self(*vm.global_self(), owner.player()->handlePtr());
  vm.call_function<void>(id);
  }

void GameScript::useInteractive(const std::shared_ptr<zenkit::INpc>& hnpc, std::string_view func) {
  auto fn = vm.find_symbol_by_name(func);
  if(fn == nullptr)
    return;

  if(auto* n = findNpc(hnpc); n!=nullptr && n->isPlayer() && nbStates.count(this))
    nbCtx("object: ",func);
  ScopeVar self(*vm.global_self(),hnpc);
  try {
    vm.call_function<void>(fn);
    }
  catch (...) {
    Log::i("unable to use interactive [",func,"]");
    }
  }

Attitude GameScript::guildAttitude(const Npc &p0, const Npc &p1) const {
  auto selfG = std::min<size_t>(gilCount-1,p0.guild());
  auto npcG  = std::min<size_t>(gilCount-1,p1.guild());
  auto ret   = gilAttitudes[selfG*gilCount+npcG];
  return Attitude(ret);
  }

Attitude GameScript::personAttitude(const Npc &p0, const Npc &p1) const {
  if(!p0.isPlayer() && !p1.isPlayer())
    return guildAttitude(p0,p1);

  Attitude att=ATT_NULL;
  const Npc& npc = p0.isPlayer() ? p1 : p0;
  // CoM uses temporary hostility/friendliness for quest fights and restores
  // it afterwards. The saved temporary value must take precedence over the
  // permanent relationship, just as oCNpc::GetAttitude does.
  // New Balance: B_SetAttributesToChapter gives EVERY human the personal attitude ATT_NEUTRAL to the hero
  // (bandits too). In NB the bandits still attack a hero who has no armor of the bandits, so the hostility
  // of the guilds (Wld_SetGuildAttitude: GIL_BDT -> GIL_NONE = ATT_HOSTILE) wins over the neutral personal
  // attitude. FRIENDLY / ANGRY / HOSTILE set by the scripts stay as they are.
  const bool nb = nbStates.find(this)!=nbStates.end();
  att = npc.tempAttitude();
  if(att==ATT_NULL)
    att = npc.attitude();
  if(att!=ATT_NULL) {
    if(nb && att==ATT_NEUTRAL && guildAttitude(p0,p1)==ATT_HOSTILE)
      return ATT_HOSTILE;
    return att;
    }
  att = guildAttitude(p0,p1);
  return att;
  }

/// The attitude without the temporary one: the permanent attitude of the character, else the one of the guilds.
Attitude GameScript::personAttitudeNoTemp(const Npc &p0, const Npc &p1) const {
  if(!p0.isPlayer() && !p1.isPlayer())
    return guildAttitude(p0,p1);

  const Npc& npc = p0.isPlayer() ? p1 : p0;
  Attitude   att = npc.attitude();
  if(att!=ATT_NULL)
    return att;
  return guildAttitude(p0,p1);
  }

bool GameScript::isFriendlyFire(const Npc& src, const Npc& dst) const {
  const int AIV_PARTYMEMBER = (owner.version().game==2) ? 15 : 36;
  if(src.isPlayer())
    return false;
  if(personAttitude(src, dst)==ATT_FRIENDLY)
    return true;
  if(src.handlePtr()->aivar[AIV_PARTYMEMBER]!=0 && dst.isPlayer())
    return true;
  return false;
  }

BodyState GameScript::schemeToBodystate(std::string_view sc) {
  if(searchScheme(sc,"MOB_SIT"))
    return BS_SIT;
  if(searchScheme(sc,"MOB_LIE"))
    return BS_LIE;
  if(searchScheme(sc,"MOB_CLIMB"))
    return BS_CLIMB;
  if(searchScheme(sc,"MOB_NOTINTERRUPTABLE"))
    return BS_MOBINTERACT;
  return BS_MOBINTERACT_INTERRUPT;
  }

void GameScript::onWldItemRemoved(const Item& itm) {
  onWldInstanceRemoved(&itm.handle());
  }

void GameScript::onWldInstanceRemoved(const zenkit::DaedalusInstance* obj) {
  if(obj==nullptr)
    return;
  auto* symbol = vm.find_symbol_by_instance(*obj);
  // Several loose items may share one script instance. Removing an older
  // object must not clear the symbol already rebound to a newer one (CoM's
  // scripted pickups/spawns); transient instances need no symbol update.
  if(symbol!=nullptr && symbol->get_instance().get()==obj)
    symbol->set_instance(nullptr);
  }

void GameScript::makeCurrent(Item* w) {
  if(w==nullptr)
    return;
  auto* s = vm.find_symbol_by_index(uint32_t(w->clsId()));
  if(s != nullptr)
    s->set_instance(w->handlePtr());
  }

bool GameScript::searchScheme(std::string_view sc, std::string_view listName) {
  std::string_view list = findSymbol(listName)->get_string();
  for(size_t i=0; i<=list.size(); ++i) {
    if(i==list.size() || list[i]==',') {
      if(sc==list.substr(0,i))
        return true;
      if(i==list.size())
         break;
      list = list.substr(i+1);
      i    = 0;
      }
    }
  return false;
  }

zenkit::DaedalusVm GameScript::createVm(Gothic& gothic) {
  auto lang   = gothic.settingsGetI("GAME", "language");
  auto script = gothic.loadScript(gothic.defaultGameDatFile(), ScriptLang(lang));
  auto exef   = zenkit::DaedalusVmExecutionFlag::ALLOW_NULL_INSTANCE_ACCESS;
    if(DirectMemory::isRequired(script) ||
     (script.find_symbol_by_name("RX_ISSPACETNET") != nullptr &&
      script.find_symbol_by_name("_BCANSAVE") != nullptr)) {
    exef |= zenkit::DaedalusVmExecutionFlag::IGNORE_CONST_SPECIFIER;
    }
  return zenkit::DaedalusVm(std::move(script), exef);
  }

bool GameScript::hasSymbolName(std::string_view name) {
  return vm.find_symbol_by_name(name)!=nullptr;
  }

uint64_t GameScript::tickCount() const {
  return owner.tickCount();
  }

void GameScript::tick(uint64_t dt) {
  if(dma!=nullptr)
    dma->tick(dt);

  auto nb = nbStates.find(this);
  if(nb==nbStates.end() || world().player()==nullptr)
    return;

  // NB fix 176: the time of the game until the summons can be called to the hero again
  nb->second.summonCallCd -= std::min(nb->second.summonCallCd,dt);
  nbTransformTick(dt);   // NB fix 181
  nbHeroWatchTick();     // NB fix 229
  if(tickCount()>=nb->second.monsterStatsNext) {
    nb->second.monsterStatsNext = tickCount()+1000;
    nbMonsterStatsTick();   // NB fix 185
    }
  nbMageBlinkJobs();         // NB fix 190
  nbImarahJobs();            // NB fix 192
  if(tickCount()>=nb->second.imarahNext) {
    nb->second.imarahNext = tickCount()+1000;
    nbImarahTick();         // NB fix 192
    nbSummonBossLog();      // NB fix 193
    }
  if(tickCount()>=nb->second.blinkNext) {
    nb->second.blinkNext = tickCount()+500;
    nbMageBlinkTick();      // NB fix 189
    }

  // the delayed functions of NB (RX_CALLBACK*)
  if(!nb->second.callbacks.empty()) {
    const uint64_t now     = tickCount();
    const bool     inDialog = owner.isInDialog();
    auto&          cbs     = nb->second.callbacks;

    std::vector<NbCallback> due;
    for(size_t i=0; i<cbs.size(); ) {
      if(cbs[i].at<=now && !(cbs[i].waitDialog && inDialog)) {
        due.push_back(std::move(cbs[i]));
        cbs.erase(cbs.begin()+ptrdiff_t(i));
        } else {
        ++i;
        }
      }

    for(auto& c:due) {
      auto* fn = vm.find_symbol_by_index(c.func);
      if(fn==nullptr || fn->type()!=zenkit::DaedalusDataType::FUNCTION)
        continue;
      if(c.npc!=nullptr && findNpcRaw(c.npc)==nullptr)
        continue; // the character is gone
      auto prevSelf  = vm.global_self() ->get_instance();
      auto prevOther = vm.global_other()->get_instance();
      auto prevItem  = vm.global_item() ->get_instance();
      if(c.npc!=nullptr)
        vm.global_self()->set_instance(c.npc);
      try {
        if(fn->has_return())
          (void)vm.call_function<int>(fn); else
          vm.call_function<void>(fn);
        }
      catch(const std::exception& e) {
        Log::e("NB: callback ",fn->name()," failed: ",e.what());
        }
      vm.global_self() ->set_instance(prevSelf);
      vm.global_other()->set_instance(prevOther);
      vm.global_item() ->set_instance(prevItem);
      }
    }

  auto& st = nb->second;
  // NB fix 68: the window to sell, when the dialog is closed
  if(st.salesPending && !Gothic::inst().isInDialog()) {
    st.salesPending = false;
    auto  ref  = st.salesNpc.lock();
    auto* npc  = ref!=nullptr ? findNpc(ref.get()) : nullptr;
    auto* hero = world().player();
    if(npc!=nullptr && hero!=nullptr && !st.salesList.empty())
      Gothic::inst().onOpenPlayerSales(*hero,*npc,st.salesList);
    }
  // NB fix 116: a new game or a loaded save starts without the mute of the scripts (a fight with a boss in the save
  // mutes it again by RX_UpdateBossMusicCoords)
  if(!st.musicMuteReset) {
    st.musicMuteReset = true;
    GameMusic::inst().setScriptMute(false);
    }
  nbBossTick();   // NB fix 100: the phases of the bosses
  nbDotTick();    // NB fix 106: the damage over time
  nbShotsTick(dt); // NB fix 157: the projectiles of the monsters
  nbPartyTeleportTick();   // NB fix 161
  if(tickCount()>=nb->second.nextPartyFocus) {
    nb->second.nextPartyFocus = tickCount()+1000;
    nbPartyFocusTick();     // NB fix 163
    }
  if(tickCount()>=nb->second.nextVobRepair) {
    nb->second.nextVobRepair = tickCount()+5000;
    nbRepairVobTrees();   // NB fix 165
    }
  // NB fix 226: RX_GamePaused / RX_GameVoting belong to the multiplayer of NB, no script sets them - in a single game they
  // are 0. A save imported from the PC brought 1 in one of them: every monster in ZS_MM_Attack stood ("CONTINUE 2" of
  // ZS_MM_Attack_Loop: the game is paused - do nothing), the wolves and the goblins did not attack the hero
  for(const char* name:{"RX_GAMEPAUSED","RX_GAMEVOTING"}) {
    auto* sym = vm.find_symbol_by_name(name);
    if(sym!=nullptr && sym->type()==zenkit::DaedalusDataType::INT && sym->count()>0 && sym->get_int()!=0) {
      nbCtx("multiplayer flag ",name," was ",sym->get_int()," - 0 in a single game");
      sym->set_int(0);
      }
    }
  // NB fix 227: the aura of speed of the necromancer (RX_ApplyBuffToSummons(0)) on the undead summons that came after it
  if(Application::tickCount()>=st.nextAuraCheck) {
    st.nextAuraCheck = Application::tickCount()+1000;
    if(nbIntVar("RX_NECRORUNEAURAACTIVE_03")!=0 && nbIntVar("RX_NECRORUNEAURA_2")!=0) {
      if(auto* hero = world().player()) {
        const float k = float(100+5*hero->talentSkill(TALENT_MAGE))/100.f;
        world().detectNpc(hero->position(),5000.f,[&](Npc& n) {
          const int32_t g = n.handle().guild;
          if(!n.isDead() && !n.isPlayer() && (g==37 || g==26 || g==55) && nbIsSummon(n))
            n.nbAuraSpeed = k;
          });
        }
      }
    }
  // NB fix 228: the flags "this summon is up" of the scripts (Hubaxis, Krayt, Jina, Tesvall, the dark servant...): the
  // scripts only set them, the plugin clears them when that creature dies or leaves the world (0x10184eb0). Without it the
  // spell said "Hubaxis jest już przyzwany..." for ever after the first Hubaxis was gone. Every 2 s: a flag on and no living
  // creature of its instance in this world - off
  if(Application::tickCount()>=st.nextSummonFlags) {
    st.nextSummonFlags = Application::tickCount()+2000;
    static const std::pair<const char*,const char*> flags[] = {
      {"CRAIT","CRAITISUP"}, {"SUMKHUBI","RX_DEMONHUB_ACTIVE"}, {"DSNAPPER","DSNAPPERISUP"}, {"DK_PET","DARKPETISUP"},
      {"PET_JINA","JINAWOLFISUP"}, {"SUMMONED_SKELETONUNIQ","SKELETONUNIQISUP"}, {"SCAVENGER_HERO_MOUNT","RX_SCAVSUMMONED"},
      {"SUMMONED_DRAGON","DRAGONSTAFFALIVE"},
      };
    bool any = false;
    for(auto& f:flags)
      if(nbIntVar(f.second)!=0)
        any = true;
    if(any && world().player()!=nullptr) {
      std::unordered_set<std::string> alive;
      for(uint32_t i=0; i<world().npcCount(); ++i) {
        auto* n = world().npcById(i);
        if(n==nullptr || n->isDead())
          continue;
        if(auto* sym = vm.find_symbol_by_index(uint32_t(n->handle().symbol_index())))
          alive.insert(sym->name());
        }
      for(auto& f:flags) {
        auto* v = vm.find_symbol_by_name(f.second);
        if(v==nullptr || v->type()!=zenkit::DaedalusDataType::INT || v->is_member() || v->get_int()==0)
          continue;
        if(alive.find(f.first)!=alive.end())
          continue;
        nbCtx("summon flag ",f.second," off: no ",f.first," alive in this world");
        v->set_int(0);
        }
      }
    }
  // NB fix 221: StartChangeWorldTime - the timer of the plugin (100 ms) takes the speed of the game down by an even step
  // to the factor in the first half of the steps and back to 1 in the second half
  if(st.slowSteps>0) {
    const int64_t k    = int64_t((Application::tickCount()-st.slowStart)/100);
    const int64_t half = std::max<int64_t>(st.slowSteps/2,1);
    const float   step = (1.f-st.slowFactor)/float(half);
    float f = 1.f;
    if(k<half)
      f = 1.f-step*float(k);
    else if(k<st.slowSteps)
      f = 1.f-step*float(st.slowSteps-k);
    else
      st.slowSteps = 0;
    f = std::clamp(f,0.05f,1.f);
    if(std::abs(f-st.slowApplied)>0.001f) {
      st.slowApplied = f;
      owner.setTimeMultiplyer(f);
      }
    }
  // NB fix 82: the music of a boss fight plays again from the start; it ends 45 s after the last blow of the fight (the
  // player has run away) or with the death of the player, as the death of the boss does (RX_StopBossMusic)
  if(Gothic::inst().isBossMusic()) {
    auto* hero = world().player();
    if(hero==nullptr || hero->isDead() || Application::tickCount()>st.bossMusicSeen+45000) {
      nbCtx("boss music: stop (",hero==nullptr || hero->isDead() ? "the hero is dead" : "no fight for 45 s",")");
      auto* fn = vm.find_symbol_by_name("RX_STOPBOSSMUSIC");
      if(fn!=nullptr && fn->type()==zenkit::DaedalusDataType::FUNCTION) {
        try {
          vm.call_function<void>(fn);
          }
        catch(const std::exception& e) {
          Log::e("NB: RX_StopBossMusic failed: ",e.what());
          }
        }
      Gothic::inst().stopBossMusic();
      } else {
      Gothic::inst().tickBossMusic();
      }
    }
  // NB fix 86: the acrobatics from the settings (KEY_ACROBAT of the PC): RX_AcrobaticActive and the overlay of the rolls
  // (RX_ApplyAcrobatic puts Humans_Acrobatic.MDS on the hero who has learned it, from Cassia)
  if(const int pref = nbAcrobaticPref.load(); pref>=0) {
    auto* var  = vm.find_symbol_by_name("RX_ACROBATICACTIVE");
    auto* fn   = vm.find_symbol_by_name("RX_APPLYACROBATIC");
    auto* hero = world().player();
    if(var!=nullptr && fn!=nullptr && hero!=nullptr && var->type()==zenkit::DaedalusDataType::INT &&
       fn->type()==zenkit::DaedalusDataType::FUNCTION && var->get_int()!=pref && !hero->isDead()) {
      var->set_int(pref);
      nbCtx("acrobatics: ",pref!=0 ? "on" : "off");
      try {
        vm.call_function<void>(fn,hero->handlePtr());
        }
      catch(const std::exception& e) {
        Log::e("NB: RX_ApplyAcrobatic failed: ",e.what());
        }
      }
    }
  // NB fix 70: the crafting window, when the hero is at the table and no dialog is open
  if(st.craftPending>=0 && !Gothic::inst().isInDialog()) {
    const int32_t craft = st.craftPending;
    st.craftPending = -1;
    if(auto* hero = world().player())
      Gothic::inst().onOpenCraft(*hero,craft);
    }
  const auto callNb = [this](zenkit::DaedalusSymbol* fn) {
    if(fn==nullptr)
      return;
    try {
      if(fn->has_return())
        (void)vm.call_function<int>(fn); else
        vm.call_function<void>(fn);
      }
    catch(const std::exception& e) {
      Log::e("NB: ",fn->name()," failed: ",e.what());
      }
    };

  // PerFrame: every frame, RX_FrameTime is the time of the frame in ms (the plugin writes it)
  if(st.perFrame!=nullptr) {
    if(st.frameTime!=nullptr)
      st.frameTime->set_int(int32_t(dt));
    callNb(st.perFrame);
    }

  // NB fix 173: the end of the sprint without the stamina (PerFrame of the scripts: RX_SpeedPotion<=0, ATR_STAMINA<=0 and
  // SPRINT_ACTIVE -> Mdl_RemoveOverlayMDS "Humans_MageSprint.MDS", SPRINT_ACTIVE=FALSE). PerFrame does it only every second frame
  // and under its own conditions (not in the change of the form, not dead); a sprint that stayed on with no stamina (the
  // overlay applied again by another script after it) ran on, another time it ended. Here as PerFrame, for the overlay
  // itself as well
  if(tickCount()>=st.nextSprintCheck) {
    st.nextSprintCheck = tickCount()+200;
    auto* pl = world().player();
    const bool    known   = vm.find_symbol_by_name("ATR_STAMINA")!=nullptr && vm.find_symbol_by_name("RX_SPEEDPOTION")!=nullptr;
    const int32_t stamina = nbIntVar("ATR_STAMINA");
    const int32_t speed   = nbIntVar("RX_SPEEDPOTION");
    if(known && pl!=nullptr && !pl->isDead() && stamina<=0 && speed<=0 && pl->hasOverlay("Humans_MageSprint.MDS")) {
      pl->delOverlay("Humans_MageSprint.MDS");
      if(auto* act = vm.find_symbol_by_name("SPRINT_ACTIVE"); act!=nullptr && act->type()==zenkit::DaedalusDataType::INT &&
         !act->is_member())
        act->set_int(0);
      nbCtx("sprint: no stamina - the sprint ends");
      }
    }

  // NB fix 62: the HUD of NB - GamePaused of the scripts (the karma of the gods, the arrows, the hunger, the icons of
  // the state; PrintScreen for 1 s, so the plugin calls it every frame - here every 250 ms) and the list of the
  // effects with their time of the plugin
  if((tickCount()>=st.nextHud || st.nextHud>tickCount()+1000) && !Gothic::inst().nbSalesOpen) {
    st.nextHud = tickCount()+250;
    callNb(st.gamePaused);
    nbBuffPanel(vm,nbSummonsCallCooldown());
    // NB fix 64: the texts of the wisp (positions from the video of the PC: the state at 86% / 36%, the list at
    // 2% from 57% down)
    if(!Gothic::inst().isInDialog()) {
      auto& fnt = Resources::font("FONT_OLD_10_WHITE.TGA",Resources::FontType::Normal,1.f);
      if(tickCount()<st.wispSeen+1000) {
        const bool busy = !st.wispItem.expired() || !st.wispBody.expired();
        const std::string txt = busy ? std::string(NbTexts::get("sWispSearch")) : std::string("Szukanie...");
        Gothic::inst().onPrintScreen(txt,84,36,1,fnt);
        }
      st.wispLines.erase(std::remove_if(st.wispLines.begin(),st.wispLines.end(),
                         [this](const std::pair<std::string,uint64_t>& l){ return l.second<tickCount(); }),st.wispLines.end());
      while(st.wispLines.size()>7)
        st.wispLines.erase(st.wispLines.begin());
      auto& lst = Resources::font("FONT_OLD_10_WHITE.TGA",Resources::FontType::Yellow,1.f);
      for(size_t i=0; i<st.wispLines.size(); ++i)
        Gothic::inst().onPrintScreen(st.wispLines[i].first,2,57+int(i)*3,1,lst);
      }
    // NB fix 106: the damage over time on the left: the fire yellow, the poison green, the magic blue
    {
      static const GthFont fire   = Resources::font("FONT_OLD_10_WHITE.TGA",Resources::FontType::Normal,1.f).tinted(Color(1.f,0.85f,0.15f));
      static const GthFont poison = Resources::font("FONT_OLD_10_WHITE.TGA",Resources::FontType::Normal,1.f).tinted(Color(0.25f,0.95f,0.25f));
      static const GthFont magic  = Resources::font("FONT_OLD_10_WHITE.TGA",Resources::FontType::Normal,1.f).tinted(Color(0.45f,0.65f,1.f));
      static const GthFont other  = Resources::font("FONT_OLD_10_WHITE.TGA",Resources::FontType::Normal,1.f).tinted(Color(1.f,0.35f,0.3f));
      st.dotLines.erase(std::remove_if(st.dotLines.begin(),st.dotLines.end(),
                        [this](const NbState::DotLine& l){ return l.until<tickCount(); }),st.dotLines.end());
      while(st.dotLines.size()>6)
        st.dotLines.erase(st.dotLines.begin());
      for(size_t i=0; i<st.dotLines.size(); ++i) {
        const auto& l = st.dotLines[i];
        const GthFont& f = l.type==8 ? fire : l.type==5 ? poison : l.type==32 ? magic : other;
        Gothic::inst().onPrintScreen(l.text,2,33+int(i)*3,1,f);
        }
    }
    }

  // NB fix 61: the search of the wisp. The scripts of NB do not look for the items (B_MM_WispDetect is never called -
  // the plugin does it in itself, UNION_ABI.DLL 0x102740c0): every 500 ms, when the wisp (WISP_DETECTOR) is near the
  // hero and has no item, the nearest item lying around the hero (10 m, without an owner, not higher than 5 m) - the
  // wisp flies to it, "Coś znaleziono!", and the code of fix 61 below gives it to the hero
  if(st.wispItem.expired() && st.wispBody.expired() && tickCount()>=st.nextWispSearch) {
    st.nextWispSearch = tickCount()+100;   // NB fix 61: was 500 ms (slow next to the speed of the plugin)
    Npc* hero = world().player();
    Npc* wisp = nullptr;
    if(hero!=nullptr && !hero->isDead()) {
      world().detectNpc(hero->position(),6000.f,[&](Npc& n) {
        if(wisp!=nullptr || n.isDead())
          return;
        auto* sym = vm.find_symbol_by_index(n.handle().symbol_index());
        if(sym!=nullptr && sym->name()=="WISP_DETECTOR")
          wisp = &n;
        });
      }
    // NB fix 175: the wisp of the plugin has no collision - it stood in the way of the hero in the narrow passages
    if(wisp!=nullptr)
      wisp->nbSetNoCollision(true);
    if(wisp==nullptr && hero!=nullptr && tickCount()>=st.nextWispLog) {
      st.nextWispLog = tickCount()+10000;
      if(auto* ws = vm.find_symbol_by_name("WISP_DETECTOR"); ws!=nullptr && findNpc(ws)!=nullptr)
        nbCtx("wisp search: WISP_DETECTOR is in the world, but not in 60 m of the hero");
      }
    // NB fix 61: the wisp follows the hero (the plugin moves it: ZS_MM_Rtn_Summoned leaves WISP_DETECTOR out of its
    // AI_GotoNpc). Farther than 3 m and nothing to do: to the hero, running; farther than 25 m: next to the hero
    bool wispNear = false;
    if(wisp!=nullptr && !Gothic::inst().isInDialog()) {
      const auto  d    = wisp->position()-hero->position();
      const float dist = d.length();
      if(dist>4000.f) {
        // NB fix 154: the flight back stops and the new place is applied at once (the transform too) - the far wisp
        // was put next to the hero, but the log showed it 40-50 m away again and again
        const float a = hero->rotation()*3.14159265f/180.f;
        wisp->clearAiQueue();
        wisp->setPosition(hero->position()+Tempest::Vec3(std::sin(a)*120.f,60.f,std::cos(a)*120.f));
        wisp->clearSpeed();   // NB fix 175
        wisp->updateTransform();
        st.wispReturn = false;
        if(tickCount()>=st.nextWispFarLog) {
          st.nextWispFarLog = tickCount()+5000;
          nbCtx("the wisp was ",int(dist)," cm away: next to the hero again");
          }
        }
      else if(dist>800.f) {
        // NB fix 64: the wisp of the plugin does not stay at the hero, it flies around and searches - back to the
        // hero only when it has nothing to do and is farther than 8 m
        if(!st.wispReturn) {
          wisp->clearAiQueue();
          st.wispNpc    = wisp->handlePtr();
          st.wispReturn = true;   // the flight itself: below, every frame
          }
        }
      wispNear = dist<=3000.f;
      st.wispSeen = tickCount();
      }
    if(wisp!=nullptr && wispNear && !Gothic::inst().isInDialog()) {
      const auto katOf = [this](std::string_view name, int32_t def) {
        auto* k = vm.find_symbol_by_name(name);
        return k!=nullptr && k->type()==zenkit::DaedalusDataType::INT ? k->get_int() : def;
        };
      // RX_Wisp_DontTakeImportantItems (the dialog of the wisp): not the armors and the potions
      auto* dti = vm.find_symbol_by_name("RX_WISP_DONTTAKEIMPORTANTITEMS");
      const bool noImportant = dti!=nullptr && dti->type()==zenkit::DaedalusDataType::INT && dti->get_int()!=0;
      const int32_t important = katOf("ITEM_KAT_ARMOR",16) | katOf("ITEM_KAT_POTIONS",128);
      for(auto i=st.wispSkip.begin(); i!=st.wispSkip.end();)
        i = (i->second<tickCount()) ? st.wispSkip.erase(i) : std::next(i);

      Item* best  = nullptr;
      float bestD = std::numeric_limits<float>::max();
      const auto hp = hero->position();
      const auto wp = wisp->position();
      std::vector<Item*> nearWisp;   // NB fix 61: the items right at the wisp - taken at once, without flying
      int nAll = 0, nOwned = 0, nSkip = 0, nImportant = 0, nKey = 0, nHigh = 0;
      // NB fix 66: the area: 10 m, with the skill „Zwiększony obszar zbierania" 20 m
      const float area = nbWispSkill(vm,8) ? 2000.f : 1000.f;
      int nUnlearned = 0;
      world().detectItem(hp,area,[&](Item& it) {
        auto& h = it.handle();
        ++nAll;
        if(h.owner!=0 || h.owner_guild!=0) {
          // NB fix 61: an item with an owner, as the plugin outside of the houses: taken when the owner (or one of
          // the guild of the owner) is not within 20 m - no one sees the theft. In the wild NB has many such items
          bool witness = false;
          world().detectNpc(it.position(),2000.f,[&](Npc& n) {
            if(witness || n.isDead() || &n==hero || &n==wisp)
              return;
            if((h.owner!=0 && int32_t(n.handle().symbol_index())==h.owner) ||
               (h.owner_guild!=0 && n.handle().guild==h.owner_guild))
              witness = true;
            });
          if(witness) {
            ++nOwned;
            return;   // a theft before the eyes of the owner
            }
          }
        if(st.wispSkip.count(it.handlePtr())) {
          ++nSkip;
          return;
          }
        if(noImportant && (h.main_flag & important)!=0) {
          ++nImportant;
          return;
          }
        if(auto* sym = vm.find_symbol_by_index(uint32_t(it.clsId())); sym!=nullptr) {
          // the plugin leaves the keys (but the lockpicks) to the hero
          const std::string_view nm = sym->name();
          if(nm.size()>=4 && nm.substr(0,4)=="ITKE" && nm!="ITKE_LOCKPICK") {
            ++nKey;
            return;
            }
          // NB fix 66: only what the wisp has learned (from Vatras it can take the documents only)
          const int skill = nbWispSkillFor(h.main_flag,nm);
          if(skill>=0 && !nbWispSkill(vm,skill)) {
            ++nUnlearned;
            return;
            }
          }
        const auto d = it.position()-hp;
        if(std::abs(d.y)>500.f) {
          ++nHigh;
          return;
          }
        // NB fix 61: the nearest to the wisp (not to the hero) - the shortest flights one after another
        const auto  dw = it.position()-wp;
        const float hw = dw.x*dw.x+dw.z*dw.z;
        if(hw<350.f*350.f && std::abs(dw.y)<400.f) {
          nearWisp.push_back(&it);
          return;
          }
        const float q = dw.quadLength();
        if(q<bestD) {
          bestD = q;
          best  = &it;
          }
        });
      // NB fix 61: what lies at the wisp goes to the hero at once (up to 5 at a time, as the plugin that takes
      // a heap of mushrooms in a moment)
      if(!nearWisp.empty()) {
        std::vector<std::string> names;
        for(size_t i=0; i<nearWisp.size() && i<5; ++i) {
          Item* itm = nearWisp[i];
          const std::string name(itm->displayName());
          const size_t      cnt = itm->count();
          if(auto ptr = world().takeItem(*itm)) {
            hero->addItem(std::move(ptr));
            nbCtx("the wisp brings: ",name," (at the wisp)");
            names.push_back(name+" x"+std::to_string(std::max<size_t>(cnt,1)));
            }
          }
        for(auto& n:names)
          st.wispLines.emplace_back(std::string(NbTexts::get("sWispName"))+": "+n,tickCount()+10000);
        st.nextWispSearch = 0;   // the next ones at once
        best = nullptr;          // a flight next time: the list of the world changed
        }
      if(tickCount()>=st.nextWispLog) {
        st.nextWispLog = tickCount()+10000;
        nbCtx("wisp search: the wisp at ",int((wisp->position()-hp).length())," cm, items in ",int(area/100.f)," m: ",nAll,
              " (not learned ",nUnlearned,", owner near ",nOwned,", skipped ",nSkip,", important ",nImportant,", keys ",nKey,", height ",nHigh,")",
              best!=nullptr ? std::string(" -> ")+std::string(best->displayName()) : std::string());
        }
      if(best!=nullptr) {
        auto ref = best->handlePtr();
        if(ref!=nullptr) {
          // NB fix 64: the wisp flies straight to the item, fast (below, every frame) - not by walking of the AI
          wisp->clearAiQueue();
          st.wispReturn = false;
          st.wispNpc   = wisp->handlePtr();
          st.wispItem  = ref;
          st.wispSince = tickCount();
          st.wispBest  = 1e9f;
          if(st.wispFound!=ref.get()) {
            st.wispFound = ref.get();
            nbCtx("the wisp found: ",best->displayName()," at ",int(std::sqrt(bestD))," cm from the wisp");
            }
          }
        }
      // NB fix 65: nothing lies around - the loot of the dead (as the skill 10 of the plugin: „Zbieranie
      // przedmiotów z przeciwników"): the nearest body in 20 m with something to take (what the hero sees in it)
      else if(nearWisp.empty() && nbWispSkill(vm,0)) {   // NB fix 66: only with the skill 0
        for(auto i=st.wispBodySkip.begin(); i!=st.wispBodySkip.end();)
          i = (i->second<tickCount()) ? st.wispBodySkip.erase(i) : std::next(i);
        for(auto i=st.wispBodySeen.begin(); i!=st.wispBodySeen.end();)
          i = (i->first.expired() || i->second+600000<tickCount()) ? st.wispBodySeen.erase(i) : std::next(i);
        Npc*  body  = nullptr;
        float bodyD = std::numeric_limits<float>::max();
        world().detectNpc(hp,area,[&](Npc& n) {
          if(!n.isDead() || &n==hero || &n==wisp || st.wispBodySkip.count(n.handlePtr()))
            return;
          if(std::abs(n.position().y-hp.y)>500.f)
            return;
          if(!n.inventory().iterator(Inventory::T_Ransack).isValid())
            return;
          // NB fix 177: ZS_Dead of NB gives the loot of the death (B_GiveDeathInv) and takes the weapon off the belt
          // (AI_UnequipWeapons, from the queue of the AI) - a body taken at once kept its sword
          auto seen = st.wispBodySeen.try_emplace(n.handlePtr(),tickCount()).first;
          if(tickCount()<seen->second+1000)
            return;
          const float q = (n.position()-wp).quadLength();
          if(q<bodyD) {
            bodyD = q;
            body  = &n;
            }
          });
        if(body!=nullptr) {
          wisp->clearAiQueue();
          st.wispReturn = false;
          st.wispNpc    = wisp->handlePtr();
          st.wispBody   = body->handlePtr();
          st.wispSince  = tickCount();
          nbCtx("the wisp found the body: ",body->displayName());
          }
        }
      }
    }

  // NB fix 65: the wisp at the body: everything that the hero would take from it goes to the inventory
  if(auto wBody = st.wispBody.lock()) {
    auto  wNpc = st.wispNpc.lock();
    auto* wisp = wNpc!=nullptr ? findNpc(wNpc.get()) : nullptr;
    auto* body = findNpc(wBody.get());
    auto* hero = world().player();
    if(wisp==nullptr || body==nullptr || hero==nullptr || !body->isDead() || tickCount()>st.wispSince+15000) {
      if(wisp!=nullptr && body!=nullptr && body->isDead())
        nbCtx("the wisp did not reach the body: ",body->displayName()," (",int((body->position()-wisp->position()).length())," cm)");
      if(body!=nullptr)
        st.wispBodySkip[wBody] = tickCount()+60000;
      st.wispBody.reset();
      }
    else {
      const auto to = body->position()+Tempest::Vec3(0,60.f,0);
      const auto d  = to-wisp->position();
      if(std::sqrt(d.x*d.x+d.z*d.z)<350.f && std::abs(d.y)<400.f) {
        std::vector<std::pair<size_t,size_t>> loot;   // the class, the count
        std::vector<std::string>              names;
        for(auto it = body->inventory().iterator(Inventory::T_Ransack); it.isValid(); ++it) {
          loot.emplace_back(it->clsId(),it.count());
          names.emplace_back(std::string(it->displayName())+" x"+std::to_string(std::max<size_t>(it.count(),1)));
          }
        for(auto& [cls,cnt]:loot)
          hero->addItem(cls,*body,cnt);
        for(auto& n:names) {
          st.wispLines.emplace_back(std::string(NbTexts::get("sWispName"))+": "+n,tickCount()+10000);
          nbCtx("the wisp brings: ",n," (from the body of ",body->displayName(),")");
          }
        // done; NB fix 177: again in 3 s, if something comes to the body later (the weapon off the belt, a drop of a
        // script) - with 10 min the wisp left it there
        st.wispBodySkip[wBody] = tickCount()+3000;
        st.wispBody.reset();
        st.nextWispSearch = 0;
        }
      else {
        const float dist = d.length();
        const float step = std::min(dist,(nbWispSkill(vm,9) ? 1500.f : 800.f)*float(dt)/1000.f);
        if(dist>1.f) {
          wisp->setPosition(wisp->position()+d*(step/dist));
          wisp->setDirection(Tempest::Vec3(d.x,0,d.z));
          wisp->clearSpeed();   // NB fix 175: see wispFly
          }
        }
      }
    }

  // NB fix 61: the wisp of Vatras brings the item it flew to (in NB the Union plugin does it, the scripts only send it
  // to the item): when it is at the item, the item goes to the inventory of the hero. Not the items of others (a theft)
  // NB fix 64: the flight of the wisp: straight, 15 m/s, as the wisp of the plugin (2-3 s for a heap of items).
  // The walking of the AI was slow and went around every stone
  const auto wispFly = [&](Npc& w, const Tempest::Vec3& to) {
    auto  d    = to-w.position();
    float dist = d.length();
    if(dist<1.f)
      return;
    // NB fix 154: farther than 15 m the wisp flies twice as fast - a running (or sprinting) hero left it behind, at 40 m it
    // was taken away and put next to him again: the wisp vanished and came back
    float speed = nbWispSkill(vm,9) ? 1500.f : 800.f;
    if(dist>1500.f)
      speed *= 2.f;
    const float step = std::min(dist,speed*float(dt)/1000.f);
    w.setPosition(w.position()+d*(step/dist));
    w.setDirection(Tempest::Vec3(d.x,0,d.z));
    // NB fix 175: the flight has no gravity. Over an edge or a slope the wisp was in the air for its move: the speed of
    // the fall grew every frame, and after a second it was more than the flight - the log showed the wisp above the
    // item (1 cm) and 36-287 m below it: it fell through the world, the item and the body stayed
    w.clearSpeed();
    };
  if(st.wispReturn && st.wispItem.expired() && st.wispBody.expired()) {
    auto  wNpc = st.wispNpc.lock();
    auto* wisp = wNpc!=nullptr ? findNpc(wNpc.get()) : nullptr;
    auto* hero = world().player();
    if(wisp==nullptr || hero==nullptr || wisp->isDead()) {
      st.wispReturn = false;
      } else {
      const auto to = hero->position()+Tempest::Vec3(0,120.f,0);
      if((to-wisp->position()).length()<300.f)
        st.wispReturn = false; else
        wispFly(*wisp,to);
      }
    }
  if(auto wItm = st.wispItem.lock()) {
    auto wNpc = st.wispNpc.lock();
    auto* wisp = wNpc!=nullptr ? findNpc(wNpc.get()) : nullptr;
    auto* hero = world().player();
    if(wisp==nullptr || wisp->isDead() || hero==nullptr || tickCount()>st.wispSince+15000) {
      // lost, or 20 s without reaching it
      if(st.wispBest<1e9f)
        nbCtx("the wisp did not reach the item: closest ",int(st.wispBest)," cm (height ",int(st.wispBestH)," cm)");
      if(auto w = st.wispItem.lock())
        st.wispSkip[w] = tickCount()+60000;
      st.wispItem.reset();
      }
    else if(auto* itm = findItem(wItm.get())) {
      wispFly(*wisp,itm->position()+Tempest::Vec3(0,60.f,0));
      // NB fix 61: the distance on the ground and the height apart - the wisp flies above the item (the 3D distance
      // of 160 cm of fix 61 was never reached: it stopped above the item and the item stayed)
      const auto  d  = itm->position()-wisp->position();
      const float dh = std::sqrt(d.x*d.x+d.z*d.z);
      if(dh<st.wispBest) {
        st.wispBest  = dh;
        st.wispBestH = std::abs(d.y);
        }
      if(dh<350.f && std::abs(d.y)<400.f) {
        // is it still lying in the world (not taken by someone already)?
        bool inWorld = false;
        for(uint32_t i=0; auto* w = world().itmById(i); ++i)
          if(w==itm) {
            inWorld = true;
            break;
            }
        st.wispItem.reset();
        st.wispBest = 1e9f;
        // the owner: checked by the search (fix 61 - no owner within 20 m)
        if(!inWorld)
          nbCtx("the wisp is at the item, but it is not in the world any more");
        if(inWorld) {
          if(itm->handle().owner!=0 || itm->handle().owner_guild!=0)
            nbCtx("the wisp takes an item of an owner (",itm->handle().owner,"/",itm->handle().owner_guild,"), no one of them near");
          const std::string name(itm->displayName());
          const size_t      cnt = itm->count();
          wisp->clearAiQueue();   // its AI_GotoItem of this item, if it did not start it yet (fix 63: a crash)
          if(auto ptr = world().takeItem(*itm)) {
            hero->addItem(std::move(ptr));
            nbCtx("the wisp brings: ",name);
            // "Ognik: name xN" on the left, as the plugin (fix 64)
            st.wispLines.emplace_back(std::string(NbTexts::get("sWispName"))+": "+name+" x"+
                                      std::to_string(std::max<size_t>(cnt,1)),tickCount()+10000);
            }
          st.nextWispSearch = 0;   // NB fix 61: the next item at once
          }
        }
      }
    else {
      st.wispItem.reset();
      }
    }

  // B_CheckMenuOption: the plugin calls it after the menu; here once in 5 s (it only reads the options)
  if(st.checkMenu!=nullptr && tickCount()>=st.nextMenuCheck) {
    st.nextMenuCheck = tickCount()+5000;
    callNb(st.checkMenu);
    }

  if(tickCount()>=st.nextMoveset) {
    st.nextMoveset = tickCount()+100;
    auto* sepSym = vm.find_symbol_by_name("GIL_SEPERATOR_HUM");
    NbMoveset::tick(*this,world(),(sepSym!=nullptr ? sepSym->get_int() : 21));

    // NB fix 43: the level of the animations of the weapons of the hero. RX_LastAnim_1h/2h is the level that the
    // player chose in the quick panel of the plugin (RX_QuickPanel_ChangeAnim: 1 fighter, 2 ... 3 master), -1 is
    // "by the talent". There is no quick panel here: a 0 (amateur) stayed from the start of the game and every
    // weapon took the animations of an amateur, also of a master. With the talent learned it goes back to -1 and
    // B_CheckMenuOption (DoCheckFightSkill1h/2h) puts on the animations of the level of the talent.
    if(auto* pl = world().player()) {
      bool changed = false;
      const auto autoLevel = [&](std::string_view lastAnim, std::string_view check, int talent) {
        auto* v = vm.find_symbol_by_name(lastAnim);
        auto* c = vm.find_symbol_by_name(check);
        if(v==nullptr || c==nullptr || v->is_member() || c->is_member() ||
           v->type()!=zenkit::DaedalusDataType::INT || c->type()!=zenkit::DaedalusDataType::INT)
          return;
        if(v->get_int()==0 && pl->handle().hitchance[talent]>=30) {
          v->set_int(-1);
          c->set_int(1);
          changed = true;
          }
        };
      autoLevel("RX_LASTANIM_1H","DOCHECKFIGHTSKILL1H",TALENT_1H);
      autoLevel("RX_LASTANIM_2H","DOCHECKFIGHTSKILL2H",TALENT_2H);
      if(changed)
        st.nextMenuCheck = 0;   // B_CheckMenuOption at the next frame
      }
    }

  // NB fix 50: the NPCs that regenerate (RX_RestoreNpcHealthSlow): 2% of the maximum every second, until full
  if(!st.regen.empty() && tickCount()>=st.nextRegen) {
    st.nextRegen = tickCount()+1000;
    st.regen.erase(std::remove_if(st.regen.begin(),st.regen.end(),[this](const std::weak_ptr<zenkit::INpc>& w) {
      auto p   = w.lock();
      auto npc = p!=nullptr ? findNpc(p) : nullptr;
      if(npc==nullptr || npc->isDead() || npc->isDown())
        return true;
      const int32_t hp  = npc->attribute(ATR_HITPOINTS);
      const int32_t max = npc->attribute(ATR_HITPOINTSMAX);
      if(hp>=max)
        return true;
      npc->changeAttribute(ATR_HITPOINTS,std::min(max-hp,std::max(1,max/50)),false);
      return false;
      }),st.regen.end());
    }

  // the timer of the plugin (UNION_ABI.DLL 0x10261040): RX_Main_CycleTrigger, at the next tick b_gettime, and again.
  // NB fix 57: the tick is 500 ms (each of the two once a second), and 16 ms while PrologCount==0 (the first moment of
  // a new game). It was 1000 ms here: everything of the two ran at the half speed - the prologue (PrologCount in
  // Hero_Poisoned, 58 steps) took ~2 minutes and the camera of the tower (60 s) ended in the middle of the speech
  {
    auto*   pc       = vm.find_symbol_by_name("PROLOGCOUNT");
    const uint64_t interval = (pc!=nullptr && pc->type()==zenkit::DaedalusDataType::INT && pc->get_int()>0) ? 500 : 16;
    if(st.nextCycle>tickCount()+1000)
      st.nextCycle = 0;   // a loaded save has another time
    if(tickCount()>=st.nextCycle) {
      st.nextCycle = tickCount()+interval;
      ++st.unionCycle;
      if(st.unionCycle==1) {
        callNb(st.cycle);
        }
      if(st.unionCycle>=2) {
        st.unionCycle = 0;
        callNb(st.getTime);
        }
      }
  }

  nbSummonsFollowTick();   // NB fix 131

  // NB fix 146: the range of drawing of the characters (NB_OG/npcDistance, the index of the choice) - 4 times a second
  if(tickCount()>=st.npcRangeNext) {
    st.npcRangeNext = tickCount()+250;
    static const float npcRange[] = {0.f, 3000.f, 4000.f, 5000.f, 6000.f, 7000.f, 10000.f};
    const int   i     = Gothic::settingsGetI("NB_OG","npcDistance");
    const float range = (i>0 && i<int(std::size(npcRange))) ? npcRange[i] : 0.f;
    if(range>0 || st.npcRangeOn)   // once more after it is switched off: everyone is drawn again
      world().nbNpcRangeTick(range);
    st.npcRangeOn = range>0;
    }

  // NB fix 139: the plugin calls RX_OnEnterLoc_<WORLD> and RX_OnEnterLocAny when a world is loaded to its end (a new game,
  // a save, a change of the world: "OnLevelFullLoaded", UNION_ABI.DLL 0x1027eb7c) - the scripts put in characters there
  // (once, by the masks), take them away, and switch the collision of the materials (the boat to the West Coast)
  if(st.enterLocWorld!=world().serialNumber() && world().player()!=nullptr) {
    st.enterLocWorld = world().serialNumber();
    std::string loc(world().name());
    if(auto dot = loc.rfind('.'); dot!=std::string::npos)
      loc.resize(dot);
    for(auto& ch:loc)
      ch = char(std::toupper(uint8_t(ch)));
    for(const std::string& fn : {"RX_ONENTERLOC_"+loc, std::string("RX_ONENTERLOCANY")}) {
      auto* sym = vm.find_symbol_by_name(fn);
      if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::FUNCTION)
        continue;
      try {
        vm.call_function<void>(sym);
        nbCtx("enter location: ",fn);
        }
      catch(const std::exception& e) {
        Log::e("NB: ",fn," failed: ",e.what());
        }
      }
    }

  const uint64_t currentSecond = tickCount()/1000;
  if(currentSecond==st.lastCycleSecond)
    return;
  st.lastCycleSecond = currentSecond;
  nbVobTreesSync(st,world());
  nbTrollTick();         // NB fix 128
  nbSummonCountTick();   // NB fix 129
  nbBodiesTick();        // NB fix 136
  // NB fix 126: the damage of the spells - counted again after a load (the constants are not in the save-game) and every
  // 3 s (the robe, the curse, the guild change it as well); the spells are made again when the values changed
  if(st.spellsWorld!=world().serialNumber() || tickCount()>=st.spellsNextCheck) {
    st.spellsWorld     = world().serialNumber();
    st.spellsNextCheck = tickCount()+3000;
    const uint64_t h = nbDamageCalcRunes();
    if(h!=0 && h!=st.spellsHash) {
      st.spellsHash  = h;
      st.spellsDirty = true;
      }
    }
  if(st.spellsDirty && spells!=nullptr) {
    st.spellsDirty = false;
    spells->refresh();
    auto* in = vm.find_symbol_by_name("ATR_INTELLECT");
    nbCtx("spells: the damage counted again (intelligence ",in!=nullptr ? in->get_int() : 0,", magic power ",
          nbIntVar("COUNTLEARNSPELL"),"%)");   // NB fix 208: the magic power in the log too
    }
  // NB fix 125: the visibility of the vobs that the scripts changed (RX_ToggleVobVisible) - the vobs of a world come
  // from its ZEN file, so it is put again on every new world (a load, a change of the world)
  if(st.vobVisualWorld!=world().serialNumber()) {
    st.vobVisualWorld = world().serialNumber();
    std::string prefix(world().name());
    for(auto& c:prefix)
      c = char(std::toupper(static_cast<unsigned char>(c)));
    prefix += '|';
    for(auto& [key,show]:st.vobVisual)
      if(key.compare(0,prefix.size(),prefix)==0)
        world().setVobVisualTree(std::string_view(key).substr(prefix.size()),show,show);
    }
  if(st.nextMissingLog>currentSecond+300)
    st.nextMissingLog = 0;   // a loaded save has another time
  if(currentSecond>=st.nextMissingLog) {
    if(st.nextMissingLog!=0)
      nbMissingReport(st,"5 min");
    st.nextMissingLog = currentSecond+300;
    }
  }

uint32_t GameScript::rand(uint32_t max) {
  return uint32_t(randGen())%max;
  }

Npc* GameScript::findNpc(zenkit::DaedalusSymbol* s) {
  if(s!=nullptr && s->is_instance_of<zenkit::INpc>()) {
    auto cNpc = reinterpret_cast<zenkit::INpc*>(s->get_instance().get());
    return findNpc(cNpc);
    }
  return nullptr;
  }

Npc* GameScript::findNpc(zenkit::INpc *handle) {
  if(handle==nullptr)
    return nullptr;
  return reinterpret_cast<Npc*>(handle->user_ptr);
  }

Npc* GameScript::findNpc(const std::shared_ptr<zenkit::INpc>& handle) {
  if(handle==nullptr)
    return nullptr;
  auto* npc = reinterpret_cast<Npc*>(handle->user_ptr);
  if(dma!=nullptr)
    npc = dma->triaRedirect(npc);   // a trialog: the self of the scripts is who speaks
  return npc;
  }

Npc* GameScript::findNpcRaw(const std::shared_ptr<zenkit::INpc>& handle) {
  if(handle==nullptr)
    return nullptr;
  return reinterpret_cast<Npc*>(handle->user_ptr);
  }

Npc* GameScript::findNpcById(const std::shared_ptr<zenkit::DaedalusInstance>& handle) {
  if(handle==nullptr)
    return nullptr;
  if(auto npc = dynamic_cast<const zenkit::INpc*>(handle.get())) {
    return reinterpret_cast<Npc*>(npc->user_ptr);
    }
  return findNpcById(handle->symbol_index());
  }

Item *GameScript::findItem(zenkit::IItem* handle) {
  if(handle==nullptr)
    return nullptr;
  auto& itData = *handle;
  assert(itData.user_ptr); // engine bug, if null
  return reinterpret_cast<Item*>(itData.user_ptr);
  }

Item *GameScript::findItemById(size_t id) {
  auto* handle = vm.find_symbol_by_index(uint32_t(id));
  if(handle==nullptr||!handle->is_instance_of<zenkit::IItem>())
    return nullptr;
  auto hitm = reinterpret_cast<zenkit::IItem*>(handle->get_instance().get());
  return findItem(hitm);
  }

Item* GameScript::findItemById(const std::shared_ptr<zenkit::DaedalusInstance>& handle) {
  if(handle==nullptr)
    return nullptr;
  if(auto itm = dynamic_cast<const zenkit::IItem*>(handle.get())) {
    assert(itm->user_ptr); // engine bug, if null
    return reinterpret_cast<Item*>(itm->user_ptr);
    }
  return findItemById(handle->symbol_index());
  }

Npc* GameScript::findNpcById(size_t id) {
  auto* handle = vm.find_symbol_by_index(uint32_t(id));
  if(handle==nullptr || !handle->is_instance_of<zenkit::INpc>())
    return nullptr;

  auto hnpc = reinterpret_cast<zenkit::INpc*>(handle->get_instance().get());
  if(hnpc==nullptr) {
    auto obj = world().findNpcByInstance(id);
    handle->set_instance(obj ? obj->handlePtr() : nullptr);
    hnpc = reinterpret_cast<zenkit::INpc*>(handle->get_instance().get());
    }
  return findNpc(hnpc);
  }

zenkit::IInfo* GameScript::findInfo(size_t id) {
  auto* sym = vm.find_symbol_by_index(uint32_t(id));
  if(sym==nullptr||!sym->is_instance_of<zenkit::IInfo>())
    return nullptr;
  auto* h = sym->get_instance().get();
  if(h==nullptr)
    Log::e("invalid c_info object: \"",sym->name(),"\"");
  return reinterpret_cast<zenkit::IInfo*>(h);
  }

void GameScript::removeItem(Item &it) {
  world().removeItem(it);
  }

void GameScript::setInstanceNPC(std::string_view name, Npc &npc) {
  auto sym = vm.find_symbol_by_name(name);
  if(sym == nullptr) {
    Tempest::Log::e("Cannot set NPC instance ", name, ": Symbol not found.");
    return;
    }
  sym->set_instance(npc.handlePtr());
  }

void GameScript::setInstanceItem(Npc &holder, size_t itemId) {
  storeItem(holder.getItem(itemId));
  }

AiOuputPipe *GameScript::openAiOuput() {
  return aiDefaultPipe.get();
  }

AiOuputPipe *GameScript::openDlgOuput(Npc &player, Npc &npc) {
  if(player.isPlayer())
    return owner.openDlgOuput(player,npc);
  return owner.openDlgOuput(npc,player);
  }

ScriptFn GameScript::playerPercAssessMagic() {
  auto id = vm.find_symbol_by_name("PLAYER_PERC_ASSESSMAGIC");
  if(id==nullptr)
    return ScriptFn();

  if(id->count()>0)
    return ScriptFn(uint32_t(id->get_int()));
  return ScriptFn();
  }

int GameScript::npcDamDiveTime() {
  auto id = vm.find_symbol_by_name("NPC_DAM_DIVE_TIME");
  if(id==nullptr)
    return 0;
  return id->get_int();
  }

int32_t GameScript::criticalDamageMultiplyer() const {
  return damCriticalMultiplier;
  }

uint32_t GameScript::lockPickId() const {
  return ItKE_lockpick!=nullptr ? ItKE_lockpick->index() : 0;
  }

std::string_view GameScript::menuMain() const {
  if(dma!=nullptr)
    return dma->menuMain();
  return ::menuMain;
  }

bool GameScript::game_initgerman() {
  return true;
  }

bool GameScript::game_initenglish() {
  return true;
  }

void GameScript::wld_settime(int hour, int minute) {
  world().setDayTime(hour,minute);
  }

int GameScript::wld_getday() {
  return int(owner.time().day());
  }

void GameScript::wld_playeffect(std::string_view visual, std::shared_ptr<zenkit::DaedalusInstance> sourceId, std::shared_ptr<zenkit::DaedalusInstance> targetId,
                                int effectLevel, int damage, int damageType, int isProjectile) {
  if(aiProcessPolicy>=NpcProcessPolicy::AiFar2)
    return;

  if(damage>0 && damageType!=0) {
    // an effect that hurts (the fireball of the Sleeper): the effect flies from the source to the target, the damage lands when it arrives
    auto dst = findNpcById(targetId);
    auto src = findNpcById(sourceId);
    if(dst!=nullptr && src!=nullptr) {
      if(const VisualFx* fx = Gothic::inst().loadVisualFx(visual))
        src->startEffect(*dst,*fx);
      const float    dist  = (dst->position()-src->position()).length();
      const uint64_t delay = uint64_t(std::min(2500.f,std::max(200.f,dist/1.5f)));   // ~15 m/s
      dst->takeEffectDamage(*src,damage,damageType,delay);
      return;
      }
    }

  if(isProjectile!=0 || damageType!=0 || damage!=0 || effectLevel!=0) {
    // TODO
    Log::i("effect not implemented [",visual.data(),"]");
    return;
    }
  const VisualFx* vfx = Gothic::inst().loadVisualFx(visual);
  if(vfx==nullptr) {
    Log::i("invalid effect [",visual.data(),"]");
    return;
    }

  auto dstNpc = findNpcById(targetId);
  auto srcNpc = findNpcById(sourceId);

  auto dstItm = findItemById(targetId);
  auto srcItm = findItemById(sourceId);

  // NB fix 150: each effect of the scripts once in the log (where it plays), to find the effects that are not seen
  if(nbStates.find(this)!=nbStates.end()) {
    static std::set<std::string> logged;
    if(logged.insert(std::string(visual)).second)
      nbCtx("effect: ",visual," on ",srcNpc!=nullptr ? srcNpc->displayName() : (srcItm!=nullptr ? srcItm->displayName() : "?"),
            srcNpc!=nullptr || srcItm!=nullptr ? "" : " (no npc or item)");
    }

  if(srcNpc!=nullptr && dstNpc!=nullptr) {
    srcNpc->startEffect(*dstNpc,*vfx);
    } else
  if(srcItm!=nullptr && dstItm!=nullptr){
    Effect e(*vfx,world(),srcItm->position());
    e.setActive(true);
    world().runEffect(std::move(e));
    }
  }

void GameScript::wld_stopeffect(std::string_view visual) {
  const VisualFx*          vfx    = Gothic::inst().loadVisualFx(visual);
  if(vfx==nullptr) {
    Log::i("invalid effect [",visual.data(),"]");
    return;
    }
  if(auto w = owner.world())
    w->stopEffect(*vfx);
  }

int GameScript::wld_getplayerportalguild() {
  int32_t g = GIL_NONE;
  if(auto p = world().player())
    g = world().guildOfRoom(p->portalName());
  return g;
  }

int GameScript::wld_getformerplayerportalguild() {
  int32_t g = GIL_NONE;
  if(auto p = world().player())
    g = world().guildOfRoom(p->formerPortalName());
  return g;
  }

void GameScript::wld_setguildattitude(int gil1, int att, int gil2) {
  if(gil1<0 || gil2<0 || gil1>=int(gilCount) || gil2>=int(gilCount))
    return;
  gilAttitudes[size_t(gil1)*gilCount+size_t(gil2)] = att;
  }

int GameScript::wld_getguildattitude(int gil1, int gil2) {
  if(gil1<0 || gil2<0 || gil1>=int(gilCount) || gil2>=int(gilCount))
    return ATT_HOSTILE; // error
  return gilAttitudes[size_t(gil1)*gilCount+size_t(gil2)];
  }

void GameScript::wld_exchangeguildattitudes(std::string_view name) {
  auto guilds = vm.find_symbol_by_name(name);
  if(guilds==nullptr)
    return;
  for(size_t i=0;i<gilTblSize;++i) {
    for(size_t r=0;r<gilTblSize;++r)
      gilAttitudes[i*gilCount+r] = guilds->get_int(uint16_t(i * gilTblSize + r));
    }
  }

bool GameScript::wld_istime(int hour0, int min0, int hour1, int min1) {
  gtime begin{hour0,min0}, end{hour1,min1};
  gtime now = owner.time();
  now = gtime(0,now.hour(),now.minute());

  if(begin<=end && begin<=now && now<end)
    return true;
  else if(end<begin && (now<end || begin<=now))
    return true;
  else
    return 0;
  }

bool GameScript::wld_isfpavailable(std::shared_ptr<zenkit::INpc> self, std::string_view name) {
  if(self==nullptr){
    return false;
    }

  auto wp = world().findFreePoint(*findNpc(self.get()),name);
  return wp!=nullptr;
  }

bool GameScript::wld_isnextfpavailable(std::shared_ptr<zenkit::INpc> self, std::string_view name) {
  if(self==nullptr){
    return false;
    }
  auto fp = world().findNextFreePoint(*findNpc(self.get()),name);
  return fp != nullptr;
  }

bool GameScript::wld_ismobavailable(std::shared_ptr<zenkit::INpc> self, std::string_view name) {
  auto npc = findNpc(self);
  if(npc==nullptr) {
    return false;
    }

  auto wp = world().availableMob(*npc, name);
  return wp != nullptr;
  }

void GameScript::wld_setmobroutine(int h, int m, std::string_view name, int st) {
  world().setMobRoutine(gtime(h,m), name, st);
  }

/// Wld_SetObjectRoutine of Gothic 2 (Addon): the same as for the mobs, the object is found by the name
void GameScript::wld_setobjectroutine(int h, int m, std::string_view name, int st) {
  world().setMobRoutine(gtime(h,m), name, st);
  }

int GameScript::wld_getmobstate(std::shared_ptr<zenkit::INpc> npcRef, std::string_view scheme) {
  auto npc = findNpc(npcRef);

  if(npc==nullptr) {
    return -1;
    }

  auto mob = world().availableMob(*npc,scheme);
  if(mob==nullptr) {
    return -1;
    }

  return std::max(0,mob->stateId());
  }

void GameScript::wld_assignroomtoguild(std::string_view name, int g) {
  world().assignRoomToGuild(name,g);
  }

bool GameScript::wld_detectnpc(std::shared_ptr<zenkit::INpc> npcRef, int inst, int state, int guild) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr) {
    return false;
    }

  Npc*  ret =nullptr;
  float dist=std::numeric_limits<float>::max();

  world().detectNpc(npc->position(), float(npc->handle().senses_range), [inst,state,guild,&ret,&dist,npc](Npc& n){
    if((inst ==-1 || int32_t(n.instanceSymbol())==inst) &&
       (state==-1 || n.isInState(uint32_t(state))) &&
       (guild==-1 || int32_t(n.guild())==guild) &&
       (&n!=npc) && !n.isDead()) {
      float d = n.qDistTo(*npc);
      if(d<dist){
        ret = &n;
        dist = d;
        }
      }
    });
  if(ret)
    vm.global_other()->set_instance(ret->handlePtr());
  return ret != nullptr;
  }

bool GameScript::wld_detectnpcex(std::shared_ptr<zenkit::INpc> npcRef, int inst, int state, int guild, int player) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr) {
    return false;
    }
  Npc*  ret =nullptr;
  float dist=std::numeric_limits<float>::max();

  world().detectNpc(npc->position(), float(npc->handle().senses_range), [inst,state,guild,&ret,&dist,npc,player](Npc& n){
    if((inst ==-1 || int32_t(n.instanceSymbol())==inst) &&
       (state==-1 || n.isInState(uint32_t(state))) &&
       (guild==-1 || int32_t(n.guild())==guild) &&
       (&n!=npc) && !n.isDead() &&
       (player!=0 || !n.isPlayer())) {
      float d = n.qDistTo(*npc);
      if(d<dist){
        ret = &n;
        dist = d;
        }
      }
    });
  if(ret)
    vm.global_other()->set_instance(ret->handlePtr());
  return ret != nullptr;
  }

bool GameScript::wld_detectitem(std::shared_ptr<zenkit::INpc> npcRef, int flags) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr) {
    return false;
    }

  Item* ret =nullptr;
  float dist=std::numeric_limits<float>::max();
  world().detectItem(npc->position(), float(npc->handle().senses_range), [npc,&ret,&dist,flags](Item& it) {
    if((it.handle().main_flag&flags)==0)
      return;
    float d = (npc->position()-it.position()).quadLength();
    if(d<dist) {
      ret = &it;
      dist= d;
      }
    });

  if(ret)
    vm.global_item()->set_instance(ret->handlePtr());
  return ret != nullptr;
  }

void GameScript::wld_spawnnpcrange(std::shared_ptr<zenkit::INpc> npcRef, int clsId, int count, float lifeTime) {
  auto at = findNpc(npcRef);
  if(at==nullptr || clsId<=0)
    return;

  (void)lifeTime;
  for(int32_t i=0;i<count;++i) {
    auto* npc = world().addNpc(size_t(clsId),at->position());
    fixNpcPositionNear(*npc,at->position(),at->rotation() + 360.f*float(i)/float(count),100);   // NB fix 223
    }
  }

void GameScript::wld_sendtrigger(std::string_view triggerTarget) {
  if(triggerTarget.empty())
    return;

  auto& world = *owner.world();
  const TriggerEvent evt(std::string{triggerTarget},"",world.tickCount(),TriggerEvent::T_Trigger);
  world.triggerEvent(evt);
  }

void GameScript::wld_senduntrigger(std::string_view triggerTarget) {
  if(triggerTarget.empty())
    return;
  auto& world = *owner.world();
  const TriggerEvent evt(std::string{triggerTarget},"",world.tickCount(),TriggerEvent::T_Untrigger);
  world.triggerEvent(evt);
  }

bool GameScript::wld_israining() {
  return world().isRaining();
  }

void GameScript::mdl_setvisual(std::shared_ptr<zenkit::INpc> npcRef, std::string_view visual) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return;
  npc->setVisual(visual);
  }

void GameScript::mdl_setvisualbody(std::shared_ptr<zenkit::INpc> npcRef, std::string_view body, int bodyTexNr, int bodyTexColor, std::string_view head, int headTexNr, int teethTexNr, int armor) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return;

  npc->setVisualBody(headTexNr,teethTexNr,bodyTexNr,bodyTexColor,body,head);
  if(armor>0) {
    if(npc->itemCount(uint32_t(armor))==0)
      npc->addItem(uint32_t(armor),1);
    npc->equipItem(uint32_t(armor));
    }
  else if(armor<0 && nbStates.count(this)==0) {
    // NO_ARMOR is -1. CoM's NPC_ChangeFace passes 0 to retain the armor.
    // NB fix 60: not in New Balance - RX_ApplyHeroHead (PerFrame, after every load and change of the world) sets the
    // head of the hero with NO_ARMOR, and in Gothic the armor stays on: here it was taken off every time
    if(auto* equipped = npc->currentArmor())
      npc->unequipItem(equipped->clsId());
    }
  }

void GameScript::mdl_setmodelfatness(std::shared_ptr<zenkit::INpc> npcRef, float fat) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->setFatness(fat);
  }

void GameScript::mdl_applyoverlaymds(std::shared_ptr<zenkit::INpc> npcRef, std::string_view overlayname) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr) {
    auto skelet = Resources::loadSkeleton(overlayname);
    npc->addOverlay(skelet,0);
    if(skelet==nullptr)
      Log::d("Mdl_ApplyOverlayMds: ", overlayname, " (not found)");   // NB fix 52: only what does not load
    // NB diagnostics (fix 47): an overlay of the hero that does not load leaves him the animations of the base
    if(npc->isPlayer())
      Log::i("[anim] hero overlay +",overlayname,skelet==nullptr ? " NOT LOADED" : "");
    }
  }

void GameScript::mdl_applyoverlaymdstimed(std::shared_ptr<zenkit::INpc> npcRef, std::string_view overlayname, int ticks) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr && ticks>0) {
    auto skelet = Resources::loadSkeleton(overlayname);
    npc->addOverlay(skelet,uint64_t(ticks));
    }
  }

void GameScript::mdl_removeoverlaymds(std::shared_ptr<zenkit::INpc> npcRef, std::string_view overlayname) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->delOverlay(overlayname);   // NB fix 172: by the name, nothing is loaded
}

void GameScript::mdl_setmodelscale(std::shared_ptr<zenkit::INpc> npcRef, float x, float y, float z) {
  auto npc = findNpc(npcRef);
  if(npcRef!=nullptr)
    npc->setScale(x,y,z);
  }

void GameScript::mdl_startfaceani(std::shared_ptr<zenkit::INpc> npcRef, std::string_view ani, float intensity, float time) {
  if(npcRef!=nullptr)
    findNpc(npcRef.get())->startFaceAnim(ani,intensity,uint64_t(time*1000.f));
  }

void GameScript::mdl_applyrandomani(std::shared_ptr<zenkit::INpc> npcRef, std::string_view baseAni, std::string_view randomAni) {
  if(auto npc = findNpc(npcRef))
    npc->setRandomAni(baseAni,randomAni);
  }

void GameScript::mdl_applyrandomanifreq(std::shared_ptr<zenkit::INpc> npcRef, std::string_view baseAni, float seconds) {
  if(auto npc = findNpc(npcRef))
    npc->setRandomAniFreq(baseAni,seconds);
  }

void GameScript::mdl_applyrandomfaceani(std::shared_ptr<zenkit::INpc> npcRef, std::string_view name, float timeMin, float timeMinVar, float timeMax, float timeMaxVar, float probMin) {
  (void)probMin;
  (void)timeMaxVar;
  (void)timeMax;
  (void)timeMinVar;
  (void)timeMin;
  (void)name;
  (void)npcRef;

  static bool first=true;
  if(first){
    Log::e("not implemented call [mdl_applyrandomfaceani]");
    first=false;
    }
  }

void GameScript::wld_insertnpc(int npcInstance, std::string_view spawnpoint) {
  if(npcInstance<=0)
    return;

  auto npc = world().addNpc(size_t(npcInstance),spawnpoint);
  if(npc==nullptr)
    return;
  // NB fix 143: a character put on a mover (Fregyal on the boat to the West Coast) stays where the point is: the search of
  // a free place around it took him off the deck, into the sea
  bool onMover = false;
  const auto pos0 = npc->position();
  world().forEachTrigger([&](AbstractTrigger& t) {
    if(!onMover && dynamic_cast<MoveTrigger*>(&t)!=nullptr && t.nbStandsOn(pos0))
      onMover = true;
    });
  if(onMover) {
    nbCtx("insert: ",npc->displayName()," at ",spawnpoint," - on a mover, kept at the point");
    return;
    }
  fixNpcPosition(*npc,0,150);
  if((npc->position()-pos0).length()>100.f)
    nbCtx("insert: ",npc->displayName()," at ",spawnpoint," moved ",int((npc->position()-pos0).length())," cm to a free place");
  }

void GameScript::wld_removenpc(int npcInstance) {
  if(auto npc = findNpcById(size_t(npcInstance)))
    world().removeNpc(*npc);
  }

void GameScript::wld_insertitem(int itemInstance, std::string_view spawnpoint) {
  if(spawnpoint.empty() || itemInstance<=0 || !isItemInstance(itemInstance,"Wld_InsertItem"))
    return;

  world().addItem(size_t(itemInstance),spawnpoint);
  }

void GameScript::wld_insertobject(std::string_view visual, std::string_view spawnpoint) {
  world().addObject(visual,spawnpoint);
  }

void GameScript::npc_settofightmode(std::shared_ptr<zenkit::INpc> npcRef, int weaponSymbol) {
  if(npcRef!=nullptr && weaponSymbol>=0)
    findNpc(npcRef.get())->setToFightMode(size_t(weaponSymbol));
  }

void GameScript::npc_settofistmode(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->setToFistMode();
  }

bool GameScript::npc_isinstate(std::shared_ptr<zenkit::INpc> npcRef, int stateFn) {
  auto npc = findNpc(npcRef);
  // the developer tool of the dialogs holds a dialog without the state of the talk
  if(npc!=nullptr && DialogTest::active && npc==DialogTest::talkNpc && size_t(stateFn)==ZS_Talk)
    return true;
  if(npc!=nullptr)
    return npc->isInState(uint32_t(stateFn));
  return false;
  }

bool GameScript::npc_isinroutine(std::shared_ptr<zenkit::INpc> npcRef, int stateFn) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    return npc->isInRoutine(uint32_t(stateFn));
  return false;
  }

bool GameScript::npc_wasinstate(std::shared_ptr<zenkit::INpc> npcRef, int stateFn) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    return npc->wasInState(uint32_t(stateFn));
  return false;
  }

int GameScript::npc_getdisttowp(std::shared_ptr<zenkit::INpc> npcRef, std::string_view wpname) {
  auto  npc = findNpc(npcRef);
  //NOTE: in CoM, some way-point and free-points share same name - need to be precise
  auto* wp  = world().findWayPoint(wpname);

  if(npc!=nullptr && wp!=nullptr){
    float ret = std::sqrt(npc->qDistTo(wp));
    if(ret<float(std::numeric_limits<int32_t>::max()))
      return int32_t(ret); else
      return std::numeric_limits<int32_t>::max();
    } else {
    if(wp==nullptr)
      Log::d("Npc_GetDistToWP: no way point '",wpname,"'");
    return std::numeric_limits<int32_t>::max();
    }
  }

void GameScript::npc_exchangeroutine(std::shared_ptr<zenkit::INpc> npcRef, std::string_view rname) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr) {
    auto& v = npc->handle();
    string_frm name("Rtn_",rname,'_',v.id);

    auto* sym = vm.find_symbol_by_name(name);
    size_t d = sym != nullptr ? sym->index() : 0;
    if(hasDma())
      Log::d("Npc_ExchangeRoutine: ",npc->displayName()," -> ",rname,d==0 ? " (no such routine)" : "");
    if(d>0) {
      // NB fix 76: the name for RX_ContainsRoutine
      if(auto it = nbStates.find(this); it!=nbStates.end())
        it->second.routines[npc] = NbState::Routine{npcRef.get(),std::string(std::string_view(name))};
      npc->excRoutine(d);
      }
    }
  }

bool GameScript::npc_isdead(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  return npc==nullptr || isDead(*npc);
  }

bool GameScript::npc_knowsinfo(std::shared_ptr<zenkit::INpc> npcRef, int infoinstance) {
  auto npc = findNpc(npcRef);
  if(!npc)
    return false;

  zenkit::INpc& vnpc = npc->handle();
  return doesNpcKnowInfo(vnpc, uint32_t(infoinstance));
  }

void GameScript::npc_settalentskill(std::shared_ptr<zenkit::INpc> npcRef, int t, int lvl) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->setTalentSkill(Talent(t),lvl);
  }

int GameScript::npc_gettalentskill(std::shared_ptr<zenkit::INpc> npcRef, int skillId) {
  auto npc = findNpc(npcRef);
  return npc==nullptr ? 0 : npc->talentSkill(Talent(skillId));
  }

void GameScript::npc_settalentvalue(std::shared_ptr<zenkit::INpc> npcRef, int t, int lvl) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->setTalentValue(Talent(t),lvl);
  }

int GameScript::npc_gettalentvalue(std::shared_ptr<zenkit::INpc> npcRef, int skillId) {
  auto npc = findNpc(npcRef);
  return npc==nullptr ? 0 : npc->talentValue(Talent(skillId));
  }

void GameScript::npc_setrefusetalk(std::shared_ptr<zenkit::INpc> npcRef, int timeSec) {
  auto npc = findNpc(npcRef);
  if(npc)
    npc->setRefuseTalk(uint64_t(std::max(timeSec*1000,0)));
  }

bool GameScript::npc_refusetalk(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  return npc && npc->isRefuseTalk();
  }

int GameScript::npc_hasitems(std::shared_ptr<zenkit::INpc> npcRef, int itemId) {
  auto npc = findNpc(npcRef);
  return npc!=nullptr ? int(npc->itemCount(uint32_t(itemId))) : 0;
  }

bool GameScript::npc_hasspell(std::shared_ptr<zenkit::INpc> npcRef, int splId) {
  auto npc = findNpc(npcRef);
  return npc!=nullptr && npc->inventory().hasSpell(splId);
  }

int GameScript::npc_getinvitem(std::shared_ptr<zenkit::INpc> npcRef, int itemId) {
  auto npc = findNpc(npcRef);
  auto itm = npc==nullptr ? nullptr : npc->getItem(uint32_t(itemId));
  storeItem(itm);
  if(itm!=nullptr) {
    return int(itm->handle().symbol_index());
    }
  return -1;
  }

// This seems specific to Gothic 1, where the inventory was grouped into categories.
// In the shared inventory, these categories are just following one after the other.
// So we should transform this to iterate over the different categories in the
// shared inventory
int GameScript::npc_getinvitembyslot(std::shared_ptr<zenkit::INpc> npcRef, int cat, int slotnr) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr) {
    storeItem(nullptr);
    return 0;
    }

  // The category flag names were global, but for the scripts, only npc_getinvitembyslot
  // ever used them, so as long as nobody implements the Gothic 1 inventory, they can
  // be limited to the comments below.
  ItmFlags f = ITM_CAT_NONE;
  switch(cat) {
    case 1: // INV_WEAPON
      f = ItmFlags(ITM_CAT_NF|ITM_CAT_FF|ITM_CAT_MUN);
      break;
    case 2: // INV_ARMOR
      f = ITM_CAT_ARMOR;
      break;
    case 3: // INV_RUNE
      f = ITM_CAT_RUNE;
      break;
    case 4: // INV_MAGIC
      f = ITM_CAT_MAGIC;
      break;
    case 5: // INV_FOOD
      f = ITM_CAT_FOOD;
      break;
    case 6: // INV_POTION
      f = ITM_CAT_POTION;
      break;
    case 7: // INV_DOC
      f = ITM_CAT_DOCS;
      break;
    case 8: // INV_MISC
      f = ItmFlags(ITM_CAT_LIGHT|ITM_CAT_NONE);
      break;
    default:
      // 0 is "no category" in the scripts of the mods: there is no item, it is not an error
      if(cat!=0)
        Log::e("Unknown item category ", cat);
      storeItem(nullptr);
      return 0;
    }

  auto itm = npc==nullptr ? nullptr : npc->inventory().findByFlags(f, uint32_t(slotnr));
  // Store the found item in the global item var
  storeItem(itm);

  return itm!=nullptr ? int(itm->count()) : 0;
  }

int GameScript::npc_removeinvitem(std::shared_ptr<zenkit::INpc> npcRef, int itemId) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->delItem(uint32_t(itemId),1);
  return 0;
  }

int GameScript::npc_removeinvitems(std::shared_ptr<zenkit::INpc> npcRef, int itemId, int amount) {
  auto npc = findNpc(npcRef);

  if(npc!=nullptr && amount>0)
    npc->delItem(uint32_t(itemId),uint32_t(amount));

  return 0;
  }

int GameScript::npc_getbodystate(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);

  if(npc!=nullptr)
    return int32_t(npc->bodyState());
  return int32_t(0);
  }

std::shared_ptr<zenkit::INpc> GameScript::npc_getlookattarget(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  return npc && npc->lookAtTarget() ? npc->lookAtTarget()->handlePtr() : nullptr;
  }

int GameScript::npc_getdisttonpc(std::shared_ptr<zenkit::DaedalusInstance> aRef, std::shared_ptr<zenkit::DaedalusInstance> bRef) {
  auto a = findNpcById(aRef);
  auto b = findNpcById(bRef);

  if(a!=nullptr && b==nullptr) {
    // B_FetchWeapon of Gothic 1 asks Npc_GetDistToNpc(self, item): the original engine takes any vob here, the item is the place of a weapon
    // that lies about (the type of the argument was checked: an exception of the scripts, the character did not fetch the weapon)
    if(auto itm = findItemById(bRef)) {
      const float len = (itm->position()-a->position()).length();
      if(len>float(std::numeric_limits<int32_t>::max()))
        return std::numeric_limits<int32_t>::max();
      return int(len);
      }
    }

  if(a==nullptr || b==nullptr)
    return std::numeric_limits<int32_t>::max();

  float ret = std::sqrt(a->qDistTo(*b));
  if(ret>float(std::numeric_limits<int32_t>::max()))
    return std::numeric_limits<int32_t>::max();
  return int(ret);
  }

bool GameScript::npc_hasequippedarmor(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  return npc!=nullptr && npc->currentArmor()!=nullptr;
  }

void GameScript::npc_setperctime(std::shared_ptr<zenkit::INpc> npcRef, float sec) {
  auto npc = findNpc(npcRef);
  if(npc)
    npc->setPerceptionTime(uint64_t(sec*1000));
  }

void GameScript::npc_percenable(std::shared_ptr<zenkit::INpc> npcRef, int pr, int fn) {
  auto npc = findNpc(npcRef);
  if(npc && fn>=0)
    npc->setPerceptionEnable(PercType(pr),size_t(fn));
  }

void GameScript::npc_percdisable(std::shared_ptr<zenkit::INpc> npcRef, int pr) {
  auto npc = findNpc(npcRef);
  if(npc)
    npc->setPerceptionDisable(PercType(pr));
  }

std::string GameScript::npc_getnearestwp(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  auto wp  = npc ? world().findWayPoint(npc->position()) : nullptr;
  if(wp)
    return wp->name;
  return "";
  }

std::string GameScript::npc_getnextwp(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  auto wp  = npc ? world().findNextWayPoint(*npc) : nullptr;
  if(wp)
    return wp->name;
  return "";
  }

void GameScript::npc_clearaiqueue(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc)
    npc->clearAiQueue();
  }

bool GameScript::npc_isplayer(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  return npc && npc->isPlayer();
  }

int GameScript::npc_getstatetime(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc)
    return int32_t(npc->stateTime()/1000);
  return 0;
  }

void GameScript::npc_setstatetime(std::shared_ptr<zenkit::INpc> npcRef, int val) {
  auto npc = findNpc(npcRef);
  if(npc)
    npc->setStateTime(val*1000);
  }

void GameScript::npc_changeattribute(std::shared_ptr<zenkit::INpc> npcRef, int atr, int val) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr && atr>=0)
    npc->changeAttribute(Attribute(atr),val,false);
  }

bool GameScript::npc_isonfp(std::shared_ptr<zenkit::INpc> npcRef, std::string_view val) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return false;

  auto w = npc->currentWayPoint();
  if(w==nullptr || !MoveAlgo::isClose(*npc,*w,MAX_AI_USE_DISTANCE) || !w->checkName(val))
    return false;
  return w->isFreePoint();
  }

int GameScript::npc_getheighttonpc(std::shared_ptr<zenkit::INpc> aRef, std::shared_ptr<zenkit::INpc> bRef) {
  auto a = findNpc(aRef);
  auto b = findNpc(bRef);
  float ret = 0;
  if(a!=nullptr && b!=nullptr)
    ret = std::abs(a->position().y - b->position().y);
  return int32_t(ret);
  }

std::shared_ptr<zenkit::IItem> GameScript::npc_getequippedmeleeweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr && npc->currentMeleeWeapon() != nullptr) {
    return npc->currentMeleeWeapon()->handlePtr();
    }
  return nullptr;
  }

std::shared_ptr<zenkit::IItem> GameScript::npc_getequippedrangedweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr && npc->currentRangedWeapon() != nullptr) {
    return npc->currentRangedWeapon()->handlePtr();
    }
  return nullptr;
  }

std::shared_ptr<zenkit::IItem> GameScript::npc_getequippedarmor(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr && npc->currentArmor()!=nullptr) {
    return npc->currentArmor()->handlePtr();
    }
  return nullptr;
  }

bool GameScript::npc_canseenpc(std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc> otherRef) {
  // 'see' functions are intended as ray-cast, ignoring hnpc->senses mask
  // https://discord.com/channels/989316194148433950/989333514543587339/1226664463697182760
  auto other = findNpc(otherRef);
  auto npc   = findNpc(npcRef);

  if(npc!=nullptr && other!=nullptr){
    return npc->canSeeNpc(*other,false);
    }
  return false;
  }

bool GameScript::npc_canseenpcfreelos(std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc> otherRef) {
  auto npc = findNpc(npcRef);
  auto oth = findNpc(otherRef);

  if(npc!=nullptr && oth!=nullptr){
    return npc->canSeeNpc(*oth,true);
    }
  return false;
  }

bool GameScript::npc_canseeitem(std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::IItem> itemRef) {
  auto npc = findNpc(npcRef);
  auto itm = findItem(itemRef.get());

  if(npc!=nullptr && itm!=nullptr){
    return npc->canSeeItem(*itm,false);
    }
  return false;
  }

bool GameScript::npc_hasequippedweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  return (npc!=nullptr &&
     (npc->currentMeleeWeapon()!=nullptr ||
      npc->currentRangedWeapon()!=nullptr));
  }

bool GameScript::npc_hasequippedmeleeweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  return npc!=nullptr && npc->currentMeleeWeapon()!=nullptr;
  }

bool GameScript::npc_hasequippedrangedweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  return npc!=nullptr && npc->currentRangedWeapon()!=nullptr;
  }

int GameScript::npc_getactivespell(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return -1;

  Item* w = npc->activeWeapon();
  if(w==nullptr || !w->isSpellOrRune())
    return -1;

  makeCurrent(w);
  return w->spellId();
  }

bool GameScript::npc_getactivespellisscroll(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return false;

  Item* w = npc->activeWeapon();
  if(w==nullptr || !w->isSpell())
    return false;

  return true;
  }

bool GameScript::npc_isinfightmode(std::shared_ptr<zenkit::INpc> npcRef, int modeI) {
  auto npc  = findNpc(npcRef);
  auto mode = FightMode(modeI);

  if(npc==nullptr){
    return false;
    }

  auto st  = npc->weaponState();
  bool ret = false;
  if(mode==FightMode::FMODE_NONE){
    ret = (st==WeaponState::NoWeapon);
    }
  else if(mode==FightMode::FMODE_FIST){
    ret = (st==WeaponState::Fist);
    }
  else if(mode==FightMode::FMODE_MELEE){
    ret = (st==WeaponState::W1H || st==WeaponState::W2H);
    }
  else if(mode==FightMode::FMODE_FAR){
    ret = (st==WeaponState::Bow || st==WeaponState::CBow);
    }
  else if(mode==FightMode::FMODE_MAGIC){
    ret = (st==WeaponState::Mage);
    }
  return ret;
  }

void GameScript::npc_settarget(std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc> otherRef) {
  auto oth = findNpc(otherRef);
  auto npc = findNpc(npcRef);
  if(npc)
    npc->setTarget(oth);
  }

/**
 * @brief WorldScript::npc_gettarget
 * Fill 'other' with the current npc target.
 * set by Npc_SetTarget () or Npc_GetNextTarget ().
 * - return: current target saved -> TRUE
 * no target saved -> FALSE
 */
bool GameScript::npc_gettarget(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  auto s   = vm.global_other();

  if(npc!=nullptr && npc->target()) {
    s->set_instance(npc->target()->handlePtr());
    return true;
    }

  s->set_instance(nullptr);
  return false;
  }

bool GameScript::npc_getnexttarget(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  Npc* ret = nullptr;

  if(npc!=nullptr){
    float dist = float(npc->handle().senses_range);
    dist*=dist;

    world().detectNpc(npc->position(),float(npc->handle().senses_range),[&,npc](Npc& oth){
      if(&oth!=npc && !oth.isDown() && oth.isEnemy(*npc) && npc->canSenseNpc(oth,true)!=SensesBit::SENSE_NONE){
        float qd = oth.qDistTo(*npc);
        if(qd<dist){
          dist=qd;
          ret = &oth;
          }
        }
      return false;
      });
    if(ret!=nullptr)
      npc->setTarget(ret);
    }

  auto s = vm.global_other();
  if(ret!=nullptr) {
    s->set_instance(ret->handlePtr());
    return true;
    } else {
    s->set_instance(nullptr);
    return false;
    }
  }

void GameScript::npc_sendpassiveperc(std::shared_ptr<zenkit::INpc> npcRef, int id, std::shared_ptr<zenkit::INpc> victimRef, std::shared_ptr<zenkit::INpc> otherRef) {
  auto npc    = findNpc(npcRef);
  auto other  = findNpc(otherRef);
  auto victim = findNpc(victimRef);

  if(npc && other && victim)
    world().sendPassivePerc(*npc,*other,*victim,id);
  else if(npc && other)
    world().sendPassivePerc(*npc,*other,id);
  }

void GameScript::npc_sendsingleperc(std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc> otherRef, int id) {
  auto other  = findNpc(otherRef);
  auto npc    = findNpc(npcRef);

  if(npc && other)
    other->perceptionProcess(*npc,nullptr,0,PercType(id));
  }

bool GameScript::npc_checkinfo(std::shared_ptr<zenkit::INpc> npcRef, int imp) {
  auto n    = findNpc(npcRef);
  auto hero = findNpc(vm.global_other());
  if(n==nullptr || hero==nullptr)
    return false;

  auto& pl  = hero->handle();
  auto& npc = n->handle();
  for(auto& info:dialogsInfo) {
    if(info->npc!=int32_t(npc.symbol_index()) || info->important!=imp)
      continue;
    bool npcKnowsInfo = doesNpcKnowInfo(pl,info->symbol_index());
    if(npcKnowsInfo && !info->permanent)
      continue;
    bool valid=false;
    if(info->condition) {
      auto* conditionSymbol = vm.find_symbol_by_index(uint32_t(info->condition));
      if (conditionSymbol != nullptr)
        valid = vm.call_function<int>(conditionSymbol)!=0;
      }
    if(valid) {
      return true;
      }
    }
  return false;
  }

int GameScript::npc_getportalguild(std::shared_ptr<zenkit::INpc> npcRef) {
  int32_t g   = GIL_NONE;
  auto    npc = findNpc(npcRef);
  if(npc!=nullptr)
    g = world().guildOfRoom(npc->portalName());
  return g;
  }

bool GameScript::npc_isinplayersroom(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  auto pl  = world().player();

  if(npc!=nullptr && pl!=nullptr) {
    auto g1 = pl ->portalName();
    auto g2 = npc->portalName();
    if(g1==g2)
      return true;
    }
  return false;
  }

std::shared_ptr<zenkit::IItem> GameScript::npc_getreadiedweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr) {
    return 0;
    }

  auto ret = npc->activeWeapon();
  if(ret!=nullptr) {
    makeCurrent(ret);
    return ret->handlePtr();
    } else {
    return nullptr;
    }
  }

bool GameScript::npc_hasreadiedweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return false;
  auto ws = npc->weaponState();
  return (ws==WeaponState::W1H || ws==WeaponState::W2H ||
          ws==WeaponState::Bow || ws==WeaponState::CBow);
  }

bool GameScript::npc_hasreadiedmeleeweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr) {
    return false;
    }
  auto ws = npc->weaponState();
  return ws==WeaponState::W1H || ws==WeaponState::W2H;
  }

bool GameScript::npc_hasreadiedrangedweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return false;
  auto ws = npc->weaponState();
  return ws==WeaponState::Bow || ws==WeaponState::CBow;
  }

bool GameScript::npc_hasrangedweaponwithammo(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  return npc!=nullptr && npc->inventory().hasRangedWeaponWithAmmo();
  }

int GameScript::npc_isdrawingspell(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return 0;

  auto ret = npc->activeWeapon();
  if(ret==nullptr || !ret->isSpell())
    return 0;

  makeCurrent(ret);
  return int32_t(ret->clsId());
  }

int GameScript::npc_isdrawingweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return 0;

  auto ret = npc->activeWeapon();
  if(ret==nullptr || !ret->isSpell())
    return 0;

  makeCurrent(ret);
  return int32_t(ret->clsId());
  }

void GameScript::npc_perceiveall(std::shared_ptr<zenkit::INpc> npcRef) {
  (void)npcRef; // nop
  }

void GameScript::npc_stopani(std::shared_ptr<zenkit::INpc> npcRef, std::string_view name) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->stopAnim(name);
  }

int GameScript::npc_settrueguild(std::shared_ptr<zenkit::INpc> npcRef, int gil) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->setTrueGuild(gil);
  return 0;
  }

int GameScript::npc_gettrueguild(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    return npc->trueGuild();
  return int32_t(GIL_NONE);
  }

void GameScript::npc_clearinventory(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->clearInventory();
  }

int GameScript::npc_getattitude(std::shared_ptr<zenkit::INpc> aRef, std::shared_ptr<zenkit::INpc> bRef) {
  auto a = findNpc(aRef);
  auto b = findNpc(bRef);

  if(a!=nullptr && b!=nullptr){
    return personAttitude(*a,*b);
    }
  return ATT_NEUTRAL;
  }

int GameScript::npc_getpermattitude(std::shared_ptr<zenkit::INpc> aRef, std::shared_ptr<zenkit::INpc> bRef) {
  auto a = findNpc(aRef);
  auto b = findNpc(bRef);

  if(a!=nullptr && b!=nullptr) {
    if(a->isPlayer() || b->isPlayer()) {
      const Npc& npc = a->isPlayer() ? *b : *a;
      if(npc.attitude()!=ATT_NULL)
        return npc.attitude();
      }
    return guildAttitude(*a,*b);
    }
  return ATT_NEUTRAL;
  }

void GameScript::npc_setattitude(std::shared_ptr<zenkit::INpc> npcRef, int att) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->setAttitude(Attitude(att));
  }

void GameScript::npc_settempattitude(std::shared_ptr<zenkit::INpc> npcRef, int att) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->setTempAttitude(Attitude(att));
  }

bool GameScript::npc_hasbodyflag(std::shared_ptr<zenkit::INpc> npcRef, int bodyflag) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return false;
  return npc->hasStateFlag(BodyState(bodyflag));
  }

int GameScript::npc_getlasthitspellid(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr){
    return 0;
    }
  return npc->lastHitSpellId();
  }

int GameScript::npc_getlasthitspellcat(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return SPELL_GOOD;

  const int id    = npc->lastHitSpellId();
  auto&     spell = spellDesc(id);
  return spell.spell_type;
  }

void GameScript::npc_playani(std::shared_ptr<zenkit::INpc> npcRef, std::string_view name) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->playAnimByName(name,BS_NONE);
  }

bool GameScript::npc_isdetectedmobownedbynpc(std::shared_ptr<zenkit::INpc> usrRef, std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  auto usr = findNpc(usrRef);

  if(npc!=nullptr && usr!=nullptr && usr->interactive()!=nullptr){
    auto* inst = vm.find_symbol_by_index(npc->instanceSymbol());
    auto  ow   = usr->interactive()->ownerName();
    return inst->name() == ow;
    }
  return false;
  }

bool GameScript::npc_isdetectedmobownedbyguild(std::shared_ptr<zenkit::INpc> npcRef, int guild) {
  static bool first=true;
  if(first){
    Log::e("not implemented call [npc_isdetectedmobownedbyguild]");
    first=false;
    }

  auto npc = findNpc(npcRef);
  (void)guild;

  if(npc!=nullptr && npc->detectedMob()!=nullptr) {
    auto  ow   = npc->detectedMob()->ownerName();
    (void)ow;
    //vm.setReturn(inst.name==ow ? 1 : 0);
    return false;
    }
  return false;
  }

std::string GameScript::npc_getdetectedmob(std::shared_ptr<zenkit::INpc> npcRef) {
  auto usr = findNpc(npcRef);
  if(usr!=nullptr && usr->detectedMob()!=nullptr){
    auto i = usr->detectedMob();
    return std::string(i->schemeName());
    }
  return "";
  }

bool GameScript::npc_ownedbynpc(std::shared_ptr<zenkit::IItem> itmRef, std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  auto itm = findItem(itmRef.get());
  if(itm==nullptr || npc==nullptr) {
    return false;
    }

  auto* sym = vm.find_symbol_by_index(uint32_t(itm->handle().owner));
  return sym != nullptr && npc->handlePtr()==sym->get_instance();
  }

bool GameScript::npc_canseesource(std::shared_ptr<zenkit::INpc> npcRef) {
  auto self = findNpc(npcRef);
  if(!self)
    return false;
  return self->canSeeSource();
  }

// Used (only?) in Gothic 1 in B_AssessEnemy, to prevent attacks during cutscenes.
bool GameScript::npc_isincutscene(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  auto w = Gothic::inst().world();
  if(w==nullptr)
    return false;

  if(npc!=nullptr && owner.isNpcInDialog(*npc))
    return true;

  return false;
  }

int GameScript::npc_getdisttoitem(std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::IItem> itmRef) {
  auto itm = findItem(itmRef.get());
  auto npc = findNpc(npcRef);
  if(itm==nullptr || npc==nullptr) {
    return std::numeric_limits<int32_t>::max();
    }
  auto dp = itm->position()-npc->position();
  return int32_t(dp.length());
  }

int GameScript::npc_getheighttoitem(std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::IItem> itmRef) {
  auto itm = findItem(itmRef.get());
  auto npc = findNpc(npcRef);
  if(itm==nullptr || npc==nullptr) {
    return std::numeric_limits<int32_t>::max();
    }
  auto dp = int32_t(itm->position().y-npc->position().y);
  return std::abs(dp);
  }

int GameScript::npc_getdisttoplayer(std::shared_ptr<zenkit::INpc> npcRef) {
  auto pl  = world().player();
  auto npc = findNpc(npcRef);
  if(pl==nullptr || npc==nullptr) {
    return std::numeric_limits<int32_t>::max();
    }
  auto dp = pl->position()-npc->position();
  auto l  = dp.length();
  if(l>float(std::numeric_limits<int32_t>::max())) {
    return std::numeric_limits<int32_t>::max();
    }
  return int32_t(l);
  }

int GameScript::npc_getactivespellcat(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return SPELL_GOOD;

  const Item* w = npc->activeWeapon();
  if(w==nullptr || !w->isSpellOrRune())
    return SPELL_GOOD;

  const int id    = w->spellId();
  auto&     spell = spellDesc(id);
  return spell.spell_type;
  }

int GameScript::npc_setactivespellinfo(std::shared_ptr<zenkit::INpc> npcRef, int v) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->setActiveSpellInfo(v);
  return 0;
  }

int GameScript::npc_getactivespelllevel(std::shared_ptr<zenkit::INpc> npcRef) {
  int  v   = 0;
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    v = npc->activeSpellLevel();
  return v;
  }

void GameScript::ai_processinfos(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpcRaw(npcRef);
  auto pl  = owner.player();
  if(pl!=nullptr && npc!=nullptr) {
    aiOutOrderId=0;
    npc->aiPush(AiQueue::aiProcessInfo(*pl));
    }
  }

void GameScript::ai_output(std::shared_ptr<zenkit::INpc> selfRef, std::shared_ptr<zenkit::INpc> targetRef, std::string_view outputname) {
  auto target = findNpc(targetRef);
  auto self   = findNpc(selfRef);

  if(target==nullptr && owner.version().game==1)
    target = findNpc(vm.global_other());

  if(self!=nullptr && target!=nullptr) {
    self->aiPush(AiQueue::aiOutput(*target,outputname,aiOutOrderId));
    ++aiOutOrderId;
    }
  }

void GameScript::ai_stopprocessinfos(std::shared_ptr<zenkit::INpc> selfRef) {
  auto self = findNpcRaw(selfRef);
  if(self) {
    self->aiPush(AiQueue::aiStopProcessInfo(aiOutOrderId));
    ++aiOutOrderId;
    }
  }

void GameScript::ai_standup(std::shared_ptr<zenkit::INpc> selfRef) {
  auto self = findNpc(selfRef);
  if(self!=nullptr)
    self->aiPush(AiQueue::aiStandup());
  }

void GameScript::ai_standupquick(std::shared_ptr<zenkit::INpc> selfRef) {
  auto self = findNpc(selfRef);
  if(self!=nullptr)
    self->aiPush(AiQueue::aiStandupQuick());
  }

void GameScript::ai_continueroutine(std::shared_ptr<zenkit::INpc> selfRef) {
  auto self = findNpc(selfRef);
  if(self!=nullptr)
    self->aiPush(AiQueue::aiContinueRoutine());
  }

void GameScript::ai_stoplookat(std::shared_ptr<zenkit::INpc> selfRef) {
  auto self = findNpc(selfRef);
  if(self!=nullptr)
    self->aiPush(AiQueue::aiStopLookAt());
  }

void GameScript::ai_lookat(std::shared_ptr<zenkit::INpc> selfRef, std::string_view waypoint) {
  auto self = findNpc(selfRef);
  auto to  = world().findPoint(waypoint);
  if(self!=nullptr)
    self->aiPush(AiQueue::aiLookAt(to));
  }

void GameScript::ai_lookatnpc(std::shared_ptr<zenkit::INpc> selfRef, std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc  = findNpc(npcRef);
  auto self = findNpc(selfRef);
  if(self!=nullptr)
    self->aiPush(AiQueue::aiLookAtNpc(npc));
  }

void GameScript::ai_removeweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiRemoveWeapon());
  }

void GameScript::ai_unreadyspell(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiRemoveWeapon());
  }

void GameScript::ai_turnaway(std::shared_ptr<zenkit::INpc> selfRef, std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc  = findNpc(npcRef);
  auto self = findNpc(selfRef);
  if(self!=nullptr)
    self->aiPush(AiQueue::aiTurnAway(npc));
  }

void GameScript::ai_turntonpc(std::shared_ptr<zenkit::INpc> selfRef, std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc  = findNpc(npcRef);
  auto self = findNpc(selfRef);
  if(self!=nullptr)
    self->aiPush(AiQueue::aiTurnToNpc(npc));
  }

void GameScript::ai_whirlaround(std::shared_ptr<zenkit::INpc> selfRef, std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc  = findNpc(npcRef);
  auto self = findNpc(selfRef);
  if(self!=nullptr)
    self->aiPush(AiQueue::aiWhirlToNpc(npc));
  }

void GameScript::ai_outputsvm(std::shared_ptr<zenkit::INpc> selfRef, std::shared_ptr<zenkit::INpc> targetRef, std::string_view name) {
  auto target = findNpc(targetRef);
  auto self   = findNpc(selfRef);
  // NB fix 230: what the hero says himself is often a refusal of the scripts without a text ("$DOESNTWORK" of
  // Spell_Logic_TeleportXardas in a fight) - in the log with the functions, as "[print]"
  if(self!=nullptr && self->isPlayer())
    nbPrintLog(vm,name,"[hero] says ");

  if(target==nullptr && owner.version().game==1)
    target = findNpc(vm.global_other());

  if(self!=nullptr && target!=nullptr) {
    self->aiPush(AiQueue::aiOutputSvm(*target,name,aiOutOrderId));
    ++aiOutOrderId;
    }
  }

void GameScript::ai_outputsvm_overlay(std::shared_ptr<zenkit::INpc> selfRef, std::shared_ptr<zenkit::INpc> targetRef, std::string_view name) {
  auto target = findNpc(targetRef);
  auto self   = findNpc(selfRef);
  // NB fix 230: what the hero says himself is often a refusal of the scripts without a text ("$DOESNTWORK" of
  // Spell_Logic_TeleportXardas in a fight) - in the log with the functions, as "[print]"
  if(self!=nullptr && self->isPlayer())
    nbPrintLog(vm,name,"[hero] says ");

  if(target==nullptr && owner.version().game==1)
    target = findNpc(vm.global_other());

  if(self!=nullptr && target!=nullptr) {
    self->aiPush(AiQueue::aiOutputSvmOverlay(*target,name,aiOutOrderId));
    ++aiOutOrderId;
    }
  }

void GameScript::ai_startstate(std::shared_ptr<zenkit::INpc> selfRef, int func, int state, std::string_view wp) {
  auto self = findNpc(selfRef);
  if(self!=nullptr && func>0) {
    Npc*  oth = findNpc(vm.global_other());
    Npc*  vic = findNpc(vm.global_victim());
    auto& st  = aiState(size_t(func));
    // Who starts a fight with whom and why (the reason is the script variable AIV_ATTACKREASON, the AR_* constants of B_Attack): this is what
    // the log needs when a group of characters joins a fight, see B_AssessFightSound and the scripts that call B_Attack.
    if(const auto* fsym = vm.find_symbol_by_index(uint32_t(func))) {
      if(fsym->name()=="ZS_ATTACK" || fsym->name()=="ZS_Attack")
        Log::d("[attack] ",self->displayName()," starts ",fsym->name()," against ",oth!=nullptr ? oth->displayName() : std::string_view("-"),
               ", victim ",vic!=nullptr ? vic->displayName() : std::string_view("-"),", AIV_ATTACKREASON ",self->handle().aivar[9]);
      }
    self->aiPush(AiQueue::aiStartState(st.funcIni,state,oth,vic,wp));
    }
  }

void GameScript::ai_playani(std::shared_ptr<zenkit::INpc> npcRef, std::string_view name) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr && dma!=nullptr)
    dma->noteAiPlayAni(*npc,name);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiPlayAnim(name));
  }

void GameScript::ai_setwalkmode(std::shared_ptr<zenkit::INpc> npcRef, int modeBits) {
  int32_t weaponBit = 0x80;
  auto npc = findNpc(npcRef);

  int32_t mode = modeBits & (~weaponBit);
  if(npc!=nullptr && mode>=0 && mode<=3){ //TODO: weapon flags
    npc->aiPush(AiQueue::aiSetWalkMode(WalkBit(mode)));
    }
  }

void GameScript::ai_wait(std::shared_ptr<zenkit::INpc> npcRef, float ms) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr && ms>0)
    npc->aiPush(AiQueue::aiWait(uint64_t(ms*1000)));
  }

void GameScript::ai_waitms(std::shared_ptr<zenkit::INpc> npcRef, int ms) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr && ms>0)
    npc->aiPush(AiQueue::aiWait(uint64_t(ms)));
  }

void GameScript::ai_aligntowp(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc)
    npc->aiPush(AiQueue::aiAlignToWp());
  }

void GameScript::ai_waittillend(std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc> otherRef) {
  auto* npc = findNpc(npcRef);
  auto* other = findNpc(otherRef);
  if(npc!=nullptr && other!=nullptr)
    npc->aiWaitTillEnd(*other);
  }

void GameScript::ai_aimat(std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc> otherRef) {
  auto* npc = findNpc(npcRef);
  auto* other = findNpc(otherRef);
  if(npc!=nullptr && other!=nullptr)
    npc->aiPush(AiQueue::aiAimAt(*other));
  }

void GameScript::ai_shootat(std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc> otherRef) {
  auto* npc = findNpc(npcRef);
  auto* other = findNpc(otherRef);
  if(npc!=nullptr && other!=nullptr)
    npc->aiPush(AiQueue::aiShootAt(*other));
  }

void GameScript::ai_stopaim(std::shared_ptr<zenkit::INpc> npcRef) {
  if(auto* npc = findNpc(npcRef))
    npc->aiPush(AiQueue::aiStopAim());
  }

void GameScript::ai_dropitem(std::shared_ptr<zenkit::INpc> npcRef, int itemInstance) {
  if(itemInstance>0)
    if(auto* npc = findNpc(npcRef))
      npc->aiPush(AiQueue::aiDropItem(itemInstance));
  }

void GameScript::ai_snd_play(std::shared_ptr<zenkit::INpc> npcRef, std::string_view sound) {
  std::string name(sound);
  for(auto& c:name)
    c = char(std::toupper(static_cast<unsigned char>(c)));
  if(auto* npc = findNpc(npcRef))
    npc->aiPush(AiQueue::aiSndPlay(name));
  }

void GameScript::ai_stopfx(std::shared_ptr<zenkit::INpc> npcRef, std::string_view effect) {
  if(auto* npc = findNpc(npcRef))
    npc->aiPush(AiQueue::aiStopFx(effect));
  }

int GameScript::wld_removeitem(std::shared_ptr<zenkit::IItem> itemRef) {
  auto* item = findItem(itemRef.get());
  if(item==nullptr)
    return 0;
  // Only remove loose world items. Inventory objects belong to their inventory.
  for(uint32_t i=0; auto* loose = world().itmById(i); ++i) {
    if(loose==item) {
      world().removeItem(*loose);
      return 1;
      }
    }
  return 0;
  }

void GameScript::ai_gotowp(std::shared_ptr<zenkit::INpc> npcRef, std::string_view waypoint) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return;

  auto to = world().findWayPoint(npc->position(), waypoint);
  if(to!=nullptr) {
    npc->aiPush(AiQueue::aiGoToPoint(*to));
    return;
    }

  // in vanilla 'ai_gotowp' sometimes is used incorrectly, so we need to check all other points
  to = world().findPoint(waypoint, false);
  if(to!=nullptr)
    npc->aiPush(AiQueue::aiGoToPoint(*to));
  }

void GameScript::ai_gotofp(std::shared_ptr<zenkit::INpc> npcRef, std::string_view waypoint) {
  auto npc = findNpc(npcRef);

  if(npc) {
    auto to = world().findFreePoint(*npc,waypoint);
    if(to!=nullptr)
      npc->aiPush(AiQueue::aiGoToPoint(*to));
    }
  }

void GameScript::ai_playanibs(std::shared_ptr<zenkit::INpc> npcRef, std::string_view ani, int bs) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiPlayAnimBs(ani,BodyState(bs)));
  }

void GameScript::ai_equiparmor(std::shared_ptr<zenkit::INpc> npcRef, int id) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiEquipArmor(id));
  }

void GameScript::ai_equipbestarmor(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiEquipBestArmor());
  }

int GameScript::ai_equipbestmeleeweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiEquipBestMeleeWeapon());
  return 0;
  }

int GameScript::ai_equipbestrangedweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiEquipBestRangedWeapon());
  return 0;
  }

bool GameScript::ai_usemob(std::shared_ptr<zenkit::INpc> npcRef, std::string_view tg, int state) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiUseMob(tg,state));
  return 0;
  }

void GameScript::ai_teleport(std::shared_ptr<zenkit::INpc> npcRef, std::string_view tg) {
  auto npc = findNpc(npcRef);
  auto pt  = world().findPoint(tg);
  // NB fix 145: RX_WestCoast_Loop puts Fregyal on the boat of the West Coast just before it sails (AI_Teleport to
  // WP_START_000); in this world the point is not on the boat, where the hero arrives (FP_START_000) - he is put next to
  // the hero instead, on the deck
  if(npc!=nullptr && !npc->isPlayer() && tg=="WP_START_000") {
    std::string wn(world().name());
    for(auto& ch:wn)
      ch = char(std::toupper(uint8_t(ch)));
    auto* hero = world().player();
    if(hero!=nullptr && wn.find("WESTCOAST")!=std::string::npos) {
      const float a = hero->rotationRad()+float(M_PI)*0.5f;
      npc->clearAiQueue();
      npc->setPosition(hero->position()+Tempest::Vec3(std::cos(a)*80.f,20.f,std::sin(a)*80.f));
      npc->updateTransform();
      nbCtx("AI_Teleport: ",npc->displayName()," -> next to the hero (instead of ",tg,")");
      return;
      }
    }
  if(npc!=nullptr && pt!=nullptr)
    npc->aiPush(AiQueue::aiTeleport(*pt));
  if(npc!=nullptr && !npc->isPlayer())   // NB fix 143: where the companions go (the voyage to the West Coast)
    nbCtx("AI_Teleport: ",npc->displayName()," -> ",tg,pt==nullptr ? " (no such point)" : "");
  }

void GameScript::ai_stoppointat(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiStopPointAt());
  }

void GameScript::ai_drawweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiDrawWeapon());
  }

void GameScript::ai_readymeleeweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiReadyMeleeWeapon());
  }

void GameScript::ai_readyrangedweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiReadyRangedWeapon());
  }

void GameScript::ai_readyspell(std::shared_ptr<zenkit::INpc> npcRef, int spell, int mana) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr && mana>0)
    npc->aiPush(AiQueue::aiReadySpell(spell,mana));
  }

void GameScript::ai_attack(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiAttack());
  }

void GameScript::ai_flee(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiFlee());
  }

void GameScript::ai_dodge(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiDodge());
  }

void GameScript::ai_unequipweapons(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiUnEquipWeapons());
  }

void GameScript::ai_unequiparmor(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiUnEquipArmor());
  }

void GameScript::ai_gotonpc(std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc> toRef) {
  auto to  = findNpc(toRef);
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiGoToNpc(to));
  }

void GameScript::ai_gotonextfp(std::shared_ptr<zenkit::INpc> npcRef, std::string_view to) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiGoToNextFp(to));
  }

void GameScript::ai_aligntofp(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiAlignToFp());
  }

void GameScript::ai_useitem(std::shared_ptr<zenkit::INpc> npcRef, int item) {
  auto npc = findNpc(npcRef);
  if(npc)
    npc->aiPush(AiQueue::aiUseItem(item));
  }

void GameScript::ai_useitemtostate(std::shared_ptr<zenkit::INpc> npcRef, int item, int state) {
  auto npc = findNpc(npcRef);
  if(npc)
    npc->aiPush(AiQueue::aiUseItemToState(item,state));
  }

void GameScript::ai_setnpcstostate(std::shared_ptr<zenkit::INpc> npcRef, int state, int radius) {
  auto npc = findNpc(npcRef);
  if(npc && state>0)
    npc->aiPush(AiQueue::aiSetNpcsToState(size_t(state),radius));
  }

void GameScript::ai_finishingmove(std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc> othRef) {
  auto oth = findNpc(othRef);
  auto npc = findNpc(npcRef);
  if(npc!=nullptr && oth!=nullptr)
    npc->aiPush(AiQueue::aiFinishingMove(*oth));
  }

void GameScript::ai_takeitem(std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::IItem> itmRef) {
  auto itm = findItem(itmRef.get());
  auto npc = findNpc(npcRef);
  if(npc!=nullptr && itm!=nullptr)
    npc->aiPush(AiQueue::aiTakeItem(*itm));
  }

void GameScript::ai_gotoitem(std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::IItem> itmRef) {
  auto itm = findItem(itmRef.get());
  auto npc = findNpc(npcRef);
  if(npc!=nullptr && itm!=nullptr)
    npc->aiPush(AiQueue::aiGotoItem(*itm));
  // NB fix 61: the wisp of Vatras goes to an item it has found; the plugin gives the item to the hero then
  if(npc!=nullptr && itm!=nullptr && nbStates.count(this)) {
    auto* sym = vm.find_symbol_by_index(npcRef->symbol_index());
    if(sym!=nullptr && sym->name()=="WISP_DETECTOR") {
      auto& st = nbStates[this];
      st.wispNpc   = npcRef;
      st.wispItem  = itmRef;
      st.wispSince = tickCount();
      // NB fix 62: once for an item (the text "Coś znaleziono!" is drawn by the HUD, fix 64)
      if(st.wispFound!=itmRef.get()) {
        st.wispFound = itmRef.get();
        st.wispBest  = 1e9f;
        nbCtx("the wisp found: ",itm->displayName()," at ",int((itm->position()-npc->position()).length())," cm");
        }
      }
    }
  }

void GameScript::ai_pointat(std::shared_ptr<zenkit::INpc> npcRef, std::string_view waypoint) {
  auto npc = findNpc(npcRef);
  auto to  = world().findPoint(waypoint);
  if(npc!=nullptr && to!=nullptr)
    npc->aiPush(AiQueue::aiPointAt(*to));
  }

void GameScript::ai_pointatnpc(std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc> otherRef) {
  auto other = findNpc(otherRef);
  auto npc   = findNpc(npcRef);
  if(npc!=nullptr && other!=nullptr)
    npc->aiPush(AiQueue::aiPointAtNpc(*other));
  }

int GameScript::ai_printscreen(std::string_view msg, int posx, int posy, std::string_view font, int timesec) {
  nbPrintLog(vm,msg);   // NB fix 229
  auto npc = findNpc(vm.global_self());
  if(npc==nullptr)
    npc = owner.player();
  if(npc==nullptr) {
    Gothic::inst().onPrintScreen(msg,posx,posy,timesec,Resources::font(font,Resources::FontType::Normal,1.0));
    return 0;
    }
  npc->aiPush(AiQueue::aiPrintScreen(timesec,font,posx,posy,msg));
  return 0;
  }

int GameScript::mob_hasitems(std::string_view tag, int item) {
  return int(world().hasItems(tag,uint32_t(item)));
  }

void GameScript::mob_createitems(std::string_view tag, int itemInstance, int amount) {
  if(itemInstance<=0 || amount<=0)
    return;
  auto* cls = vm.find_symbol_by_index(uint32_t(itemInstance));
  if(cls==nullptr || cls->type()!=zenkit::DaedalusDataType::INSTANCE)
    return;
  for(uint32_t i=0; auto* mob = world().mobsiById(i); ++i) {
    const auto name = mob->tag();
    const bool matches = name.size()==tag.size() && std::equal(name.begin(),name.end(),tag.begin(),[](char a, char b) {
      return std::toupper(static_cast<unsigned char>(a))==std::toupper(static_cast<unsigned char>(b));
      });
    if(mob->isContainer() && matches) {
      mob->inventory().addItem(size_t(itemInstance),size_t(amount),world());
      return;
      }
    }
  Log::d("Mob_CreateItems: container not found: ",tag);
  }

void GameScript::ta_min(std::shared_ptr<zenkit::INpc> npcRef, int start_h, int start_m, int stop_h, int stop_m, int action, std::string_view waypoint) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->addRoutine(gtime(start_h,start_m),gtime(stop_h,stop_m),uint32_t(action),waypoint);
  }

void GameScript::log_createtopic(std::string_view topicName, int section) {
  if(section==QuestLog::Mission || section==QuestLog::Note)
    quests.add(topicName,QuestLog::Section(section));
  }

void GameScript::log_settopicstatus(std::string_view topicName, int status) {
  if(status==int32_t(QuestLog::Status::Running) ||
     status==int32_t(QuestLog::Status::Success) ||
     status==int32_t(QuestLog::Status::Failed ) ||
     status==int32_t(QuestLog::Status::Obsolete)) {
    // NB fix 225: the status of the topic in the log - NB: LOG_SUCCESS is 4 (the "obsolete" of the engine, a mission done,
    // not closed yet), B_CloseTopic_Now turns it into 2 (LOG_CLOSED: "Zadanie zakończone!")
    static const char* names[] = {"-","running","closed","failed","success (4)"};
    nbCtx("journal status: ",topicName," -> ",names[status]);
    quests.setStatus(topicName,QuestLog::Status(status));
    }
  }

void GameScript::logMoveToTop(std::string_view topic) {
  quests.moveToTop(topic);
  }

void GameScript::log_addentry(std::string_view topicName, std::string_view entry) {
  if(nbStates.count(this))
    nbCtx("journal: ",topicName," | ",entry.substr(0,100));
  quests.addEntry(topicName,entry);
  }

void GameScript::equipitem(std::shared_ptr<zenkit::INpc> npcRef, int cls) {
  auto self = findNpc(npcRef);
  if(self!=nullptr) {
    if(self->itemCount(uint32_t(cls))==0)
      self->addItem(uint32_t(cls),1);
    self->useItem(uint32_t(cls),Item::NSLOT,true);
    }
  }

/// The scripts of the original game sometimes give an item by something that is not an item: B_GiveTradeInv_Addon_Samuel of Gothic II
/// asks for the integer RI_ProtFire02 (5, the index of FloatToInt) instead of ItRi_Prot_Fire_02. The original engine creates nothing then and
/// the script goes on. Here the initialization of the instance threw an exception, and the dialog of the trade ended with an error.
int GameScript::clearHeroAttackReason(Npc& hero) {
  if(attackReasonVar==-2) {
    const auto* sym = vm.find_symbol_by_name("AIV_ATTACKREASON");
    attackReasonVar = (sym!=nullptr && sym->type()==zenkit::DaedalusDataType::INT) ? sym->get_int() : -1;
    if(attackReasonVar<0 || attackReasonVar>=int(zenkit::INpc::aivar_count))
      attackReasonVar = -1;
    }
  if(attackReasonVar<0)
    return 0;
  auto& v   = hero.handle().aivar[attackReasonVar];
  auto  old = v;
  v = 0;
  return old;
  }

bool GameScript::isItemInstance(int itemInstance, std::string_view caller) {
  auto* sym = vm.find_symbol_by_index(uint32_t(itemInstance));
  if(sym!=nullptr && sym->type()==zenkit::DaedalusDataType::INSTANCE)
    return true;
  Log::d(caller,": ",itemInstance," (",sym!=nullptr ? sym->name() : std::string("?"),") is not an item instance, nothing is created");
  return false;
  }

void GameScript::createinvitem(std::shared_ptr<zenkit::INpc> npcRef, int itemInstance) {
  if(itemInstance<=0 || !isItemInstance(itemInstance,"CreateInvItem"))
    return; // an empty slot of a script or not an item, the original engine does nothing
  auto self = findNpc(npcRef);
  if(self!=nullptr) {
    Item* itm = self->addItem(uint32_t(itemInstance),1);
    storeItem(itm);
    }
  }

void GameScript::createinvitems(std::shared_ptr<zenkit::INpc> npcRef, int itemInstance, int amount) {
  if(itemInstance<=0 || amount<=0 || !isItemInstance(itemInstance,"CreateInvItems"))
    return;
  auto self = findNpc(npcRef);
  if(self!=nullptr && amount>0) {
    Item* itm = self->addItem(uint32_t(itemInstance),size_t(amount));
    storeItem(itm);
    }
  }

void GameScript::perc_setrange(int perc, int dist) {
  if(perc<0 || perc>=PERC_Count)
    return;
  perceptionRanges.range[perc] = dist;
  }

int GameScript::hlp_getinstanceid(std::shared_ptr<zenkit::DaedalusInstance> instance) {
  // Log::d("hlp_getinstanceid: name \"",handle.name,"\" not found");
  return instance == nullptr ? -1 : int(instance->symbol_index());
  }

int GameScript::hlp_random(int bound) {
  uint32_t mod = uint32_t(std::max(1,bound));
  return int32_t(randGen() % mod);
  }

bool GameScript::hlp_isvalidnpc(std::shared_ptr<zenkit::INpc> npcRef) {
  auto self = findNpc(npcRef);
  return self != nullptr;
  }

bool GameScript::hlp_isitem(std::shared_ptr<zenkit::IItem> itemRef, int instanceSymbol) {
  auto item = findItem(itemRef.get());
  if(item!=nullptr){
    auto& v = item->handle();
    return int(v.symbol_index()) == instanceSymbol;
    } else {
      return false;
    }
  }

bool GameScript::hlp_isvaliditem(std::shared_ptr<zenkit::IItem> itemRef) {
  auto item = findItem(itemRef.get());
  return item!=nullptr;
  }

std::shared_ptr<zenkit::INpc> GameScript::hlp_getnpc(int instanceSymbol) {
  auto npc = findNpcById(uint32_t(instanceSymbol));
  if(npc != nullptr)
    return npc->handlePtr();
  else
    return nullptr;
  }

void GameScript::info_addchoice(int infoInstance, std::string_view text, int func) {
  auto info = findInfo(uint32_t(infoInstance));
  if(info==nullptr)
    return;
  zenkit::IInfoChoice choice {};
  choice.text     = text;
  choice.function = func;
  info->add_choice(choice);
  }

void GameScript::info_clearchoices(int infoInstance) {
  auto info = findInfo(uint32_t(infoInstance));
  if(info==nullptr)
    return;
  info->choices.clear();
  }

bool GameScript::infomanager_hasfinished() {
  return !owner.isInDialog();
  }

void GameScript::snd_play(std::string_view fileS) {
  if(aiProcessPolicy>=NpcProcessPolicy::AiFar2)
    return;

  std::string file {fileS};
  for(auto& c:file)
    c = char(std::toupper(c));
  Gothic::inst().emitGlobalSound(file);
  }

void GameScript::snd_play3d(std::shared_ptr<zenkit::INpc> npcRef, std::string_view fileS) {
  if(aiProcessPolicy>=NpcProcessPolicy::AiFar2)
    return;

  std::string file {fileS};
  Npc*        npc  = findNpc(npcRef);
  if(npc==nullptr)
    return;
  for(auto& c:file)
    c = char(std::toupper(c));
  auto sfx = ::Sound(*owner.world(),::Sound::T_3D,file,npc->centerPosition(),0.f,false);
  sfx.play();
  }

void GameScript::exitsession() {
  if(DialogTest::active || DialogTest::guardExit) {
    Log::i("ExitSession of the scripts is ignored by the dialog test");
    return;
    }
  if(auto* pl = world().player())
    Log::i("ExitSession called by the script, the player is at ",pl->position().x," ",pl->position().y," ",pl->position().z);
  owner.exitSession();
  }

void GameScript::sort(std::vector<GameScript::DlgChoice> &dlg) {
  std::sort(dlg.begin(),dlg.end(),[](const GameScript::DlgChoice& l,const GameScript::DlgChoice& r){
    return std::tie(l.sort,l.scriptFn)<std::tie(r.sort,r.scriptFn); // small hack with scriptfn to reproduce behavior of original game
    });
  }

void GameScript::setNpcInfoKnown(const zenkit::INpc& npc, const zenkit::IInfo& info) {
  auto id = std::make_pair(vm.find_symbol_by_instance(npc)->index(),vm.find_symbol_by_instance(info)->index());
  dlgKnownInfos.insert(id);
  }

bool GameScript::setInfoKnown(Npc& npc, std::string_view infoName, bool known) {
  auto* sym = vm.find_symbol_by_name(infoName);
  if(sym==nullptr || !sym->is_instance_of<zenkit::IInfo>())
    return false;
  const auto key = std::make_pair(size_t(npc.instanceSymbol()),size_t(sym->index()));
  if(known)
    dlgKnownInfos.insert(key); else
    dlgKnownInfos.erase(key);
  return true;
  }

bool GameScript::isInfoTold(size_t infoInstance) {
  auto* sym = vm.find_symbol_by_index(uint32_t(infoInstance));
  if(sym==nullptr || !sym->is_instance_of<zenkit::IInfo>())
    return false;
  auto info = std::dynamic_pointer_cast<zenkit::IInfo>(sym->get_instance());
  return info!=nullptr && dlgKnownInfos.find(std::make_pair(size_t(info->npc),infoInstance))!=dlgKnownInfos.end();
  }

void GameScript::setInfoTold(size_t infoInstance, bool told) {
  auto* sym = vm.find_symbol_by_index(uint32_t(infoInstance));
  if(sym==nullptr || !sym->is_instance_of<zenkit::IInfo>())
    return;
  auto info = std::dynamic_pointer_cast<zenkit::IInfo>(sym->get_instance());
  if(info==nullptr)
    return;
  const auto key = std::make_pair(size_t(info->npc),infoInstance);
  if(told)
    dlgKnownInfos.insert(key); else
    dlgKnownInfos.erase(key);
  }

bool GameScript::npcKnowsInfo(Npc& npc, size_t infoInstance) const {
  return doesNpcKnowInfo(npc.handle(),infoInstance);
  }

DialogTest& GameScript::dialogTest() {
  if(dlgTest==nullptr)
    dlgTest.reset(new DialogTest(*this));
  return *dlgTest;
  }

bool GameScript::doesNpcKnowInfo(const zenkit::INpc& npc, size_t infoInstance) const {
  auto id = std::make_pair(vm.find_symbol_by_instance(npc)->index(),infoInstance);
  return dlgKnownInfos.find(id)!=dlgKnownInfos.end();
  }

// NB fix 70, 71: the crafting windows of New Balance. The plugin keeps the recipes in its code (nbcraft.h) and shows
// only those that the hero knows: the test of UNION_ABI.DLL at 0x101a5940, written here by the table and the number
// "kind" of the recipe
namespace {
int32_t nbScriptInt(zenkit::DaedalusVm& vm, std::string_view name, uint16_t index = 0, int32_t missing = 0) {
  std::string up(name);
  for(auto& c:up)
    c = char(std::toupper(uint8_t(c)));
  auto* s = vm.find_symbol_by_name(up);
  if(s==nullptr || s->type()!=zenkit::DaedalusDataType::INT || s->is_member() || index>=s->count())
    return missing;
  return s->get_int(index);
  }

void nbScriptSetInt(zenkit::DaedalusVm& vm, std::string_view name, int32_t v, uint16_t index = 0) {
  std::string up(name);
  for(auto& c:up)
    c = char(std::toupper(uint8_t(c)));
  auto* s = vm.find_symbol_by_name(up);
  if(s!=nullptr && s->type()==zenkit::DaedalusDataType::INT && !s->is_member() && index<s->count())
    s->set_int(v,index);
  }

const NbCraft::Recipe* nbRecipe(int32_t craft, size_t index) {
  auto& list = NbCraft::recipes(craft);
  return index<list.size() ? &list[index] : nullptr;
  }

// the bows: "VAR : value | VAR : value" (0x1002dd70 / 0x1002e040), every variable must have its value (no variable: -1)
bool nbCraftCond(zenkit::DaedalusVm& vm, std::string_view cond) {
  if(cond.empty() || cond=="TRUE" || cond=="true")
    return true;
  size_t pos = 0;
  while(pos<=cond.size()) {
    size_t end = cond.find('|',pos);
    if(end==std::string_view::npos)
      end = cond.size();
    std::string_view part = cond.substr(pos,end-pos);
    const size_t colon = part.find(':');
    if(colon!=std::string_view::npos) {
      auto trim = [](std::string_view v) {
        while(!v.empty() && std::isspace(uint8_t(v.front()))) v.remove_prefix(1);
        while(!v.empty() && std::isspace(uint8_t(v.back())))  v.remove_suffix(1);
        return v;
        };
      const auto name  = trim(part.substr(0,colon));
      const auto value = trim(part.substr(colon+1));
      const int32_t want = std::atoi(std::string(value).c_str());
      if(nbScriptInt(vm,name,0,-1)!=want)
        return false;
      }
    pos = end+1;
    }
  return true;
  }

bool nbCraftKnown(zenkit::DaedalusVm& vm, Npc& hero, int32_t craft, const NbCraft::Recipe& r, size_t heartCls) {
  const auto v = [&vm](std::string_view name) { return nbScriptInt(vm,name); };
  const auto smith = [&vm](int32_t i) { return nbScriptInt(vm,"PLAYER_TALENT_SMITH",uint16_t(i))!=0; };
  const auto runes = [&vm](int32_t i) { return nbScriptInt(vm,"PLAYER_TALENT_RUNES",uint16_t(i))!=0; };
  const auto guild = [&hero]() { const auto g = hero.guild(); return g==14 || g==16; };
  const int32_t k = r.kind;
  switch(craft) {
    case 0: {
      // the potions of the talents (Health_01..perm_stamina): PLAYER_TALENT_ALCHEMY[kind]
      if(k<19 && k!=12)
        return nbScriptInt(vm,"PLAYER_TALENT_ALCHEMY",uint16_t(k))>0;
      switch(k) {
        case 12: // the Eye of Innos: the recipe with the heart that the hero has
          return heartCls!=size_t(-1) && hero.inventory().itemCount(heartCls)>0;
        // these the plugin reads by the symbol: without the variable the recipe is shown
        case 19: return nbScriptInt(vm,"KNOWFULLHEALTH2",0,1)!=0;
        case 20: return nbScriptInt(vm,"RX_FULLPOTIONKNOWS",0,1)!=0;
        case 21: return v("KNOWSPEED3")!=0;
        case 22: return v("KNOWPERMINT")!=0;
        case 23: return v("KNOWPROTFALL")!=0;
        case 24: return v("KNOWFIREDEF")!=0;
        case 25: return v("KNOWMAGDEF")!=0;
        case 26: return v("KNOWFAKERECEPT")==0 && v("MIS_MYBREW")==1 && v("FTCONSPOTION")==0;
        case 27: return v("SOULRIVER")==1;   // the plugin checks also something of the engine (0x1004d580)
        case 28: return v("KNOWS_MCELIXIER")!=0 && v("KNOWS_BLOODFLY")==0;
        case 29: return v("KNOWS_MCELIXIER")!=0 && v("KNOWS_BLOODFLY")==1;
        case 30: return v("READ_AZGOLOR")!=0 && v("RX_EYEAWAKEN")==0;
        case 31: return v("KNOW_NIMROD_MAKE")!=0 && v("RX_PUMAJIRCRAFTED")==0;
        // the plugin asks also "MIS_TiamantMuritan " (with a space - no such variable, so it always passes)
        case 32: return v("MAKEMURITANSWEET")!=0 && v("RX_MURITANCRAFT")==0;
        case 33: return v("READBOOKSDONE_09")!=0;
        case 34: return v("READBOOKSDONE_08")!=0;
        case 35: return v("KNOWS_SCHLAFHAMMER")!=0;
        case 36: return v("KNOWS_LOUSHAMMER")!=0;
        case 37: return v("KNOWS_SCHNELLERHERING")!=0;
        case 38: return v("READBOOKSDONE_99")!=0;
        case 39: return v("MAKEPUREALCOHOLBOOZE")!=0;
        case 40: return v("MAKEPUREALCOHOLGROG")!=0;
        case 41:
        case 42: return v("MAKEPUREALCOHOLROM")!=0;
        case 43: return v("ANPOISONKNOW")!=0;
        case 44:
        case 45: return v("MAKEPOISONKNOW")!=0;
        case 46: case 47: case 48: case 49: case 50:
          return v("RX_POISONKNOWLEVEL")>=k-45;
        case 51: return v("RX_POISONNEWRECEPIES")==1 && v("RX_POISON_RECIPEANIMAL")!=0;
        case 52: return v("RX_POISONNEWRECEPIES")==1 && v("RX_POISON_RECIPEORC")!=0;
        case 53: return v("RX_POISONNEWRECEPIES")==1 && v("RX_POISON_RECIPEUNDEAD")!=0;
        case 54: case 55: case 56: case 57:   // the tobaccos, the joint
          return true;
        case 58: return v("GREEN_EXTREM")==1;
        case 59:
        case 60: return v("KNOW_JOINTRECIPE")==1;
        case 61: case 62: case 63: case 64: case 65:   // the permanent potions from ItMi_PotionPermBlank
          return true;
        case 66: case 67: case 68:
          return v("RX_POISONUNIVERSAL")==1;
        case 69:
        case 70: return nbScriptInt(vm,"PLAYER_TALENT_ALCHEMY",13)>0;
        case 71:
        case 72: return nbScriptInt(vm,"PLAYER_TALENT_ALCHEMY",14)>0;
        case 73: return nbScriptInt(vm,"RX_SCOUT_SKILL_POWDER_TYPE_FIRE",     0,1)!=0;
        case 74: return nbScriptInt(vm,"RX_SCOUT_SKILL_POWDER_TYPE_SMOKE",    0,1)!=0;
        case 75: return nbScriptInt(vm,"RX_SCOUT_SKILL_POWDER_TYPE_EXPLOSION",0,1)!=0;
        case 76: return nbScriptInt(vm,"RX_SCOUT_SKILL_POWDER_TYPE_ACID",     0,1)!=0;
        case 77: return nbScriptInt(vm,"RX_SCOUT_SKILL_POWDER_TYPE_MAGIC",    0,1)!=0;
        case 78: return nbScriptInt(vm,"RX_SCOUT_SKILL_POWDER_TYPE_UNDEAD",   0,1)!=0;
        }
      return false;
      }
    case 1: { // the anvil (0x101a7f8b)
      if(k<13)   // read by the symbol: without the variable the recipe is shown
        return nbScriptInt(vm,"PLAYER_TALENT_SMITH",uint16_t(k),1)!=0;
      if(k>=65 && k<=69)
        return smith(k-30);
      switch(k) {
        case 13: return v("KNOWNORESWORD")!=0;
        case 14: case 15: case 16:
          return v("KNOWNORESWORD")!=0 && v("NETBEKLEADME_STEP2")!=0;
        case 17: return v("KNOWS_MOONBLADES_LEFT")!=0  && smith(0);
        case 18: return v("KNOWS_MOONBLADES_RIGHT")!=0 && smith(0);
        case 19: case 20:
          return v("SIGILKNOW")!=0 && smith(0);
        case 21: case 22: case 23: case 24: case 25: case 26: case 27: case 28:
          return v("LOVKACH_W"+std::to_string(k-20))!=0 && smith(0);
        case 29: case 30:
          return v("KNOWHOWTOMAKEARROWS")!=0;
        case 31: return v("KNOWHOWTOMAKEARROWSKER")!=0;
        case 34: return v("MIS_MONASTERYSECRET")==4;
        case 35: case 36: case 37:
          return v("ORCWEAPON_LINE_STR_0"+std::to_string(k-34))!=0;
        case 38: case 39: case 40:
          return v("ORCWEAPON_LINE_HP_0"+std::to_string(k-37))!=0;
        case 41: return v("ORCPRIMEWEAPON")!=0;
        case 42: case 43: case 44: case 45: case 46:
          return v("RX_REC_WEAPON_0"+std::to_string(k-41))!=0 && smith(0);
        case 47: case 48: case 49: case 50:
          return v("RX_REC_WEAPON_06")==k-46 && smith(0);
        case 51: return v("RX_REC_WEAPON_07")!=0 && smith(0);
        case 70: case 71: case 72:
          return v("ORCWEAPON_LINE_NEW_0"+std::to_string(k-69))!=0;
        }
      return false;
      }
    case 2: { // the runes (0x101aac26)
      if(k<=4)
        return true;   // the rune stones and the blank runes
      if(std::string_view(r.result)=="ItMi_RuneBenTeleport")
        return nbScriptInt(vm,"RX_WC_CANCREATETPRUNE",0,-1)!=0 && nbScriptInt(vm,"RX_WC_TPRUNECREATED",0,-1)==0;
      switch(k) {
        case 132: return v("KNOWS_CRESTMAKE")!=0 && v("CRESTNATUREISDONE")==0;
        case 101: return v("NETBEKLEADME_STEP2")!=0 && v("NETBEKLEADME_STEP2DONE")==0 && v("HOWCANMAKERUNE")!=0;
        case 7:   return runes(7) && nbScriptInt(vm,"PLAYER_MAKE_RUNES",7)==0;
        case 48:  return v("HEROKNOWNTRANS_BLOODFLY")!=0;
        case 55:  return v("HEROKNOWNTRANS_FIREWARAN")!=0;
        case 54:  return v("HEROKNOWNTRANS_WARG")!=0;
        case 57:  return v("HEROKNOWNTRANS_SHADOWBEAST")!=0;
        case 58:  return v("HEROKNOWNTRANS_DRAGONSNAPPER")!=0;
        }
      return runes(k);
      }
    case 3: // the bows: the conditions of the recipe
      return nbCraftCond(vm,r.cond);
    case 4: { // the demon runes (0x101aa33e)
      if(k==0)
        return runes(40) && v("XARDASTEACHCASTDEMON_SIMPLE")!=0 && v("SUMMONRUNEDONE")==0;
      if(k>=1 && k<=3)
        return v("XARDASTEACHCASTDEMON_SIMPLE")!=0;
      if(k>=4 && k<=6)
        return v("XARDASTEACHCASTDEMON_LORD")!=0;
      return true;
      }
    case 5: { // the armors (0x101ab290)
      if(r.level>0) {   // the reinforcement of an armor
        const int32_t knows = v("RX_HEROKNOWSREFORCEMENT");
        return r.level>=2 ? knows==2 : knows>=1;
        }
      switch(k) {
        case 16: return v("KNOWMAKECRAWLERARMOR")!=0;
        case 17: return v("LIGHTARMORMADE")==0 && smith(33);
        case 18: return v("DARKARMORMADE")==0  && smith(34);   // 63 - the same armor for another version of the game
        case 28: case 29: case 30: case 31:
          return v("KNOW_HUNTARMOR_0"+std::to_string(k-27))!=0;
        case 50: return v("RX_NB_DH_ARMORRECIPE_READ")!=0;
        case 58: return v("RX_SCOUT_BLACKHOODRECIPE")!=0;
        case 62: return v("RX_SCOUT_ARMORLEGRECIPE")!=0;
        case 73: case 74:
          return hero.guild()==16 && ((v("RX_AW_NOTESMASK1")>>11)&1)!=0;
        }
      return false;
      }
    case 6: { // the soups (0x101a9dd1)
      static const char* const vars[] = {"EDDA_SOUP_00","EDDA_SOUP_01","EDDA_SOUP_02","EDDA_SOUP_03","EDDA_SOUP_04",
                                         "SNAF_MEAL_02","SNAF_MEAL_01","SNAF_MEAL_03","REZEPT_COMPOTE_01","REZEPT_COMPOTE_02"};
      if(k==0)
        return nbScriptInt(vm,vars[0],0,1)!=0;
      return k>0 && k<10 && v(vars[k])!=0;
      }
    case 7: // the skins: the bow strings (0x101ac2ac)
      return v("BOWMAKE_01")!=0 && k>=0 && k<=4 && v("BOWMAKE_0"+std::to_string(k+1))!=0;
    case 8: { // the smelting (0x101aa95b)
      if(k==1 || k==2 || k==22)
        return true;
      if(k==0 || k==3 || (k>=6 && k<=8))
        return v("KNOWHOWTOOREFUS")!=0;
      if(k==5 || k==21)
        return v("KNOWHOWTOOREFUSGOLD")!=0;
      if(k>=9 && k<=20)
        return v("ORCSWEAPONSTEEL")!=0;
      return false;
      }
    case 9: { // the jewelry (0x101aa652)
      switch(k) {
        case 0: return v("JEWERLY_LEVEL_L1")!=0;
        case 1: return v("JEWERLY_LEVEL_L2")!=0;
        case 2: case 3: return v("JEWERLY_LEVEL_L3")!=0;
        case 4: case 5: return v("RX_JEWELMAX")!=0;
        }
      return false;
      }
    case 10: // the pies (0x101abf9b)
      if(k<=3)
        return true;
      if(r.id==4 || r.id==5)
        return guild();
      if(k>=6 && k<=10)
        return v("THEKLA_CAKE_0"+std::to_string(k-5))!=0;
      return false;
    case 11: { // the scrolls (0x101abbb5)
      static const char* const vars[] = {"HOWCANMAKERUNE_ITSC7","HOWCANMAKERUNE_ITSC1","HOWCANMAKERUNE_ITSC2",
                                         "HOWCANMAKERUNE_ITSC4","HOWCANMAKERUNE_ITSC5","HOWCANMAKERUNE_ITSC6"};
      return k>=0 && k<6 && v(vars[k])!=0;
      }
    case 12: // the food over the fire (0x101abf09): the island fish and the dried meat - a guild of the plugin
      return (r.id==4 || r.id==5) ? guild() : true;
    }
  return false;
  }
}

size_t GameScript::nbCraftAvailable(Npc& hero, size_t cls) {
  // as the plugin: an equipped item does not go to the table (RX_Craft_EquippedItem)
  size_t n = hero.inventory().itemCount(cls);
  if(auto* it = hero.getItem(cls); it!=nullptr && it->isEquipped())
    n -= std::min<size_t>(n,it->equipCount());
  return n;
  }

std::vector<NbCraft::Item> GameScript::nbCraftList(Npc& hero, int32_t craft) {
  std::vector<NbCraft::Item> ret;
  const auto itemSym = [this](std::string_view name) -> zenkit::DaedalusSymbol* {
    std::string up(name);
    for(auto& c:up)
      c = char(std::toupper(uint8_t(c)));
    auto* sym = vm.find_symbol_by_name(up);
    return nbIsItemSymbol(vm,sym) ? sym : nullptr;
    };
  for(auto& r:NbCraft::recipes(craft)) {
    // the Eye of Innos: the recipe 5000+i is the i-th heart (the second ingredient)
    size_t heart = size_t(-1);
    if(craft==0 && r.kind==12 && r.ingredients.size()>1)
      if(auto* h = itemSym(r.ingredients[1].item))
        heart = h->index();
    if(!nbCraftKnown(vm,hero,craft,r,heart))
      continue;
    auto* res = itemSym(r.result);
    if(res==nullptr) {
      Log::e("NB: craft: no item \"",r.result,"\"");
      continue;
      }
    NbCraft::Item it;
    it.id    = r.id;
    it.index = size_t(&r-NbCraft::recipes(craft).data());
    it.tab   = r.tab;
    it.cls   = res->index();
    it.count = std::max(1,r.count);
    // the runes are all called "Runa": the plugin shows the description (the spell), as the inventory
    if(auto inst = vm.init_instance<zenkit::IItem>(res); inst!=nullptr)
      it.name = !inst->description.empty() ? inst->description : inst->name;
    if(it.name.empty())
      it.name = r.result;
    bool ok = true;
    for(auto& in:r.ingredients) {
      auto* sym = itemSym(in.item);
      if(sym==nullptr) {
        Log::e("NB: craft: no item \"",in.item,"\" (recipe ",r.id,")");
        ok = false;
        break;
        }
      // the same item twice in a recipe: one count
      auto same = std::find_if(it.ingredients.begin(),it.ingredients.end(),[&](auto& p){ return p.first==sym->index(); });
      if(same!=it.ingredients.end())
        same->second += in.count; else
        it.ingredients.emplace_back(sym->index(),in.count);
      }
    if(ok)
      ret.push_back(std::move(it));
    }
  nbCtx("craft window ",craft,": ",ret.size()," known recipes");
  return ret;
  }

std::string GameScript::nbCraftCreate(Npc& hero, int32_t craft, const NbCraft::Item& item, size_t count) {
  auto* rec = nbRecipe(craft,item.index);
  if(rec==nullptr || count==0)
    return std::string();
  for(auto& [cls,need]:item.ingredients)
    if(nbCraftAvailable(hero,cls)<size_t(need)*count)
      return std::string();
  // as the plugin (0x101b5a90 and 0x101b4b30): the ingredients go, the runes are counted, the CRAFT_* variables, the
  // items, the message, RX_RaiseAlchemy / RX_RaiseSmith (grade, count) and RX_CraftCallBack
  for(auto& [cls,need]:item.ingredients)
    hero.delItem(cls,uint32_t(size_t(need)*count));
  const int32_t n    = int32_t(count);
  const int32_t made = n*std::max(1,rec->count);
  if(craft==2 && rec->kind>3 && rec->kind!=101 && rec->kind!=132 &&
     nbScriptInt(vm,"PLAYER_MAKE_RUNES",uint16_t(rec->kind))==0) {
    // the first rune of a kind: PLAYER_MAKE_RUNES[kind] = 1, TalentCount_Rune += grade
    nbScriptSetInt(vm,"PLAYER_MAKE_RUNES",1,uint16_t(rec->kind));
    nbScriptSetInt(vm,"TALENTCOUNT_RUNE",nbScriptInt(vm,"TALENTCOUNT_RUNE")+rec->grade);
    }
  nbScriptSetInt(vm,"CRAFT_COUNT_TRUE",n);
  nbScriptSetInt(vm,"CRAFT_EXP",       rec->exp*n);
  hero.addItem(item.cls,size_t(made));
  nbScriptSetInt(vm,"CRAFT_ITEM",      int32_t(item.cls));
  nbScriptSetInt(vm,"CRAFT_CAT",       craft);
  nbScriptSetInt(vm,"CRAFT_COUNT",     made);
  nbScriptSetInt(vm,"CRAFT_TAB",       rec->tab);
  nbScriptSetInt(vm,"CRAFT_SKILL",     rec->grade);
  nbCtx("craft ",craft,": ",item.name," x",made," (recipe ",item.id,")");

  ScopeVar self(*vm.global_self(), hero.handlePtr());
  const auto call = [this](const char* name, auto... args) {
    auto* fn = vm.find_symbol_by_name(name);
    if(fn==nullptr || fn->type()!=zenkit::DaedalusDataType::FUNCTION)
      return;
    try {
      vm.call_function<void>(fn,args...);
      }
    catch(const std::exception& e) {
      Log::e("NB: ",name," failed: ",e.what());
      }
    };
  if(craft==0)
    call("RX_RAISEALCHEMY",rec->grade,made);
  else if(craft==1 || craft==5)
    call("RX_RAISESMITH",rec->grade,made);
  call("RX_CRAFTCALLBACK");
  return nbCraftText("RX_CRAFT_CREATED","Stworzono: ")+item.name+" x"+std::to_string(made);
  }

void GameScript::nbCraftEnd(Npc& hero) {
  // as the plugin when the window is closed: RX_EndCraft (SELF is the hero: AIV_INVINCIBLE, PLAYER_MOBSI_PRODUCTION),
  // then the hero leaves the table
  if(auto* fn = vm.find_symbol_by_name("RX_ENDCRAFT"); fn!=nullptr && fn->type()==zenkit::DaedalusDataType::FUNCTION) {
    ScopeVar self(*vm.global_self(), hero.handlePtr());
    try {
      vm.call_function<void>(fn);
      }
    catch(const std::exception& e) {
      Log::e("NB: RX_EndCraft failed: ",e.what());
      }
    }
  hero.setInteraction(nullptr);
  nbCtx("craft window closed");
  }

int32_t GameScript::nbCraftSkill(int32_t craft) {
  switch(craft) {
    case 0:         return nbScriptInt(vm,"TALENTCOUNT_ALCHEMY");
    case 1: case 5: return nbScriptInt(vm,"TALENTCOUNT_SMITH");
    case 2: case 4: return nbScriptInt(vm,"TALENTCOUNT_RUNE");
    }
  return -1;
  }

std::string GameScript::nbCraftText(std::string_view symbol, std::string_view fallback) {
  // the strings of the scripts (as the plugin), but the Russian ones (cp1251) of the original scripts are replaced by
  // the Polish text: most of their letters are above 0xC0
  auto* s = vm.find_symbol_by_name(symbol);
  if(s==nullptr || s->type()!=zenkit::DaedalusDataType::STRING || s->is_member())
    return std::string(fallback);
  std::string txt(s->get_string());
  size_t letters = 0, high = 0;
  for(auto c:txt) {
    const uint8_t u = uint8_t(c);
    if(std::isalpha(u) || u>=0xC0)
      ++letters;
    if(u>=0xC0)
      ++high;
    }
  if(txt.empty() || letters==0 || high*2>letters)
    return std::string(fallback);
  return txt;
  }

// NB fix 79, 80: the locks of New Balance. On the PC the plugin calls HandleEvent(key) for every key: the key Y
// (KEY_BREAKCHEST_STR) opens the chest in the focus with the rune ItRu_Unlock (75 of mana) or by strength (RX_ForcePickLock,
// taught by Karl, a melee weapon, 12 of strength a turn of the lock over 50); Shift+Y (KEY_BREAKCHEST_DEX) with lock picks
// (the talent, 100 of dexterity). On the touch screen it is the interaction with the chest in the focus, as with a person:
// the way is the one in the hands (the weapon - strength, the rune - magic, nothing - lock picks), the action button calls
// HandleEvent with its key, the scripts do the rest (the checks, the animation, the texts, the mana, the stamina)
bool GameScript::nbHasBreakLock() {
  auto* fn = vm.find_symbol_by_name("HANDLEEVENT");
  auto* gb = vm.find_symbol_by_name("MOB_SETBRAKECHEST");
  return fn!=nullptr && fn->type()==zenkit::DaedalusDataType::FUNCTION && !fn->is_external() &&
         gb!=nullptr && gb->is_external();
  }

bool GameScript::nbCanBreakLock(const Interactive* mob) const {
  return mob!=nullptr && mob->isContainer() && mob->isLocked() && !mob->pickLockCode().empty();
  }

int32_t GameScript::nbLockMode(const Npc& pl) {
  if(!pl.isPlayer() || pl.isDead() || pl.isDown() || pl.interactive()!=nullptr || !nbHasBreakLock())
    return 0;
  const auto ws = pl.weaponState();
  if(ws==WeaponState::W1H || ws==WeaponState::W2H)
    return nbScriptInt(vm,"RX_FORCEPICKLOCK")!=0 ? 1 : 0;
  if(ws==WeaponState::Mage) {
    auto* sp = const_cast<Npc&>(pl).inventory().activeWeapon();
    return (sp!=nullptr && sp->spellId()==nbScriptInt(vm,"SPL_UNLOCKCHEST",0,99)) ? 2 : 0;
    }
  if(ws==WeaponState::NoWeapon)
    return (pl.talentSkill(TALENT_PICKLOCK)>0 && pl.attribute(ATR_DEXTERITY)>=100) ? 3 : 0;
  return 0;
  }

void GameScript::nbSetFocusMob(Interactive* mob) {
  if(auto it = nbStates.find(this); it!=nbStates.end())
    it->second.focusMob = mob;
  }

void GameScript::nbSetFocusNpc(Npc* npc) {
  if(auto it = nbStates.find(this); it!=nbStates.end())
    it->second.focusNpc = npc!=nullptr ? npc->handlePtr() : nullptr;
  }

void GameScript::nbBreakLock(Npc& hero, Interactive& mob, int32_t mode) {
  auto* fn = vm.find_symbol_by_name("HANDLEEVENT");
  if(fn==nullptr || fn->type()!=zenkit::DaedalusDataType::FUNCTION || mode<=0)
    return;
  auto& st = nbStates[this];

  // the key Y tries the rune first, then the strength: the way in the hands only (the rune known by the scripts is hidden
  // for the weapon, the strength for the rune), as the player has chosen it by what he has drawn
  auto* runes = vm.find_symbol_by_name("PLAYER_TALENT_RUNES");
  auto* force = vm.find_symbol_by_name("RX_FORCEPICKLOCK");
  const bool runesOk = runes!=nullptr && runes->type()==zenkit::DaedalusDataType::INT && !runes->is_member() && runes->count()>99;
  const bool forceOk = force!=nullptr && force->type()==zenkit::DaedalusDataType::INT && !force->is_member();
  const int32_t runeWas  = runesOk ? runes->get_int(99) : 0;
  const int32_t forceWas = forceOk ? force->get_int() : 0;
  if(mode==1 && runesOk)
    runes->set_int(0,99);
  if(mode==2 && forceOk)
    force->set_int(0);

  const char* key = mode==3 ? "KEY_BREAKCHEST_DEX" : "KEY_BREAKCHEST_STR";
  nbCtx("lock: ",key," (mode ",mode,", turns ",int(mob.pickLockCode().size()),")");
  st.focusMob   = &mob;
  st.keyPressed = key;
  try {
    vm.call_function<void>(fn,int32_t(0));
    }
  catch(const std::exception& e) {
    Log::e("NB: HandleEvent failed: ",e.what());
    }
  st.keyPressed.clear();
  if(mode==1 && runesOk)
    runes->set_int(runeWas,99);
  if(mode==2 && forceOk)
    force->set_int(forceWas);
  }

/// NB fix 129: RX_SummonCount - the places of the summoned creatures of the hero. The scripts add to it when they call a
/// creature (a golem 2, a wolf 1...) and compare it with GetSummonCountMax; the plugin takes it away when a creature dies
/// or goes (UNION_ABI.DLL 0x101384b0). Here it is counted again from the living creatures every 0.5 s - the places are the
/// ones that the scripts add for the instance (DK_Pet 4, Sid 1 or 2, a summoned golem 2, the others 1); the wisp and the
/// pets take none
void GameScript::nbSummonCountTick() {
  auto nb = nbStates.find(this);
  if(nb==nbStates.end())
    return;
  auto& st = nb->second;
  if(tickCount()<st.summonNextCount)
    return;
  st.summonNextCount = tickCount()+500;
  auto* var  = vm.find_symbol_by_name("RX_SUMMONCOUNT");
  auto* hero = world().player();
  if(hero==nullptr)
    nbSummonsAlive = 0;
  if(var==nullptr || var->type()!=zenkit::DaedalusDataType::INT || hero==nullptr)
    return;
  static const std::unordered_map<std::string_view,int32_t> places = {
    {"DK_PET", 4},
    {"ELIGOR_DEMON", 2},
    {"ICEWOLF_SUMMONED", 3},
    {"SHADOWBEAST_SKELETON_SUMMON", 2},
    {"SKELETON_SUMMON_SPEAR1", 1},
    {"SKELETON_SUMMON_SPEAR2", 1},
    {"SKELETON_SUMMON_SPEAR3", 1},
    {"SUMKHUBI", 2},
    {"SUMMONED_DEMON", 2},
    {"SUMMONED_DEMONLORD", 2},
    {"SUMMONED_DEMONLORD_UP_01", 2},
    {"SUMMONED_DEMONLORD_UP_02", 2},
    {"SUMMONED_DEMON_LOW", 2},
    {"SUMMONED_DEMON_SCROLL", 2},
    {"SUMMONED_DEMON_UP_01", 2},
    {"SUMMONED_DEMON_UP_02", 2},
    {"SUMMONED_DRAGON", 2},
    {"SUMMONED_FIREGOLEM", 2},
    {"SUMMONED_GOBBO_SKELETON", 1},
    {"SUMMONED_GOBBO_SKELETON_DARK", 1},
    {"SUMMONED_GOLEM", 2},
    {"SUMMONED_GUARDIAN", 2},
    {"SUMMONED_ICEGOLEM", 2},
    {"SUMMONED_SKELETON", 1},
    {"SUMMONED_SKELETONADVANCED", 1},
    {"SUMMONED_SKELETONBOSS", 1},
    {"SUMMONED_SKELETON_ARCHER_1", 1},
    {"SUMMONED_SKELETON_ARCHER_2", 1},
    {"SUMMONED_SKELETON_ARCHER_3", 1},
    {"SUMMONED_SKELETON_SCROLL", 1},
    {"SUMMONED_SKELETON_WARRIOR", 1},
    {"SUMMONED_SKELETON_WARRIORADVANCED", 1},
    {"SUMMONED_SKELETON_WARRIORBOSS", 1},
    {"SUMMONED_SWAMPGOLEM", 2},
    {"SUMMONED_WARG", 1},
    {"SUMMONED_WARG1", 1},
    {"SUMMONED_WARG2", 1},
    {"SUMMONED_WARG3", 1},
    {"SUMMONED_WARG_SCROLL", 1},
    {"SUMMONED_ZOMBIE", 1},
    {"SUMMONED_ZOMBIE_ADVANCED", 1},
    {"SUMMONED_ZOMBIE_BOSS", 1},
    {"TREANT", 2}
    };
  const auto intVar = [this](std::string_view n) {
    auto* s = vm.find_symbol_by_name(n);
    return s!=nullptr && s->type()==zenkit::DaedalusDataType::INT ? s->get_int() : 0;
    };
  const int32_t golemGuild = intVar("GIL_SUMMONED_GOLEM");
  int32_t count = 0, alive = 0;
  world().detectNpc(hero->position(),1.0e6f,[&](Npc& n) {
    if(n.isDead() || n.isPlayer() || !nbIsSummon(n))
      return;
    auto* sym = vm.find_symbol_by_index(uint32_t(n.handle().symbol_index()));
    const std::string_view inst = sym!=nullptr ? std::string_view(sym->name()) : std::string_view();
    if(inst=="WISP_DETECTOR" || inst=="NIXDOG" || inst=="PET_JINA" || inst=="HERO_TOTEM_VOB")
      return;
    ++alive;
    if(inst=="SUMMONED_SKELETONUNIQ") {
      count += intVar("RX_NECROSUMMONERUPSID")!=0 ? 1 : 2;
      return;
      }
    if(auto p = places.find(inst); p!=places.end()) {
      count += p->second;
      return;
      }
    count += (golemGuild!=0 && int32_t(n.guild())==golemGuild) ? 2 : 1;
    });
  nbSummonsAlive = alive;   // NB fix 131
  if(var->get_int()!=count) {
    nbCtx("summon places: ",var->get_int()," -> ",count);
    var->set_int(count);
    }
  }

int32_t GameScript::nbOreStock(const Interactive& mob) const {
  const int32_t kind = nbOreKind(mob);
  if(kind<0)
    return -1;
  const int32_t v = nbOreParse(mob,kind);
  return v<0 ? 1 : v;
  }

/// NB fix 136: the bodies of the dead characters and animals with nothing to take (no item that the looting shows) go away
/// after BodyDelay ms - one sees at once which bodies are still worth a look, and the world has less to draw and to think
/// about. A body with items stays until it is emptied. Nothing goes while the inventory or a dialog is open (the looting of
/// a body holds its inventory), nor the hero, the unconscious, nor a body in a cutscene. The option "Usuwanie pustych ciał"
void GameScript::nbBodiesTick() {
  auto nb = nbStates.find(this);
  if(nb==nbStates.end())
    return;
  auto& st = nb->second;
  static constexpr uint64_t BodyDelay = 2000;   // 2 s (6 s at first: too long)
  if(Gothic::settingsGetI("NB_OG","bRemoveBodies")==0 || nbUiBusy || world().currentCs()!=nullptr) {
    st.bodyEmptySince.clear();
    return;
    }
  auto* hero = world().player();
  if(hero==nullptr)
    return;
  const uint64_t now = tickCount();
  std::unordered_map<const Npc*,uint64_t> seen;
  std::vector<Npc*> gone;
  world().detectNpc(hero->position(),1.0e6f,[&](Npc& n) {
    if(n.isPlayer() || !n.isDead())
      return;
    if(n.inventory().iterator(Inventory::T_Ransack).isValid())
      return;   // something to take
    uint64_t since = now;
    if(auto it = st.bodyEmptySince.find(&n); it!=st.bodyEmptySince.end())
      since = it->second;
    if(now>=since+BodyDelay)
      gone.push_back(&n); else
      seen[&n] = since;
    });
  st.bodyEmptySince = std::move(seen);
  for(auto* n:gone)
    world().removeNpc(*n);
  if(!gone.empty())
    nbCtx("bodies: ",gone.size()," empty removed");
  }

/// NB fix 143: the plugin asks RX_IsCutsceneActive every frame and, on the keys G or R, calls Script_Use_Button_Cutscene
/// (UNION_ABI.DLL 0x100bcd90, 0x10235530): the voyage of the boat of the druid, the concerts, the jail. Asked here 4 times
/// a second, for the button "POMIŃ" of the touch screen
bool GameScript::nbCutsceneActive() {
  auto nb = nbStates.find(this);
  if(nb==nbStates.end())
    return false;
  auto& st = nb->second;
  if(tickCount()<st.cutsceneNextCheck)
    return st.cutsceneActive;
  st.cutsceneNextCheck = tickCount()+250;
  st.cutsceneActive    = false;
  auto* fn = vm.find_symbol_by_name("RX_ISCUTSCENEACTIVE");
  if(fn==nullptr || fn->type()!=zenkit::DaedalusDataType::FUNCTION || world().player()==nullptr)
    return false;
  try {
    st.cutsceneActive = vm.call_function<int32_t>(fn)!=0;
    }
  catch(const std::exception& e) {
    Log::e("NB: RX_IsCutsceneActive failed: ",e.what());
    }
  return st.cutsceneActive;
  }

void GameScript::nbSkipCutscene() {
  auto* fn = vm.find_symbol_by_name("SCRIPT_USE_BUTTON_CUTSCENE");
  if(fn==nullptr || fn->type()!=zenkit::DaedalusDataType::FUNCTION)
    return;
  nbCtx("scene skipped (Script_Use_Button_Cutscene)");
  try {
    vm.call_function<void>(fn);
    }
  catch(const std::exception& e) {
    Log::e("NB: Script_Use_Button_Cutscene failed: ",e.what());
    }
  if(auto nb = nbStates.find(this); nb!=nbStates.end())
    nb->second.cutsceneNextCheck = 0;
  }

/// NB fix 131: RX_SummonMode of the scripts - 0 the summoned creatures attack (active), 1 they attack nobody (passive)
bool GameScript::nbSummonsPassive() {
  auto* mode = vm.find_symbol_by_name("RX_SUMMONMODE");
  return mode!=nullptr && mode->type()==zenkit::DaedalusDataType::INT && mode->get_int()==1;
  }

/// NB fix 131: the choice TRYB of the panel - RX_QuickPanel_SummonMode of the scripts switches RX_SummonMode and writes
/// the mode on the screen (the quick panel of the plugin calls it)
void GameScript::nbSummonsToggleMode() {
  auto* fn = vm.find_symbol_by_name("RX_QUICKPANEL_SUMMONMODE");
  if(fn!=nullptr && fn->type()==zenkit::DaedalusDataType::FUNCTION) {
    try {
      vm.call_function<void>(fn);
      }
    catch(const std::exception& e) {
      Log::e("NB: RX_QuickPanel_SummonMode failed: ",e.what());
      }
    } else {
    auto* mode = vm.find_symbol_by_name("RX_SUMMONMODE");
    if(mode!=nullptr && mode->type()==zenkit::DaedalusDataType::INT)
      mode->set_int(mode->get_int()==1 ? 0 : 1);
    }
  nbCtx("summons: mode ",nbSummonsPassive() ? "passive" : "active");
  }

/// NB fix 181: the state of the transformation of the hero, which the plugin keeps (UNION_ABI.DLL 0x10223340): the spells of
/// the transformation (SPELL_LOGIC_TRF*) set HeroTransOpen and allow no new one while it is set, the plugin clears it.
///  - the hero became a creature: HEROTRANS = 1, HeroTransOpen = 0, the counters and the shields 0, TempTransTime = the
///    intellect + 2 * the level + 120 (the seconds of a scroll, B_GetTime counts HeroTRANSCOUNT and warns after them);
///  - the hero is human again: HEROTRANS, the counters, HEROTRANSRUNE / HeroTransScroll, the poison and the bleeding 0,
///    RX_ResetAnimsOnLoad, the sprint of before (RX_SaveSprintTime) back;
///  - HeroTransOpen 3 s without a transformation (the spell did not take): all 0, the bloodfly of the day not counted.
/// Without it the first transformation was the only one: HeroTransOpen stayed and every later spell stopped at once.
// NB fix 185: the plugin calls RX_AST_UpdateMonsterStats for every npc of the world once a second (0x1023c1a0, SELF = the
// npc). The scripts (RX_ApplyMonsterStats) give the npc the life, the protection and the strength of the difficulty, once:
// AIVAR[89] says it is done. On Nightmare (RX_IsLeg) 150% of the life, RX_MonsterProtRatio 150, RX_MonsterPower 200;
// on the difficulty "Gothic" the options bOptionMonsterHealth/Power/Prot. Without this call the npcs had the life of
// their instance (Im'Arah: 25000 instead of 37500 on Nightmare)
// NB fix 189: the mages of New Balance teleport near their enemy (UNION_ABI.DLL 0x102542d0, every 500 ms; the chosen place
// 0x10253b40, the teleport itself is the special attack 11, 0x1007b230). Only the npcs of the list of the plugin
// (0x10253fc0), in ZS_Attack/ZS_MM_Attack with the weapon out, an enemy that lives at most 30 m away and no special
// attack going on (AIVRX_SPECIAL_ATTACK). The enemy seen: 8 checks (4 s), not seen: 4 checks (2 s); then the teleport,
// when the npc runs (the plugin: S_MAGRUN, S_1HRUN, the strafes). Im'Arah not while ImarahHeals is set
void GameScript::nbMageBlinkTick() {
  static const char* names[] = {
    "IMARAH","SKELETON_ARCHEMAGEDUNGEON","MADNESS_CONJURER","KDF_511_DARON","RIGELLION","DEMENTOR_UD",
    "DMT_DEMENTOR_SCOUTCAV_01","PAL_6104_RITTER_MAGE","AD_BOSS6","AL_BOSS7","SAURON_INT",
    };
  auto* zsA  = vm.find_symbol_by_name("ZS_ATTACK");
  auto* zsMM = vm.find_symbol_by_name("ZS_MM_ATTACK");
  auto* imar = vm.find_symbol_by_name("IMARAH");
  std::set<uint32_t> ids;
  for(auto* n:names)
    if(auto* sym = vm.find_symbol_by_name(n); sym!=nullptr && sym->type()==zenkit::DaedalusDataType::INSTANCE)
      ids.insert(sym->index());
  if(ids.empty() || (zsA==nullptr && zsMM==nullptr))
    return;
  constexpr int32_t AIVRX_SPECIAL_ATTACK = 100;
  auto& st = nbStates[this];
  auto& w  = world();
  for(uint32_t i=0; i<w.npcCount(); ++i) {
    auto* npc = w.npcById(i);
    if(npc==nullptr || npc->isPlayer() || npc->isDead() || npc->isDown())
      continue;
    const uint32_t inst = npc->handle().symbol_index();
    if(ids.find(inst)==ids.end())
      continue;
    if(imar!=nullptr && inst==imar->index() && nbIntVar("IMARAHHEALS")>0)
      continue;
    if(npc->weaponState()==WeaponState::NoWeapon)
      continue;
    if(!((zsA!=nullptr && npc->isInState(zsA->index())) || (zsMM!=nullptr && npc->isInState(zsMM->index()))))
      continue;
    Npc* t = npc->target();
    if(t==nullptr || t->isDead() || npc->newBalanceGetNpcVar(AIVRX_SPECIAL_ATTACK)!=0)
      continue;
    const float dist = (t->position()-npc->position()).length();
    if(dist>3000.f)
      continue;
    auto& c = st.blinkCount[npc];
    if(npc->canSeeNpc(*t,true)) {
      if(++c.first<8)
        continue;
      } else {
      if(++c.second<4)
        continue;
      }
    const auto bs = npc->bodyStateMasked();
    if(bs!=BS_RUN && bs!=BS_WALK)
      continue;   // the plugin teleports a mage that runs to its enemy
    if(nbMageBlink(*npc,*t,dist)) {
      c = {0,0};
      // the enemy far away without a bow or a spell: the next teleport later (the plugin: -(10..20) and -(5..10))
      if(dist>2000.f && t->weaponState()!=WeaponState::Bow && t->weaponState()!=WeaponState::CBow &&
         t->weaponState()!=WeaponState::Mage)
        c = {-(10+int32_t(rand(11))),-(5+int32_t(rand(6)))};
      }
    }
  }

bool GameScript::nbMageBlink(Npc& npc, Npc& target, float dist) {
  auto* phys = world().physic();
  if(phys==nullptr)
    return false;
  const Tempest::Vec3 np = npc.position(), tp = target.position();
  // the distance of the jump (0x10253b40): near the enemy 500-1000 (30%: 1000-1500) along its view, otherwise along the way
  // to it 500-1500 (far) or 700-1200
  float rMin = 700, rMax = 1200, sideMin = 0, sideMax = 25;
  Tempest::Vec3 dir = tp-np;
  if(dist<700.f) {
    const float a = target.rotationRad();
    dir = Tempest::Vec3(std::cos(a),0,std::sin(a));
    if(rand(101)>70) { rMin = 1000; rMax = 1500; } else { rMin = 500; rMax = 1000; }
    if(rand(2)==1) { sideMin = 30; sideMax = 90; }
    } else
  if(dist>1500.f) {
    rMin = 500; rMax = 1500;
    }
  dir.y = 0;
  if(dir.length()<1.f)
    return false;
  dir = dir/dir.length();
  const Tempest::Vec3 center = np+Tempest::Vec3(0,200,0);
  for(int attempt=0; attempt<15; ++attempt) {
    float off = (sideMin+float(rand(uint32_t(sideMax-sideMin)+1u)))*float(M_PI)/180.f;
    if(rand(2)==0)
      off = -off;
    const float cs = std::cos(off), sn = std::sin(off);
    const Tempest::Vec3 d(dir.x*cs-dir.z*sn,0,dir.x*sn+dir.z*cs);
    const float r = rMin+float(rand(uint32_t(rMax-rMin)+1u));
    const Tempest::Vec3 p = center+d*r;
    if((p-tp).length()>=3500.f)
      continue;
    // NB fix 190: the ground under the place at the height of the mage or of its enemy (the ray from above both - not from
    // inside a hill, not onto a roof); room for the body over it. Before the mage stood on a high place and fell (BS_FALL, BS_LIE)
    const float top = std::max(np.y,tp.y)+250.f;
    auto land = phys->landRay(Tempest::Vec3(p.x,top,p.z),top-std::min(np.y,tp.y)+400.f);
    if(!land.hasCol || (std::abs(land.v.y-np.y)>300.f && std::abs(land.v.y-tp.y)>300.f))
      continue;
    const Tempest::Vec3 dst = land.v;
    if(phys->ray(dst+Tempest::Vec3(0,40,0),dst+Tempest::Vec3(0,190,0)).hasCol)
      continue;
    // the enemy sees the place
    if(phys->ray(tp+Tempest::Vec3(0,150,0),dst+Tempest::Vec3(0,150,0)).hasCol)
      continue;
    nbMageBlinkStart(npc,target,dst);
    return true;
    }
  return false;
  }

// NB fix 190: the special attack 11 of the plugin (0x1007b230): the mage plays S_TESTSHOCKWAVE (or T_WARN), the effect of the
// teleport shows at the place it goes to and where it stands, it moves between 45% and 65% of the animation (here 55%),
// faces its enemy; AIVRX_SPECIAL_ATTACK is 11 until the animation ends
void GameScript::nbMageBlinkStart(Npc& npc, Npc& target, const Tempest::Vec3& dst) {
  constexpr int32_t AIVRX_SPECIAL_ATTACK = 100;
  const auto np = npc.position();
  for(auto* name:{"SPELLFX_TELEPORT_RING","SPELLFX_TELEPORT_CAST"}) {
    if(const VisualFx* fx = Gothic::inst().loadVisualFx(name)) {
      for(auto& at:{dst,np}) {
        Effect e(*fx,world(),at);
        e.setActive(true);
        world().runEffect(std::move(e));
        }
      }
    }
  npc.setDirection(target.position()-np);
  const Animation::Sequence* sq = npc.playAnimByName("S_TESTSHOCKWAVE",BS_NONE);
  if(sq==nullptr)
    sq = npc.playAnimByName("T_WARN",BS_NONE);
  const uint64_t len = sq!=nullptr ? uint64_t(std::max(sq->totalTime(),400.f)) : 900;
  auto& st = nbStates[this];
  NbState::BlinkJob job;
  job.npc    = npc.handlePtr();
  job.target = target.handlePtr();
  job.dst    = dst;
  job.teleAt = tickCount()+len*55/100;
  job.endAt  = tickCount()+len;
  st.blinkJobs.push_back(job);
  npc.newBalanceSetNpcVar(AIVRX_SPECIAL_ATTACK,11);
  nbCtx("blink: ",npc.displayName()," teleports ",int((dst-np).length())," cm (to ",int((dst-target.position()).length()),
        " cm from ",target.isPlayer() ? std::string_view("the hero") : target.displayName(),"), animation ",
        sq!=nullptr ? int(len) : -1," ms");
  }

void GameScript::nbMageBlinkJobs() {
  constexpr int32_t AIVRX_SPECIAL_ATTACK = 100;
  auto& jobs = nbStates[this].blinkJobs;
  if(jobs.empty())
    return;
  const uint64_t now = tickCount();
  for(size_t i=0; i<jobs.size(); ) {
    auto& j   = jobs[i];
    auto  ref = j.npc.lock();
    Npc*  npc = ref!=nullptr ? findNpc(ref) : nullptr;
    if(npc==nullptr || npc->isDead() || npc->isDown()) {
      if(npc!=nullptr)
        npc->newBalanceSetNpcVar(AIVRX_SPECIAL_ATTACK,0);
      jobs.erase(jobs.begin()+ptrdiff_t(i));
      continue;
      }
    if(!j.moved && now>=j.teleAt) {
      j.moved = true;
      npc->setPosition(j.dst);
      auto tr = j.target.lock();
      if(Npc* t = tr!=nullptr ? findNpc(tr) : nullptr)
        npc->setDirection(t->position()-j.dst);
      }
    if(now>=j.endAt) {
      npc->newBalanceSetNpcVar(AIVRX_SPECIAL_ATTACK,0);
      jobs.erase(jobs.begin()+ptrdiff_t(i));
      continue;
      }
    ++i;
    }
  }

// NB fix 192: the special attacks of Im'Arah (UNION_ABI.DLL 0x1007fe60, the timer of her tactic). Once a second
// AIVRX_SPECIAL_ATTACK_TIMER (103) counts up and AIVRX_SPECIAL_ATTACK_COOLDOWN (102) down; with no cooldown and no attack
// going on, every N-th second she chooses one (AIVRX_SPECIAL_ATTACK 100, AIVRX_SPECIAL_ATTACK_STAGE 101 = 1):
//  - life at most 50%: 35% the lightning (2), 35% the fire bolts of her spell (3), 30% the fan of fire bolts (1); N 8..19;
//  - life at most 75%: 60% the lightning, 40% the fan; N 10..20;
//  - otherwise the fan; N 10..20. The first N is 15..30.
// The plugin: Im'Arah of the guild DMT with her enemy in the fight. Here as for the teleport (fix 189): ZS_Attack, the
// weapon (the spell) out, the enemy lives; not during a teleport and not while she heals (ImarahHeals)
void GameScript::nbImarahTick() {
  constexpr int32_t SA = 100, SA_STAGE = 101, SA_COOLDOWN = 102, SA_TIMER = 103;
  auto* imar = vm.find_symbol_by_name("IMARAH");
  auto* zsA  = vm.find_symbol_by_name("ZS_ATTACK");
  if(imar==nullptr || imar->type()!=zenkit::DaedalusDataType::INSTANCE || zsA==nullptr)
    return;
  auto& st = nbStates[this];
  auto& w  = world();
  for(uint32_t i=0; i<w.npcCount(); ++i) {
    auto* npc = w.npcById(i);
    if(npc==nullptr || npc->isPlayer() || npc->isDead() || npc->isDown())
      continue;
    if(npc->handle().symbol_index()!=imar->index())
      continue;
    Npc* t = npc->target();
    if(t==nullptr || t->isDead() || npc->weaponState()==WeaponState::NoWeapon || !npc->isInState(zsA->index()))
      continue;
    if(st.imarahEvery<=0)
      st.imarahEvery = 15+int32_t(rand(16));
    const int32_t timer = npc->newBalanceGetNpcVar(SA_TIMER);
    if(npc->newBalanceGetNpcVar(SA_COOLDOWN)==0 && npc->newBalanceGetNpcVar(SA_STAGE)==0 &&
       npc->newBalanceGetNpcVar(SA)==0 && timer!=0 && timer%st.imarahEvery==0 && nbIntVar("IMARAHHEALS")<=0) {
      const int32_t hp  = npc->attribute(ATR_HITPOINTS);
      const int32_t max = std::max(1,npc->attribute(ATR_HITPOINTSMAX));
      int32_t attack = 1;
      if(hp<=max*50/100) {
        const int32_t r = int32_t(rand(100));
        attack = r<35 ? 2 : (r<70 ? 3 : 1);
        st.imarahEvery = 8+int32_t(rand(12));
        }
      else if(hp<=max*75/100) {
        attack = int32_t(rand(101))<=60 ? 2 : 1;
        st.imarahEvery = 10+int32_t(rand(11));
        }
      else {
        st.imarahEvery = 10+int32_t(rand(11));
        }
      nbImarahStart(*npc,*t,attack);
      }
    npc->newBalanceSetNpcVar(SA_TIMER,timer+1);
    if(const int32_t cd = npc->newBalanceGetNpcVar(SA_COOLDOWN); cd>0)
      npc->newBalanceSetNpcVar(SA_COOLDOWN,cd-1);
    }
  }

/// NB fix 192: the damage of a special attack (0x1006f4f0): AIVRX_SPECIAL_ATTACK_DAMAGE_0x; with RX_MonsterPower of at
/// least 150 (Nightmare 200) it is the damage * power/100 * 0.75
static int32_t nbSpecialDamage(int32_t base, int32_t power) {
  const float k = float(power)/100.f;
  if(k<1.5f)
    return base;
  return int32_t(double(int32_t(float(base)*k))*0.75);
  }

void GameScript::nbImarahStart(Npc& npc, Npc& target, int32_t attack) {
  constexpr int32_t SA = 100, SA_STAGE = 101;
  auto& st = nbStates[this];
  NbState::ImarahJob job;
  job.npc    = npc.handlePtr();
  job.target = target.handlePtr();
  job.attack = attack;
  job.state  = 1;
  job.start  = tickCount();
  job.until  = job.start+10000;   // the plugin gives up a step after 10 s
  if(attack==2) {
    // 1..3 lightnings, 2.5 s one after another; the wave 2 s after the flash (1.3..2 s when the enemy is over 20 m away)
    const float   dist = (target.position()-npc.position()).length();
    const int32_t num  = 1+int32_t(rand(3));
    for(int32_t i=0; i<num; ++i) {
      NbState::ImarahStrike s;
      s.startAt = job.start+uint64_t(i)*2500;
      s.waveAt  = uint64_t(dist>2000.f ? 13+int32_t(rand(8)) : 20)*100;   // the delay, here until the flash
      s.until   = s.startAt+10000;
      job.strikes.push_back(s);
      }
    }
  npc.newBalanceSetNpcVar(SA,attack);
  npc.newBalanceSetNpcVar(SA_STAGE,2);
  npc.clearAiQueue();
  st.imarahJobs.push_back(std::move(job));
  static const char* names[] = {"","the fan of fire bolts","the lightning","the bolts of her spell"};
  nbCtx("imarah: special attack ",attack," (",names[attack],"), life ",npc.attribute(ATR_HITPOINTS),"/",
        npc.attribute(ATR_HITPOINTSMAX),", next choice after ",st.imarahEvery," s");
  }

void GameScript::nbImarahEnd(Npc& npc, int32_t cooldown, bool resetTimer) {
  constexpr int32_t SA = 100, SA_STAGE = 101, SA_COOLDOWN = 102, SA_TIMER = 103;
  npc.newBalanceSetNpcVar(SA_STAGE,0);
  npc.newBalanceSetNpcVar(SA_COOLDOWN,cooldown);
  if(resetTimer)
    npc.newBalanceSetNpcVar(SA_TIMER,0);
  npc.newBalanceSetNpcVar(SA,0);
  }

void GameScript::nbShootDir(Npc& src, const Tempest::Vec3& from, const Tempest::Vec3& dir, float range,
                            const VisualFx& vfx, int32_t damage, int32_t damageType) {
  auto& st = nbStates[this];
  NbState::Shot sh;
  sh.vfx    = &vfx;
  sh.src    = src.handlePtr();
  sh.pos    = from;
  sh.damage = damage;
  sh.type   = damageType;
  float vel = vfx.emTrjEaseVel;
  if(auto* k = vfx.key(SpellFxKey::Cast); k!=nullptr && k->emTrjEaseVel>0)
    vel = k->emTrjEaseVel;
  sh.speed  = std::clamp(vel>0 ? vel/1000.f : 1.f, 0.3f, 3.f);
  const float len = dir.length();
  sh.dir    = len>0.001f ? dir/len : Tempest::Vec3(std::cos(src.rotationRad()),0,std::sin(src.rotationRad()));
  sh.left   = range;
  sh.until  = tickCount()+uint64_t(sh.left/sh.speed)+500;
  sh.fx     = Effect(vfx,world(),sh.pos,SpellFxKey::Cast);
  sh.fx.setActive(true);
  sh.fx.setLooped(true);
  st.shots.push_back(std::move(sh));
  }

/// NB fix 192: the steps of the special attacks of Im'Arah
///  1 (0x1006f630): S_TESTSHOCKWAVE, on her SPELLFX_IMARAH_GREEN or _RED (half and half; red: she turns to her enemy
///    from 10% to 55% of the animation); at 55% a fan of SPELLFX_IMARAH_FIREBOLT from -90 to +94 degrees, every
///    8 degrees, with AIVRX_SPECIAL_ATTACK_DAMAGE_01 of fire each; 3.5 s later the end, the cooldown 10 s;
///  2 (0x10070050): for each lightning S_LIGHTINGATTACK; at 70% of the animation SPELLFX_LIGHTNINGFLASH_HEAVENSRAGE and
///    SPELLFX_SPECIALATTACK02_CLOUD where the enemy stands; the delay later SPELLFX_SPECIALATTACK02_ELECTROWAVE there with
///    AIVRX_SPECIAL_ATTACK_DAMAGE_02 of magic for the enemy (here: when he is less than 7 m from the place). Im'Arah
///    herself less than 7 m from it: she takes the damage less her protection against magic (at least 1 life left),
///    IMARAHCURSE, T_PALELITE, the cloud on her, the attack ends. The end: the cooldown 3 s; 15% (not cursed) attack 3;
///  3 (0x10070860): S_RADIUSATTACK; at 30% five projectiles of her spell (SPELLFX_<spell>, else SPELLFX_IMARAH_FIREBOLT)
///    at her enemy and 1 and 2 steps to the sides (600 cm, less when he is over 15 m away), AIVRX_SPECIAL_ATTACK_DAMAGE_03
///    of fire each; then the end, the cooldown 5 s
void GameScript::nbImarahJobs() {
  auto& jobs = nbStates[this].imarahJobs;
  if(jobs.empty())
    return;
  const uint64_t now   = tickCount();
  const int32_t  power = nbIntVar("RX_MONSTERPOWER");
  const auto playAt = [this](std::string_view name, const Tempest::Vec3& at) {
    if(const VisualFx* fx = Gothic::inst().loadVisualFx(name)) {
      Effect e(*fx,world(),at);
      e.setActive(true);
      world().runEffect(std::move(e));
      }
    };
  const auto playOn = [](Npc& npc, std::string_view name) {
    if(const VisualFx* fx = Gothic::inst().loadVisualFx(name))
      npc.startEffect(npc,*fx);
    };
  const auto anim = [](Npc& npc, std::string_view name) -> uint64_t {
    const Animation::Sequence* sq = npc.playAnimByName(name,BS_NONE);
    return sq!=nullptr ? uint64_t(std::max(sq->totalTime(),300.f)) : 0;
    };
  const auto facing = [](const Npc& npc) {
    const float a = npc.rotationRad();
    return Tempest::Vec3(std::cos(a),0,std::sin(a));
    };
  const auto origin = [](Npc& npc) {
    auto p = npc.mapBone("ZS_RIGHTHAND");
    return p==Tempest::Vec3() ? npc.collosionCenter() : p;
    };

  std::vector<std::pair<std::weak_ptr<zenkit::INpc>,std::weak_ptr<zenkit::INpc>>> chain;
  for(size_t i=0; i<jobs.size(); ) {
    auto& j   = jobs[i];
    auto  ref = j.npc.lock();
    Npc*  npc = ref!=nullptr ? findNpc(ref) : nullptr;
    auto  tr  = j.target.lock();
    Npc*  tgt = tr!=nullptr ? findNpc(tr) : nullptr;
    if(npc==nullptr || npc->isDead() || npc->isDown()) {
      if(npc!=nullptr)
        nbImarahEnd(*npc,0,false);
      jobs.erase(jobs.begin()+ptrdiff_t(i));
      continue;
      }
    bool done = false;
    if(j.attack==1) {
      if(j.state==1) {
        if(tgt!=nullptr)
          npc->setDirection(tgt->position()-npc->position());
        const uint64_t len = anim(*npc,"S_TESTSHOCKWAVE");
        j.red     = rand(2)==0;
        playOn(*npc,j.red ? "SPELLFX_IMARAH_RED" : "SPELLFX_IMARAH_GREEN");
        j.animEnd = now+len;
        j.fireAt  = now+len*55/100;
        j.state   = 2;
        nbCtx("imarah: the fan (",j.red ? "red" : "green","), animation ",len," ms");
        }
      else if(j.state==2) {
        const uint64_t len = j.animEnd-j.start;
        if(j.red && tgt!=nullptr && now>=j.start+len*10/100 && now<j.fireAt)
          npc->setDirection(tgt->position()-npc->position());
        if(now>=j.fireAt) {
          const VisualFx* fx = Gothic::inst().loadVisualFx("SPELLFX_IMARAH_FIREBOLT");
          const int32_t   dmg = nbSpecialDamage(npc->newBalanceGetNpcVar(104),power);
          const auto      from = origin(*npc);
          const auto      fw   = facing(*npc);
          float slope = 0;
          if(tgt!=nullptr) {
            const auto  d = tgt->collosionCenter()-from;
            const float h = std::sqrt(d.x*d.x+d.z*d.z);
            slope = h>100.f ? std::clamp(d.y/h,-0.5f,0.5f) : 0.f;
            }
          int32_t n = 0;
          for(int32_t ang=-90; fx!=nullptr && ang<=94; ang+=8) {
            const float a = float(ang)*float(M_PI)/180.f, cs = std::cos(a), sn = std::sin(a);
            const Tempest::Vec3 d(fw.x*cs-fw.z*sn,slope,fw.x*sn+fw.z*cs);
            nbShootDir(*npc,from,d,3250.f,*fx,dmg,8);   // DAM_FIRE
            ++n;
            }
          nbCtx("imarah: ",n," fire bolts, ",dmg," damage each");
          j.endAt = now+3500;
          j.state = 5;
          }
        }
      else if(j.state==5 && now>=j.endAt) {
        nbImarahEnd(*npc,10,true);
        done = true;
        }
      }
    else if(j.attack==2) {
      bool cursed = false, all = true;
      for(auto& s:j.strikes) {
        if(s.state==3)
          continue;
        all = false;
        if(now>=s.until) {
          s.state = 3;
          continue;
          }
        if(s.state==0 && now>=s.startAt) {
          if(tgt!=nullptr)
            npc->setDirection(tgt->position()-npc->position());
          const uint64_t len   = anim(*npc,"S_LIGHTINGATTACK");
          const uint64_t delay = s.waveAt;
          s.flashAt = now+len*70/100;
          s.waveAt  = s.flashAt+delay;
          s.state   = 1;
          }
        if(s.state==1 && now>=s.flashAt) {
          if(tgt==nullptr || tgt->isDead()) {
            s.state = 3;
            continue;
            }
          s.pos = tgt->position()+Tempest::Vec3(0,50,0);
          playAt("SPELLFX_LIGHTNINGFLASH_HEAVENSRAGE",s.pos);
          playAt("SPELLFX_SPECIALATTACK02_CLOUD",s.pos);
          s.state = 2;
          }
        if(s.state==2 && now>=s.waveAt) {
          s.state = 3;
          playAt("SPELLFX_SPECIALATTACK02_ELECTROWAVE",s.pos);
          const int32_t dmg = nbSpecialDamage(npc->newBalanceGetNpcVar(105),power);
          float tdist = -1;
          if(tgt!=nullptr && !tgt->isDead()) {
            tdist = (tgt->position()-s.pos).length();
            if(tdist<700.f)
              nbVecDamage(*npc,*tgt,dmg,32);   // DAM_MAGIC
            }
          const float self = (npc->position()-s.pos).length();
          nbCtx("imarah: the wave, ",dmg," damage, the enemy ",int(tdist)," cm from it, she ",int(self)," cm");
          if(self<700.f) {
            // her own lightning hits her
            const int32_t hurt = std::max(0,dmg-npc->protection(PROT_MAGIC));
            const int32_t hp   = npc->attribute(ATR_HITPOINTS);
            npc->changeAttribute(ATR_HITPOINTS,hp>hurt ? -hurt : 1-hp,false);
            snd_play("IMARAHCURSE");
            anim(*npc,"T_PALELITE");
            playOn(*npc,"SPELLFX_SPECIALATTACK02_CLOUD");
            nbCtx("imarah: cursed by her own lightning, -",hurt," life");
            cursed = true;
            break;
            }
          }
        }
      if(cursed || all) {
        nbImarahEnd(*npc,3,false);
        if(!cursed && rand(100)<15 && tgt!=nullptr && !tgt->isDead())
          chain.emplace_back(j.npc,j.target);
        done = true;
        }
      }
    else if(j.attack==3) {
      if(j.state==1) {
        if(tgt!=nullptr)
          npc->setDirection(tgt->position()-npc->position());
        const uint64_t len = anim(*npc,"S_RADIUSATTACK");
        j.fireAt = now+len*30/100;
        j.state  = 2;
        }
      else if(j.state==2 && now>=j.fireAt) {
        const VisualFx* fx = nullptr;
        if(auto* w = npc->activeWeapon(); w!=nullptr && w->isSpellOrRune())
          fx = spellVfx(w->spellId());
        if(fx==nullptr)
          fx = Gothic::inst().loadVisualFx("SPELLFX_IMARAH_FIREBOLT");
        if(fx!=nullptr && tgt!=nullptr && !tgt->isDead()) {
          const auto    from = origin(*npc);
          const auto    tp   = tgt->collosionCenter();
          const float   dist = (tp-from).length();
          const int32_t step = dist>1500.f ? std::max(0,600-(int32_t(dist)-1500)/4) : 600;
          const int32_t dmg  = nbSpecialDamage(npc->newBalanceGetNpcVar(106),power);
          Tempest::Vec3 side = tp-from;
          side = Tempest::Vec3(-side.z,0,side.x);
          const float sl = side.length();
          side = sl>1.f ? side/sl : Tempest::Vec3(1,0,0);
          int32_t acc = 0;
          for(int32_t k=0; k<3; ++k) {
            const float off[2] = {float(acc+int32_t(rand(101))), -float((step+int32_t(rand(101)))*k)};
            for(int32_t m=0; m<(k==0 ? 1 : 2); ++m)
              nbShootDir(*npc,from,tp+side*off[m]-from,3250.f,*fx,dmg,8);   // DAM_FIRE
            acc += step;
            }
          nbCtx("imarah: 5 bolts of her spell, ",dmg," damage each, the step ",step," cm");
          }
        nbImarahEnd(*npc,5,true);
        done = true;
        }
      }
    if(!done && j.attack!=2 && now>=j.until+5000) {
      nbImarahEnd(*npc,10,true);
      done = true;
      }
    if(done) {
      jobs.erase(jobs.begin()+ptrdiff_t(i));
      continue;
      }
    ++i;
    }
  for(auto& c:chain) {
    auto a = c.first.lock(), b = c.second.lock();
    Npc* npc = a!=nullptr ? findNpc(a) : nullptr;
    Npc* tgt = b!=nullptr ? findNpc(b) : nullptr;
    if(npc!=nullptr && tgt!=nullptr)
      nbImarahStart(*npc,*tgt,3);
    }
  }

/// NB fix 193: a summoned creature of the hero at a boss. The scripts decide (ZS_MM_Attack_Loop): a summon without
/// BIT_CAN_AT_BOSS (VAR_BIT_MASK, 16), or with it and RX_SummonCount over GetSummonCountMax, that fights a boss (RX_IsBoss)
/// or an immortal npc loses all its life - it goes. The golems get the bit in RX_SetGolemStats when ATR_INTELLECT is
/// at least 300. Here: in the log once for each summon, what the scripts will see
void GameScript::nbSummonBossLog() {
  auto nb = nbStates.find(this);
  auto* hero = world().player();
  if(nb==nbStates.end() || hero==nullptr)
    return;
  auto& seen = nb->second.summonBossLog;
  constexpr int32_t VAR_BIT_MASK = 71, BIT_CAN_AT_BOSS = 16, AIVRX_NPC_LP = 53;
  world().detectNpc(hero->position(),4000.f,[&](Npc& n) {
    if(n.isPlayer() || n.isDead() || !nbIsSummon(n))
      return;
    Npc* t = n.target();
    if(t==nullptr || t->isDead())
      return;
    const bool boss = t->handle().aivar[90]!=0 || t->newBalanceGetNpcVar(AIVRX_NPC_LP)>0;
    if(!boss || seen.find(&n)!=seen.end())
      return;
    seen[&n] = tickCount();
    const bool can = (n.newBalanceGetNpcVar(VAR_BIT_MASK) & BIT_CAN_AT_BOSS)!=0;
    const int32_t cnt = nbIntVar("RX_SUMMONCOUNT"), max = nbIntVar("RX_SUMMONCOUNTMAX");
    nbCtx("summon at a boss: ",n.displayName()," -> ",t->displayName(),", BIT_CAN_AT_BOSS ",can ? 1 : 0,
          ", summons ",cnt,"/",max,", ATR_INTELLECT ",nbIntVar("ATR_INTELLECT"),
          (!can || cnt>max) ? " - the scripts send it away" : " - it may fight the boss");
    });
  if(seen.size()>64)
    seen.clear();
  }

/// NB fix 198: a summoned creature of the hero bitten by an enemy near it while its own target is far or out of sight
/// turns to that enemy. The scripts do it with the change of the target (B_MM_AssessDamage, RX_SetReAttackNpc), but they
/// tell the enemies apart by Hlp_GetInstanceID (AIV_LASTTARGET): a golem that has one desert snake as its target took the
/// bites of another desert snake for the bites of its target and stood, 20 m from that target, until the hero hit the
/// snake that bit it
void GameScript::nbSummonHitBy(Npc& summon, Npc& attacker) {
  if(summon.isPlayer() || attacker.isPlayer() || summon.isDown() || attacker.isDown() || &summon==&attacker)
    return;
  if(!nbIsSummon(summon) || nbIsSummon(attacker) || !summon.isEnemy(attacker))
    return;
  Npc* tg = summon.target();
  if(tg==&attacker)
    return;
  constexpr float Near = 800.f, Far = 1500.f;
  if(summon.qDistTo(attacker)>Near*Near)
    return;
  // NB fix 222: "out of sight" was Npc_CanSeeNpc with the view angle (+-100 degrees): a golem between two dragon snappers
  // did not see the one it fought, behind it at 1-2 m, at each bite of the other one - it took the biter as the target, then
  // the bite of the first one turned it back, and so on: every change starts ZS_MM_Attack anew (the queue cleared, the turn
  // and the blow broken off), so the golem stood among them and hit seldom. Now: the free line of sight (a wall, a rock),
  // a target within the reach of a blow is never "out of sight", and one change of the target in 3 s at most.
  const bool tgGone = tg==nullptr || tg->isDown();
  const bool tgFar  = !tgGone && summon.qDistTo(*tg)>Far*Far;
  const bool tgNear = !tgGone && summon.qDistTo(*tg)<400.f*400.f;
  const bool tgHid  = !tgGone && !tgNear && !summon.canSeeNpc(*tg,true);
  if(!tgGone && !tgFar && !tgHid)
    return;
  auto& sw = nbStates[this].summonSwitch;
  if(sw.size()>256)
    sw.clear();
  const uint64_t now = tickCount();
  if(!tgGone && now<sw[&summon]+3000)
    return;
  sw[&summon] = now;
  if(nbSummonsPassive())
    return;   // the passive mode of the panel: the summons do not fight
  auto* sepHum = vm.find_symbol_by_name("GIL_SEPERATOR_HUM");
  const int32_t sep = sepHum!=nullptr && sepHum->type()==zenkit::DaedalusDataType::INT ? sepHum->get_int() : 16;
  const ScriptFn st = int32_t(summon.guild())<sep ? ZS_Attack : ZS_MM_Attack;
  if(!st.isValid())
    return;
  nbCtx("summon: ",summon.displayName()," turns to ",attacker.displayName()," that hits it; its target ",
        tg!=nullptr ? tg->displayName() : std::string_view("-"),tgGone ? " is down" : tgFar ? " is far" : " is out of sight",
        tg!=nullptr ? std::string(" (")+std::to_string(int(std::sqrt(summon.qDistTo(*tg))/100.f))+" m)" : std::string());
  summon.setTarget(&attacker);
  summon.clearAiQueue();
  summon.aiPush(AiQueue::aiStartState(st,0,&attacker,nullptr,""));
  }

/// NB fix 208: the magic damage of the magic weapons of the hero (UNION_ABI.DLL 0x100dbb50, called by the hook of the
/// damage 0x100dcbf0 for a blow of the hero with a one- or two-handed weapon, not transformed, not at a friend):
///   base of the weapon (a table of the plugin: the blade of the adept 50 ... the blade of Beliar 475; the staves 50 ... 480)
///   + the intelligence by the circle of the magic: no circle 1/8, 1st 1/5 (20%), 2nd 1/4, 3rd 1/2, 4th 70%, 5th and 6th 100%
///   * (0.5 + (5 + the magic power (CountLearnSpell) + the skill of the weapon, 1H or 2H) / 200)
/// minus the protection of the target against magic, added to the blow (when above 0)
int32_t GameScript::nbMagicWeaponDamage(Npc& src, Npc& target) {
  if(!src.isPlayer() || nbIntVar("HEROTRANS")!=0 || int(target.handle().type)==2)
    return 0;
  const auto ws = src.weaponState();
  if(ws!=WeaponState::W1H && ws!=WeaponState::W2H)
    return 0;
  auto* w = src.inventory().activeWeapon();
  if(w==nullptr)
    return 0;
  auto* sym = findSymbol(w->clsId());
  if(sym==nullptr)
    return 0;
  std::string name(sym->name());
  for(auto& c:name)
    c = char(std::toupper(static_cast<unsigned char>(c)));
  static const std::unordered_map<std::string,int32_t> base = {
    {"ITMW_BLADE_ADEPT",50},  {"ITMW_BLADE_MAGE",90},  {"ITMW_BLADE_MAGE_YARK",145}, {"ITMW_BLADE_SPELL",250},
    {"ITMW_BLADE_SENDER",275},{"ITMW_BLADE_STORM",300},{"ITMW_BLADE_ECLIPSE",350},   {"ITMW_BLADE_BELIARCLAW",350},
    {"ITMW_BLADE_BELIARCLAW_SENYAK",410}, {"ITMW_BLADE_BELIAR",475},
    {"ITMW_2H_STAFF_NEWBIE",50}, {"ITMW_STAFF_DRUID_01",50},
    {"ITMW_2H_G3_STAFFDRUID_01",90}, {"ITMW_2H_G3_STAFFFIRE_01",90}, {"ITMW_2H_G3_STAFFWATER_01",90},
    {"ITMW_2H_KMR_BLACKSTAFF_01",90}, {"ITMW_STAFF_DRUID_02",90}, {"ITMW_2H_YARIK",150},
    {"ITMW_ADDON_STAB01",275}, {"ITMW_ADDON_STAB02",275}, {"ITMW_ADDON_STAB03",275}, {"ITMW_ADDON_STAB04",275},
    {"ITMW_STAFF_DRUID_03",275}, {"ITMW_2H_MI_STAFF",300}, {"ITMW_2H_SERPENTSTAFF",325}, {"ITMW_STAFF_CLAW",335},
    {"ITMW_STAFF_CLAWSENYAK",450}, {"ITMW_2H_DRAGONSTAFF_01",410}, {"ITMW_STAFF_BLOOD",480},
    {"ITMW_2H_KMR_DAEMONSTAFF_01",430}, {"ITMW_2H_ORCSTAB",180}, {"ITMW_2H_ORCSTAFF_01",180},
    };
  auto it = base.find(name);
  if(it==base.end())
    return 0;
  const int32_t intel  = nbIntVar("ATR_INTELLECT");
  const int32_t power  = nbIntVar("COUNTLEARNSPELL");
  const int32_t circle = src.talentSkill(TALENT_MAGE);
  int32_t v = it->second;
  switch(circle) {
    case 0:  v += intel/8;         break;
    case 1:  v += intel/5;         break;
    case 2:  v += intel/4;         break;
    case 3:  v += intel/2;         break;
    case 4:  v += intel*70/100;    break;
    default: v += intel;           break;
    }
  if(v<=0)
    return 0;
  const int32_t skill = src.handle().hitchance[w->is2H() ? TALENT_2H : TALENT_1H];
  const int32_t dmg   = int32_t(double(v)*(double(5+power+skill)/200.0+0.5));
  const int32_t prot  = target.protection(PROT_MAGIC);
  if(prot<0)
    return 0;   // immune to magic
  const int32_t add   = std::max(dmg-prot,0);
  static uint64_t lastLog = 0;
  if(tickCount()>lastLog+3000) {
    lastLog = tickCount();
    nbCtx("magic weapon ",name,": base ",it->second,", intelligence ",intel," (circle ",circle,"), magic power ",power,
          ", skill ",skill," -> ",dmg," magic, protection ",prot," -> +",add);
    }
  return add;
  }

/// NB fix 208: the necromancer (0x100611c8 in the hook of the damage 0x1005d440): a blow of a summoned creature goes
/// through a part of the protection against edge and blunt: 5% with RX_NecroSummonBoostDamageLearned, and 5% + the magic
/// power / 2 with RX_NecroRuneAuraActive_02. Returns the percent
int32_t GameScript::nbSummonArmorIgnore(Npc& src) {
  if(!nbIsSummon(src))
    return 0;
  int32_t pen = 0;
  if(nbIntVar("RX_NECROSUMMONBOOSTDAMAGELEARNED")!=0)
    pen += 5;
  if(nbIntVar("RX_NECRORUNEAURAACTIVE_02")!=0)
    pen += 5+nbIntVar("COUNTLEARNSPELL")/2;
  return std::clamp(pen,0,100);
  }

void GameScript::nbMonsterStatsTick() {
  auto* fn = vm.find_symbol_by_name("RX_AST_UPDATEMONSTERSTATS");
  if(fn==nullptr || fn->type()!=zenkit::DaedalusDataType::FUNCTION)
    return;
  auto& w    = world();
  auto* hero = w.player();
  auto  prevSelf = vm.global_self()->get_instance();
  size_t done = 0;
  for(uint32_t i=0; i<w.npcCount(); ++i) {
    auto* npc = w.npcById(i);
    if(npc==nullptr || npc==hero || npc->isDead())
      continue;
    auto& h = npc->handle();
    if(h.aivar[89]!=0)
      continue;   // done already (RX_ApplyMonsterStats sets it; it is in the save)
    vm.global_self()->set_instance(npc->handlePtr());
    try {
      vm.call_function<void>(fn);
      }
    catch(const std::exception& e) {
      Log::e("NB: RX_AST_UpdateMonsterStats failed: ",e.what());
      break;
      }
    if(h.aivar[89]!=0)
      ++done;
    }
  vm.global_self()->set_instance(prevSelf);
  if(done>0)
    nbCtx("monster stats: ",done," npcs (hp ratio ",nbIntVar("RX_MONSTERHPRATIO"),", power ",nbIntVar("RX_MONSTERPOWER"),
          ", prot ",nbIntVar("RX_MONSTERPROTRATIO"),", nightmare ",nbIntVar("RX_NIGHTMARE"),")");
  }

void GameScript::nbTransformTick(uint64_t dt) {
  auto* hero = world().player();
  auto  nb   = nbStates.find(this);
  if(hero==nullptr || nb==nbStates.end())
    return;
  auto& st = nb->second;
  const auto get = [this](std::string_view n) { return nbIntVar(n); };
  const auto set = [this](std::string_view n, int32_t v) {
    if(auto* s = vm.find_symbol_by_name(n); s!=nullptr && s->type()==zenkit::DaedalusDataType::INT && !s->is_member())
      s->set_int(v);
    };
  const auto call = [this](std::string_view n, auto... args) {
    auto* fn = vm.find_symbol_by_name(n);
    if(fn==nullptr || fn->type()!=zenkit::DaedalusDataType::FUNCTION)
      return;
    try {
      if(fn->has_return())
        (void)vm.call_function<int>(fn,args...); else
        vm.call_function<void>(fn,args...);
      }
    catch(const std::exception& e) {
      Log::e("NB: ",n," failed: ",e.what());
      }
    };

  const bool trf = hero->isTransformed();
  if(trf && !st.trfWas) {
    st.trfWas    = true;
    st.trfOpenMs = 0;
    if(get("HEROTRANSRUNE")>0)
      set("HEROTRANSSCROLL",0);
    set("HEROTRANS",1);
    set("HEROTRANSOPEN",0);
    set("HEROTRANSCOUNT",0);
    set("TRANSFORMWARN",0);
    set("RX_BLOODFLYTRANSCHECK",0);
    set("RX_MAGICSHIELDACTIVE",0);
    set("RX_DH_SKILLSHIELDVALUE",0);
    set("RX_VAMP_SKILLMAGIC_BLOODSHIELDDAMAGEMAX",0);
    set("RX_VAMP_SKILLMAGIC_BLOODSHIELDDAMAGE",0);
    const int32_t time = std::max(0,get("ATR_INTELLECT"))+2*hero->handle().level+120;
    set("TEMPTRANSTIME",time);
    nbCtx("transformation: on (scroll ",get("HEROTRANSSCROLL")>0 ? "yes" : "no",", ",time," s)");
    return;
    }
  if(!trf && st.trfWas) {
    st.trfWas = false;
    for(auto n:{"HEROTRANS","HEROTRANSOPEN","HEROTRANSCOUNT","TRANSFORMWARN","TEMPTRANSTIME","HEROTRANSRUNE",
                "HEROTRANSSCROLL","POISONED","RX_HARDPOISONED","PLAYERISBLEEDING"})
      set(n,0);
    call("RX_RESETANIMSONLOAD");
    if(const int32_t sprint = get("RX_SAVESPRINTTIME"); sprint>0) {
      call("RX_APPLYSPRINT",int32_t(0),sprint);
      set("RX_SAVESPRINTTIME",0);
      }
    nbCtx("transformation: off");
    return;
    }
  if(!trf && get("HEROTRANSOPEN")>0 && get("HEROTRANS")<=0) {
    st.trfOpenMs += dt;
    if(st.trfOpenMs<3000)
      return;
    st.trfOpenMs = 0;
    for(auto n:{"HEROTRANS","HEROTRANSOPEN","HEROTRANSRUNE","HEROTRANSCOUNT","TRANSFORMWARN"})
      set(n,0);
    if(get("RX_BLOODFLYTRANSCHECK")==1) {
      set("RX_BLOODFLYTRANSCHECK",0);
      set("RX_BLOODFLYTIMES",std::max(0,get("RX_BLOODFLYTIMES")-1));
      }
    nbCtx("transformation: HeroTransOpen without a transformation for 3 s - cleared");
    return;
    }
  st.trfOpenMs = 0;
  }

/// NB fix 176: the seconds until the summons can be called to the hero again (0 - now)
int32_t GameScript::nbSummonsCallCooldown() {
  auto nb = nbStates.find(this);
  if(nb==nbStates.end())
    return 0;
  return int32_t((nb->second.summonCallCd+999)/1000);
  }

/// NB fix 176: the choice PRZYWOŁAJ of the panel - KEY_SUMMON_TELEPORT of the plugin (UNION_ABI.DLL 0x10235bae,
/// 0x10138ac0): every summoned creature of the hero, wherever it is in the world, stands near him and follows him again
/// (its AI goes back to its start state, "ResetAI() -> FOLLOW" of the plugin). Then 30 s of the time of the game until the
/// next call, 15 s for the necromancer of the way RX_NecroSummonWay == 179935. -1: not yet (the time left)
int32_t GameScript::nbSummonsCall() {
  auto& st   = nbStates[this];
  auto* hero = world().player();
  if(hero==nullptr || hero->isDead())
    return 0;
  if(st.summonCallCd>0)
    return -1;
  std::vector<Npc*> with;
  world().detectNpc(hero->position(),1.0e6f,[&](Npc& n) {
    if(n.isDead() || n.isPlayer() || !nbIsSummon(n))
      return;
    auto* sym = vm.find_symbol_by_index(uint32_t(n.handle().symbol_index()));
    const std::string_view inst = sym!=nullptr ? std::string_view(sym->name()) : std::string_view();
    if(inst=="WISP_DETECTOR" || inst=="HERO_TOTEM_VOB")
      return;
    with.push_back(&n);
    });
  if(with.empty())
    return 0;
  const auto pos = hero->position();
  for(size_t i=0; i<with.size(); ++i) {
    auto&       n = *with[i];
    const float a = hero->rotationRad()+float(M_PI)+(float(i)-float(with.size()-1)/2.f)*0.7f;   // behind the hero
    n.clearAiQueue();
    n.setTarget(nullptr);
    fixNpcPositionNear(n,pos,float(a*180.0/M_PI),250.f);   // NB fix 223: on the floor of the hero
    if(const auto start = n.handle().start_aistate; start!=0)
      n.aiPush(AiQueue::aiStartState(ScriptFn(size_t(start)),0,nullptr,nullptr,""));
    }
  st.summonCallCd = nbIntVar("RX_NECROSUMMONWAY")==179935 ? 15000 : 30000;
  nbCtx("summons: ",with.size()," called to the hero, again in ",int(st.summonCallCd/1000)," s");
  return int32_t(with.size());
  }

/// NB fix 131: the choice ATAK of the panel (RX_Cmd_Enemy of the plugin, which no script calls) - the mode becomes active,
/// and every fighting summoned creature of the hero attacks the enemy in the focus of the hero
void GameScript::nbSummonsAttack(Npc& enemy) {
  auto* hero = world().player();
  if(hero==nullptr || enemy.isDead())
    return;
  if(nbSummonsPassive())
    nbSummonsToggleMode();
  auto* sepHum = vm.find_symbol_by_name("GIL_SEPERATOR_HUM");
  const int32_t sep = sepHum!=nullptr && sepHum->type()==zenkit::DaedalusDataType::INT ? sepHum->get_int() : 16;
  size_t count = 0;
  world().detectNpc(hero->position(),1.0e6f,[&](Npc& n) {
    if(n.isDead() || n.isDown() || n.isPlayer() || &n==&enemy || !nbIsSummon(n))
      return;
    auto* sym = vm.find_symbol_by_index(uint32_t(n.handle().symbol_index()));
    const std::string_view inst = sym!=nullptr ? std::string_view(sym->name()) : std::string_view();
    if(inst=="WISP_DETECTOR" || inst=="NIXDOG" || inst=="PET_JINA" || inst=="HERO_TOTEM_VOB")
      return;
    const ScriptFn st = int32_t(n.guild())<sep ? ZS_Attack : ZS_MM_Attack;
    if(!st.isValid())
      return;
    n.setTarget(&enemy);
    n.clearAiQueue();
    n.aiPush(AiQueue::aiStartState(st,0,&enemy,nullptr,""));
    ++count;
    });
  Gothic::inst().onPrint(count>0 ? "Przyzwańce: atak!" : "Przyzwańce: nikt nie może zaatakować");
  nbCtx("summons: ",count," attack ",enemy.displayName());
  }

/// NB fix 131: the summoned creatures go away (RX_ResetSummons of the plugin, the choice RESET of the panel, the change of
/// the world); the pets stay, NIXDOG and PET_JINA
// NB fix 164: the potion of the protection against the next blow (ItPo_HitPotion: RX_NextHitIgnore = TRUE). The plugin of
// NB (UNION_ABI.DLL 0x1013AB90) takes the next blow on the hero away: the flag goes back to 0, the text sPotionHitProtected,
// the effect SPELLFX_HEAL on the hero. The port did not have it - the potion did nothing
bool GameScript::nbNextHitIgnore(Npc& hero) {
  if(!hero.isPlayer())
    return false;
  auto* flag = vm.find_symbol_by_name("RX_NEXTHITIGNORE");
  if(flag==nullptr || flag->type()!=zenkit::DaedalusDataType::INT || flag->is_member() || flag->get_int()!=1)
    return false;
  if(nbIntVar("RX_IMMORTALSTATE")==1)
    return false;
  flag->set_int(0);
  if(auto txt = NbTexts::get("sPotionHitProtected"); !txt.empty())
    Gothic::inst().onPrintScreen(txt,-1,40,2,Resources::font(1.f));
  if(const VisualFx* vfx = Gothic::inst().loadVisualFx("SPELLFX_HEAL")) {
    Effect e(*vfx,world(),hero,SpellFxKey::Cast);
    e.setActive(true);
    hero.runEffect(std::move(e));
    }
  nbCtx("hit potion: the blow taken away");
  return true;
  }

void GameScript::nbVecDamage(Npc& src, Npc& dst, int32_t damage, int32_t damageType) {
  if(damage<=0 || dst.isDead())
    return;
  if(nbNextHitIgnore(dst))   // NB fix 164
    return;
  int32_t prot = 0;
  for(int i=0; i<int(zenkit::DamageType::NUM); ++i)
    if(damageType & (1<<i))
      prot = std::max(prot,dst.protection(Protection(i)));
  if(prot<0)
    return;   // immune
  int32_t minimal = 0;
  if(auto* m = vm.find_symbol_by_name("NPC_MINIMAL_DAMAGE"); m!=nullptr && m->type()==zenkit::DaedalusDataType::INT && !m->is_member())
    minimal = m->get_int();
  dst.hurtBy(src,std::max(damage-prot,minimal));
  }

void GameScript::nbShoot(Npc& src, Npc& dst, const VisualFx& vfx, int32_t damage, int32_t damageType) {
  auto& st = nbStates[this];
  NbState::Shot sh;
  sh.vfx    = &vfx;
  sh.src    = src.handlePtr();
  sh.dst    = dst.handlePtr();
  sh.pos    = vfx.emTrjOriginNode.empty() ? src.collosionCenter() : src.mapBone(vfx.emTrjOriginNode);
  sh.damage = damage;
  sh.type   = damageType;
  float vel = vfx.emTrjEaseVel;
  if(auto* k = vfx.key(SpellFxKey::Cast); k!=nullptr && k->emTrjEaseVel>0)
    vel = k->emTrjEaseVel;
  sh.speed  = std::clamp(vel>0 ? vel/1000.f : 1.f, 0.3f, 3.f);
  // NB fix 158: the projectile is aimed once, at the place of the target when it is cast, and flies straight (as in
  // Gothic): a step aside and it flies past. Its way: to the target and 15 m further
  const auto tg = vfx.emTrjTargetNode.empty() ? dst.collosionCenter() : dst.mapBone(vfx.emTrjTargetNode);
  sh.dir    = tg-sh.pos;
  const float len = sh.dir.length();
  sh.dir    = len>1.f ? sh.dir/len : Tempest::Vec3(std::cos(src.rotationRad()),0,std::sin(src.rotationRad()));
  sh.left   = len+1500.f;
  sh.until  = tickCount()+uint64_t(sh.left/sh.speed)+500;
  sh.fx     = Effect(vfx,world(),sh.pos,SpellFxKey::Cast);
  sh.fx.setActive(true);
  sh.fx.setLooped(true);
  st.shots.push_back(std::move(sh));
  }

void GameScript::nbShotsTick(uint64_t dt) {
  auto nb = nbStates.find(this);
  if(nb==nbStates.end() || nb->second.shots.empty())
    return;
  auto&          shots = nb->second.shots;
  const uint64_t now   = tickCount();
  for(size_t i=0; i<shots.size(); ) {
    auto& sh  = shots[i];
    Npc*  src = findNpc(sh.src.lock());
    Npc*  hit  = nullptr;
    bool  gone = now>sh.until || sh.left<=0.f;
    if(!gone) {
      // NB fix 158: the straight flight; it stops at the first character (not the caster) or at the world on its way
      const float stp  = std::min(sh.speed*float(dt),sh.left);
      const auto  next = sh.pos+sh.dir*stp;
      const auto  r    = world().physic()->rayNpc(sh.pos,next,src);
      if(r.npcHit!=nullptr && !r.npcHit->isDead()) {
        hit    = r.npcHit;
        sh.pos = next;
        }
      else if(r.hasCol && r.npcHit==nullptr) {
        sh.pos = r.v;
        gone   = true;
        }
      else {
        sh.pos   = next;
        sh.left -= stp;
        }
      }
    Tempest::Matrix4x4 m;
    m.identity();
    m.translate(sh.pos);
    sh.fx.setObjMatrix(m);
    sh.fx.tick(dt);
    if(hit==nullptr && !gone) {
      ++i;
      continue;
      }
    // the effect of the hit (emFXCollDyn on the character, emFXCollStat on the world or at the end of the way)
    Effect::onCollide(world(),sh.vfx,sh.pos,hit,src,0);
    if(hit!=nullptr && src!=nullptr)
      nbVecDamage(*src,*hit,sh.damage,sh.type);
    sh.fx.setActive(false);
    shots.erase(shots.begin()+ptrdiff_t(i));
    }
  }

void GameScript::nbClearShots() {
  auto nb = nbStates.find(this);
  if(nb!=nbStates.end())
    nb->second.shots.clear();
  }

// NB fix 163: ZS_Attack makes a companion (AIV_PARTYMEMBER or NPCTYPE_FRIEND) that fights a creature not in the focus
// (C_Npc.noFocus, so that the player does not hit him), ZS_Attack_End gives the focus back. A state that is started over it
// without its end (AI_StartState(npc, ..., 0, ""), as AI_OnDead of the shaman of the West Coast does with Fregyal) left
// the companion without the focus for good: no talk. A companion out of ZS_Attack gets his focus back
void GameScript::nbPartyFocusTick() {
  auto* hero = world().player();
  auto* atk  = vm.find_symbol_by_name("ZS_ATTACK");
  if(hero==nullptr || atk==nullptr)
    return;
  constexpr size_t AIV_PARTYMEMBER = 15;
  world().detectNpc(hero->position(),5000.f,[&](Npc& n) {
    auto& h = n.handle();
    if(&n==hero || !h.no_focus || h.aivar[AIV_PARTYMEMBER]==0 || n.isDead())
      return;
    if(n.isInState(ScriptFn(atk->index())))
      return;
    h.no_focus = false;
    nbCtx("focus: ",n.displayName()," can be in the focus again (out of ZS_Attack)");
    });
  }

// NB fix 161: the companions of the hero (AIV_PARTYMEMBER) and his summoned creatures go with him when he teleports (a
// rune, a scroll, a script): his position jumps by more than 30 m from one frame to the next in the same world. Only those
// that were with him (up to 30 m from where he was) come; who waits somewhere else stays there
/// NB fix 229: a line of the log for every message of the scripts on the screen, with the functions that showed it - the
/// refusals of NB are such messages ("Hubaxis jest już przyzwany..." of Spell_Logic_SumKhub). The same text once in 10 s.
/// NB fix 229: an animation of the walk (the run, the stand, the turn, the jump, the swim... with any weapon) - not logged
static bool nbIsLocomotion(const std::string& n) {
  if(n.rfind("R_",0)==0 || n.rfind("C_",0)==0)
    return true;   // the random moves at the stand, the looks
  static const char* weap[] = {"FIST","1H","2H","BOW","CBOW","MAG"};
  static const char* loco[] = {"RUN","WALK","SNEAK","STAND","TURN","JUMP","FALL","SWIM","DIVE","STRAFE","WATCH","AIM"};
  size_t pos = 0;
  bool   any = false;
  while(pos<=n.size()) {
    size_t e = n.find('_',pos);
    if(e==std::string::npos)
      e = n.size();
    std::string_view t(n.data()+pos,e-pos);
    pos = e+1;
    if(t.empty() || t=="S" || t=="T" || t=="2")
      continue;
    for(auto w:weap)
      if(t.rfind(w,0)==0 && t.size()>std::strlen(w)) {
        t.remove_prefix(std::strlen(w));
        break;
        }
    bool ok = false;
    for(auto l:loco)
      if(t.rfind(l,0)==0)
        ok = true;
    if(!ok)
      return false;
    any = true;
    }
  return any;
  }

// NB fix 230: the lines that wait to be written - the messages of one function shown at once are one line ("a | b | c"),
// e.g. the 12 lines of the stats of Hubaxis (PRINT_DEMONSTATS)
static std::string nbPrintPendTag, nbPrintPendText, nbPrintPendChain;
static uint64_t    nbPrintPendAt = 0;

void GameScript::nbPrintFlush(bool force) {
  if(nbPrintPendText.empty())
    return;
  if(!force && Application::tickCount()<nbPrintPendAt+150)
    return;
  if(nbPrintPendChain=="?" || nbPrintPendChain.empty())
    Log::i(nbPrintPendTag,nbPrintPendText); else
    Log::i(nbPrintPendTag,nbPrintPendText," (",nbPrintPendChain,")");
  nbPrintPendText.clear();
  }

void GameScript::nbPrintLog(zenkit::DaedalusVm& vm, std::string_view text, const char* tag) {
  if(text.size()<=2)
    return;   // the glyphs of the icons of PrintStatusBars
  static std::unordered_map<std::string,uint64_t> seen;
  if(seen.size()>2000)
    seen.clear();
  const uint64_t now = Application::tickCount();
  std::string key = std::string(tag)+std::string(text);
  auto it = seen.find(key);
  // NB fix 230: the time of the last showing, also of a left out one - a text that stays on the screen (shown again on
  // every frame, as the count of the arrows of PRINT_ARROW_COUNT) is in the log once, not every 10 s
  const bool again = (it!=seen.end() && now<it->second+10000);
  seen[key] = now;
  if(again)
    return;
  // at most 20 lines in 10 s (a function of the scripts that shows a counter every second)
  static uint64_t window = 0;
  static int      lines  = 0, skipped = 0;
  if(now>window+10000) {
    if(skipped>0)
      Log::i("[print] ... ",skipped," more messages left out");
    window  = now;
    lines   = 0;
    skipped = 0;
    }
  const std::string chain = vm.call_chain();
  if(!nbPrintPendText.empty() && nbPrintPendTag==tag && nbPrintPendChain==chain && now<nbPrintPendAt+100) {
    nbPrintPendText += " | ";
    nbPrintPendText += text;
    nbPrintPendAt    = now;
    return;
    }
  nbPrintFlush(true);
  if(++lines>20) {
    ++skipped;
    return;
    }
  nbPrintPendTag   = tag;
  nbPrintPendText  = std::string(text);
  nbPrintPendChain = chain;
  nbPrintPendAt    = now;
  }

/// NB fix 229: the watch of the hero (the idea of the player: "a logger on the player"). Every 0.5 s: the guild, the
/// weapon in the hands (the rune, the spell), the armor and the weapons at the belt, the transformation, the summoned
/// creatures - a line when one of them changes. The animations of the actions (not the walk, the run, the stand), at most
/// 20 lines in 10 s. Every 2 s: the variables of the scripts and the aivars of the hero that changed (the ones that change
/// all the time - the timers, the counters - are left out: 4 times in the first 20 s, more than 8 times in 5 min or more
/// than 15 times in all - NB fix 230).
void GameScript::nbHeroWatchTick() {
  auto nb = nbStates.find(this);
  auto* hero = world().player();
  if(nb==nbStates.end() || hero==nullptr)
    return;
  auto& st = nb->second;
  const uint64_t now = Application::tickCount();
  nbPrintFlush(false);   // NB fix 230

  const auto instName = [this](const Item* it) -> std::string {
    if(it==nullptr)
      return "-";
    auto* sym = vm.find_symbol_by_index(uint32_t(it->clsId()));
    return sym!=nullptr ? sym->name() : std::string("?");
    };

  // the animations of the actions
  {
    std::vector<Pose::LayerInfo> lays;
    hero->nbAnimLayers(lays);
    std::string cur;
    for(auto& l:lays) {
      if(l.seq==nullptr)
        continue;
      const std::string& n = l.seq->name;
      if(nbIsLocomotion(n))
        continue;
      if(!cur.empty())
        cur += " + ";
      cur += n;
      }
    if(!cur.empty() && cur!=st.heroAnim) {
      if(now>st.heroAnimWindow+10000) {
        st.heroAnimWindow = now;
        st.heroAnimLines  = 0;
        }
      if(++st.heroAnimLines<=20)
        Log::i("[hero] anim: ",cur);
      }
    st.heroAnim = cur;
  }

  if(now>=st.heroWatchNext) {
    st.heroWatchNext = now+500;
    const auto report = [](const char* what, std::string& was, const std::string& cur) {
      if(cur==was)
        return;
      if(!was.empty())
        Log::i("[hero] ",what,": ",was," -> ",cur); else
        Log::i("[hero] ",what,": ",cur);
      was = cur;
      };
    auto& inv = const_cast<Inventory&>(hero->inventory());

    report("guild",st.heroGuild,std::to_string(hero->handle().guild)+" (true "+std::to_string(hero->trueGuild())+")");

    static const char* wsName[] = {"none","fist","1h","2h","bow","cbow","magic"};
    const auto ws = uint8_t(hero->weaponState());
    std::string w = ws<7 ? wsName[ws] : "?";
    if(hero->weaponState()==WeaponState::Mage) {
      const Item* sp = inv.activeWeapon();
      w += " "+instName(sp);
      if(sp!=nullptr)
        w += " (spell "+std::to_string(sp->spellId())+")";
      }
    else if(hero->weaponState()!=WeaponState::NoWeapon && hero->weaponState()!=WeaponState::Fist) {
      w += " "+instName(inv.activeWeapon());
      }
    report("in hands",st.heroWeapon,w);
    report("armor",st.heroArmor,instName(inv.currentArmor())+(inv.currentHelmet()!=nullptr ? " + "+instName(inv.currentHelmet()) : std::string()));
    report("melee",st.heroMelee,instName(inv.currentMeleeWeapon()));
    report("ranged",st.heroRange,instName(inv.currentRangedWeapon()));

    std::string trans = "-";
    if(hero->isTransformed()) {
      auto* sym = vm.find_symbol_by_index(hero->handle().symbol_index());
      trans = sym!=nullptr ? sym->name() : std::string("?");
      }
    report("transformation",st.heroTrans,trans);

    // the summoned creatures: the names (the life only in the line of a change)
    std::vector<std::pair<std::string,std::string>> sum;
    // NB fix 230: in the whole world, with the distance from the hero (a summon 50 m away is still summoned: in the log of
    // 25.11 the list "flickered" and "Hubaxis jest już przyzwany..." looked like a mistake)
    for(uint32_t i=0, cnt=world().npcCount(); i<cnt; ++i) {
      auto* n = world().npcById(i);
      if(n==nullptr || n->isDead() || n->isPlayer() || !nbIsSummon(*n))
        continue;
      auto* sym = vm.find_symbol_by_index(n->handle().symbol_index());
      std::string nm = sym!=nullptr ? sym->name() : std::string("?");
      sum.emplace_back(nm,nm+" "+std::to_string(n->attribute(ATR_HITPOINTS))+"/"+std::to_string(n->attribute(ATR_HITPOINTSMAX))+
                       " "+std::to_string(int(std::sqrt(n->qDistTo(*hero))/100.f))+" m");
      }
    std::sort(sum.begin(),sum.end());
    std::string key, full;
    for(auto& i:sum) {
      if(!key.empty()) { key += ", "; full += ", "; }
      key  += i.first;
      full += i.second;
      }
    if(key.empty()) {
      key  = "-";
      full = "-";
      }
    if(key!=st.heroSummons) {
      Log::i("[hero] summons (",int(sum.size()),"): ",full);
      st.heroSummons = key;
      }
    }

  if(now<st.varNext)
    return;
  st.varNext = now+2000;

  // the variables of the scripts
  if(st.varWorld!=&world()) {
    st.varWorld = &world();
    st.varIdx.clear();
    st.varElem.clear();
    for(uint32_t i=0; i<vm.symbols().size(); ++i) {
      auto* sym = vm.find_symbol_by_index(i);
      if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::INT || sym->is_const() || sym->is_member() || sym->is_external())
        continue;
      if(sym->name().find('.')!=std::string::npos || sym->count()==0 || sym->count()>64)
        continue;   // the local variables of the functions
      for(uint32_t e=0; e<sym->count(); ++e) {
        st.varIdx .push_back(i);
        st.varElem.push_back(uint16_t(e));
        }
      }
    st.varLast.assign(st.varIdx.size(),0);
    st.varCnt .assign(st.varIdx.size(),0);
    st.varTotal.assign(st.varIdx.size(),0);
    st.varUp.assign(st.varIdx.size(),0);
    st.varMute.assign(st.varIdx.size(),0);
    for(size_t k=0; k<st.varIdx.size(); ++k)
      st.varLast[k] = vm.find_symbol_by_index(st.varIdx[k])->get_int(st.varElem[k]);
    st.varLearnUntil = now+20000;
    st.varCntStart   = now;
    Log::i("[var] watch: ",int(st.varIdx.size())," variables of the scripts");
    return;
    }

  if(st.varLearnUntil!=0 && now>=st.varLearnUntil) {
    // the end of the first 20 s: the variables that changed 4 times or more (every 2-5 s) are the timers and the counters.
    // NB fix 230: not 2 - a summon and its flag in these 20 s (RX_DEMONHUB_ACTIVE, RX_SUMMONCOUNT) were left out. Before the
    // changes of this tick are written, so the timers do not get one line each first.
    std::string muted;
    int cnt = 0;
    for(size_t k=0; k<st.varIdx.size(); ++k) {
      if(st.varCnt[k]>=4 && !st.varMute[k]) {
        st.varMute[k] = 1;
        if(++cnt<=30) {
          auto* sym = vm.find_symbol_by_index(st.varIdx[k]);
          muted += (muted.empty() ? "" : ", ")+sym->name();
          if(sym->count()>1)
            muted += "["+std::to_string(st.varElem[k])+"]";
          }
        }
      }
    Log::i("[var] left out (change all the time): ",cnt>0 ? muted : std::string("-"),cnt>30 ? " ..." : "");
    st.varLearnUntil = 0;
    std::fill(st.varCnt.begin(),st.varCnt.end(),0);
    st.varCntStart = now;
    }
  const bool learn = now<st.varLearnUntil;
  if(now>st.varCntStart+300000) {
    st.varCntStart = now;
    std::fill(st.varCnt.begin(),st.varCnt.end(),0);
    std::fill(st.aivCnt.begin(),st.aivCnt.end(),0);
    }

  const auto varName = [&](size_t k) {
    auto* sym = vm.find_symbol_by_index(st.varIdx[k]);
    std::string n = sym->name();
    if(sym->count()>1)
      n += "["+std::to_string(st.varElem[k])+"]";
    return n;
    };

  int lines = 0, more = 0;
  for(size_t k=0; k<st.varIdx.size(); ++k) {
    const int32_t v = vm.find_symbol_by_index(st.varIdx[k])->get_int(st.varElem[k]);
    if(v==st.varLast[k])
      continue;
    const int32_t was = st.varLast[k];
    st.varLast[k] = v;
    if(st.varMute[k])
      continue;
    if(st.varCnt[k]<255)
      ++st.varCnt[k];
    if(learn)
      continue;
    // NB fix 230: also the counters of every kill (RX_STATEXP, MONSTERKILLED, RANKPOINTS...): 15 lines in all, then left out
    if(st.varTotal[k]<255)
      ++st.varTotal[k];
    // NB fix 232: a counter (only up: the experience, the kills, the rank) is left out after 6 steps; the summons are never
    // left out (RX_SUMMONCOUNT was, after 9 summons and deaths of golems in 5 min)
    if(v>was) {
      if(st.varUp[k]<255)
        ++st.varUp[k];
      } else {
      st.varUp[k] = 0;
      }
    const std::string vn = varName(k);
    const bool keep = vn.find("SUMMON")!=std::string::npos || vn.find("ISUP")!=std::string::npos ||
                      vn.find("ALIVE")!=std::string::npos  || vn.find("_ACTIVE")!=std::string::npos;
    if(!keep && (st.varCnt[k]>8 || st.varTotal[k]>15 || st.varUp[k]>=6)) {
      st.varMute[k] = 1;
      Log::i("[var] ",vn,": ",st.varUp[k]>=6 ? "a counter" : "changes all the time",", left out");
      continue;
      }
    if(lines<40) {
      Log::i("[var] ",varName(k),": ",was," -> ",v);
      ++lines;
      } else {
      ++more;
      }
    }
  if(more>0)
    Log::i("[var] ... and ",more," more");

  // the variables of the hero himself (aivar), with the names of the constants AIV_* / AIVRX_*
  const auto& h = hero->handle();
  if(st.aivName.empty()) {
    st.aivName.assign(zenkit::INpc::aivar_count,std::string());
    for(uint32_t i=0; i<vm.symbols().size(); ++i) {
      auto* sym = vm.find_symbol_by_index(i);
      if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::INT || !sym->is_const() || sym->count()!=1)
        continue;
      const auto& n = sym->name();
      if(n.rfind("AIV_",0)!=0 && n.rfind("AIVRX_",0)!=0)
        continue;
      const int32_t v = sym->get_int();
      if(v<0 || v>=int32_t(zenkit::INpc::aivar_count))
        continue;
      auto& dst = st.aivName[size_t(v)];
      if(dst.empty())
        dst = n;
      else if(dst.find('/')==std::string::npos)
        dst += "/"+n;
      }
    }
  if(st.aivOwner!=&h) {
    st.aivOwner = &h;
    st.aivLast.assign(h.aivar,h.aivar+zenkit::INpc::aivar_count);
    st.aivCnt .assign(zenkit::INpc::aivar_count,0);
    st.aivTotal.assign(zenkit::INpc::aivar_count,0);
    st.aivMute.assign(zenkit::INpc::aivar_count,0);
    return;
    }
  for(size_t k=0; k<zenkit::INpc::aivar_count; ++k) {
    const int32_t v = h.aivar[k];
    if(v==st.aivLast[k])
      continue;
    const int32_t was = st.aivLast[k];
    st.aivLast[k] = v;
    if(st.aivMute[k])
      continue;
    if(st.aivTotal[k]<255)
      ++st.aivTotal[k];
    if(++st.aivCnt[k]>8 || st.aivTotal[k]>15 || (v==was+1 && st.aivTotal[k]>=6)) {
      st.aivMute[k] = 1;
      Log::i("[hero] aivar[",int(k),"] ",st.aivName[k],": changes all the time, left out");
      continue;
      }
    Log::i("[hero] aivar[",int(k),"] ",st.aivName[k],": ",was," -> ",v);
    }
  }

void GameScript::nbPartyTeleportTick() {
  auto& st   = nbStates[this];
  auto* hero = world().player();
  if(hero==nullptr)
    return;
  const auto pos = hero->position();
  if(st.partyWorld!=&world()) {
    st.partyWorld   = &world();
    st.partyHeroPos = pos;
    return;
    }
  const auto from = st.partyHeroPos;
  st.partyHeroPos = pos;
  if((pos-from).length()<3000.f || hero->isDead())
    return;

  constexpr size_t AIV_PARTYMEMBER = 15;
  std::vector<Npc*> party;
  world().detectNpc(from,3000.f,[&](Npc& n) {
    if(&n==hero || n.isDead() || n.isDown())
      return;
    if(n.handle().aivar[AIV_PARTYMEMBER]!=0 || nbIsSummon(n))
      party.push_back(&n);
    });
  if(party.empty())
    return;
  const float base = hero->rotationRad()+float(M_PI);   // behind the hero, in a half circle
  for(size_t i=0; i<party.size(); ++i) {
    auto&       n = *party[i];
    const float a = base+(float(i)-float(party.size()-1)*0.5f)*0.6f;
    n.clearAiQueue();
    n.setPosition(pos+Tempest::Vec3(std::cos(a)*150.f,20.f,std::sin(a)*150.f));
    n.updateTransform();
    }
  nbCtx("teleport: ",int(party.size())," companions with the hero (",int((pos-from).length()/100.f)," m)");
  }

void GameScript::nbInsertVobTree(std::string_view path) {
  auto& st   = nbStates[this];
  auto& wrld = world();
  nbVobTreesSync(st,wrld);
  const auto file = nbFileName(path);
  for(auto& t:st.vobTrees)
    if(t.world==wrld.name() && t.file==file)
      return;
  NbState::VobTree t;
  t.world = std::string(wrld.name());
  t.file  = file;
  t.vobs  = nbLoadVobTree(wrld,file,Vob::Startup);
  if(t.vobs==nullptr)
    return;
  nbCtx("part of the world: ",file," (",int(t.vobs->size())," objects)");
  st.vobTrees.push_back(std::move(t));
  }

// NB fix 165: the parts of the world that the scripts inserted when the port did not have RX_InsertVobTree yet (before
// fix 56) or did not find: the flag of the script is set, the objects are not there - the dig place of the treasure of the
// note of the dead pirate on the West Coast (RX_WestCoast_Loop: RX_WC_DeadPirateNote 1 -> 2 and the tree)
int32_t GameScript::nbIntVar(std::string_view name) {
  auto* s = vm.find_symbol_by_name(name);
  return (s!=nullptr && s->type()==zenkit::DaedalusDataType::INT && !s->is_member()) ? s->get_int() : -1;
  }

void GameScript::nbRepairVobTrees() {
  struct Repair { const char* level; const char* flag; int32_t value; const char* tree; };
  static const Repair all[] = {
    {"WESTCOAST_ZEN","RX_WC_DEADPIRATENOTE",2,"VOBTREE/VOBTREE_WC_TREASURE_QUEST.ZEN"},
    };
  auto var = [this](const char* name) -> int32_t {
    auto* s = vm.find_symbol_by_name(name);
    return (s!=nullptr && s->type()==zenkit::DaedalusDataType::INT && !s->is_member()) ? s->get_int() : -1;
    };
  auto& st = nbStates[this];
  nbVobTreesSync(st,world());
  for(auto& r:all) {
    if(var("CURRENTLEVEL")!=var(r.level) || var(r.flag)!=r.value)
      continue;
    const auto file = nbFileName(r.tree);
    bool has = false;
    for(auto& t:st.vobTrees)
      if(t.world==world().name() && t.file==file)
        has = true;
    static std::set<std::string> tried;   // once in a world: a missing file is not looked for again and again
    if(has || !tried.insert(std::to_string(reinterpret_cast<uintptr_t>(this))+"/"+std::string(world().name())+"/"+file).second)
      continue;
    Log::i("NB: the part of the world \"",file,"\" was not inserted (",r.flag,"=",r.value,") - inserted now");
    nbInsertVobTree(r.tree);
    }
  }

size_t GameScript::nbResetSummons(bool keepWisp, bool print) {
  auto* hero = world().player();
  if(hero==nullptr)
    return 0;
  std::vector<Npc*> gone;
  world().detectNpc(hero->position(),1.0e6f,[&](Npc& n) {
    if(n.isPlayer() || !nbIsSummon(n))
      return;
    auto* sym = vm.find_symbol_by_index(uint32_t(n.handle().symbol_index()));
    const std::string_view inst = sym!=nullptr ? std::string_view(sym->name()) : std::string_view();
    if(inst=="NIXDOG" || inst=="PET_JINA" || inst=="HERO_TOTEM_VOB" || (keepWisp && inst=="WISP_DETECTOR"))
      return;
    gone.push_back(&n);
    });
  // NB fix 232: the creatures that fought a removed summon lose it as the target (after loading a save they stood with
  // a golem 690 m away as the target: "[AGGRO] ... out of range 69358", until the leash after 6 s)
  if(!gone.empty()) {
    for(uint32_t i=0, cnt=world().npcCount(); i<cnt; ++i) {
      auto* n = world().npcById(i);
      if(n!=nullptr && n->target()!=nullptr && std::find(gone.begin(),gone.end(),n->target())!=gone.end())
        n->setTarget(nullptr);
      }
    }
  for(auto* n:gone)
    world().removeNpc(*n);
  nbSummonsAlive = 0;
  if(auto* var = vm.find_symbol_by_name("RX_SUMMONCOUNT"); var!=nullptr && var->type()==zenkit::DaedalusDataType::INT)
    var->set_int(0);
  if(!gone.empty())
    nbCtx("summons removed: ",gone.size());
  if(print)
    Gothic::inst().onPrint(gone.empty() ? "Przyzwańce: brak" : "Przyzwańce odesłane");
  return gone.size();
  }

/// NB fix 131: the hero is moved far in a moment (a teleport rune, a teleport stone, the console, the map of the mods) -
/// his summoned creatures and pets, near the place he left, come with him and stand around him. In another world (ZEN)
/// they are not taken: GameSession::implChangeWorld sends them away
void GameScript::nbSummonsFollowTick() {
  auto nb = nbStates.find(this);
  if(nb==nbStates.end())
    return;
  auto& st   = nb->second;
  auto* hero = world().player();
  if(hero==nullptr)
    return;
  const auto pos = hero->position();
  if(st.followWorld!=world().serialNumber()) {
    st.followWorld = world().serialNumber();
    st.followPos   = pos;
    return;
    }
  const auto from = st.followPos;
  st.followPos = pos;
  if((pos-from).quadLength()<1200.f*1200.f || hero->isDead())
    return;
  std::vector<Npc*> with;
  world().detectNpc(from,8000.f,[&](Npc& n) {
    if(n.isDead() || n.isPlayer() || !nbIsSummon(n))
      return;
    auto* sym = vm.find_symbol_by_index(uint32_t(n.handle().symbol_index()));
    const std::string_view inst = sym!=nullptr ? std::string_view(sym->name()) : std::string_view();
    if(inst=="HERO_TOTEM_VOB")
      return;
    with.push_back(&n);
    });
  for(size_t i=0; i<with.size(); ++i) {
    auto&       n = *with[i];
    const float a = hero->rotationRad()+float(M_PI)+(float(i)-float(with.size()-1)/2.f)*0.7f;   // behind the hero
    n.clearAiQueue();
    fixNpcPositionNear(n,pos,float(a*180.0/M_PI),250.f);   // NB fix 223: on the floor of the hero
    }
  if(!with.empty())
    nbCtx("summons: ",with.size()," teleported with the hero (",int((pos-from).length()/100.f)," m)");
  }

/// NB fix 128: the trolls of New Balance throw stones - the plugin does it, the scripts know nothing of it
/// (UNION_ABI.DLL 0x103889b0). A troll (GIL_TROLL) that runs at its enemy further than 8.5 m stops now and then and plays
/// T_STONETHROW of its model: further than 12 m with a chance of 2% in a frame of the plugin, further than 8.5 m with 1%;
/// after the throw a pause of 7.5-12.5 s or 10-16 s. The stone (ItTrollStone) flies from the right hand at the enemy -
/// the plugin lets it fall by its physics (0x10389560); here it is a bullet with the blunt damage of the strength of the troll.
/// Not here: the throw of a goblin (T_GOBBOTHROW, a goblin near the troll is thrown) and the speeds of the animation by the
/// distance
void GameScript::nbTrollTick() {
  auto  nb = nbStates.find(this);
  if(nb==nbStates.end())
    return;
  auto&          st   = nb->second;
  auto*          hero = world().player();
  const uint64_t now  = tickCount();
  if(hero==nullptr)
    return;
  auto* gilTroll = vm.find_symbol_by_name("GIL_TROLL");
  if(gilTroll==nullptr || gilTroll->type()!=zenkit::DaedalusDataType::INT)
    return;
  const int32_t troll = gilTroll->get_int();

  // the stones in the air: the moment of the throw in the animation
  for(auto it=st.trolls.begin(); it!=st.trolls.end();) {
    auto& t   = it->second;
    auto  tr  = t.troll.lock();
    auto* npc = tr!=nullptr ? findNpc(tr) : nullptr;
    if(npc==nullptr) {
      it = st.trolls.erase(it);
      continue;
      }
    if(t.releaseAt!=0 && now>=t.releaseAt) {
      t.releaseAt = 0;
      auto  tg  = t.target.lock();
      auto* tgt = tg!=nullptr ? findNpc(tg) : nullptr;
      auto* sym = vm.find_symbol_by_name("ITTROLLSTONE");
      if(tgt!=nullptr && !tgt->isDead() && !npc->isDead() && !npc->isDown() && sym!=nullptr) {
        Item  stone(world(),sym->index(),Item::T_Inventory);
        auto  from = npc->mapBone("BIP01 R HAND");
        if(from==Tempest::Vec3())
          from = npc->mapHeadBone();
        auto& b = world().shootBulletFrom(stone,*npc,from,*tgt);
        b.setOrigin(npc);
        DamageCalculator::Damage dmg = {};
        dmg[zenkit::DamageType::BLUNT] = npc->attribute(ATR_STRENGTH);
        b.setDamage(dmg);
        b.setHitChance(1.f);
        nbCtx("troll throws a stone: ",npc->displayName()," -> ",tgt->displayName());
        }
      }
    ++it;
    }

  // a new throw, ten times a second
  if(now<st.trollNextLook)
    return;
  st.trollNextLook = now+100;
  world().detectNpc(hero->position(),4000.f,[&](Npc& npc) {
    if(npc.isDead() || npc.isDown() || npc.isPlayer() || int32_t(npc.guild())!=troll)
      return;
    auto& t = st.trolls[npc.handlePtr().get()];
    if(t.troll.expired())
      t.troll = npc.handlePtr();
    // NB fix 130: the reason, when a troll near the fight does not throw (every 5 s in the log)
    const auto why = [&](std::string_view r, float d) {
      if(now<t.nextLog)
        return;
      t.nextLog = now+5000;
      nbCtx("troll ",npc.displayName()," does not throw: ",r," (",int(d/100.f)," m)");
      };
    Npc* tgt = npc.target();
    if(tgt==nullptr || tgt->isDead())
      return;
    const float dist = (tgt->position()-npc.position()).length();
    if(t.releaseAt!=0 || now<t.nextTry)
      return;
    if(npc.isInAir())
      return why("in the air",dist);
    if(dist<=850.f)
      return;   // near: it fights with the fists
    if(dist>3000.f)
      return why("the target is too far",dist);
    if(!npc.canSeeNpc(*tgt,true))
      return why("does not see the target",dist);
    if(!npc.hasAnim("T_STONETHROW")) {
      t.nextTry = now+60000;
      return why("its model has no T_STONETHROW",dist);
      }
    // the chance of the plugin is in a frame; here every 100 ms: 2% of a frame (60 frames a second) is about 12%
    const bool far = dist>1200.f;
    if(rand(100)>=(far ? 12 : 6))
      return;
    npc.setDirection(tgt->position());
    auto* sq = npc.playAnimByName("T_STONETHROW",BS_HIT);
    if(sq==nullptr) {
      // NB fix 130: the animation that plays cannot be cut now (a blow, a turn): the throw starts it over, as the plugin
      npc.stopAnim("");
      sq = npc.playAnimByName("T_STONETHROW",BS_HIT);
      }
    if(sq==nullptr)
      return why("the animation did not start",dist);
    // the stone leaves the hand in the middle of the throw (the event TRO_STONETHROW is not read by OpenGothic)
    const uint64_t len = uint64_t(std::max(400.f,sq->totalTime()));
    t.target    = tgt->handlePtr();
    t.releaseAt = now+len*55/100;
    t.nextTry   = now+len+(far ? 7500+uint64_t(rand(5000)) : 10000+uint64_t(rand(6000)));
    nbCtx("troll ",npc.displayName()," starts the throw (",int(dist/100.f)," m)");
    });
  }

/// NB fix 126: RX_DamageCalcRunes of UNION_ABI.DLL (0x10083400): SPL_DAMAGE_* and SPL_COST_* of the spells from the
/// intelligence of the hero (ATR_INTELLECT of the scripts). The value of a spell: f(base, part of the intelligence, the
/// step of the cap) - the part over the threshold of the step grows slower (0x10082190); the heal and the strikes of the
/// paladins and the templars: g(base) - the whole intelligence and the strength or the dexterity (0x100823d0).
/// Not here: the mode of the plugin with the other table (0x10082610, a special version of the mod) and the karma of the
/// gods (varNewKarmaEnabled, not in these scripts). Returns a hash of the values (to know when they change)
uint64_t GameScript::nbDamageCalcRunes() {
  auto* hero = world().player();
  if(hero==nullptr)
    return 0;
  uint64_t hash = 1469598103934665603ull;
  const auto upper = [](std::string_view n) {
    std::string s(n);
    for(auto& c:s)
      c = char(std::toupper(static_cast<unsigned char>(c)));
    return s;
    };
  const auto get = [&](std::string_view n) -> int32_t {
    auto* s = vm.find_symbol_by_name(upper(n));
    return s!=nullptr && s->type()==zenkit::DaedalusDataType::INT && !s->is_member() ? s->get_int() : 0;
    };
  const auto set = [&](std::string_view n, int32_t v) {
    hash = (hash ^ uint64_t(uint32_t(v))) * 1099511628211ull;
    auto* s = vm.find_symbol_by_name(upper(n));
    if(s!=nullptr && s->type()==zenkit::DaedalusDataType::INT && !s->is_member())
      s->set_int(v);
    };

  const int32_t intel  = get("ATR_INTELLECT");
  const int32_t guild  = int32_t(hero->guild());
  const int32_t circle = hero->talentSkill(TALENT_MAGE);   // the circle of the magic
  const bool    robe   = get("MageRobeIsUp")==1;
  const bool    mage   = guild==19 || guild==14 || guild==4 || guild==15 || guild==18 || guild==17 || guild==5;

  // the parts of the intelligence: a - the most spells, b - the dark and the necromancy, c - the others, d - the water
  int32_t a = 0, b = 0, c = 0, d = 0;
  if(mage) {
    if(robe) {
      a = int32_t(double(intel)*0.5);
      c = intel;
      d = int32_t(float(intel)*0.65f);
      b = int32_t(float(intel)*0.75f);
      } else {
      a = b = c = d = intel/4;
      }
    } else {
    if(robe) {
      a = intel/8;
      c = intel/4;
      b = d = intel/6;
      } else {
      a = b = c = d = intel/10;
      }
    }

  // the thresholds of the steps (0x100820c0)
  const bool    g15 = guild==15;
  const int32_t thr[5] = { g15 ? 60 : 45, g15 ? 90 : 70, g15 ? 185 : 155, g15 ? 420 : 300, g15 ? 750 : 550 };
  const int32_t div[5] = { 6, 5, 4, 3, 2 };
  const bool    fullSnc = get("IsFullSnc")==1;
  const int32_t learn   = get("countLearnSpell");
  const bool    curse   = get("PlayerIsCurse")!=0;
  const bool    shaman  = get("RX_IsShamanWay")==0x12f3601;

  const auto f = [&](int32_t base, int32_t x, int32_t step) -> int32_t {
    if(0<=step && step<5 && x>thr[step])
      x = thr[step] + (x-thr[step])/div[step];
    if(fullSnc)
      x = int32_t(float(x)*0.85f);
    int32_t v = base + x + (base+x)*learn/100;
    if(curse)
      v = v*50/100;
    if(shaman)
      v = v*25/100;
    return v;
    };
  const int32_t strDex = std::max(hero->attribute(ATR_STRENGTH),hero->attribute(ATR_DEXTERITY));
  const bool    palDmg = get("RX_PaladinSkill_MagicDamage")!=0;
  const bool    tplStr = get("RX_Tpl_Skill_MagicStr")!=0 && guild==16 && get("RX_IsDarkWayWarrior")==0 &&
                         get("RX_IsDHWay")==0 && get("RX_IsScoutWay")==0 && get("RX_IsVampWay")==0;
  const auto g = [&](int32_t base) -> int32_t {
    const int32_t bb = palDmg ? base+200 : base;
    int32_t v = bb + intel + (bb+intel)*learn/100;
    if(tplStr)
      v += strDex;
    if(curse)
      v = v*50/100;
    if(shaman)
      v = v*25/100;
    return v;
    };
  (void)d;

      set("SPL_DAMAGE_HELPRUNE", f(40, int(float(intel)*0.45f), 0));
      set("SPL_DAMAGE_FIREBOLT", f(45, a, 1));
      set("SPL_COST_FIREBOLT", 12);
      set("SPL_DAMAGE_INSTANTFIREBALL", f(125, a, 2));
      set("SPL_COST_INSTANTFIREBALL", 25);
      set("SPL_DAMAGE_FIRELANCE", f(50, a, 2));
      set("SPL_COST_FIRELANCE", 20);
      set("SPL_DAMAGE_HOLYFIRE", f(100, int(float(intel)*0.5f), 4));
      set("SPL_COST_HOLYFIRE", 25);
      set("SPL_DAMAGE_INSTANTFIRESTORM", f(265, a, 3));
      set("SPL_COST_INSTANTFIRESTORM", 50);
      set("SPL_DAMAGE_FIREWAVE", f(650, a, 4));
      set("SPL_COST_FIREWAVE", 200);
      set("SPL_DAMAGE_CHARGEFIREBALL", f(135, a, 4));
      set("STEP_CHARGEFIREBALL", 30);
      set("SPL_COST_CHARGEFIREBALL", 30);
      set("SPL_DAMAGE_EXPLOSION", f(500, a, 4));
      set("SPL_COST_EXPLOSION", 60);
      set("SPL_DAMAGE_FIRESTORM", f(200, a, 5));
      set("STEP_FIRESTORM", 45);
      set("SPL_COST_FIRESTORM", 45);
      set("SPL_DAMAGE_FIREMETEOR", f(900, a, 6));
      set("SPL_COST_FIREMETEOR", 100);
      set("SPL_DAMAGE_FIRERAIN", f(1000, a, 6));
      set("SPL_COST_FIRERAIN", 350);
      set("SPL_DAMAGE_MASTEROFDISASTER", f(1500, a, 6));
      set("SPL_COST_MASTEROFDISASTER", 80);
      set("SPL_COST_FIRELIGHT", 250);
      set("SPL_COST_ORCFIREBALL", 50);
      set("SPL_DAMAGE_ZAP", f(90, a, 1));
      set("SPL_COST_ZAP", 10);
      set("SPL_DAMAGE_ICEBOLT", f(85, a, 1));
      set("SPL_COST_ICEBOLT", 10);
      set("SPL_DAMAGE_ICELANCE", f(110, a, 2));
      set("SPL_COST_ICELANCE", 20);
      set("SPL_DAMAGE_LIGHTNINGFLASH", f(150, a, 2));
      set("SPL_COST_LIGHTNINGFLASH", 25);
      set("SPL_DAMAGE_CHARGEZAP", f(155, a, 3));
      set("SPL_COST_CHARGEZAP", 25);
      set("STEP_CHARGEZAP", 20);
      set("SPL_DAMAGE_ICEEXPLOISION", f(230, d, 3));
      set("SPL_COST_ICEEXPLOISION", 40);
      set("SPL_DAMAGE_ICECUBE", 0);
      set("SPL_COST_ICECUBE", 75);
      set("SPL_DAMAGE_WATERFIST", f(450, d, 4));
      set("SPL_COST_WATERFIST", 60);
      set("SPL_DAMAGE_LIGHTNINGSPHERE", f(650, a, 4));
      set("SPL_COST_LIGHTNINGSPHERE", 70);
      set("SPL_DAMAGE_GEYSER", f(700, d, 5));
      set("SPL_COST_GEYSER", 80);
      set("SPL_DAMAGE_ELECTROWAVE", f(775, d, 5));
      set("SPL_COST_ELECTROWAVE", 100);
      set("SPL_DAMAGE_ICEWAVE", 0);
      set("SPL_COST_ICEWAVE", 250);
      set("SPL_DAMAGE_THUNDERSTORM", f(1300, a, 6));
      set("SPL_COST_THUNDERSTORM", 350);
      set("SPL_DAMAGE_ICERAIN", f(1100, d, 6));
      set("SPL_COST_ICERAIN", 350);
      set("SPL_COST_ADANOSBALL", 250);
      set("SPL_COST_SUMMONSHOAL", 50);
      set("SPL_COST_RAPIDICEBOLT", 30);
      set("SPL_DAMAGE_BERZERK", f(65, c, 1));
      set("SPL_Damage_Windfist", f(70, c, 2));
      set("SPL_DAMAGE_GREENTENTACLE", f(120, c, 2));
      set("SPL_DAMAGE_STONEFIRST", f(120, c, 2));
      set("SPL_DAMAGE_MAGSPHERE", f(190, c, 3));
      set("SPL_Damage_Extricate", f(350, c, 4));
      set("SPL_DAMAGE_Acid", f(get("Trophy_MuritanToothIsUp")!=0 ? 450 : 375, c, 4));
      set("SPL_DAMAGE_GURUMASS", f(300, c, 4));
      set("SPL_Whirlwind_DAMAGE", f(720, c, 5));
      set("SPL_DAMAGE_CONCUSSIONBOLT", f(1000, c, 6));
      set("SPL_DAMAGE_GuruWrath", f(1000, c, 6));
      set("SPL_DAMAGE_Quake", f(1800, c, 6));
      set("SPL_COST_ConcussionBolt", 80);
      set("SPL_COST_GURUWRATH", 80);
      set("SPL_COST_WINDFIST", 20);
      set("SPL_COST_SLEEP", 50);
      set("SPL_COST_BERZERK", 10);
      set("SPL_COST_TELEKINESIS", 15);
      set("SPL_COST_CHARM", 100);
      set("SPL_COST_FEAR", 75);
      set("SPL_COST_GREENTENTACLE", 50);
      set("SPL_COST_SEVEREFETIDITY", 50);
      set("SPL_COST_ACID", 60);
      set("SPL_COST_CONTROL", 60);
      set("SPL_COST_WHIRLWIND", 70);
      set("SPL_COST_GURUWRATH", 100);
      set("SPL_COST_QUAKE", 300);
      set("SPL_COST_EXTRICATE", 75);
      set("SPL_COST_MAGSPHERE", 35);
      set("SPL_COST_GURUMASS", 60);
      set("SPL_COST_STONEFIRST", 25);
      set("SPL_DAMAGE_Deathbolt", f(80, b, 1));
      set("SPL_Damage_Lacerate", f(140, b, 2));
      set("SPL_DAMAGE_GREENSTRIKE", f(200, b, 2));
      set("SPL_Swarm_Damage", f(25, b, 2));
      set("SPL_DAMAGE_Energyball", f(250, b, 3));
      set("SPL_DAMAGE_NECROBOLT", f(350, b, 4));
      set("SPL_DAMAGE_Deathball", f(450, b, 4));
      set("SPL_Damage_Skull", f(700, b, 5));
      set("SPL_Damage_MassDeath", f(1000, b, 6));
      set("SPL_Damage_BLOODRAIN", f(1400, b, 6));
      set("SPL_DAMAGE_DOT_FIRST", f(30, a, 1));
      set("SPL_DAMAGE_DOT_SECOND", f(60, a, 2));
      set("SPL_DAMAGE_DARKBALL", f(160, b, 3));
      set("SPL_DAMAGE_BIGDARKBALL", f(300, b, 4));
      set("SPL_DAMAGE_DARKSPEAR", f(450, b, 5));
      set("SPL_DAMAGE_DARKWAVE", f(1100, b, 6));
      set("SPL_COST_DEATHBOLT", 10);
      set("SPL_COST_SWARM", 15);
      set("SPL_COST_SUMMONGOBLINSKELETON", 50);
      set("SPL_COST_SUMMONSKELETON", 100);
      set("SPL_COST_LACERATE", 20);
      set("SPL_Cost_EnergyBall", 35);
      set("SPL_COST_SUMMONDEMON", 150);
      set("SPL_COST_SUMMONZOMBIE", 75);
      set("SPL_COST_DEATHBALL", 60);
      set("SPL_COST_ARMYOFDARKNESS", 100);
      set("SPL_COST_MASSDEATH", 300);
      set("SPL_COST_NECROBOLT", 75);
      set("SPL_COST_SKULL", 90);
      set("SPL_COST_RAGE", 100);
      set("SPL_COST_BLOODRAIN", 400);
      set("SPL_DAMAGE_BELIARSRUNE_01", f(130, a, 4));
      set("SPL_DAMAGE_BELIARSRUNE_02", f(230, a, 4));
      set("SPL_DAMAGE_BELIARSRUNE_03", f(330, a, 4));
      set("SPL_DAMAGE_BELIARSRUNE_04", f(430, a, 4));
      set("SPL_DAMAGE_BELIARSRUNE_05", f(530, a, 4));
      set("SPL_DAMAGE_BELIARSRUNE_06", f(600, a, 4));
      set("SPL_DAMAGE_SUPERBELIARSRUNE", f(850, int(float(intel)*0.65f), 5));
      set("SPL_COST_BELIARSRUNE_SUPER", 85);
      set("SPL_COST_BELIARSRUNE_06", 85);
      set("SPL_COST_BELIARSRUNE_05", 75);
      set("SPL_COST_BELIARSRUNE_04", 60);
      set("SPL_COST_BELIARSRUNE_03", 45);
      set("SPL_COST_BELIARSRUNE_02", 30);
      set("SPL_COST_BELIARSRUNE_01", 100);
      set("SPL_DAMAGE_DESTROYGUARDIANS", f(3500, c, 6));
      set("SPL_Damage_BreathOfDeath", f(900, a, 4));
      set("SPL_Damage_AdanosBall", f(900, a, 6));
      set("SPL_DAMAGE_ABYSSSPELL", f(550, c, 6));
      set("SPL_DAMAGE_DESTROYUNDEAD", f(120*circle+700, c, 6));
      set("SPL_COST_DESTROYUNDEAD", 100);
      set("SPL_COST_BREATHOFDEATH", 90);
      set("SPL_COST_LIGHT", 5);
      set("SPL_COST_SUMMONWOLF", 10);
      set("SPL_COST_ABYSSSPELL", 100);
      set("SPL_COST_ADANOSBALL", 100);
      set("SPL_COST_SHRINK", 150);
      set("SPL_COST_SUMMONGOLEM", 75);
      set("STEP_WINDFIST", 20);
      set("SPL_DAMAGE_PALHOLYBOLT", g(350));
      set("SPL_DAMAGE_PALREPELEVIL", g(700));
      set("SPL_DAMAGE_PALDESTROYEVIL", g(1000));
      set("SPL_DAMAGE_TPLLIGHTSTRIKE", g(90));
      set("SPL_DAMAGE_TPLMEDIUMSTRIKE", g(225));
      set("SPL_DAMAGE_TPLHEAVYSTRIKE", g(465));
      set("SPL_DAMAGE_TPLSUPERSTRIKE", g(800));
      set("SPL_COST_DARKRUNE_STEAL", get("DarkSkill1")*50);
      set("SPL_COST_DARKRUNE_DEFENSE", get("DarkSkill2")*50);
      set("SPL_COST_DARKRUNE_ILLNESS", get("DarkSkill3")*50);
      set("SPL_COST_DARKRUNE_Absorb", get("DarkSkill5")*50);
      set("SPL_COST_DARKRUNE_LORD", get("DarkSkill6")*50);
      set("SPL_COST_DARKSTATSUP", get("DarkSkill7")*50);
      set("SPL_COST_DARKRUNE_EDEFENSE", get("DarkSkill8")*50);

  return hash;
  }

/// NB fix 127: the training dummies of New Balance (AIM, AIM_100/300/600 - Onar's farm, the militia in the city) carry
/// ID_TRAINING_AIM in aivar[43]
bool GameScript::nbIsTrainingDummy(const Npc& npc) {
  auto* id = vm.find_symbol_by_name("ID_TRAINING_AIM");
  if(id==nullptr || id->type()!=zenkit::DaedalusDataType::INT || id->is_member() || id->get_int()==0)
    return false;
  const auto& h = const_cast<Npc&>(npc).handle();
  constexpr size_t AIV_MM_REAL_ID = 43;
  return AIV_MM_REAL_ID<std::size(h.aivar) && h.aivar[AIV_MM_REAL_ID]==id->get_int();
  }

bool GameScript::nbIsSummon(const Npc& npc) {
  // NB fix 81: RX_IsSummonNpc of the scripts - the summoned creatures of New Balance carry RX_SUMMON_ID in aivar[91]
  auto* id = vm.find_symbol_by_name("RX_SUMMON_ID");
  if(id==nullptr || id->type()!=zenkit::DaedalusDataType::INT || id->is_member())
    return false;
  const auto& h = const_cast<Npc&>(npc).handle();
  constexpr size_t AIV_SUMMON = 91;
  return AIV_SUMMON<std::size(h.aivar) && h.aivar[AIV_SUMMON]==id->get_int() && id->get_int()!=0;
  }

void GameScript::nbOnDamage(Npc& target, Npc& attacker) {
  nbBossHit(target,attacker);   // NB fix 100
  // NB fix 82: the part of RX_OnDamage (the plugin calls it at every damage) for the music of the bosses: the player or his
  // summoned creature fights a boss (RX_IsBoss) - RX_CheckMusicBoss chooses and starts the music, or keeps it on
  auto* isBoss = vm.find_symbol_by_name("RX_ISBOSS");
  auto* check  = vm.find_symbol_by_name("RX_CHECKMUSICBOSS");
  if(isBoss==nullptr || check==nullptr || isBoss->type()!=zenkit::DaedalusDataType::FUNCTION ||
     check->type()!=zenkit::DaedalusDataType::FUNCTION)
    return;
  const auto boss = [this,isBoss](Npc& n) {
    try {
      return vm.call_function<int>(isBoss,n.handlePtr())!=0;
      }
    catch(const std::exception& e) {
      Log::e("NB: RX_IsBoss failed: ",e.what());
      return false;
      }
    };
  // NB fix 83: the rest of RX_OnDamage that the fights need - the timer of the fight (BIT_FIGHT_TIMER_STARTED) and the knock
  // down by the blows of the player (RX_NpcProcs: a person falls by the chance of the weapon skill of the player)
  const auto call = [this](std::string_view name, auto... args) {
    auto* fn = vm.find_symbol_by_name(name);
    if(fn==nullptr || fn->type()!=zenkit::DaedalusDataType::FUNCTION)
      return;
    try {
      vm.call_function<void>(fn,args...);
      }
    catch(const std::exception& e) {
      Log::e("NB: ",name," failed: ",e.what());
      }
    };
  const int32_t fightBit = nbScriptInt(vm,"BIT_FIGHT_TIMER_STARTED",0,-1);
  if(attacker.isPlayer() && fightBit>=0)
    call("SETVAR2_TRUE",target.handlePtr(),fightBit);
  if(target.isPlayer() && fightBit>=0)
    call("SETVAR2_TRUE",attacker.handlePtr(),fightBit);
  if(attacker.isPlayer() && !target.isPlayer() && !target.isDead())
    call("RX_NPCPROCS",attacker.handlePtr(),target.handlePtr());

  Npc* b = nullptr;
  if((target.isPlayer() || nbIsSummon(target)) && boss(attacker))
    b = &attacker; else
  if((attacker.isPlayer() || nbIsSummon(attacker)) && boss(target))
    b = &target;
  if(b==nullptr)
    return;
  try {
    vm.call_function<void>(check,b->handlePtr());
    }
  catch(const std::exception& e) {
    Log::e("NB: RX_CheckMusicBoss failed: ",e.what());
    }
  }

void GameScript::nbBeforeHit(Npc& target, Npc& attacker) {
  // NB fix 83: RX_ApplyStagger of RX_OnDamage - the character hit by the player (or the attacker of the player) is
  // interrupted (stumbles) or not by the strength/dexterity of the hero against its kind: C_NPC.bodystate_interruptable_override.
  // The bosses, the arena fighters and some creatures are never staggered (the script decides)
  auto* fn = vm.find_symbol_by_name("RX_APPLYSTAGGER");
  if(fn==nullptr || fn->type()!=zenkit::DaedalusDataType::FUNCTION)
    return;
  Npc* n = nullptr;
  if(attacker.isPlayer() && !target.isPlayer())
    n = &target; else
  if(target.isPlayer() && !attacker.isPlayer())
    n = &attacker;
  if(n==nullptr)
    return;
  try {
    vm.call_function<void>(fn,n->handlePtr());
    }
  catch(const std::exception& e) {
    Log::e("NB: RX_ApplyStagger failed: ",e.what());
    }
  }

// NB fix 100: the functions of the bosses that the plugin calls by the name of the instance of the character
zenkit::DaedalusSymbol* GameScript::nbInstanceFunc(std::string_view prefix, Npc& npc) {
  auto* inst = vm.find_symbol_by_index(npc.handle().symbol_index());
  if(inst==nullptr)
    return nullptr;
  auto* fn = vm.find_symbol_by_name(std::string(prefix)+inst->name());
  return fn!=nullptr && fn->type()==zenkit::DaedalusDataType::FUNCTION ? fn : nullptr;
  }

void GameScript::nbCallInstanceFunc(zenkit::DaedalusSymbol* fn, Npc& self, Npc* other, int32_t* arg) {
  if(fn==nullptr)
    return;
  ScopeVar sv(*vm.global_self(), self.handlePtr());
  ScopeVar ov(*vm.global_other(), other!=nullptr ? other->handlePtr() : vm.global_other()->get_instance());
  try {
    if(arg!=nullptr) {
      if(fn->has_return())
        (void)vm.call_function<int>(fn,*arg); else
        vm.call_function<void>(fn,*arg);
      } else {
      if(fn->has_return())
        (void)vm.call_function<int>(fn); else
        vm.call_function<void>(fn);
      }
    }
  catch(const std::exception& e) {
    Log::e("NB: ",fn->name()," failed: ",e.what());
    }
  }

/// UNION_ABI.DLL 0x10054220, at every damage: the attacker and the victim with AI_OnTick_<instance> join the fight of
/// the bosses (their timer starts), then AI_OnHit_<attacker> (SELF the attacker, OTHER the victim) and
/// AI_OnGetDamage_<victim> (SELF the victim, OTHER the attacker). The phases of the bosses are in these functions.
void GameScript::nbBossHit(Npc& target, Npc& attacker) {
  auto nb = nbStates.find(this);
  if(nb==nbStates.end() || target.isDead() || attacker.isDead())
    return;
  auto& st = nb->second;
  auto join = [&](Npc& n) {
    if(nbInstanceFunc("AI_ONTICK_",n)==nullptr)
      return;
    auto h = n.handlePtr();
    for(auto& w:st.bossFight)
      if(w.lock()==h)
        return;
    st.bossFight.push_back(h);
    nbCtx("boss fight: ",n.displayName()," joins (AI_OnTick)");
    };
  join(attacker);
  join(target);
  nbCallInstanceFunc(nbInstanceFunc("AI_ONHIT_",attacker),attacker,&target,nullptr);
  if(!target.isDead() && !attacker.isDead())
    nbCallInstanceFunc(nbInstanceFunc("AI_ONGETDAMAGE_",target),target,&attacker,nullptr);
  }

/// UNION_ABI.DLL 0x100546A0, every second of the game: a character of the fight that is dead, or 70 m from the hero, or
/// the hero is dead - the fight is broken (0x10054030: the variables 35..51 of NB - the phases, the spells counted - are
/// cleared, full health, the timer 0, AI_OnBreak_<instance>). The others: the timer (aivar[95]) goes 1..300 and
/// AI_OnTick_<instance>(timer) is called with SELF the character.
void GameScript::nbBossTick() {
  auto nb = nbStates.find(this);
  if(nb==nbStates.end())
    return;
  auto& st   = nb->second;
  auto* hero = world().player();
  if(hero==nullptr)
    return;

  if(!st.bossFightLoaded) {
    // after a load: the fights that went on (the timer is in the save)
    st.bossFightLoaded = true;
    world().detectNpc(hero->position(),7000.f,[&](Npc& n) {
      if(!n.isDead() && n.handle().aivar[95]>0 && nbInstanceFunc("AI_ONTICK_",n)!=nullptr)
        st.bossFight.push_back(n.handlePtr());
      });
    }
  if(st.bossFight.empty() || tickCount()<st.nextBossTick)
    return;
  st.nextBossTick = tickCount()+1000;

  for(size_t i=st.bossFight.size(); i>0; --i) {
    auto  ref = st.bossFight[i-1].lock();
    auto* npc = ref!=nullptr ? findNpcRaw(ref) : nullptr;
    if(npc==nullptr) {
      st.bossFight.erase(st.bossFight.begin()+ptrdiff_t(i-1));
      continue;
      }
    const bool far = (npc->position()-hero->position()).length()>=7000.f;
    if(!npc->isDead() && !hero->isDead() && !far)
      continue;
    st.bossFight.erase(st.bossFight.begin()+ptrdiff_t(i-1));
    for(int32_t v=35; v<=51; ++v)
      npc->newBalanceSetNpcVar(v,0);
    npc->handle().aivar[95] = 0;
    if(!npc->isDead())
      npc->changeAttribute(ATR_HITPOINTS,npc->attribute(ATR_HITPOINTSMAX),false);
    nbCtx("boss fight: ",npc->displayName()," broken (",npc->isDead() ? "dead" : hero->isDead() ? "the hero is dead" : "70 m away",")");
    nbCallInstanceFunc(nbInstanceFunc("AI_ONBREAK_",*npc),*npc,nullptr,nullptr);
    }

  // the functions may change the list (a new character joins): a copy
  auto list = st.bossFight;
  for(auto& w:list) {
    auto  ref = w.lock();
    auto* npc = ref!=nullptr ? findNpcRaw(ref) : nullptr;
    if(npc==nullptr || npc->isDead())
      continue;
    auto* fn = nbInstanceFunc("AI_ONTICK_",*npc);
    if(fn==nullptr)
      continue;
    int32_t& timer = npc->handle().aivar[95];
    timer = timer>=300 ? 1 : timer+1;
    int32_t arg = timer;
    nbCallInstanceFunc(fn,*npc,nullptr,&arg);
    }
  }

/// NB fix 106: RX_CallDamage of the plugin (UNION_ABI.DLL 0x10314430: at every damage of the hero) - RX_MainDamageFunc:
/// the bonus of the stamina and the armor, the runes, the effects of the weapons (RX_WeaponDamage: fire, poison - with the
/// damage over time, the special weapons). The extra damage comes back in RX_GlobalAddDamage (B_MagicHurtNpc).
int32_t GameScript::nbCallDamage(Npc& victim, Npc& hero, bool isSpell, int32_t splId, bool splash) {
  if(nbStates.find(this)==nbStates.end())
    return 0;
  auto* fn = vm.find_symbol_by_name("RX_CALLDAMAGE");
  if(fn==nullptr || fn->type()!=zenkit::DaedalusDataType::FUNCTION)
    return 0;
  auto var = [this](std::string_view name) -> zenkit::DaedalusSymbol* {
    auto* s = vm.find_symbol_by_name(name);
    return s!=nullptr && s->type()==zenkit::DaedalusDataType::INT && !s->is_member() ? s : nullptr;
    };
  auto* add = var("RX_GLOBALADDDAMAGE");
  if(add==nullptr)
    return 0;
  add->set_int(0);
  if(auto* s = var("RX_LASTDAMAGEWASSPELL"))
    s->set_int(isSpell ? 1 : 0);
  if(auto* s = var("ISSPLASHDAMAGE"))
    s->set_int(splash ? 1 : 0);
  if(isSpell) {
    if(auto* s = var("RX_LASTSPELLID"))
      s->set_int(splId);
    }
  ScopeVar sv(*vm.global_self(),  hero.handlePtr());
  ScopeVar ov(*vm.global_other(), victim.handlePtr());
  try {
    if(fn->has_return())
      (void)vm.call_function<int>(fn); else
      vm.call_function<void>(fn);
    }
  catch(const std::exception& e) {
    Log::e("NB: RX_CallDamage failed: ",e.what());
    return 0;
    }
  return std::max(0,add->get_int());
  }

/// NB fix 106: the splash of the plugin (UNION_ABI.DLL 0x100BAA70): a blow of the hero with a melee weapon also hits the
/// enemies next to the target - in the reach of the weapon and in front of the hero (the angle and the part of the damage
/// by the kind of the weapon), more with the skill of the weapon: 10% + (part-10%) * skill/100
void GameScript::nbSplash(Npc& target, Npc& hero) {
  if(nbStates.find(this)==nbStates.end() || !hero.isPlayer() || target.isPlayer())
    return;
  const auto ws = hero.weaponState();
  if(ws!=WeaponState::W1H && ws!=WeaponState::W2H)
    return;
  auto* weapon = hero.inventory().activeWeapon();
  if(weapon==nullptr)
    return;
  const int32_t flags = weapon->itemFlag();
  int32_t pct = 45;      // a sword of one hand
  float   ang = 42.f;
  if(flags & (1<<17)) {        // an axe of two hands
    pct = 85; ang = 80.f;
    } else
  if(flags & (1<<16)) {        // a sword of two hands
    pct = 75; ang = 70.f;
    } else
  if(flags & (1<<15)) {        // an axe of one hand
    pct = 75; ang = 75.f;
    }
  const int32_t talent = std::clamp(hero.handle().hitchance[ws==WeaponState::W2H ? 2 : 1],0,120);
  const int32_t eff    = 10+(pct-10)*talent/100;
  const FightAlgo fa;   // the reach (the preferred distance of the attack, as the hits of OpenGothic) and the angle

  std::vector<Npc*> list;
  world().detectNpc(hero.position(),600.f,[&](Npc& n) {
    if(&n==&hero || &n==&target || n.isDead() || n.isDown() || n.isPlayer() || nbIsSummon(n))
      return;
    if(personAttitude(n,hero)!=ATT_HOSTILE && n.target()!=&hero)
      return;
    if(!fa.isInWRange(hero,n,*this) || !fa.isInFocusAngle(hero,n,ang))
      return;
    list.push_back(&n);
    });
  if(list.empty())
    return;
  nbCtx("splash: ",int(list.size())," more, ",eff,"% of the blow");
  Npc::nbSplashPct = std::max(1,eff);
  for(auto* n:list)
    n->takeDamage(hero,nullptr);
  Npc::nbSplashPct = 0;
  }

/// NB fix 106: the ticks of the damage over time: the damage less the protection of its kind (at least a tenth), NotKill
/// leaves 1 point of health; the ticks on the hero or done by the hero are written on the left of the screen
void GameScript::nbDotTick() {
  auto nb = nbStates.find(this);
  if(nb==nbStates.end() || nb->second.dots.empty())
    return;
  auto&          st   = nb->second;
  const uint64_t now  = tickCount();
  auto*          hero = world().player();
  auto           list = st.dots;   // a tick may add new ones
  st.dots.clear();
  for(auto& d:list) {
    auto  tRef = d.target.lock();
    auto* t    = tRef!=nullptr ? findNpcRaw(tRef) : nullptr;
    if(t==nullptr || t->isDead())
      continue;
    if(d.next>now) {
      st.dots.push_back(d);
      continue;
      }
    auto  aRef = d.attacker.lock();
    auto* a    = aRef!=nullptr ? findNpcRaw(aRef) : nullptr;

    int32_t prot = 0;
    switch(d.type) {
      case 2:  prot = t->protection(PROT_BLUNT); break;
      case 4:  prot = t->protection(PROT_EDGE);  break;
      case 8:  prot = t->protection(PROT_FIRE);  break;
      case 32: prot = t->protection(PROT_MAGIC); break;
      case 64: prot = t->protection(PROT_POINT); break;
      default: break;
      }
    if(prot<0)   // immune
      prot = d.damage;
    int32_t dmg = std::max(d.damage-prot,(d.damage+9)/10);
    if(d.notKill)
      dmg = std::min(dmg,t->attribute(ATR_HITPOINTS)-1);
    if(dmg>0) {
      if(a!=nullptr && !t->isPlayer() && !t->isTalk())
        t->setOther(a);
      t->changeAttribute(ATR_HITPOINTS,-dmg,false);
      if(hero!=nullptr && (t==hero || a==hero)) {
        const char* kind = d.type==8 ? "Ogie\xF1" : d.type==5 ? "Trucizna" : d.type==32 ? "Magia" : "Obra\xBF""enia";
        std::string txt = std::string(kind)+": -"+std::to_string(dmg);
        if(t!=hero)
          txt += " ("+std::string(t->displayName())+")";
        st.dotLines.push_back({txt,d.type,now+3000});
        }
      }
    if(--d.ticks>0 && !t->isDead()) {
      d.next += d.interval;
      if(d.next<now)
        d.next = now+d.interval;
      st.dots.push_back(d);
      }
    }
  }
