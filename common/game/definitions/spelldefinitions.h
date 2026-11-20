#pragma once

#include <zenkit/DaedalusVm.hh>
#include <zenkit/addon/daedalus.hh>

class SpellDefinitions final {
  public:
    SpellDefinitions(zenkit::DaedalusVm &vm);
    ~SpellDefinitions();

    const zenkit::ISpell& find(std::string_view instanceName) const;
    /// NB fix 126: the instances of the spells run again - they read the SPL_DAMAGE_* and SPL_COST_* that changed (the
    /// original engine makes a new oCSpell for every cast)
    void                  refresh();

  private:
    zenkit::DaedalusVm&                          vm;
    std::vector<std::shared_ptr<zenkit::ISpell>> spl;
  };
