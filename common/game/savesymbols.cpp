#include "savesymbols.h"

#include <Tempest/Log>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

using namespace Tempest;

namespace {

using zenkit::DaedalusDataType;

enum Cat : uint8_t {
  C_Void     = 0,   // not in the variables of a save-game: a function, a class, a constant, a member
  C_Instance = 1,   // an instance (a character, an item, a dialog...)
  C_Var      = 2,   // a variable, a parameter or a local of a function
  };

/// the temporary strings of the compiler: "\xFF123" - another number in other scripts, never in a save-game
bool isTemp(std::string_view n) {
  return !n.empty() && uint8_t(n[0])==0xFF;
  }

bool isVar(const zenkit::DaedalusSymbol& s) {
  const auto t = s.type();
  if(t!=DaedalusDataType::INT && t!=DaedalusDataType::FLOAT && t!=DaedalusDataType::STRING)
    return false;
  return s.count()>0 && !s.is_member() && !s.is_const();
  }

/// how GameScript::saveSym writes the symbol
Cat catNow(const zenkit::DaedalusSymbol& s) {
  if(s.type()==DaedalusDataType::INSTANCE)
    return C_Instance;
  return isVar(s) ? C_Var : C_Void;
  }

/// the symbols of a save-game: written by the names of game/symtab
bool refByName(const zenkit::DaedalusSymbol& s) {
  const auto t = s.type();
  return (t==DaedalusDataType::FUNCTION || t==DaedalusDataType::PROTOTYPE || t==DaedalusDataType::INSTANCE) && !isTemp(s.name());
  }

uint64_t fingerprint(zenkit::DaedalusVm& vm) {
  static const zenkit::DaedalusVm* cachedVm   = nullptr;
  static size_t                    cachedSize = 0;
  static uint64_t                  cachedHash = 0;
  auto& dat = vm.symbols();
  if(cachedVm==&vm && cachedSize==dat.size())
    return cachedHash;
  uint64_t h = 1469598103934665603ull;
  for(auto& s:dat) {
    for(char c:s.name()) {
      h ^= uint8_t(c);
      h *= 1099511628211ull;
      }
    h ^= uint64_t(s.type())+0x100;
    h *= 1099511628211ull;
    }
  cachedVm   = &vm;
  cachedSize = dat.size();
  cachedHash = h;
  return h;
  }

std::string upper(std::string_view s) {
  std::string r(s);
  for(auto& c:r)
    c = char(std::toupper(uint8_t(c)));
  return r;
  }

struct OldSym {
  Cat         cat = C_Void;
  std::string name;   // empty: not known
  };

/// the symbols of the save-game from its variables (game/daedalus)
bool readOld(Serialize& fin, std::vector<OldSym>& old) {
  if(!fin.setEntry("game/daedalus"))
    return false;
  uint32_t sz = 0;
  fin.read(sz);
  if(sz>4*1024*1024)
    return false;
  old.resize(sz);
  for(auto& o:old) {
    uint32_t t = 0;
    fin.read(t);
    switch(DaedalusDataType(t)) {
      case DaedalusDataType::INT:
      case DaedalusDataType::FLOAT: {
        uint32_t cnt = 0;
        fin.read(o.name,cnt);
        for(uint32_t i=0; i<cnt; ++i) {
          int32_t v = 0;
          fin.read(v);
          }
        o.cat = C_Var;
        break;
        }
      case DaedalusDataType::STRING: {
        uint32_t cnt = 0;
        fin.read(o.name,cnt);
        std::string v;
        for(uint32_t i=0; i<cnt; ++i)
          fin.read(v);
        o.cat = C_Var;
        break;
        }
      case DaedalusDataType::INSTANCE: {
        uint8_t dataClass = 0;
        fin.read(dataClass);
        if(dataClass>0) {
          uint32_t id = 0;
          fin.read(o.name,id);
          if(dataClass==3) {
            uint32_t itmClass = 0;
            fin.read(itmClass);
            }
          }
        o.cat = C_Instance;
        break;
        }
      case DaedalusDataType::VOID:
        break;
      default:
        return false;   // not a save-game of this program
      }
    }
  return true;
  }

/// a file of the names of the old scripts next to the save-games (old_symbols*.txt: the name of the symbol N on the line N+1);
/// it is taken only if it agrees with the names of the variables of the save-game
bool fromFile(const std::vector<OldSym>& old, std::vector<std::string>& names, std::string& file) {
  std::error_code ec;
  for(auto& it:std::filesystem::directory_iterator(std::filesystem::path("."),ec)) {
    const std::string fn = it.path().filename().string();
    std::string low = fn;
    for(auto& c:low)
      c = char(std::tolower(uint8_t(c)));
    if(low.rfind("old_symbols",0)!=0 || low.size()<4 || low.compare(low.size()-4,4,".txt")!=0)
      continue;
    std::ifstream in(it.path(),std::ios::binary);
    std::vector<std::string> list;
    std::string line;
    while(std::getline(in,line)) {
      if(!line.empty() && line.back()=='\r')
        line.pop_back();
      list.push_back(line);
      }
    if(list.size()!=old.size()) {
      Log::i("[save] ",fn,": ",list.size()," names, the save-game has ",old.size()," symbols - not these scripts");
      continue;
      }
    size_t same = 0, named = 0;
    for(size_t i=0; i<old.size(); ++i) {
      if(old[i].name.empty() || isTemp(old[i].name))
        continue;
      ++named;
      if(upper(list[i])==upper(old[i].name))
        ++same;
      }
    if(named==0 || same*1000<named*999) {
      Log::i("[save] ",fn,": ",same," of ",named," names agree with the save-game - not these scripts");
      continue;
      }
    names = std::move(list);
    file  = fn;
    return true;
    }
  return false;
  }

/// the indices of the anchors that keep their order (the longest increasing sequence of the new indices)
std::vector<uint32_t> monotonic(const std::vector<uint32_t>& map) {
  std::vector<uint32_t> idx;                 // old indices with a known new index
  for(uint32_t i=0; i<map.size(); ++i)
    if(map[i]!=uint32_t(-1))
      idx.push_back(i);
  std::vector<uint32_t> tailVal, tailIdx;    // the smallest end of a sequence of the length k+1
  std::vector<int32_t>  prev(idx.size(),-1);
  for(uint32_t p=0; p<idx.size(); ++p) {
    const uint32_t v  = map[idx[p]];
    const auto     at = size_t(std::lower_bound(tailVal.begin(),tailVal.end(),v)-tailVal.begin());
    if(at>0)
      prev[p] = int32_t(tailIdx[at-1]);
    if(at==tailVal.size()) {
      tailVal.push_back(v);
      tailIdx.push_back(p);
      } else {
      tailVal[at] = v;
      tailIdx[at] = p;
      }
    }
  std::vector<uint32_t> ret(tailIdx.size());
  int32_t p = tailIdx.empty() ? -1 : int32_t(tailIdx.back());
  for(size_t k=ret.size(); k>0 && p>=0; --k) {
    ret[k-1] = idx[size_t(p)];
    p        = prev[size_t(p)];
    }
  return ret;
  }

struct Stats {
  size_t byName = 0, byScope = 0, byPlace = 0, unknown = 0;
  };

/// the symbols between two anchors: the same order of the kinds, with one block of symbols added or removed at most
void alignGap(const std::vector<OldSym>& old, const std::vector<Cat>& now, uint32_t o0, uint32_t o1, uint32_t n0, uint32_t n1,
              std::vector<uint32_t>& map, Stats& st) {
  const int64_t lenO = int64_t(o1)-int64_t(o0);
  const int64_t lenN = int64_t(n1)-int64_t(n0);
  if(lenO<=0)
    return;
  auto oc = [&](int64_t i) { return old[size_t(o0+i)].cat; };
  auto nc = [&](int64_t i) { return now[size_t(n0+i)]; };
  // a known name in the gap: the symbol is removed or moved (map[] has it already, or it is lost)
  auto free = [&](int64_t i) { return map[size_t(o0+i)]==uint32_t(-1); };

  int64_t pre = 0;
  while(pre<lenO && pre<lenN && oc(pre)==nc(pre))
    ++pre;
  int64_t suf = 0;
  while(suf<lenO && suf<lenN && oc(lenO-1-suf)==nc(lenN-1-suf))
    ++suf;

  const int64_t d = lenN-lenO;
  auto put = [&](int64_t i, int64_t n) {
    if(!free(i))
      return;
    map[size_t(o0+i)] = uint32_t(n0+n);
    ++st.byPlace;
    };

  if(d==0 && pre==lenO) {
    for(int64_t i=0; i<lenO; ++i)
      put(i,i);
    return;
    }

  int64_t kMin = 0, kMax = -1;
  if(d>0) {
    // a block of d symbols is added at k: old [0,k) -> [0,k), old [k,lenO) -> [k+d,lenN)
    kMin = std::max<int64_t>(0,lenO-suf);
    kMax = std::min<int64_t>(pre,lenO);
    if(kMin<=kMax) {
      for(int64_t i=0; i<kMin; ++i)
        put(i,i);
      for(int64_t i=kMax; i<lenO; ++i)
        put(i,i+d);
      }
    }
  else if(d<0) {
    // a block of -d symbols of the save-game is removed at k
    const int64_t del = -d;
    kMin = std::max<int64_t>(0,lenO-del-suf);
    kMax = std::min<int64_t>(pre,lenN);
    if(kMin<=kMax) {
      for(int64_t i=0; i<kMin; ++i)
        put(i,i);
      for(int64_t i=kMax+del; i<lenO; ++i)
        put(i,i+d);
      }
    }
  else {
    // the same count, other kinds: the same beginning and the same end only
    for(int64_t i=0; i<pre; ++i)
      put(i,i);
    for(int64_t i=lenO-suf; i<lenO; ++i)
      put(i,i);
    }
  for(int64_t i=0; i<lenO; ++i)
    if(map[size_t(o0+i)]==uint32_t(-1) && old[size_t(o0+i)].cat!=C_Var)
      ++st.unknown;
  }

}

