#include "musicdefinitions.h"

#include <Tempest/Log>

#include "gothic.h"
#include "resources.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>

using namespace Tempest;

MusicDefinitions::MusicDefinitions() {
  vm = Gothic::inst().createPhoenixVm("Music.dat");

  vm->enumerate_instances_by_class_name("C_MusicTheme", [this](zenkit::DaedalusSymbol& s) {
    themes.push_back(vm->init_instance<zenkit::IMusicTheme>(&s));
    });
  try {
    loadRecordedMusic();
    }
  catch(const std::exception& e) {
    recorded.clear();
    recordedIds.clear();
    recordedThemes.clear();
    Log::e("CoM music definitions: ",e.what());
    }
  try {
    loadBassMusic();
    }
  catch(const std::exception& e) {
    bass.clear();
    bassThemes.clear();
    Log::e("NB music: ",e.what());
    }
  }

MusicDefinitions::~MusicDefinitions() {
  }

const zenkit::IMusicTheme* MusicDefinitions::operator [](std::string_view name) const {
  std::string key(name);
  for(auto& c:key)
    c = char(std::toupper(static_cast<unsigned char>(c)));
  if(auto it = recordedThemes.find(key); it!=recordedThemes.end())
    return &it->second;
  // NB fix 183: BassMusicController replaces the themes of Music.dat by their names (and adds the missing ones)
  if(auto it = bassThemes.find(key); it!=bassThemes.end())
    return &it->second;
  if(!vm)
    return nullptr;

  auto id = vm->find_symbol_by_name(name);
  if(id==nullptr)
    return nullptr;
  for(auto& i:themes) {
    auto* sym = vm->find_symbol_by_instance(i);
    if(sym->index() == id->index())
      return i.get();
    }
  return nullptr;
  }

const MusicDefinitions::RecordedTrack* MusicDefinitions::recordedTrack(std::string_view file) const {
  auto it = recorded.find(std::string(file));
  return it!=recorded.end() ? &it->second : nullptr;
  }

const MusicDefinitions::RecordedTrack* MusicDefinitions::recordedTrack(uint32_t instance) const {
  auto it = recordedIds.find(instance);
  return it!=recordedIds.end() ? recordedTrack(it->second) : nullptr;
  }

const MusicDefinitions::BassTrack* MusicDefinitions::bassTrack(std::string_view file) const {
  if(bass.empty())
    return nullptr;
  auto it = bass.find(std::string(file));
  return it!=bass.end() ? &it->second : nullptr;
  }

// NB fix 183: the plugin BassMusicController of New Balance reads the themes from System/Autorun/BASSMUSICCONTROLLER_*.D
// (instance NAME(C_MUSICTHEME_...) { file = "a.mp3|b.mp3"; vol = 0.3; }) and plays the mp3 files of the theme instead of
// the DirectMusic theme of the same name. Music.dat of the mod points some zones to .sgt files that are not in the game
// (DSP_DAY_STD: DarkSagaPirateCamp.sgt of the valley of Adanos) - there was no music in those places
void MusicDefinitions::loadBassMusic() {
  std::vector<std::string> sources;
  auto add = [&](std::vector<uint8_t>&& data) {
    if(!data.empty())
      sources.emplace_back(reinterpret_cast<const char*>(data.data()),data.size());
    };
  if(const auto* dir = Resources::vdfsIndex().resolve("/SYSTEM/AUTORUN")) {
    for(auto& i:dir->children()) {
      std::string name = i.name();
      for(auto& c:name)
        c = char(std::toupper(static_cast<unsigned char>(c)));
      if(i.type()!=zenkit::VfsNodeType::FILE || name.rfind("BASSMUSICCONTROLLER",0)!=0 ||
         name.size()<2 || name.compare(name.size()-2,2,".D")!=0)
        continue;
      add(Resources::getFileData(i.name()));
      }
    }
  for(auto& s:sources)
    parseBassScript(s);
  if(!bassThemes.empty())
    Log::i("NB music: ",bassThemes.size()," BassMusicController themes, ",bass.size()," playlists");
  if(!bassMissing.empty())
    Log::e("NB music: ",bassMissing.size()," mp3 files not found, e.g. ",*bassMissing.begin());
  bassMissing.clear();
  }

