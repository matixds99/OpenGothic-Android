#include "worldstatestorage.h"

#include <Tempest/MemWriter>
#include <Tempest/MemReader>

#include "gamesession.h"
#include "world/world.h"
#include "serialize.h"

WorldStateStorage::WorldStateStorage(World &w)
  :name(w.name()){
  Tempest::MemWriter wr{storage};
  Serialize          sr{wr};
  w.save(sr);
  }

void WorldStateStorage::save(Serialize &fout) const {
  fout.setEntry("worlds/",name,".zip");
  fout.write(storage);
  if(symMap!=nullptr) {
    // NB fix 209: the world was saved with older scripts: its symbols -> the symbols of this save-game
    fout.setEntry("worlds/",name,".symmap");
    fout.write(uint8_t(1),*symMap);
    }
  }

void WorldStateStorage::load(Serialize& fin) {
  fin.requireEntry("worlds/",name,".zip");
  fin.read(storage);
  symMap = nullptr;
  if(fin.setEntry("worlds/",name,".symmap")) {
    uint8_t version = 0;
    auto    map     = std::make_shared<std::vector<uint32_t>>();
    fin.read(version);
    if(version==1) {
      fin.read(*map);
      symMap = std::move(map);
      }
    }
  }

bool WorldStateStorage::compareName(std::string_view n) const {
  if(n.size()!=name.size())
    return false;
  for(size_t i=0; i<n.size(); ++i) {
    auto a = std::tolower(n[i]);
    auto b = std::tolower(name[i]);
    if(a!=b)
      return false;
    }
  return true;
  }