void SaveSymbols::write(Serialize& fout, zenkit::DaedalusVm& vm) {
  auto&    dat = vm.symbols();
  uint32_t n   = 0;
  for(auto& s:dat)
    if(refByName(s))
      ++n;
  fout.setEntry("game/symtab");
  fout.write(uint8_t(1),uint32_t(dat.size()),fingerprint(vm),n);
  for(auto& s:dat)
    if(refByName(s))
      fout.write(uint32_t(s.index()),s.name());
  }

Serialize::SymbolMap SaveSymbols::build(Serialize& fin, zenkit::DaedalusVm& vm) {
  auto&          dat   = vm.symbols();
  const uint32_t count = uint32_t(dat.size());

  if(fin.setEntry("game/symtab")) {
    uint8_t  version = 0;
    uint32_t oldCount = 0, n = 0;
    uint64_t hash = 0;
    fin.read(version,oldCount,hash,n);
    if(version!=1)
      throw SaveGameError("game/symtab: unknown version "+std::to_string(version));
    if(oldCount==count && hash==fingerprint(vm))
      return nullptr;   // the same scripts
    auto map = std::make_shared<std::vector<uint32_t>>(oldCount,uint32_t(-1));
    size_t found = 0;
    std::string name;
    for(uint32_t i=0; i<n; ++i) {
      uint32_t idx = 0;
      fin.read(idx,name);
      if(idx>=oldCount)
        continue;
      if(auto* s = vm.find_symbol_by_name(name); s!=nullptr && s->type()!=DaedalusDataType::CLASS) {
        (*map)[idx] = s->index();
        ++found;
        }
      }
    Log::i("[save] the scripts are not the scripts of the save-game (",oldCount," symbols, now ",count,"): ",
           found," of ",n," functions and instances are found by their names, ",n-found," are not in the scripts now");
    return map;
    }

  // a save-game without game/symtab (before NB fix 209)
  std::vector<OldSym> old;
  if(!readOld(fin,old)) {
    Log::e("[save] the variables of the save-game are not readable: the symbols are taken as they are");
    return nullptr;
    }
  if(old.size()==count) {
    bool same = true;
    for(uint32_t i=0; i<count && same; ++i) {
      if(!old[i].name.empty() && !isTemp(old[i].name) && upper(old[i].name)!=dat[i].name())
        same = false;
      if(old[i].cat!=catNow(dat[i]))
        same = false;
      }
    if(same)
      return nullptr;
    }

  auto  mapPtr = std::make_shared<std::vector<uint32_t>>(old.size(),uint32_t(-1));
  auto& map    = *mapPtr;
  Stats st;

  // the names of a file of the old scripts
  std::vector<std::string> names;
  std::string              file;
  if(fromFile(old,names,file)) {
    size_t found = 0, n = 0;
    for(size_t i=0; i<names.size(); ++i) {
      if(names[i].empty() || isTemp(names[i]))
        continue;
      ++n;
      if(auto* s = vm.find_symbol_by_name(names[i])) {
        map[i] = s->index();
        ++found;
        }
      }
    Log::i("[save] the scripts are not the scripts of the save-game (",old.size()," symbols, now ",count,"): the names of ",file,
           ", ",found," of ",n," are found");
    return mapPtr;
    }

  // 1. the names of the variables, the characters and the items
  for(size_t i=0; i<old.size(); ++i) {
    if(old[i].name.empty() || isTemp(old[i].name))
      continue;
    auto* s = vm.find_symbol_by_name(old[i].name);
    if(s==nullptr || catNow(*s)!=old[i].cat)
      continue;
    map[i] = s->index();
    ++st.byName;
    }
  // 2. the function (or the instance) before its first parameter or local: "F" before "F.PAR0"
  for(size_t i=1; i<old.size(); ++i) {
    if(map[i]==uint32_t(-1) || map[i]==0 || map[i-1]!=uint32_t(-1) || !old[i-1].name.empty())
      continue;
    const auto dot = old[i].name.find('.');
    if(dot==std::string::npos || dot==0)
      continue;
    auto* s = vm.find_symbol_by_name(std::string_view(old[i].name).substr(0,dot));
    if(s==nullptr || s->index()+1!=map[i] || catNow(*s)!=old[i-1].cat)
      continue;
    map[i-1] = s->index();
    ++st.byScope;
    }
  // 3. the symbols between the known ones: by their place
  std::vector<Cat> now(count);
  for(uint32_t i=0; i<count; ++i)
    now[i] = catNow(dat[i]);
  const auto anchors = monotonic(map);
  uint32_t   o0 = 0, n0 = 0;   // the beginning of the gap
  for(size_t a=0; a<=anchors.size(); ++a) {
    const uint32_t o1 = a<anchors.size() ? anchors[a]      : uint32_t(old.size());
    const uint32_t n1 = a<anchors.size() ? map[anchors[a]] : count;
    if(o1>o0 && n1>=n0)
      alignGap(old,now,o0,o1,n0,n1,map,st);
    o0 = o1+1;
    n0 = n1+1;
    }
  Log::i("[save] the scripts are not the scripts of the save-game (",old.size()," symbols, now ",count,"): ",
         st.byName," found by their names, ",st.byScope," functions by their parameters, ",st.byPlace," by their place, ",
         st.unknown," functions and instances are not known (put old_symbols.txt of the old scripts next to the save-games)");
  return mapPtr;
  }

