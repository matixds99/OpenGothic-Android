#pragma once

class ParticleFx;

/// NB fix 260: the traces that the PC version draws: a pale streak after the blade of a swing and a bright streak after
/// a flying arrow or a bolt. The particles are made in the code (no data of the game): a soft white sprite generated
/// in the memory, in the world frame, so they stay where they were emitted and fade out in a fraction of a second
namespace NbTrail {
  const ParticleFx* arrow();   // a tiny bright dots after the ammunition
  const ParticleFx* sword();   // a wide pale smear after the blade
  }
