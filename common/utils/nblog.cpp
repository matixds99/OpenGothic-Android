#include "nblog.h"

#include <Tempest/Log>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <string_view>
#include <vector>

#include "game/gamescript.h"
#include "world/objects/npc.h"
#include "world/world.h"
#include "gothic.h"

using namespace Tempest;

namespace {

// the kinds of the problems in the summary, in its order
enum Kind : int {
  KCrash,       // the crash of the previous start
  KMissingFn,   // a function of the plugin that is not in the port, called by a script
  KScript,      // an error of the scripts (zenkit: a member without an instance, an exception)
  KStuck,       // a character that stands with a target, starts its state again and again, its AI queue does not move
  KAsset,       // a file of the game that is not there (a sound, a texture, a model)
  KError,       // any other error line
  KData,        // the warnings of the loading of the data (the parts of the files that are not read fully)
  KCount
  };

const char* const KindName[KCount] = {
  "crash of the previous start", "functions of the plugin missing in the port", "script errors",
  "characters stuck in a fight or a state", "missing files (sounds, textures, models)", "other errors",
  "data warnings while loading",
  };

struct Issue {
  uint64_t count = 0;
  uint64_t first = 0;   // ms since the start
  };

struct State {
  std::mutex                               sync;
  std::map<std::string,Issue>              issues[KCount];
  uint64_t                                 kindTotal[KCount] = {};
  std::deque<std::string>                  hidden;      // the last lines kept out of log.txt
  uint64_t                                 hiddenCount = 0;
  int                                      marks = 0;
  };

State& state() {
  static State s;
  return s;
  }

std::atomic<int> logLevel{NbLog::Normal};

uint64_t sinceStart() {
  static const auto t0 = std::chrono::steady_clock::now();
  return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-t0).count());
  }

bool startsWith(std::string_view s, std::string_view p) {
  return s.size()>=p.size() && s.substr(0,p.size())==p;
  }

// the text between the first pair of quotes, or between [ and ]
std::string_view quoted(std::string_view s, char open, char close) {
  const auto a = s.find(open);
  if(a==std::string_view::npos)
    return {};
  const auto b = s.find(close,a+1);
  if(b==std::string_view::npos)
    return {};
  return s.substr(a+1,b-a-1);
  }

std::string clip(std::string_view s, size_t n) {
  // the counter of the repeated lines is not a part of the problem: "... (x10 so far)"
  const auto rep = s.find(" (x");
  if(rep!=std::string_view::npos && s.find(" so far)",rep)!=std::string_view::npos)
    s = s.substr(0,rep);
  std::string ret(s.substr(0,n));
  if(s.size()>n)
    ret += "...";
  return ret;
  }

void add(Kind k, std::string key) {
  auto& st = state();
  std::lock_guard<std::mutex> g(st.sync);
  st.kindTotal[k]++;
  auto& m  = st.issues[k];
  auto  it = m.find(key);
  if(it==m.end()) {
    if(m.size()>=200)
      return;   // the summary keeps the first 200 problems of a kind
    it = m.emplace(std::move(key),Issue{0,sinceStart()}).first;
    }
  it->second.count++;
  }

std::string fmtTime(uint64_t ms) {
  const uint64_t s = ms/1000;
  char buf[32];
  std::snprintf(buf,sizeof(buf),"%u:%02u",unsigned(s/60),unsigned(s%60));
  return buf;
  }

}

uint64_t NbLog::sinceStartMs() {
  return sinceStart();
  }

void NbLog::setLevel(int lv) {
  logLevel.store(std::clamp(lv,0,2));
  }

int NbLog::level() {
  return logLevel.load();
  }