Serialize::SymbolMap SaveSymbols::compose(const Serialize::SymbolMap& inner, const Serialize::SymbolMap& outer) {
  if(outer==nullptr)
    return inner;
  if(inner==nullptr)
    return outer;
  auto ret = std::make_shared<std::vector<uint32_t>>(inner->size(),uint32_t(-1));
  for(size_t i=0; i<inner->size(); ++i) {
    const uint32_t v = (*inner)[i];
    (*ret)[i] = (v==uint32_t(-1) || v>=outer->size()) ? uint32_t(-1) : (*outer)[v];
    }
  return ret;
  }

namespace {

/// NB fix 211: what the code of an item instance writes into its fields, read from the code (it is not run: some instances
/// call the functions of the game, which may not be called while a world is loaded)
struct ItemLook {
  uint32_t    symbol = 0;
  std::string visual, name, description;
  int32_t     value = 0, mainFlag = 0, munition = 0, owner = 0, magic = 0;
  int32_t     onEquip = 0, onUnequip = 0, onState[zenkit::IItem::state_count] = {};
  };

struct ItemLooks {
  const zenkit::DaedalusVm*                             vm    = nullptr;
  size_t                                                count = 0;
  std::vector<ItemLook>                                 items;
  std::unordered_map<std::string,std::vector<size_t>>   strict, loose;
  };

struct Members {
  uint32_t visual=0, name=0, description=0, value=0, mainFlag=0, munition=0, owner=0, magic=0, onEquip=0, onUnequip=0, onState=0;
  };

uint32_t memberIndex(zenkit::DaedalusVm& vm, const char* name) {
  auto* s = vm.find_symbol_by_name(name);
  return s!=nullptr ? s->index() : uint32_t(-1);
  }

void readLook(zenkit::DaedalusVm& vm, const Members& m, uint32_t address, ItemLook& look, int depth) {
  using Op = zenkit::DaedalusOpcode;
  struct Val { int kind = 0; int32_t i = 0; uint32_t sym = 0; uint8_t index = 0; };   // 0 unknown, 1 int, 2 symbol
  std::vector<Val> st;
  auto pop = [&st]() { Val v; if(!st.empty()) { v = st.back(); st.pop_back(); } return v; };
  auto asInt = [&](const Val& v, int32_t& out) {
    if(v.kind==1) { out = v.i; return true; }
    if(v.kind!=2) return false;
    if(v.sym==m.value)    { out = look.value;    return true; }
    if(v.sym==m.mainFlag) { out = look.mainFlag; return true; }
    auto* s = vm.find_symbol_by_index(v.sym);
    if(s==nullptr || s->type()!=DaedalusDataType::INT || !s->is_const() || v.index>=s->count())
      return false;
    out = s->get_int(v.index);
    return true;
    };
  auto asStr = [&](const Val& v, std::string& out) {
    if(v.kind!=2) return false;
    if(v.sym==m.name)        { out = look.name;        return true; }
    if(v.sym==m.visual)      { out = look.visual;      return true; }
    if(v.sym==m.description) { out = look.description; return true; }
    auto* s = vm.find_symbol_by_index(v.sym);
    if(s==nullptr || s->type()!=DaedalusDataType::STRING || !s->is_const() || v.index>=s->count())
      return false;
    out = s->get_string(v.index);
    return true;
    };
  const uint32_t end = vm.size();
  for(int n=0; n<4000 && address<end; ++n) {
    const auto in = vm.instruction_at(address);
    address += in.size;
    switch(in.op) {
      case Op::PUSHI:  st.push_back({1,in.immediate,0,0}); break;
      case Op::PUSHV:
      case Op::PUSHVI: st.push_back({2,0,in.symbol,0}); break;
      case Op::PUSHVV: st.push_back({2,0,in.symbol,in.index}); break;
      case Op::MOVI:
      case Op::MOVVF: {
        const Val dst = pop(), src = pop();
        int32_t v = 0;
        if(dst.kind!=2 || !asInt(src,v))
          break;
        if(dst.sym==m.value)          look.value     = v;
        else if(dst.sym==m.mainFlag)  look.mainFlag  = v;
        else if(dst.sym==m.munition)  look.munition  = v;
        else if(dst.sym==m.owner)     look.owner     = v;
        else if(dst.sym==m.magic)     look.magic     = v;
        else if(dst.sym==m.onEquip)   look.onEquip   = v;
        else if(dst.sym==m.onUnequip) look.onUnequip = v;
        else if(dst.sym==m.onState && dst.index<zenkit::IItem::state_count) look.onState[dst.index] = v;
        break;
        }
      case Op::MOVS: {
        const Val dst = pop(), src = pop();
        std::string v;
        if(dst.kind!=2 || !asStr(src,v))
          break;
        if(dst.sym==m.visual)           look.visual      = v;
        else if(dst.sym==m.name)        look.name        = v;
        else if(dst.sym==m.description) look.description = v;
        break;
        }
      case Op::MOVSS: case Op::MOVF: case Op::MOVVI: case Op::ADDMOVI: case Op::SUBMOVI: case Op::MULMOVI: case Op::DIVMOVI:
        pop(); pop();
        break;
      case Op::BL: {
        // a function that sets the fields of the item (as a prototype); its parameters and its result are not known
        st.clear();
        if(depth<3)
          readLook(vm,m,uint32_t(in.address),look,depth+1);
        st.push_back({});
        break;
        }
      case Op::BE:
        st.clear();
        st.push_back({});
        break;
      case Op::GMOVI:
        break;
      case Op::RSR:
        return;
      case Op::B: case Op::BZ:
        return;   // a condition: the rest is not read
      case Op::PLUS: case Op::NEGATE: case Op::NOT: case Op::CMPL:
        pop(); st.push_back({});
        break;
      case Op::NOP:
        break;
      default:
        pop(); pop(); st.push_back({});   // the binary operations
        break;
      }
    }
  }

std::string keyStrict(std::string_view visual, std::string_view name, int32_t value) {
  return upper(visual)+"|"+std::string(name)+"|"+std::to_string(value);
  }

std::string keyLoose(std::string_view visual, std::string_view name) {
  return upper(visual)+"|"+std::string(name);
  }

bool isItemClass(zenkit::DaedalusVm& vm, const zenkit::DaedalusSymbol& s) {
  if(s.type()!=DaedalusDataType::INSTANCE)
    return false;
  auto* p = vm.find_symbol_by_index(s.parent());
  for(int depth=0; p!=nullptr && p->type()!=DaedalusDataType::CLASS && depth<8; ++depth)
    p = vm.find_symbol_by_index(p->parent());
  return p!=nullptr && p->name()=="C_ITEM";
  }

}

