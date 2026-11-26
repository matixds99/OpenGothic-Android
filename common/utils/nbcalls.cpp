#include "nbcalls.h"

#include <zenkit/DaedalusVm.hh>
#include <zenkit/addon/daedalus.hh>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>

namespace {
struct Entry { int64_t last = -1000000; };

struct State {
  std::unordered_map<std::string,Entry> list;
  int64_t       minMs = 250;
  std::ofstream out;
  std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
  bool          loaded = false;
  };

State& state() {
  static State s;
  return s;
  }

void load(State& s) {
  s.loaded = true;
  std::ifstream in("nbcalls.cfg");
  if(!in)
    in.open("nblog.cfg");
  if(!in)
    return;
  std::string line;
  while(std::getline(in,line)) {
    while(!line.empty() && (line.back()=='\r' || line.back()==' '))
      line.pop_back();
    if(line.rfind("MINMS",0)==0) {
      s.minMs = std::atoi(line.c_str()+5);
      continue;
      }
    if(line.empty() || line[0]<'0' || line[0]>'9')
      continue;
    auto sp = line.find(' ');
    if(sp==std::string::npos)
      continue;
    s.list[line.substr(sp+1)] = Entry();
    }
  if(!s.list.empty())
    s.out.open("nbcalls.txt",std::ios::app);
  }

std::string npcName(zenkit::DaedalusSymbol* sym) {
  if(sym==nullptr)
    return {};
  auto inst = std::dynamic_pointer_cast<zenkit::INpc>(sym->get_instance());
  return inst==nullptr ? std::string() : inst->name[0];
  }
}

void NbCalls::hook(zenkit::DaedalusVm& vm) {
  auto& s = state();
  if(!s.loaded)
    load(s);
  if(s.list.empty() || !s.out.is_open())
    return;
  s.out << "nblog: start (OpenGothic)\r\n";
  vm.on_call = [](zenkit::DaedalusVm& v, zenkit::DaedalusSymbol const* sym) {
    auto& st = state();
    auto it = st.list.find(sym->name());
    if(it==st.list.end())
      return;
    const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-st.t0).count();
    if(now-it->second.last<st.minMs)
      return;
    it->second.last = now;
    st.out << '[' << now << "] " << sym->name();
    if(auto a = npcName(v.global_self()); !a.empty())
      st.out << " self=" << a;
    if(auto b = npcName(v.global_other()); !b.empty())
      st.out << " other=" << b;
    st.out << "\r\n";
    st.out.flush();
    };
  }

void NbCalls::event(const char* what, const std::string& self) {
  auto& s = state();
  if(!s.loaded || !s.out.is_open())
    return;
  const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-s.t0).count();
  s.out << '[' << now << "] PLUGIN " << what << " self=" << self << "\r\n";
  s.out.flush();
  }