void NbLog::observe(bool error, const char* text) {
  if(text==nullptr)
    return;
  const std::string_view s(text);
  if(startsWith(s,"[crash]") || startsWith(s,"[summary]") || startsWith(s,"====") || startsWith(s,"[mark "))
    return;   // the lines of this logger
  if(s.find("not implemented call [")!=std::string_view::npos) {
    add(KMissingFn,std::string(quoted(s,'[',']')));
    return;
    }
  if(startsWith(s,"[thrash] ") || startsWith(s,"[idle] ") || s.find(" STUCK 5 s")!=std::string_view::npos) {
    // the name of the character and its state: "[idle] Name stands ..." / "NB ai: Name STUCK 5 s: state=ZS_X"
    std::string key;
    if(startsWith(s,"NB ai: ")) {
      const auto e  = s.find(" STUCK 5 s");
      const auto st = s.find("state=");
      key = std::string(s.substr(7,e-7));
      if(st!=std::string_view::npos)
        key += " ("+std::string(s.substr(st+6,s.find(' ',st)-st-6))+", the AI queue does not move)";
      } else {
      key = clip(s,110);
      }
    add(KStuck,std::move(key));
    return;
    }
  if(startsWith(s,"unable to load archive")) {
    add(KData,"archives that are not game archives (the DLLs and ZIPs of Union in Data)");   // NB fix 204
    return;
    }
  if(startsWith(s,"unable to load sound fx")) {
    return;   // NB fix 204: the file of the sound is in the line before it
    }
  if(startsWith(s,"unable to load")) {
    // unable to load sound "x.wav" / unable to load sound fx: NAME / unable to load texture ...
    std::string_view name = quoted(s,'"','"');
    if(name.empty()) {
      const auto c = s.rfind(": ");
      name = c!=std::string_view::npos ? s.substr(c+2) : s;
      }
    const auto kind = s.substr(15,s.find_first_of(" :\"",15)-15);
    add(KAsset,std::string(kind)+" "+clip(name,80));
    return;
    }
  if(startsWith(s,"[zenkit] ")) {
    const auto m = s.substr(9);
    if(m.find("bytes remaining")!=std::string_view::npos || m.find("bytes overflowed")!=std::string_view::npos ||
       m.find("not fully parsed")!=std::string_view::npos || m.find("Unknown volume flags")!=std::string_view::npos ||
       m.find("Unexpected value")!=std::string_view::npos || startsWith(m,"Parsing ") ||
       m.find("VDFS tool disk")!=std::string_view::npos) {
      add(KData,clip(m,60));
      return;
      }
    add(KScript,clip(m,140));
    return;
    }
  if(startsWith(s,"NB: ") && s.find(" failed")!=std::string_view::npos) {
    add(KScript,clip(s,140));
    return;
    }
  if(error && !startsWith(s,"DMG2")) {
    // NB fix 204: not the broken rest of a binary path
    size_t letters = 0;
    for(char c:s)
      if((c>='a' && c<='z') || (c>='A' && c<='Z'))
        ++letters;
    if(letters>=4)
      add(KError,clip(s,120));
    }
  }

bool NbLog::filter(const char* text) {
  if(text==nullptr)
    return true;
  const std::string_view s(text);
  const int lv = logLevel.load();
  if(lv>=Full)
    return true;
  bool keep = true;
  if(lv<=Quiet) {
    if(startsWith(s,"NB ai: ") && s.find(" STUCK 5 s")==std::string_view::npos)
      keep = false;
    else if(startsWith(s,"NB trace: ") || startsWith(s,"DMG2") || startsWith(s,"[hero-ai] ") ||
            startsWith(s,"vkCreateGraphicsPipelines") || startsWith(s,"[zenkit] Parsing ") || startsWith(s,"[rain] "))
      keep = false;
    }
  if(keep)
    return true;
  auto& st = state();
  std::lock_guard<std::mutex> g(st.sync);
  st.hiddenCount++;
  st.hidden.emplace_back(s.substr(0,300));
  if(st.hidden.size()>40)
    st.hidden.pop_front();
  return false;
  }

std::string NbLog::recentHidden(bool crash) {
  auto& st = state();
  std::unique_lock<std::mutex> g(st.sync,std::defer_lock);
  if(crash ? !g.try_lock() : (g.lock(),false))
    return "(not available: the log was busy at the crash)\n";
  std::string ret;
  for(auto& l:st.hidden)
    ret += l+"\n";
  return ret;
  }

std::string NbLog::summary(bool crash) {
  auto& st = state();
  std::unique_lock<std::mutex> g(st.sync,std::defer_lock);
  if(crash ? !g.try_lock() : (g.lock(),false))
    return "(not available: the log was busy at the crash)\n";
  std::string ret;
  size_t total = 0;
  for(int k=0; k<KCount; ++k)
    total += st.issues[k].size();
  ret += "problems: "+std::to_string(total)+" kinds, the log level "+std::to_string(logLevel.load())+
         " (0 quiet, 1 normal, 2 full), lines kept out of the log: "+std::to_string(st.hiddenCount)+
         ", markers: "+std::to_string(st.marks)+"\n";
  for(int k=0; k<KCount; ++k) {
    auto& m = st.issues[k];
    if(m.empty())
      continue;
    ret += "- "+std::string(KindName[k])+": "+std::to_string(m.size())+" ("+std::to_string(st.kindTotal[k])+" lines)\n";
    // the most frequent first; the data warnings only as the count of the lines
    std::vector<std::pair<std::string,Issue>> v(m.begin(),m.end());
    std::sort(v.begin(),v.end(),[](auto& a, auto& b){ return a.second.count>b.second.count; });
    const size_t show = k==KData ? 3 : 25;
    for(size_t i=0; i<v.size() && i<show; ++i)
      ret += "    x"+std::to_string(v[i].second.count)+"  "+v[i].first+"  (first at "+fmtTime(v[i].second.first)+")\n";
    if(v.size()>show)
      ret += "    ... "+std::to_string(v.size()-show)+" more\n";
    }
  return ret;
  }