void MusicDefinitions::parseBassScript(std::string_view src) {
  // comments out, the rest upper-case: the names of the instances and the fields are not case-sensitive in Daedalus
  std::string text;
  text.reserve(src.size());
  for(size_t i=0; i<src.size(); ++i) {
    if(src[i]=='/' && i+1<src.size() && src[i+1]=='/') {
      while(i<src.size() && src[i]!='\n')
        ++i;
      text.push_back('\n');
      continue;
      }
    if(src[i]=='/' && i+1<src.size() && src[i+1]=='*') {
      const size_t end = src.find("*/",i+2);
      i = end==std::string_view::npos ? src.size() : end+1;
      text.push_back(' ');
      continue;
      }
    if(src[i]=='"') {
      const size_t end = src.find('"',i+1);
      const size_t stop = end==std::string_view::npos ? src.size() : end+1;
      for(; i<stop; ++i)
        text.push_back(char(std::toupper(static_cast<unsigned char>(src[i]))));
      --i;
      continue;
      }
    text.push_back(char(std::toupper(static_cast<unsigned char>(src[i]))));
    }

  auto isId = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c=='_'; };
  size_t at = 0;
  while((at = text.find("INSTANCE",at))!=std::string::npos) {
    if(at>0 && isId(text[at-1])) {
      at += 8;
      continue;
      }
    size_t p = at+8;
    while(p<text.size() && std::isspace(static_cast<unsigned char>(text[p])))
      ++p;
    const size_t nameBeg = p;
    while(p<text.size() && isId(text[p]))
      ++p;
    const std::string name = text.substr(nameBeg,p-nameBeg);
    const size_t open  = text.find('{',p);
    const size_t close = open==std::string::npos ? std::string::npos : text.find('}',open);
    if(name.empty() || close==std::string::npos)
      break;
    at = close+1;

    const std::string body = text.substr(open+1,close-open-1);
    auto field = [&](std::string_view key) -> std::string {
      size_t f = 0;
      while((f = body.find(key,f))!=std::string::npos) {
        const bool start = f==0 || !isId(body[f-1]);
        size_t e = f+key.size();
        f = e;
        if(!start || (e<body.size() && isId(body[e])))
          continue;
        while(e<body.size() && std::isspace(static_cast<unsigned char>(body[e])))
          ++e;
        if(e>=body.size() || body[e]!='=')
          continue;
        const size_t semi = body.find(';',e);
        std::string v = body.substr(e+1,semi==std::string::npos ? std::string::npos : semi-e-1);
        const size_t b = v.find_first_not_of(" \t\r\n\"");
        const size_t l = v.find_last_not_of(" \t\r\n\"");
        return b==std::string::npos ? std::string() : v.substr(b,l-b+1);
        }
      return std::string();
      };

    const std::string file = field("FILE");
    if(file.empty())
      continue;
    BassTrack track;
    size_t s = 0;
    while(s<=file.size()) {
      size_t e = file.find('|',s);
      if(e==std::string::npos)
        e = file.size();
      std::string f = file.substr(s,e-s);
      const size_t b = f.find_first_not_of(" \t");
      const size_t l = f.find_last_not_of(" \t");
      if(b!=std::string::npos) {
        f = f.substr(b,l-b+1);
        // the plugin opens the files through the VDFS of the game: the name alone, the folder is not needed
        if(f.find('.')==std::string::npos)
          f += ".MP3";
        if(Resources::hasFile(f))
          track.files.push_back(std::move(f));
        else
          bassMissing.insert(f);
        }
      s = e+1;
      }
    if(track.files.empty())
      continue;   // the theme of Music.dat stays
    const std::string vol = field("VOL");
    if(!vol.empty()) {
      char* end = nullptr;
      const float v = std::strtof(vol.c_str(),&end);
      if(end!=vol.c_str() && v>0.f)
        track.vol = std::min(v,1.f);
      }

    zenkit::IMusicTheme theme;
    if(auto* orig = vm ? vm->find_symbol_by_name(name) : nullptr) {
      for(auto& i:themes)
        if(vm->find_symbol_by_instance(i)->index()==orig->index()) {
          theme = *i;
          break;
          }
      }
    theme.file = file;
    theme.vol  = track.vol;
    bass[file] = std::move(track);
    bassThemes[name] = std::move(theme);
    }
  }

