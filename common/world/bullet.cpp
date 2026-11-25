#include "bullet.h"

#include <algorithm>

#include <Tempest/Log>

#include "graphics/visualfx.h"
#include "world/objects/item.h"
#include "world/objects/npc.h"
#include "world.h"
#include "world/focus.h"
#include "gothic.h"

using namespace Tempest;

Bullet::Bullet(World& owner, const Item& itm, const Vec3& pos)
  :wrld(&owner) {
  obj = wrld->physic()->bulletObj(this);
  if(itm.isSpellOrRune()) {
    obj->setSpellId(itm.spellId());
    }

  if(itm.isSpellOrRune()) {
    material   = ItemMaterial::MAT_COUNT;
    int32_t id = itm.spellId();
    const VisualFx* vfx = owner.script().spellVfx(id);
    if(vfx!=nullptr) {
      auto e = Effect(*vfx,owner,pos,SpellFxKey::Cast);
      setView(std::move(e));
      }
    } else {
    material = uint8_t(itm.handle().material);
    setView(owner.addView(itm.handle()));
    // the magic and the fire ammunition (SPELLFX_ARROW, SPELLFX_FIREARROW, SPELLFX_BOLT) flies with its effect
    if(!itm.handle().effect.empty()) {
      if(const VisualFx* fx = Gothic::inst().loadVisualFx(itm.handle().effect)) {
        Effect e(*fx,owner,pos,SpellFxKey::Cast);
        e.setObjectMesh(&view);
        setView(std::move(e));
        }
      }
    }

  setPosition(pos);
  }

Bullet::~Bullet() {
  // NB fix 236: a projectile of a spell near the hero that hit nothing ([shot] in the log)
  if(!nbOwnerName.empty() && !nbHitLogged)
    Tempest::Log::i("[shot] ",nbOwnerName," spell ",spellId()," hits nothing (flew away or vanished)");
  vfx.setBullet(nullptr,*wrld);
  wrld->physic()->deleteObj(obj);
  }

void Bullet::setPosition(const Tempest::Vec3& p) {
  obj->setPosition(p);
  updateMatrix();
  }

void Bullet::setPosition(float x, float y, float z) {
  obj->setPosition(Vec3(x,y,z));
  updateMatrix();
  }

void Bullet::setDirection(const Tempest::Vec3& dir) {
  obj->setDirection(dir);
  updateMatrix();
  }

void Bullet::setTargetRange(float t) {
  obj->setTargetRange(t);
  }

void Bullet::setView(MeshObjects::Mesh &&m) {
  view = std::move(m);
  updateMatrix();
  }

void Bullet::setView(Effect &&p) {
  vfx = std::move(p);
  vfx.setActive(true);
  vfx.setLooped(true);
  vfx.setBullet(this,*wrld);
  vfx.setPhysicsDisable();
  updateMatrix();
  }

bool Bullet::isSpell() const {
  return obj->isSpell();
  }

int32_t Bullet::spellId() const {
  return obj->spellId();
  }

void Bullet::setOrigin(Npc *n) {
  ow = n;
  if(obj!=nullptr)
    nbStart = obj->position();   // NB fix 238
  vfx.setOrigin(n);
  // NB fix 236: the name for the log, while the owner surely lives
  auto* pl = wrld->player();
  if(n!=nullptr && pl!=nullptr && isSpell() && n->qDistTo(*pl)<=3000.f*3000.f)
    nbOwnerName = n->isPlayer() ? std::string("hero") : std::string(n->displayName());
  }

Npc *Bullet::origin() const {
  return ow;
  }

void Bullet::tickHoming(uint64_t dt) {
  if(homing==nullptr || obj==nullptr || isFinished())
    return;
  // the target is checked every tick: it may be gone from the world, dead or down
  homing = wrld->validateFocus(Focus(*homing)).npc;
  if(homing==nullptr || homing->isDead() || homing->isDown()) {
    homing = nullptr;
    return;
    }
  const auto  cur   = obj->direction();
  const float speed = cur.length();
  auto        to    = homing->collosionCenter()-obj->position();
  const float len   = to.length();

  // NB fix 102: at close range the spell starts inside the body of the target (or reaches its center): the ray of the
  // physics from inside the body hits nothing, the spell turned around the center and never hit. Close to the center -
  // the hit at once
  const auto  bb    = homing->bounds();
  const float half  = std::max(bb.bbox[1].x-bb.bbox[0].x, bb.bbox[1].z-bb.bbox[0].z)*0.5f;
  const float reach = std::clamp(half*0.7f,40.f,150.f);
  if(len<=reach) {
    Npc& target = *homing;
    homing = nullptr;
    if(onCollide(target))
      onStop();
    return;
    }
  if(speed<=0.f)
    return;
  // NB fix 238: a projectile of a character turns only so fast - it follows the hero, but a quick step aside dodges it
  if(homingTurn>0.f && dt>0) {
    const auto  a   = cur/speed;
    const auto  b   = to/len;
    const float cs  = std::clamp(a.x*b.x+a.y*b.y+a.z*b.z,-1.f,1.f);
    const float ang = std::acos(cs);
    const float mx  = homingTurn*float(M_PI)/180.f*float(dt)/1000.f;
    if(ang>mx && ang>1e-4f) {
      const float t = mx/ang;
      auto d = a*(1.f-t)+b*t;
      const float dl = d.length();
      if(dl>1e-5f)
        setDirection(d*(speed/dl));
      return;
      }
    }
  setDirection(to*(speed/len));
  }

