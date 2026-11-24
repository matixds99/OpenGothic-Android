#include "damagecalculator.h"
#include "utils/nblog.h"

#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "world/world.h"
#include "world/bullet.h"
#include "gothic.h"

#include <Tempest/Log>

// https://forum.worldofplayers.de/forum/threads/127320-Damage-System?p=2198181#post2198181
// https://strafkolonie-online.net/forum/board/thread/1895-info-erkl%C3%A4rung-der-berechnung-der-trefferchance-im-fernkampf/

static float mix(float x, float y, float a) {
  return x + (y-x)*a;
  }

DamageCalculator::Val DamageCalculator::damageValue(Npc& src, Npc& other, const Bullet* b, bool isSpell, const DamageCalculator::Damage& splDmg, const CollideMask bMsk) {
  DamageCalculator::Val ret;
  if(b!=nullptr) {
    ret = rangeDamage(src,other,*b,bMsk,isSpell ? &splDmg : nullptr);
    } else
  if(isSpell) {
    ret = rangeDamage(src,other,splDmg,bMsk);
    }
  else {
    ret = swordDamage(src,other);
    }

#if 0
  // debug
  ret.value = MinDamage;
#endif

  {
  // debug v2: who hits whom, what the bullet carries and what the weapon in hand says
  auto& hs = src.handle();
  auto& ho = other.handle();
  Tempest::Log::d("DMG2: ",b!=nullptr ? "ranged" : "melee",
                  " attacker='",std::string(src.displayName()),"' player=",int(src.isPlayer()),
                  " target='",std::string(other.displayName()),"' player=",int(other.isPlayer()),
                  " str=",src.attribute(Attribute::ATR_STRENGTH),
                  " dex=",src.attribute(Attribute::ATR_DEXTERITY),
                  " dtype=",damageTypeMask(src),
                  " value=",ret.value);
  Item* weapon = b!=nullptr ? src.currentRangedWeapon() : src.activeWeapon();
  if(NbLog::level()>=NbLog::Full) {
    for(unsigned int i=0; i<zenkit::DamageType::NUM; ++i) {
      int bulletDmg = b!=nullptr ? int(b->damage().val[i]) : int(splDmg.val[i]);
      int itemDmg   = weapon!=nullptr ? int(weapon->handle().damage[i]) : -999;
      Tempest::Log::d("DMG2:  type ",i," npcDamage=",hs.damage[i]," usedDmg=",bulletDmg," weaponItemDmg=",itemDmg," targetProt=",ho.protection[i]);
      }
    } else {
    // NB fix 203: one line for the types that take part in the blow (damage of the character / of the bullet or the
    // spell / of the weapon, and the protection of the target), instead of 8 lines
    std::string parts;
    for(unsigned int i=0; i<zenkit::DamageType::NUM; ++i) {
      int bulletDmg = b!=nullptr ? int(b->damage().val[i]) : int(splDmg.val[i]);
      int itemDmg   = weapon!=nullptr ? int(weapon->handle().damage[i]) : 0;
      if(hs.damage[i]==0 && bulletDmg==0 && itemDmg==0)
        continue;
      parts += " ["+std::to_string(i)+": "+std::to_string(hs.damage[i])+"/"+std::to_string(bulletDmg)+"/"+
               std::to_string(itemDmg)+" prot "+std::to_string(ho.protection[i])+"]";
      }
    if(!parts.empty())
      Tempest::Log::d("DMG2:  types (npc/used/weapon)",parts);
    }
  }

  if(ret.hasHit && !ret.invincible && Gothic::inst().version().game==2)
    ret.value = std::max<int32_t>(ret.value,MinDamage);
  return ret;
  }

DamageCalculator::Val DamageCalculator::damageFall(Npc& npc, float speed) {
  auto  gl = npc.guild();
  auto& g  = npc.world().script().guildVal();

  float   gravity     = DynamicWorld::gravity;
  float   fallTime    = speed/gravity;
  float   height      = 0.5f*std::abs(gravity)*fallTime*fallTime;
  float   h0          = npc.falldownHeight();   // the same height as the one of the deep fall (the hero has more than the scripts say)
  float   dmgPerMeter = float(g.falldown_damage[gl]);
  int32_t prot        = npc.protection(::PROT_FALL);

  Val ret;
  ret.invincible = (prot<0);
  ret.value      = int32_t(dmgPerMeter*(height-h0)/100.f - float(prot));
  if(ret.value<=0 || ret.invincible) {
    ret.value = 0;
    return ret;
    }
  ret.hasHit = true;
  return ret;
  }