std::shared_ptr<zenkit::IItem> SaveSymbols::itemByLook(zenkit::DaedalusVm& vm, const zenkit::IItem& saved) {
  static ItemLooks looks;
  if(looks.vm!=&vm || looks.count!=vm.symbols().size()) {
    looks = ItemLooks();
    looks.vm    = &vm;
    looks.count = vm.symbols().size();
    Members m;
    m.visual      = memberIndex(vm,"C_ITEM.VISUAL");
    m.name        = memberIndex(vm,"C_ITEM.NAME");
    m.description = memberIndex(vm,"C_ITEM.DESCRIPTION");
    m.value       = memberIndex(vm,"C_ITEM.VALUE");
    m.mainFlag    = memberIndex(vm,"C_ITEM.MAINFLAG");
    m.munition    = memberIndex(vm,"C_ITEM.MUNITION");
    m.owner       = memberIndex(vm,"C_ITEM.OWNER");
    m.magic       = memberIndex(vm,"C_ITEM.MAGIC");
    m.onEquip     = memberIndex(vm,"C_ITEM.ON_EQUIP");
    m.onUnequip   = memberIndex(vm,"C_ITEM.ON_UNEQUIP");
    m.onState     = memberIndex(vm,"C_ITEM.ON_STATE");
    const auto t0 = std::chrono::steady_clock::now();
    for(uint32_t i=0; i<looks.count; ++i) {
      auto* s = vm.find_symbol_by_index(i);
      if(s==nullptr || !isItemClass(vm,*s))
        continue;
      ItemLook look;
      look.symbol = i;
      // the prototype first (instance X(ItemPR_Y)), then the instance
      if(auto* p = vm.find_symbol_by_index(s->parent()); p!=nullptr && p->type()==DaedalusDataType::PROTOTYPE)
        readLook(vm,m,p->address(),look,0);
      readLook(vm,m,s->address(),look,0);
      if(look.visual.empty() || look.name.empty())
        continue;
      const size_t id = looks.items.size();
      looks.strict[keyStrict(look.visual,look.name,look.value)].push_back(id);
      looks.loose [keyLoose (look.visual,look.name)].push_back(id);
      looks.items.push_back(std::move(look));
      }
    const uint64_t ms = uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-t0).count());
    Log::i("[save] the looks of ",looks.items.size()," items of the scripts (",ms," ms)");
    }
  const ItemLook* found = nullptr;
  for(int pass=0; pass<2 && found==nullptr; ++pass) {
    auto& table = pass==0 ? looks.strict : looks.loose;
    auto  it    = table.find(pass==0 ? keyStrict(saved.visual,saved.name,saved.value) : keyLoose(saved.visual,saved.name));
    if(it==table.end())
      continue;
    if(it->second.size()!=1)
      return nullptr;   // more items look so: not known which one
    found = &looks.items[it->second[0]];
    }
  if(found==nullptr)
    return nullptr;
  auto* sym = vm.find_symbol_by_index(found->symbol);
  if(sym==nullptr)
    return nullptr;
  // the fields of the scripts of that item (the rest of the item comes from the save-game)
  auto ret = std::make_shared<zenkit::IItem>();
  try {
    vm.allocate_instance(ret,sym);
    }
  catch(const std::exception&) {
    return nullptr;
    }
  ret->magic      = found->magic;
  ret->on_equip   = found->onEquip;
  ret->on_unequip = found->onUnequip;
  for(size_t i=0; i<zenkit::IItem::state_count; ++i)
    ret->on_state[i] = found->onState[i];
  ret->owner      = found->owner;
  ret->munition   = found->munition;
  return ret;
  }