void NbLog::writeSummary(const char* why) {
  const std::string text = summary();
  Log::i("==== NB summary (",why!=nullptr ? why : "",", ",fmtTime(sinceStart())," since the start) ====");
  size_t a = 0;
  while(a<text.size()) {
    auto b = text.find('\n',a);
    if(b==std::string::npos)
      b = text.size();
    Log::i("[summary] ",std::string_view(text).substr(a,b-a));
    a = b+1;
    }
  Log::i("==== end of the summary ====");
  }

int NbLog::mark() {
  int n = 0;
  {
    auto& st = state();
    std::lock_guard<std::mutex> g(st.sync);
    n = ++st.marks;
  }
  auto* w  = Gothic::inst().world();
  auto* pl = Gothic::inst().player();
  Log::i("==== [mark ",n,"] at ",fmtTime(sinceStart())," since the start ====");
  if(w==nullptr || pl==nullptr) {
    Log::i("[mark ",n,"] no world");
    return n;
    }
  const auto t  = w->time();
  const auto p  = pl->position();
  char tm[48];
  std::snprintf(tm,sizeof(tm),"day %d %02d:%02d",int(t.day()),int(t.hour()),int(t.minute()));
  Log::i("[mark ",n,"] world ",w->name(),", ",tm,", the hero at ",int(p.x)," ",int(p.y)," ",int(p.z),
         ", life ",pl->attribute(ATR_HITPOINTS),"/",pl->attribute(ATR_HITPOINTSMAX),", mana ",pl->attribute(ATR_MANA),"/",
         pl->attribute(ATR_MANAMAX),", weapon ",int(pl->weaponState()),", body ",int(pl->bodyStateMasked()));
  if(auto* tg = pl->target())
    Log::i("[mark ",n,"] the target of the hero: ",tg->displayName()," at ",int(std::sqrt(pl->qDistTo(*tg)))," cm, life ",
           tg->attribute(ATR_HITPOINTS),"/",tg->attribute(ATR_HITPOINTSMAX));
  // the characters around the hero, the nearest first
  std::vector<std::pair<float,Npc*>> near;
  w->detectNpc(p,3000.f,[&](Npc& n) {
    if(&n!=pl)
      near.emplace_back(pl->qDistTo(n),&n);
    });
  std::sort(near.begin(),near.end(),[](auto& a, auto& b){ return a.first<b.first; });
  for(size_t i=0; i<near.size() && i<12; ++i) {
    Npc& c = *near[i].second;
    auto* tg = c.target();
    Log::i("[mark ",n,"]   ",c.displayName()," ",int(std::sqrt(near[i].first))," cm: state ",c.nbStateName(),
           ", life ",c.attribute(ATR_HITPOINTS),"/",c.attribute(ATR_HITPOINTSMAX),
           c.isDead() ? " (dead)" : c.isDown() ? " (down)" : "",", target ",
           tg==nullptr ? std::string("-") : tg->isPlayer() ? std::string("hero") : std::string(tg->displayName()),
           c.nbFightInfo(),", weapon ",int(c.weaponState()),", body ",int(c.bodyStateMasked()),", ai [",c.nbAiQueueDesc(),"]");
    }
  if(near.size()>12)
    Log::i("[mark ",n,"]   ... ",near.size()-12," more within 30 m");
  return n;
  }

void NbLog::reportPreviousCrash() {
  std::error_code ec;
  if(!std::filesystem::exists("crash.log",ec))
    return;
  const auto mt   = std::filesystem::last_write_time("crash.log",ec);
  if(ec)
    return;
  const auto tick = int64_t(mt.time_since_epoch().count());
  int64_t seen = 0;
  if(std::ifstream in("crash.seen"); in)
    in >> seen;
  if(tick==seen)
    return;   // reported already
  std::ifstream in("crash.log");
  std::string text((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
  if(text.size()>6000)
    text = text.substr(0,6000)+"\n...";
  Log::e("[crash] the previous start crashed - crash.log:");
  size_t a = 0;
  std::string first;
  while(a<text.size()) {
    auto b = text.find('\n',a);
    if(b==std::string::npos)
      b = text.size();
    const auto line = std::string_view(text).substr(a,b-a);
    if(first.empty() && !line.empty())
      first = std::string(line);
    Log::i("[crash]   ",line);
    a = b+1;
    }
  add(KCrash,clip(first,100));
  if(std::ofstream out("crash.seen"); out)
    out << tick;
  }