DamageCalculator::Val DamageCalculator::rangeDamage(Npc& nsrc, Npc& nother, const Bullet& b, const CollideMask bMsk, const Damage* overrideDamage) {
  float dist       = b.pathLength();
  bool  noHit      = dist>float(MaxMagRange);
  bool  invincible = !checkDamageMask(nsrc,nother,&b,overrideDamage);
  auto  dmg        = overrideDamage!=nullptr ? *overrideDamage : b.damage();

  if(!b.isSpell()) {
    auto& script    = nsrc.world().script();
    float hitChance = float(script.rand(100))/100.f;
    float hitCh     = 0;
    bool  g2        = Gothic::inst().version().game==2;
    float refRange  = g2 ? ReferenceBowRangeG2 : ReferenceBowRangeG1;
    float maxRange  = float(MaxBowRange);
    float chance    = b.hitChance();

    if(dist<refRange)
      hitCh = mix(1.f, chance, (dist / refRange));
    else if(dist<maxRange)
      hitCh = mix(chance, 0.f, (dist-refRange) / (maxRange-refRange));
    else
      hitCh = 0;

    noHit = (dist>float(MaxBowRange) || hitCh<=hitChance);

    if(!g2 && !noHit && !invincible) {
      const int32_t mul        = script.criticalDamageMultiplyer();
      const int     critChance = int(script.rand(100));
      if(std::lround(100.f * b.critChance())>critChance)
        dmg *= mul;
      }
    }

  if(noHit)
    return Val(0,false,invincible);

  if(invincible)
    return Val(0,true,true);

  if((bMsk & (COLL_APPLYDAMAGE | COLL_APPLYDOUBLEDAMAGE | COLL_APPLYHALVEDAMAGE | COLL_DOEVERYTHING))==0)
    return Val(0,true,true);

  return rangeDamage(nsrc,nother,dmg,bMsk);
  }

DamageCalculator::Val DamageCalculator::rangeDamage(Npc&, Npc& nother, Damage dmg, const CollideMask bMsk) {
  auto& other = nother.handle();

  if(bMsk & COLL_APPLYDOUBLEDAMAGE)
    dmg*=2;
  if(bMsk & COLL_APPLYHALVEDAMAGE)
    dmg/=2;

  int  value = 0;
  bool invincible = true;
  for(unsigned int i=0; i<zenkit::DamageType::NUM; ++i) {
    if(dmg[size_t(i)]==0)
      continue;
    int vd = std::max(dmg[size_t(i)] - other.protection[i],0);
    if(other.protection[i]>=0) { // Filter immune
      value     += vd;
      invincible = false;
      }
    }

  return Val(value,true,invincible);
  }

