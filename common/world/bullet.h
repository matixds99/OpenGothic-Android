#pragma once

#include <cstdint>

#include "physics/dynamicworld.h"

#include "game/damagecalculator.h"
#include "graphics/meshobjects.h"
#include "graphics/effect.h"

class World;
class Item;
class Npc;

class Bullet final : public DynamicWorld::BulletCallback {
  public:
    Bullet()=default;
    Bullet(World &owner, const Item &itm, const Tempest::Vec3& pos);
    Bullet(Bullet&&)=default;
    ~Bullet() override;
    Bullet& operator=(Bullet&&)=default;

    enum Flg:uint8_t {
      NoFlags = 0,
      Stopped = 1,
      };

    void     setPosition  (const Tempest::Vec3& p);
    void     setPosition  (float x,float y,float z);

    void     setDirection (const Tempest::Vec3& dir);
    void     setTargetRange(float tgRange);

    void     setView      (MeshObjects::Mesh&&   m);
    void     setView      (Effect&& p);

    bool     isSpell() const;
    int32_t  spellId() const;

    void     setOrigin(Npc* n);
    Npc*     origin() const;

    void     setTarget(const Npc* n);
    // NB fix 84: the spell of the player follows its target (the projectiles of Gothic: a moving creature is hit, the
    // touch screen aims only at the start)
    void     setHoming(Npc* n, float turnDegPerSec = 0.f) { homing = n; homingTurn = turnDegPerSec; }   // NB fix 238: 0 - follows at once
    void     tickHoming(uint64_t dt);

    Flg      flags()     const { return flg;  }
    ItemMaterial itemMaterial() const;

    auto     damage() const -> const DamageCalculator::Damage& { return dmg; }
    void     setDamage(DamageCalculator::Damage d) { dmg=d; }

    float    critChance() const { return critCh; }
    void     setCritChance(float v) { critCh=v; }
    float    hitChance() const { return hitCh; }
    void     setHitChance(float v) { hitCh=v; }

    bool     isFinished() const;
    float    pathLength() const;

    bool     onEffectCollide(Npc& other);

  protected:
    void     onStop() override;
    void     onMove() override;
    void     onCollide(zenkit::MaterialGroup matId) override;
    bool     onCollide(Npc& other) override;
    void     nbLogShot(const std::string& what);   // NB fix 236
    std::string nbOwnerName;
    bool        nbHitLogged = false;

  private:
    DynamicWorld::BulletBody* obj=nullptr;
    World*                    wrld=nullptr;
    Npc*                      ow=nullptr;
    Npc*                      homing=nullptr;   // NB fix 84
    float                     homingTurn=0.f;   // NB fix 238: the turn of the projectile, degrees a second (0: at once)
    Tempest::Vec3             nbStart;          // NB fix 238: where it was shot from (the log)

    DamageCalculator::Damage  dmg        = {};
    float                     hitCh      = 1.f;
    float                     critCh     = 0.f;

    MeshObjects::Mesh         view;
    Effect                    vfx;

    uint8_t                   material=0;
    Flg                       flg=NoFlags;

    void updateMatrix();
  };