namespace {

struct NpcLooks {
  const zenkit::DaedalusVm*                                vm    = nullptr;
  size_t                                                   count = 0;
  std::unordered_map<std::string,std::vector<uint32_t>>    byKey;
  };

bool isNpcClass(zenkit::DaedalusVm& vm, const zenkit::DaedalusSymbol& s) {
  if(s.type()!=DaedalusDataType::INSTANCE)
    return false;
  auto* p = vm.find_symbol_by_index(s.parent());
  for(int depth=0; p!=nullptr && p->type()!=DaedalusDataType::CLASS && depth<8; ++depth)
    p = vm.find_symbol_by_index(p->parent());
  return p!=nullptr && p->name()=="C_NPC";
  }

/// the id and the name that the code of the instance writes (only its own lines, the functions it calls are not read)
bool readNpcLook(zenkit::DaedalusVm& vm, uint32_t address, uint32_t mId, uint32_t mName, int32_t& id, std::string& name) {
  using Op = zenkit::DaedalusOpcode;
  struct Val { int kind = 0; int32_t i = 0; uint32_t sym = 0; uint8_t index = 0; };
  std::vector<Val> st;
  auto pop = [&st]() { Val v; if(!st.empty()) { v = st.back(); st.pop_back(); } return v; };
  bool hasId = false, hasName = false;
  const uint32_t end = vm.size();
  for(int n=0; n<4000 && address<end && !(hasId && hasName); ++n) {
    const auto in = vm.instruction_at(address);
    address += in.size;
    switch(in.op) {
      case Op::PUSHI:  st.push_back({1,in.immediate,0,0}); break;
      case Op::PUSHV:
      case Op::PUSHVI: st.push_back({2,0,in.symbol,0}); break;
      case Op::PUSHVV: st.push_back({2,0,in.symbol,in.index}); break;
      case Op::MOVI: {
        const Val dst = pop(), src = pop();
        if(dst.kind!=2 || dst.sym!=mId)
          break;
        if(src.kind==1) {
          id = src.i;
          hasId = true;
          }
        else if(src.kind==2) {
          auto* s = vm.find_symbol_by_index(src.sym);
          if(s!=nullptr && s->type()==DaedalusDataType::INT && s->is_const() && src.index<s->count()) {
            id    = s->get_int(src.index);
            hasId = true;
            }
          }
        break;
        }
      case Op::MOVS: {
        const Val dst = pop(), src = pop();
        if(dst.kind!=2 || dst.sym!=mName || dst.index!=0 || src.kind!=2)
          break;
        auto* s = vm.find_symbol_by_index(src.sym);
        if(s!=nullptr && s->type()==DaedalusDataType::STRING && s->is_const() && src.index<s->count()) {
          name    = s->get_string(src.index);
          hasName = true;
          }
        break;
        }
      case Op::BL: case Op::BE:
        st.clear();
        st.push_back({});
        break;
      case Op::RSR: case Op::B: case Op::BZ:
        return hasId && hasName;
      case Op::GMOVI: case Op::NOP:
        break;
      case Op::PLUS: case Op::NEGATE: case Op::NOT: case Op::CMPL:
        pop(); st.push_back({});
        break;
      case Op::MOVSS: case Op::MOVF: case Op::MOVVF: case Op::MOVVI:
      case Op::ADDMOVI: case Op::SUBMOVI: case Op::MULMOVI: case Op::DIVMOVI:
        pop(); pop();
        break;
      default:
        pop(); pop(); st.push_back({});
        break;
      }
    }
  return hasId && hasName;
  }

}

