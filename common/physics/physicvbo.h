#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <set>

#include <zenkit/Material.hh>

#include "physics/physics.h"
#include "resources.h"

class PackedMesh;

class PhysicVbo : public btTriangleIndexVertexArray {
  public:
    using Vertex = Resources::Vertex;

    PhysicVbo(PackedMesh&& packed);
    PhysicVbo(const std::vector<btVector3>* v);

    PhysicVbo(const PhysicVbo&)=delete;
    PhysicVbo(PhysicVbo&&)=delete;
    ~PhysicVbo();

    /// NB fix 139: RX_SetMaterialCollision of New Balance - the triangles of a material collide or not, in every mesh of
    /// the collision (the land and the objects), now and in the meshes made later. The name is compared without the case;
    /// true when a mesh has changed
    static bool             setMaterialCollision(std::string_view material, bool collide);
    /// the materials switched off are taken out (before the shape is built)
    bool                    applyMaterialCollision();
    /// NB fix 141: the segment added last has no collision in the data (a switchable material of New Balance)
    void                    nbMarkLastNoCollision(bool v) { if(!segments.empty()) segments.back().dataOff = v; }

    void                    addIndex(const std::vector<uint32_t>& index, size_t iboOff, size_t iboLen, zenkit::MaterialGroup material);
    void                    addIndex(const std::vector<uint32_t>& index, size_t iboOff, size_t iboLen, zenkit::MaterialGroup material, const char* sector);
    zenkit::MaterialGroup   materialId(size_t segment) const;
    auto                    sectorName(size_t segment) const -> const char*;
    bool                    useQuantization() const;
    bool                    isEmpty() const;

    void                    adjustMesh();

    std::string_view        validateSectorName(std::string_view name) const;

  private:
    PhysicVbo(const std::vector<Vertex>& v);

    void addSegment(size_t indexSize,size_t offset, zenkit::MaterialGroup material, const char* sector,
                    std::string_view materialName = {});

    struct Segment {
      size_t                off;
      int                   size;
      zenkit::MaterialGroup mat;
      const char*           sector=nullptr;
      std::string           material;   // NB fix 139: the name, in the upper case
      bool                  off_ = false;
      bool                  dataOff = false;   // NB fix 141: the material has no collision in the data
      };

    static std::set<PhysicVbo*>&  all();
    static std::set<std::string>& disabledMaterials();
    static std::set<std::string>& enabledMaterials();
  public:
    static bool                   nbSwitchable(std::string_view material);
  private:

    std::vector<uint32_t>         idOrig;   // NB fix 139: the triangles before a material was taken out
    std::vector<btVector3>        vStorage;
    const std::vector<btVector3>& vert;
    std::vector<uint32_t>         id;
    std::vector<Segment>          segments;
  };