DamageCalculator::Val DamageCalculator::swordDamage(Npc& nsrc, Npc& nother) {
  if(!checkDamageMask(nsrc,nother,nullptr))
    return Val(0,true,true);

  auto& script = nsrc.world().script();
  auto& src    = nsrc.handle();
  auto& other  = nother.handle();

  // NB fix 208: the protection of the target for this blow - the summons of the necromancer go through a part of the
  // protection against edge and blunt (the magic power)
  int32_t prot[zenkit::DamageType::NUM] = {};
  for(unsigned int i=0; i<zenkit::DamageType::NUM; ++i)
    prot[i] = other.protection[i];
  if(Gothic::inst().version().game==2) {
    if(const int32_t pen = script.nbSummonArmorIgnore(nsrc); pen>0) {
      for(unsigned int i : {unsigned(zenkit::DamageType::BLUNT),unsigned(zenkit::DamageType::EDGE)})
        if(prot[i]>0)
          prot[i] = prot[i]*(100-pen)/100;
      }
    }

  // Swords/Fists
  const int dtype      = damageTypeMask(nsrc);
  Talent    tal        = TALENT_UNKNOWN;
  int       str        = nsrc.attribute(Attribute::ATR_STRENGTH);
  int       critChance = int(script.rand(100));

  int value = 0;

  if(auto w = nsrc.inventory().activeWeapon()) {
    if(w->is2H())
      tal = TALENT_2H; else
      tal = TALENT_1H;
    }

  if(Gothic::inst().version().game==2) {
    if(nsrc.isMonster() && tal==TALENT_UNKNOWN) {
      // regular monsters always do critical damage
      critChance = -1;
      }

    // NB fix 186: the blow of a summoned creature of New Balance without a weapon (the stone golem of the necromancer):
    // on the PC (Union) the strength is not in it - only the damage of the creature minus the protection, kind by kind.
    // A kind that the protection stops gives 1, the whole blow at least 20. The screenshots from the PC: the golem
    // (intelligence 292: blunt 297, fly 173) hits the archer of the builders (blunt 75, fly 50) for 345, the warrior
    // crawler (blunt 675, fly 40) for 134, the ore eater (blunt 300, fly 300) for 20. With the strength of fix 155
    // (751) OpenGothic gave about 1100-1300.
    // Only the creatures that have their own damage (src.damage); the others hit by the strength as before
    int ownDamage = 0;
    for(unsigned int i=0; i<zenkit::DamageType::NUM; ++i)
      if((dtype & (1<<i))!=0)
        ownDamage += std::max(src.damage[i],0);
    if(tal==TALENT_UNKNOWN && ownDamage>0 && script.nbIsSummon(nsrc)) {
      bool invincible = true;
      for(unsigned int i=0; i<zenkit::DamageType::NUM; ++i) {
        if((dtype & (1<<i))==0 || prot[i]<0)
          continue;   // not this kind, or immune
        value += std::max(src.damage[i]-prot[i],1);
        invincible = false;
        }
      if(!invincible)
        value = std::max(value,20);
      return Val(value,true,invincible);
      }

    bool invincible = true;
    // NB fix 155: with several kinds of damage (the golems and the trolls: DAM_BLUNT|DAM_FLY, the magic weapons) Gothic 2
    // adds the strength once and shares the whole (strength + damage) among the kinds by their part of the damage.
    // OpenGothic added the whole strength to every kind: the stone golem of NB (750 strength, 250 blunt, 100 fly) hit for
    // 750+250 and 750+100 - nearly twice the blow of the original
    int kinds = 0, dmgSum = 0;
    for(unsigned int i=0; i<zenkit::DamageType::NUM; ++i)
      if((dtype & (1<<i))!=0) {
        ++kinds;
        dmgSum += std::max(src.damage[i],0);
        }
    for(unsigned int i=0; i<zenkit::DamageType::NUM; ++i) {
      if((dtype & (1<<i))==0)
        continue;
      int full = str + src.damage[i];
      if(kinds>1) {
        if(dmgSum>0)
          full = int((int64_t(str)+dmgSum)*std::max(src.damage[i],0)/dmgSum); else
          full = str/kinds;
        }
      int vd = std::max(full - prot[i],0);
      if(src.hitchance[tal]<=critChance)
        vd = (vd-1)/10;
      if(prot[i]>=0) { // Filter immune
        value += vd;
        invincible = false;
        }
      }

    // NB fix 208: the magic damage of a magic weapon of the hero (a staff, a magic blade)
    if(const int32_t mag = script.nbMagicWeaponDamage(nsrc,nother); mag>0) {
      value     += mag;
      invincible = false;
      }
    return Val(value,true,invincible);
    } else {
    bool invincible = true;
    const int32_t mul = script.criticalDamageMultiplyer();
    for(unsigned int i=0; i<zenkit::DamageType::NUM; ++i) {
      if((dtype & (1<<i))==0)
        continue;
      int vd = 0;
      if(nsrc.talentValue(tal)<=critChance)
        vd = std::max(str +     src.damage[i] - prot[i],0); else
        vd = std::max(str + mul*src.damage[i] - prot[i],0);
      if(prot[i]>=0) { // Filter immune
        value += vd;
        invincible = false;
        }
      }

    return Val(value,true,invincible);
    }
  }

int32_t DamageCalculator::damageTypeMask(Npc& npc) {
  if(auto w = npc.inventory().activeWeapon())
    return w->handle().damage_type;
  return npc.handle().damage_type;
  }

bool DamageCalculator::checkDamageMask(Npc& nsrc, Npc& nother, const Bullet* b, const Damage* overrideDamage) {
  auto& other = nother.handle();

  if(b!=nullptr) {
    auto dmg = overrideDamage!=nullptr ? *overrideDamage : b->damage();
    for(unsigned int i=0;i<zenkit::DamageType::NUM;++i) {
      if(dmg[size_t(i)]>0 && other.protection[i]>=0)
        return true;
      }
    } else {
    const int dtype = damageTypeMask(nsrc);
    for(unsigned int i=0;i<zenkit::DamageType::NUM;++i){
      if((dtype & (1<<i))==0)
        continue;
      return true;
      }
    }

  return false;
  }

DamageCalculator::Damage DamageCalculator::rangeDamageValue(Npc& src) {
  const int dtype = damageTypeMask(src);
  int d = Gothic::inst().version().game==2 ? src.attribute(Attribute::ATR_DEXTERITY) : 0;
  Damage ret={};
  for(unsigned int i=0;i<zenkit::DamageType::NUM;++i){
    if((dtype & (1<<i))==0)
      continue;
    ret[size_t(i)] = d + src.handle().damage[i];
    }
  return ret;
  }