zenkit::DaedalusSymbol* SaveSymbols::npcByLook(zenkit::DaedalusVm& vm, int32_t id, std::string_view name) {
  static NpcLooks looks;
  if(looks.vm!=&vm || looks.count!=vm.symbols().size()) {
    looks = NpcLooks();
    looks.vm    = &vm;
    looks.count = vm.symbols().size();
    const uint32_t mId   = memberIndex(vm,"C_NPC.ID");
    const uint32_t mName = memberIndex(vm,"C_NPC.NAME");
    for(uint32_t i=0; i<looks.count; ++i) {
      auto* s = vm.find_symbol_by_index(i);
      if(s==nullptr || !isNpcClass(vm,*s))
        continue;
      int32_t     nid = 0;
      std::string nname;
      if(readNpcLook(vm,s->address(),mId,mName,nid,nname))
        looks.byKey[std::to_string(nid)+"|"+nname].push_back(i);
      }
    }
  auto it = looks.byKey.find(std::to_string(id)+"|"+std::string(name));
  if(it==looks.byKey.end() || it->second.size()!=1)
    return nullptr;
  return vm.find_symbol_by_index(it->second[0]);
  }

void SaveSymbols::scriptsReport(zenkit::DaedalusVm& vm) {
  static bool done = false;
  if(done)
    return;
  done = true;

  auto&          dat   = vm.symbols();
  const uint64_t hash  = fingerprint(vm);
  const char*    last  = "nb_symbols_last.txt";
  const char*    lastH = "nb_symbols_last.hash";

  // the unbound functions of the plugin and of the engine that the scripts call (RX_* are listed by the log of the
  // externals of NB); NB fix 213: only the called ones - most of the rest are the unused functions of Gothic 1
  {
    std::vector<bool> called(dat.size(),false);
    for(uint32_t a=0, end=vm.size(); a<end;) {
      const auto in = vm.instruction_at(a);
      if(in.op==zenkit::DaedalusOpcode::BE && in.symbol<called.size())
        called[in.symbol] = true;
      a += std::max<uint32_t>(in.size,1);
      }
    std::string list;
    size_t      n = 0;
    for(auto& s:dat) {
      if(!s.is_external() || vm.has_external(&s) || s.name().rfind("RX_",0)==0 || !called[s.index()])
        continue;
      if(n<60)
        list += s.name()+" ";
      ++n;
      }
    if(n>0)
      Log::i("[scripts] functions of the engine or of the plugin that the scripts call and the port does not have: ",n,": ",list,
             n>60 ? "..." : "");
  }

  uint64_t lastHash = 0;
  {
    std::ifstream in(lastH);
    in >> lastHash;
  }
  if(lastHash==hash)
    return;

  std::vector<std::string> now(dat.size());
  for(size_t i=0; i<dat.size(); ++i)
    if(!isTemp(dat[i].name()))
      now[i] = dat[i].name();

  std::vector<std::string> before;
  {
    std::ifstream in(last,std::ios::binary);
    std::string   line;
    while(std::getline(in,line)) {
      if(!line.empty() && line.back()=='\r')
        line.pop_back();
      before.push_back(line);
      }
  }

  if(lastHash!=0 && !before.empty()) {
    std::unordered_map<std::string,bool> inNow;
    for(auto& s:dat)
      if(refByName(s))
        inNow[s.name()] = true;
    std::unordered_map<std::string,bool> inBefore;
    for(auto& n:before)
      if(!n.empty())
        inBefore[n] = true;

    std::string removed, addedExt;
    size_t      nRemoved = 0, nAdded = 0, nExt = 0;
    for(auto& n:before) {
      if(n.empty() || inNow.count(n)>0 || n.find('.')!=std::string::npos)
        continue;
      auto* s = vm.find_symbol_by_name(n);
      if(s!=nullptr)
        continue;   // a variable now, or the same name
      if(nRemoved<40)
        removed += n+" ";
      ++nRemoved;
      }
    for(auto& s:dat) {
      if(!refByName(s) || inBefore.count(s.name())>0 || s.name().find('.')!=std::string::npos)
        continue;
      ++nAdded;
      if(s.is_external()) {
        addedExt += s.name()+(vm.has_external(&s) ? "" : " (not in the port)")+" ";
        ++nExt;
        }
      }
    Log::i("[scripts] the scripts are not the scripts of the last start (",before.size()," symbols, now ",dat.size(),"): ",
           nAdded," new functions and instances, ",nRemoved," removed",nRemoved>0 ? ": " : "",removed,nRemoved>40 ? "..." : "");
    if(nExt>0)
      Log::i("[scripts] new functions of the plugin: ",addedExt);
    // the names of the old scripts, for the save-games without game/symtab
    const std::string keep = "old_symbols_"+std::to_string(lastHash)+".txt";
    std::ofstream out(keep,std::ios::binary);
    for(auto& n:before)
      out << n << '\n';
    Log::i("[scripts] the names of the old scripts are kept in ",keep);
  }

  {
    std::ofstream out(last,std::ios::binary);
    for(auto& n:now)
      out << n << '\n';
  }
  {
    std::ofstream out(lastH);
    out << hash << '\n';
  }
  }
