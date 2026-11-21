#include "physicvbo.h"

#include "graphics/mesh/submesh/packedmesh.h"
#include "collisionworld.h"

PhysicVbo::PhysicVbo(PackedMesh&& packed)
  :PhysicVbo(packed.vertices) {
  id = std::move(packed.indices);
  for(auto& i:packed.subMeshes)
    if(!i.material.disable_collision && i.iboLength>0) {
      addSegment(i.iboLength,i.iboOffset,i.material.group,nullptr,i.material.name);
      }
  for(size_t i=0;i<id.size();i+=3){
    std::swap(id[i+1],id[i+2]);
    }
  adjustMesh();
  applyMaterialCollision();   // NB fix 139
  }

PhysicVbo::~PhysicVbo() {
  all().erase(this);
  }

std::set<PhysicVbo*>& PhysicVbo::all() {
  static std::set<PhysicVbo*> v;
  return v;
  }

std::set<std::string>& PhysicVbo::disabledMaterials() {
  static std::set<std::string> v;
  return v;
  }

static std::string upperName(std::string_view s) {
  std::string r(s);
  for(auto& c:r)
    c = char(std::toupper(uint8_t(c)));
  return r;
  }

bool PhysicVbo::setMaterialCollision(std::string_view material, bool collide) {
  const auto name = upperName(material);
  if(collide)
    disabledMaterials().erase(name); else
    disabledMaterials().insert(name);
  bool changed = false;
  for(auto* vbo:all())
    changed |= vbo->applyMaterialCollision();
  return changed;
  }

bool PhysicVbo::applyMaterialCollision() {
  all().insert(this);
  const auto& off = disabledMaterials();
  bool changed = false;
  for(auto& sg:segments) {
    const bool want = !sg.material.empty() && off.find(sg.material)!=off.end();
    if(want==sg.off_)
      continue;
    if(idOrig.empty())
      idOrig = id;
    // a triangle taken out is made a point (all three corners in one vertex): it has no surface to stop anything
    for(size_t i=0; i<size_t(sg.size)*3; i+=3) {
      const size_t at = sg.off+i;
      if(want) {
        id[at+1] = id[at];
        id[at+2] = id[at];
        } else {
        id[at+0] = idOrig[at+0];
        id[at+1] = idOrig[at+1];
        id[at+2] = idOrig[at+2];
        }
      }
    sg.off_ = want;
    changed = true;
    }
  // the tree of the shape stays as it was built: its boxes hold the whole triangles, so they hold their points too, and
  // the triangles given back are the same as before
  return changed;
  }

PhysicVbo::PhysicVbo(const std::vector<Vertex>& v)
  :vStorage(v.size()), vert(vStorage) {
  for(size_t i=0;i<v.size();++i){
    vStorage[i] = CollisionWorld::toMeters(Tempest::Vec3(v[i].pos[0],v[i].pos[1],v[i].pos[2]));
    }
  }

PhysicVbo::PhysicVbo(const std::vector<btVector3>* v)
  :vert(*v) {
  }

void PhysicVbo::addIndex(const std::vector<uint32_t>& index, size_t iboOff, size_t iboLen, zenkit::MaterialGroup material) {
  addIndex(index,iboOff,iboLen,material,nullptr);
  }

void PhysicVbo::addIndex(const std::vector<uint32_t>& index, size_t iboOff, size_t iboLen, zenkit::MaterialGroup material,
                         const char* sector) {
  if(iboLen==0)
    return;

  size_t off = id.size();

  id.resize(off+iboLen);
  for(size_t i=0; i<iboLen; i+=3){
    id[off+i+0] = index[iboOff+i+0];
    id[off+i+1] = index[iboOff+i+2];
    id[off+i+2] = index[iboOff+i+1];
    }
  if(!idOrig.empty())   // NB fix 139: the copy of the triangles grows with them
    idOrig.insert(idOrig.end(),id.begin()+std::ptrdiff_t(off),id.end());

  addSegment(iboLen,off,material,sector,sector!=nullptr ? std::string_view(sector) : std::string_view());
  adjustMesh();
  }

void PhysicVbo::addSegment(size_t indexSize, size_t offset, zenkit::MaterialGroup material, const char* sector,
                           std::string_view materialName) {
  btIndexedMesh meshIndex={};
  meshIndex.m_numTriangles = int(indexSize/3);
  meshIndex.m_numVertices  = int32_t(vert.size());

  meshIndex.m_indexType           = PHY_INTEGER;
  meshIndex.m_triangleIndexBase   = reinterpret_cast<const uint8_t*>(&id[0]);
  meshIndex.m_triangleIndexStride = 3 * sizeof(id[0]);

  meshIndex.m_vertexBase          = reinterpret_cast<const uint8_t*>(&vert[0]);
  meshIndex.m_vertexStride        = sizeof(btVector3);

  m_indexedMeshes.push_back(meshIndex);

  Segment sgm;
  sgm.off    = offset;
  sgm.size   = int(indexSize/3);
  sgm.mat    = material;
  sgm.sector = sector;
  sgm.material = upperName(materialName);   // NB fix 139
  segments.push_back(sgm);
  }

zenkit::MaterialGroup PhysicVbo::materialId(size_t segment) const {
  if(segment<segments.size())
    return segments[segment].mat;
  return zenkit::MaterialGroup::UNDEFINED;
  }

const char* PhysicVbo::sectorName(size_t segment) const {
  if(segment<segments.size())
    return segments[segment].sector;
  return nullptr;
  }

bool PhysicVbo::useQuantization() const {
  constexpr int maxParts = (1 << MAX_NUM_PARTS_IN_BITS);
  constexpr int maxTri   = (1 << (31 - MAX_NUM_PARTS_IN_BITS));
  if(segments.size()>maxParts)
    return false;
  for(auto& i:segments)
    if(i.size>maxTri)
      return false;
  return true;
  }

bool PhysicVbo::isEmpty() const {
  return segments.size()==0;
  }

void PhysicVbo::adjustMesh(){
  for(int i=0;i<m_indexedMeshes.size();++i) {
    btIndexedMesh& meshIndex=m_indexedMeshes[i];
    Segment&       sg       =segments[size_t(i)];

    meshIndex.m_triangleIndexBase = reinterpret_cast<const uint8_t*>(&id[sg.off]);
    meshIndex.m_numTriangles      = sg.size;
    }
  }

std::string_view PhysicVbo::validateSectorName(std::string_view name) const {
  if(name.empty())
    return "";
  for(auto& i:segments)
    if(name==i.sector)
      return i.sector;
  return "";
  }