void Bullet::setTarget(const Npc* n) {
  vfx.setTarget(n);
  }

ItemMaterial Bullet::itemMaterial() const {
  if(material>=ItemMaterial::MAT_COUNT)
    return ItemMaterial::MAT_WOOD;
  return ItemMaterial(material);
  }

bool Bullet::isFinished() const {
  if((flags()&Bullet::Stopped)!=Bullet::Stopped)
    return false;
  return true;
  }

float Bullet::pathLength() const {
  return obj->pathLength();
  }

bool Bullet::onEffectCollide(Npc& other) {
  const bool ret = onCollide(other);
  if(ret)
    onStop();
  return ret;
  }

void Bullet::onStop() {
  flg = Flg(flg|Stopped);
  updateMatrix();
  }

void Bullet::onMove() {
  updateMatrix();
  }

void Bullet::onCollide(zenkit::MaterialGroup matId) {
  if(isFinished())
    return;
  nbLogShot("the world (a wall, the ground)");   // NB fix 236
  if(matId != zenkit::MaterialGroup::NONE) {
    if(material < ItemMaterial::MAT_COUNT) {
      auto s = wrld->addLandHitEffect(ItemMaterial(material),matId,obj->matrix());
      s.play();
      }
    }
  Effect::onCollide(*wrld,vfx.handle(),obj->position(),nullptr,ow,spellId());
  vfx.setLooped(false);
  vfx.setPhysicsDisable();
  wrld->runEffect(std::move(vfx));
  }

// NB fix 236: what a projectile of a spell hit, near the hero ([shot] in the log, at most 6 lines in 2 s)
void Bullet::nbLogShot(const std::string& what) {
  if(ow==nullptr || !isSpell())
    return;
  nbHitLogged = true;
  auto* pl = wrld->player();
  if(pl==nullptr || ow->qDistTo(*pl)>3000.f*3000.f)
    return;
  static uint64_t window = 0;
  static int      lines  = 0;
  const uint64_t  now    = wrld->tickCount();
  if(now>window+2000) {
    window = now;
    lines  = 0;
    }
  if(++lines>6)
    return;
  const auto p = obj!=nullptr ? obj->position() : Tempest::Vec3();
  Tempest::Log::i("[shot] ",ow->isPlayer() ? std::string("hero") : std::string(ow->displayName()),
                  " spell ",spellId()," hits ",what," after ",int((p-nbStart).length())," cm (from ",int(nbStart.x)," ",int(nbStart.y),
                  " ",int(nbStart.z)," to ",int(p.x)," ",int(p.y)," ",int(p.z),")");
  }

bool Bullet::onCollide(Npc& npc) {
  if(&npc==origin() || isFinished())
    return false;

  if(ow!=nullptr) {
    // NB fix 236: the projectiles of the hero fly through his summons and companions (aivar[91] RX_SUMMON_ID,
    // AIV_PARTYMEMBER), the projectiles of a character through its friends: before, the golems in front of the hero
    // took every rune of the hero (no damage - but the bolt was gone) and he could not hit Zeld behind them
    bool pass = false;
    if(ow->isPlayer())
      pass = wrld->script().nbIsSummon(npc) || npc.handle().aivar[15]!=0;
    else
      pass = wrld->script().isFriendlyFire(*ow, npc);
    if(pass)
      return false;
    nbLogShot(npc.isPlayer() ? std::string("hero") : std::string(npc.displayName()));
    const bool friendlyFire = false;
    if(!friendlyFire) {
      if(isSpell())
        npc.takeDamage(*ow,this,vfx.handle(),spellId()); else
        npc.takeDamage(*ow,this);
      }
    }
  vfx.setKey(*wrld,SpellFxKey::Collide);
  vfx.setLooped(false);
  vfx.setPhysicsDisable();
  wrld->runEffect(std::move(vfx));

  return true;
  }

void Bullet::updateMatrix() {
  if(obj==nullptr)
    return;
  auto mat = obj->matrix();
  view.setObjMatrix(mat);
  if(!isSpell()) {
    // the effect of the ammunition sits on its mesh: the same matrix
    vfx.setObjMatrix(mat);
    return;
    }
  // HACK: lighting bolt spell
  mat.rotateOY(90);
  vfx.setObjMatrix(mat);
  // vfx .setTarget(obj->position()+obj->direction());
  }
