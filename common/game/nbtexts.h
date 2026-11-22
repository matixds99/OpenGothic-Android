#pragma once

#include <Tempest/Dir>
#include <Tempest/File>
#include <Tempest/Log>

#include <string>
#include <string_view>
#include <unordered_map>

#include "utils/fileutil.h"
#include "gothic.h"

/// NB fix 62: the texts of the Union plugin of New Balance. The plugin has them in Russian and reads the translation
/// from AB_Translation.lbr (lines: key;"text", cp1250 as the fonts of Gothic). Here: that file from the folder of the
/// game (System or System/Autorun), and the Polish texts below when it is not there
namespace NbTexts {
  inline const std::unordered_map<std::string_view,std::string_view>& fallback() {
    static const std::unordered_map<std::string_view,std::string_view> pl = {
      {"RX_SummonTeleportCD_String", "Przywo\xB3""anie przyzwa\xF1""c\xF3w"},   // NB fix 176
      {"sHolyLight", "\x8Cwi\xEAty p\xB3omie\xF1"},
      {"sEffectResistCurse", "Ochrona przed kl\xB9twami"},
      {"sEffectAcceleration", "Przyspieszenie"},
      {"sEffectInvisible", "Niewidzialno\x9C\xE6"},
      {"sEffectStamina", "S\xB3""aba regeneracja wytrzyma\xB3o\x9C""ci"},
      {"sEffectWeakMana", "S\xB3""aba regeneracja many"},
      {"sEffectMiddleMana", "\x8Crednia regeneracja many"},
      {"sEffectStrongMana", "Silna regeneracja many"},
      {"sEffectResistPoison", "Ochrona przed truciznami"},
      {"sEffectResist", "Ochrona przed upadkiem"},
      {"sEffectMagicPower", "Bonus mocy magicznej"},
      {"sEffectStamina2", "\x8Crednia regeneracja wytrzyma\xB3o\x9C""ci"},
      {"sEffectSerafis", "Regen. wytrz. od serafisu"},
      {"sEffectHealthRegen", "Regeneracja zdrowia"},
      {"sBuffStoneGolemHeartCount", "Serce kamiennego golema"},
      {"sBuffFireGolemHeartCount", "Serce ognistego golema"},
      {"sBuffIceGolemHeartCount", "Serce lodowego golema"},
      {"sBuffSwampGolemHeartCount", "Serce bagiennego golema"},
      {"sNextIsgarothBonus", "B\xB3ogos\xB3""awie\xF1stwo - regeneracja zdrowia"},
      {"sNextIsgarothBonusSta", "B\xB3ogos\xB3""awie\xF1stwo - regeneracja wytrz."},
      {"sNextIsgarothBonusMana", "B\xB3ogos\xB3""awie\xF1stwo - regeneracja many"},
      {"sMagicShieldAbsorbeLeft", "Tarcza Magiczna: "},
      {"sNextHitPotion", "Ochrona na nast\xEApny atak"},
      {"sWispSearch", "Co\x9C znaleziono!"},
      {"sWispName", "Ognik"},
      {"itemGold", "Z\xB3oto: "},
      {"sAcquired", "Zdobyto: "},
      {"sDay", "Dzie\xF1: "},
      };
    return pl;
    }

  inline std::unordered_map<std::string,std::string> load() {
    std::unordered_map<std::string,std::string> ret;
    for(auto* dir:{u"Autorun",u""}) {
      std::u16string path = *dir!=u'\0' ?
            Gothic::nestedPath({u"system",dir,u"AB_Translation.lbr"},Tempest::Dir::FT_File) :
            Gothic::nestedPath({u"system",u"AB_Translation.lbr"},Tempest::Dir::FT_File);
      if(path.empty() || !FileUtil::exists(path))
        continue;
      try {
        Tempest::RFile f(path);
        std::string data(f.size(),'\0');
        f.read(data.data(),data.size());
        size_t at = 0;
        while(at<data.size()) {
          size_t end = data.find('\n',at);
          if(end==std::string::npos)
            end = data.size();
          std::string_view l(data.data()+at,end-at);
          at = end+1;
          while(!l.empty() && (l.back()=='\r' || l.back()==' '))
            l.remove_suffix(1);
          const size_t sep = l.find(';');
          if(sep==std::string_view::npos || sep==0)
            continue;
          std::string_view val = l.substr(sep+1);
          if(val.size()>=2 && val.front()=='"' && val.back()=='"')
            val = val.substr(1,val.size()-2);
          ret[std::string(l.substr(0,sep))] = std::string(val);
          }
        Tempest::Log::i("NB: AB_Translation.lbr - ",ret.size()," texts");
        return ret;
        }
      catch(...) {
        Tempest::Log::e("NB: unable to read AB_Translation.lbr");
        }
      }
    return ret;
    }

  /// the text of the plugin with this key (as the plugin: the translation, else the built-in Polish one, else the key)
  inline std::string_view get(std::string_view key) {
    static const auto lbr = load();
    if(auto i = lbr.find(std::string(key)); i!=lbr.end())
      return i->second;
    if(auto i = fallback().find(key); i!=fallback().end())
      return i->second;
    return key;
    }
  }