void MusicDefinitions::loadRecordedMusic() {
  // CoM's KmLib reads MUSICTRACK/MUSICZONE from GOTHIC.DAT, not MUSIC.DAT.
  // A temporary VM initializes just these data instances; no mod startup runs.
  if(!Resources::hasFile("02.OGG"))
    return;
  auto scripts = Gothic::inst().createPhoenixVm(Gothic::inst().defaultGameDatFile());
  if(scripts->find_symbol_by_name("MUSICTRACK")==nullptr || scripts->find_symbol_by_name("MUSICZONE")==nullptr)
    return;
  struct Track : zenkit::DaedalusInstance {
    std::string file;
    int32_t loop=0, fadeOut=0, fadeIn=0;
    };
  struct Zone : zenkit::DaedalusInstance {
    int32_t day=0, night=0, dayFight=0, nightFight=0;
    };
  scripts->register_member("MUSICTRACK.FILENAME",&Track::file);
  scripts->register_member("MUSICTRACK.LOOPOFFSETDURATION",&Track::loop);
  scripts->register_member("MUSICTRACK.FADEOUTSLIDEDURATION",&Track::fadeOut);
  scripts->register_member("MUSICTRACK.FADEINSLIDEDURATION",&Track::fadeIn);
  scripts->register_member("MUSICZONE.DAY",&Zone::day);
  scripts->register_member("MUSICZONE.NIGHT",&Zone::night);
  scripts->register_member("MUSICZONE.DAYFIGHT",&Zone::dayFight);
  scripts->register_member("MUSICZONE.NIGHTFIGHT",&Zone::nightFight);
  scripts->enumerate_instances_by_class_name("MUSICTRACK",[&](zenkit::DaedalusSymbol& sym) {
    auto t = scripts->init_instance<Track>(&sym);
    if(t->file.empty())
      return;
    RecordedTrack track;
    track.theme.file = "_WORK/DATA/MUSIC/KM/"+t->file+".OGG";
    track.theme.vol = 1.f;
    track.theme.loop = 1;
    track.loopStart = std::max(t->loop,0);
    track.fadeOut = std::max(t->fadeOut,0);
    track.fadeIn = std::max(t->fadeIn,0);
    recordedIds[sym.index()] = track.theme.file;
    recorded[track.theme.file] = std::move(track);
    });
  scripts->enumerate_instances_by_class_name("MUSICZONE",[&](zenkit::DaedalusSymbol& sym) {
    auto z = scripts->init_instance<Zone>(&sym);
    const int32_t ids[] = {z->day,z->night,z->dayFight,z->nightFight};
    const char* suffix[] = {"_DAY_STD","_NGT_STD","_DAY_FGT","_NGT_FGT"};
    for(size_t i=0; i<4; ++i)
      if(const auto* track = recordedTrack(uint32_t(ids[i])))
        recordedThemes[sym.name()+suffix[i]] = track->theme;
    });
  RecordedTrack menu;
  menu.theme.file = "_WORK/DATA/MUSIC/KM/02.OGG";
  menu.theme.vol = 1.f;
  menu.theme.loop = 1;
  menu.fadeIn = 1000;
  menu.fadeOut = 2000;
  recordedThemes["SYS_MENU"] = menu.theme;
  recorded[menu.theme.file] = std::move(menu);
  Log::i("CoM music: ",recorded.size()," OGG tracks, ",recordedThemes.size()," zone/menu themes");
  }
