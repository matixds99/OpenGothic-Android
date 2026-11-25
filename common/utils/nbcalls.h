#pragma once

namespace zenkit { class DaedalusVm; }

// NB fix 241: the tracer of the calls of script functions, the twin of tools/pc-logger (nblog.dll on PC).
// With nblog.cfg (or nbcalls.cfg) in the folder of the game it writes nbcalls.txt: "[ms] NAME self=.. other=.." for the
// functions of the list, the same format as nblog.txt of the PC, to be compared by tools/nbcompare.py
struct NbCalls {
  static void hook(zenkit::DaedalusVm& vm);
  };
