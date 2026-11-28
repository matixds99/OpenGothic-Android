#include "nbtrail.h"

#include <cmath>
#include <memory>

#include <Tempest/Pixmap>

#include "graphics/pfx/particlefx.h"
#include "resources.h"

using namespace Tempest;

static const Tempest::Texture2d* softSprite() {
  // a round sprite: white, the alpha falls from the center to the edge; kept to the end of the program (the device is
  // destroyed before the static objects)
  static const Tempest::Texture2d* tex = nullptr;
  if(tex==nullptr) {
    constexpr int S = 32;
    Pixmap pm(S,S,TextureFormat::RGBA8);
    auto*  p = reinterpret_cast<uint8_t*>(pm.data());
    for(int y=0; y<S; ++y)
      for(int x=0; x<S; ++x) {
        const float dx = (float(x)+0.5f)/float(S)*2.f-1.f;
        const float dy = (float(y)+0.5f)/float(S)*2.f-1.f;
        const float d  = std::sqrt(dx*dx+dy*dy);
        const float a  = d>=1.f ? 0.f : (1.f-d)*(1.f-d)*(3.f-2.f*d);   // a soft edge
        uint8_t*    px = p+(y*S+x)*4;
        px[0] = px[1] = px[2] = 255;
        px[3] = uint8_t(a*255.f);
        }
    tex = new Tempest::Texture2d(Resources::loadTexturePm(pm));
    }
  return tex;
  }

static std::unique_ptr<ParticleFx> make(const char* name, float pps, float life, float size, float endScale, float alphaStart,
                                        float alphaEnd, Material::AlphaFunc func) {
  auto fx = std::make_unique<ParticleFx>();
  fx->dbgName          = name;
  fx->ppsValue         = pps;
  fx->ppsIsLooping     = true;
  fx->shpType          = ParticleFx::EmitterType::Point;
  fx->shpFOR           = ParticleFx::Frame::World;     // stays where it was emitted
  fx->shpIsVolume      = false;
  fx->dirMode          = ParticleFx::Dir::Rand;
  fx->dirFOR           = ParticleFx::Frame::World;
  fx->velAvg           = 0.f;
  fx->velVar           = 0.f;
  fx->lspPartAvg       = life;                          // ms
  fx->lspPartVar       = 0.f;
  fx->visMaterial.tex   = softSprite();
  fx->visMaterial.alpha = func;
  fx->visOrientation   = ParticleFx::Orientation::None;
  fx->visTexIsQuadPoly = true;
  fx->visTexColorStart = Vec3(255,255,255);
  fx->visTexColorEnd   = Vec3(255,255,255);
  fx->visSizeStart     = Vec2(size,size);
  fx->visSizeEndScale  = endScale;
  fx->visAlphaStart    = alphaStart;
  fx->visAlphaEnd      = alphaEnd;
  fx->useEmittersFOR   = false;                     // the position is the world one: the particles stay behind
  fx->prefferedTime    = uint64_t(life);
  return fx;
  }

const ParticleFx* NbTrail::arrow() {
  static std::unique_ptr<ParticleFx> fx;
  if(fx==nullptr)
    fx = make("NB_TRAIL_ARROW",260.f,170.f,7.f,0.4f,0.9f,0.f,Material::AdditiveLight);
  return fx.get();
  }

const ParticleFx* NbTrail::sword() {
  static std::unique_ptr<ParticleFx> fx;
  if(fx==nullptr) {
    // the head is invisible and the smear rises behind the blade: the particles are born under the blade, so a
    // visible start made the whole blade shine; the alpha grows with the age and the size shrinks, so the brightest
    // part is a short way behind the blade and then it fades out. A plain blend (not additive): no glow
    fx = make("NB_TRAIL_SWORD",760.f,240.f,11.f,0.25f,0.f,0.5f,Material::Transparent);
    fx->shpType   = ParticleFx::EmitterType::Mesh;     // from the mesh of the blade (set by the emitter)
    }
  return fx.get();
  }
