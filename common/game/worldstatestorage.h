#pragma once

#include <Tempest/ODevice>

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

class World;
class GameSession;
class Serialize;

class WorldStateStorage final {
  public:
    WorldStateStorage()=default;
    WorldStateStorage(World &w);
    WorldStateStorage(const WorldStateStorage&)=delete;
    WorldStateStorage(WorldStateStorage&&)=default;
    WorldStateStorage& operator = (WorldStateStorage&&)=default;

    bool                 isEmpty() const { return storage.empty(); }
    void                 save(Serialize& fout) const;
    void                 load(Serialize& fin);

    bool                 compareName(std::string_view name) const;

    std::string          name;
    std::vector<uint8_t> storage;
    /// NB fix 209: the symbols of the scripts of this storage -> the symbols now (nullptr: the same scripts)
    std::shared_ptr<const std::vector<uint32_t>> symMap;
  };
