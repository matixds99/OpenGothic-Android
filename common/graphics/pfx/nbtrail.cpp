#include "nbtrail.h"

#include <cmath>
#include <memory>

#include <Tempest/Pixmap>

#include "graphics/pfx/particlefx.h"
#include "resources.h"

using namespace Tempest;

// a ribbon texture: u goes across the ribbon (a soft edge, bright in the middle), v along it (the engine fades the ribbon
// with its age by the color of the vertices); kept to the end of the program (the device dies before the statics)
static const Tempest::Texture2d* ribbonTex(float peak, uint8_t r=255, uint8_t g=255, uint8_t b=255) {
  constexpr int W = 32, H = 4;
  Pixmap pm(W,H,TextureFormat::RGBA8);
  auto*  p = reinterpret_cast<uint8_t*>(pm.data());
  for(int y=0; y<H; ++y)
    for(int x=0; x<W; ++x) {
      const float u = (float(x)+0.5f)/float(W);
      const float s = std::sin(u*3.14159265f);
      const float a = s*s*peak;
      uint8_t*    px = p+(y*W+x)*4;
      px[0] = r; px[1] = g; px[2] = b;
      px[3] = uint8_t(std::min(1.f,a)*255.f);
      }
  return new Tempest::Texture2d(Resources::loadTexturePm(pm));
  }

/// A ribbon after a point: the particles stay on the emitter (useEmittersFOR), so the ribbon of each follows the way
/// of the emitter. The particles themselves are invisible: only the ribbons are seen.
static std::unique_ptr<ParticleFx> makeRibbon(const char* name, float pps, float life, float fadeSec, float width, float peak,
                                              Material::AlphaFunc func, uint8_t cr=255, uint8_t cg=255, uint8_t cb=255) {
  auto fx = std::make_unique<ParticleFx>();
  fx->dbgName          = name;
  fx->ppsValue         = pps;
  fx->ppsIsLooping     = true;
  fx->shpType          = ParticleFx::EmitterType::Point;
  fx->shpFOR           = ParticleFx::Frame::Object;
  fx->dirMode          = ParticleFx::Dir::Rand;
  fx->dirFOR           = ParticleFx::Frame::World;
  fx->velAvg           = 0.f;
  fx->velVar           = 0.f;
  fx->lspPartAvg       = life;                          // ms
  fx->lspPartVar       = 0.f;
  const auto* tex      = ribbonTex(peak,cr,cg,cb);
  fx->visMaterial.tex   = tex;
  fx->visMaterial.alpha = func;
  fx->visOrientation   = ParticleFx::Orientation::None;
  fx->visTexIsQuadPoly = true;
  fx->visTexColorStart = Vec3(255,255,255);
  fx->visTexColorEnd   = Vec3(255,255,255);
  fx->visSizeStart     = Vec2(1.f,1.f);
  fx->visSizeEndScale  = 1.f;
  fx->visAlphaStart    = 0.f;                           // the sprite of the particle is not drawn
  fx->visAlphaEnd      = 0.f;
  fx->trlTexture       = tex;
  fx->trlFadeSpeed     = fadeSec;
  fx->trlWidth         = width;
  fx->useEmittersFOR   = true;
  fx->prefferedTime    = uint64_t(life);
  return fx;
  }

const ParticleFx* NbTrail::arrow() {
  static std::unique_ptr<ParticleFx> fx;
  if(fx==nullptr)
    fx = makeRibbon("NB_TRAIL_ARROW",30.f,300.f,0.30f,4.f,0.55f,Material::AdditiveLight);
  return fx.get();
  }

// The additive-light material of the engine is lifted (inverse tonemapping, x3) so the color must be dark and the peak low:
// white 255 at the alpha 0.09 was a solid white sheet. The particles of an emitter share one point and their ribbons add up,
// so only ~2-3 are alive at once (10 pps, 240 ms).
const ParticleFx* NbTrail::sword() {
  static std::unique_ptr<ParticleFx> fx;
  if(fx==nullptr)
    fx = makeRibbon("NB_TRAIL_SWORD",10.f,240.f,0.22f,4.f,0.16f,Material::AdditiveLight,150,160,175);
  return fx.get();
  }
