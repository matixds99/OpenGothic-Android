#pragma once

#include <zenkit/DaedalusVm.hh>

#include "game/serialize.h"

/// NB fix 209: the save-games keep the indices of the symbols of the scripts (the instances of the characters and of the
/// items, the functions of the states and of the routines, the dialogs). A new GOTHIC.DAT (an update of the scripts of the
/// mod) moves them: the save-game of the old scripts loads wrong objects ("Cannot init USE_RAWMEAT_MOUNT: not an instance").
///  - write(): the entry game/symtab - the names of the functions and of the instances of the scripts of the save-game;
///  - build(): the map of the indices of the save-game to the indices of the scripts now (nullptr: the same scripts).
///    Older save-games (without game/symtab): the names come from the variables of the save-game (game/daedalus - the
///    variables, the parameters and the locals of the functions, the characters and the items), from a file of the names
///    of the old scripts (old_symbols*.txt next to the save-games, one name for a line) or from the place between them.
namespace SaveSymbols {
  void                   write(Serialize& fout, zenkit::DaedalusVm& vm);
  Serialize::SymbolMap   build(Serialize& fin, zenkit::DaedalusVm& vm);
  /// a map of a nested save (a visited world) and the map of the save that has it: nested -> now
  Serialize::SymbolMap   compose(const Serialize::SymbolMap& inner, const Serialize::SymbolMap& outer);
  }
