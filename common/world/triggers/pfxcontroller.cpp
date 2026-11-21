#include "pfxcontroller.h"

#include "world/world.h"
#include "graphics/pfx/particlefx.h"
#include "game/serialize.h"
#include "gothic.h"

#include <Tempest/Log>

PfxController::PfxController(Vob* parent, World& world, const zenkit::VParticleEffectController& ctrl, Flags flags)
  :AbstractTrigger(parent,world,ctrl,flags) {
  killWhenDone = ctrl.kill_when_done;
  pfxName      = ctrl.pfx_name;

  if(const ParticleFx* view = Gothic::inst().loadParticleFx(ctrl.visual->name)) {
    visual = PfxEmitter(world,view);
    visual.setActive(true);
    visual.setLooped(true);
    visual.setObjMatrix(transform());
    }

  if(const ParticleFx* view = Gothic::inst().loadParticleFx(ctrl.pfx_name)) {
    lifeTime = view->maxLifetime();
    pfx = PfxEmitter(world,view);
    pfx.setActive(ctrl.initially_running);
    pfx.setLooped(true);
    pfx.setObjMatrix(transform());
    }
  }

void PfxController::save(Serialize& fout) const {
  AbstractTrigger::save(fout);
  fout.write(killed,lifeTime,pfx.isActive());
  }

void PfxController::load(Serialize& fin) {
  AbstractTrigger::load(fin);
  bool active = false;
  fin.read(killed,lifeTime,active);
  pfx.setActive(active);

  if(killed!=std::numeric_limits<uint64_t>::max())
    enableTicks();
  }

void PfxController::onTrigger(const TriggerEvent&) {
  // NB fix 152: the particles of the world started by a trigger (flying effects of the scenes) - to the log
  {
  const ParticleFx* d = Gothic::inst().loadParticleFx(pfxName);
  auto p = position();
  Tempest::Log::d("[ctx] pfx controller '",name(),"' (",pfxName,") on",d==nullptr ? " - no such particles" : "",
                  d!=nullptr && d->hasTrails() ? ", trails" : ""," at ",int(p.x),",",int(p.y),",",int(p.z),
                  killed<world.tickCount() ? " - already killed" : "");
  }
  if(killed<world.tickCount())
    return;
  pfx.setActive(true);
  if(killWhenDone) {
    killed = world.tickCount() + lifeTime;
    enableTicks();
    }
  }

void PfxController::onUntrigger(const TriggerEvent&) {
  pfx.setActive(false);
  }

void PfxController::moveEvent() {
  Vob::moveEvent();
  visual.setObjMatrix(transform());
  pfx.setObjMatrix(transform());
  }

void PfxController::tick(uint64_t /*dt*/) {
  if(killed<world.tickCount()) {
    disableTicks();
    pfx.setActive(false);
    }
  }
