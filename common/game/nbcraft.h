#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

/// NB fix 70: the recipes of the alchemy of New Balance. The plugin (UNION_ABI.DLL) has them in its code, not in the
/// scripts: they are built at 0x101b98c0 into the list at 0x10568f90, and the window shows those that pass the test at
/// 0x101a5940 (see NbCraft::visible in gamescript.cpp). The fields as in the plugin:
///   id    - the number of the recipe (5000.. - the six hearts for the Eye of Innos);
///   tab   - the tab: 0 Regeneracja, 1 Wytrzymałość, 2 Permanentne, 3 Specjalne, 4 Alkohole, 5 Trucizny;
///   kind  - what decides if the recipe is known (below 19: PLAYER_TALENT_ALCHEMY[kind], the rest - the variables of
///           the scripts, as the plugin);
///   grade - passed to RX_RaiseAlchemy(grade, count) and CRAFT_SKILL;
///   exp   - CRAFT_EXP = exp*count (the joints);
///   one recipe makes one item.
/// Not here: ItPo_ASNC_POTION_* (the scripts of NB have no such items)
namespace NbCraft {
  /// a recipe as the window shows it (the items resolved in the scripts)
  struct Item {
    int32_t                                 id   = 0;   // the number of the recipe in the plugin
    int32_t                                 tab  = 0;
    size_t                                  cls  = 0;   // what it makes
    std::string                             name;
    std::vector<std::pair<size_t,int32_t>>  ingredients; // item, how many for one
    };

  struct Ingredient {
    const char* item;
    int32_t     count;
    };
  struct Recipe {
    int32_t                 id;
    int32_t                 tab;
    int32_t                 kind;
    int32_t                 grade;
    int32_t                 exp;
    const char*             result;
    std::vector<Ingredient> ingredients;
    };

  inline const std::vector<Recipe>& alchemy() {
    static const std::vector<Recipe> list = {
      {   0,0, 0,1, 0,"ItPo_Health_01",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_Health_Herb_01",1}, {"ItPl_Blueplant",1}}},
      {   1,0, 1,2, 0,"ItPo_Health_02",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_Health_Herb_02",1}, {"ItPl_Blueplant",1}}},
      {  24,0, 1,0, 0,"ItPo_Health_02",{{"ItPo_Health_01",2}}},
      {   2,0, 2,3, 0,"ItPo_Health_03",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_Health_Herb_03",1}, {"ItPl_Blueplant",1}}},
      {  25,0, 2,0, 0,"ItPo_Health_03",{{"ItPo_Health_02",2}}},
      {  26,0,19,3, 0,"ItPo_Health_Addon_04_New",{{"ItMi_Flask",1}, {"ItFo_Alcohol",2}, {"ItPl_Health_Herb_01",1}, {"ItPl_Temp_Herb",1}, {"ItPl_Blueplant",2}, {"ItAt_Sting",6}}},
      {  17,0,14,4, 0,"ItPo_Health_Addon_04",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_Temp_Herb",1}, {"ItPl_Blueplant",1}, {"ItPo_Health_01",3}}},
      {  27,0,20,4, 0,"ItPo_Health_Addon_05",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPo_Health_Addon_04",1}, {"ItPo_Mana_Addon_04",1}, {"ItPl_Blueplant",1}, {"ItPl_Temp_Herb",1}}},
      {   3,0, 3,1, 0,"ItPo_Mana_01",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_Mana_Herb_01",1}, {"ItPl_Blueplant",1}}},
      {   4,0, 4,2, 0,"ItPo_Mana_02",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_Mana_Herb_02",1}, {"ItPl_Blueplant",1}}},
      {  22,0, 4,0, 0,"ItPo_Mana_02",{{"ItPo_Mana_01",2}}},
      {   5,0, 5,3, 0,"ItPo_Mana_03",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_Mana_Herb_03",1}, {"ItPl_Blueplant",1}}},
      {  23,0, 5,0, 0,"ItPo_Mana_03",{{"ItPo_Mana_02",2}}},
      {  16,0,13,4, 0,"ItPo_Mana_Addon_04",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_Temp_Herb",1}, {"ItPl_Blueplant",1}, {"ItPo_Mana_01",3}}},
      {  20,1,17,2, 0,"itpo_stamina",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_Blueplant",1}, {"ItPl_Temp_Herb",1}}},
      {   6,1, 6,1, 0,"ItPo_Speed",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_Speed_Herb_01",1}, {"ItPl_Temp_Herb",1}}},
      {   7,2, 7,5, 0,"ItPo_Perm_STR",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_Perm_Herb",1}, {"ItPl_Strength_Herb_01",1}}},
      {   8,2, 8,5, 0,"ItPo_Perm_DEX",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_Perm_Herb",1}, {"ItPl_Dex_Herb_01",1}}},
      {   9,2, 9,5, 0,"ItPo_Perm_Mana_craft",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_Perm_Herb",1}, {"ItPl_Mana_Herb_03",1}}},
      {  10,2,10,5, 0,"ItPo_Perm_Health_Craft",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_Perm_Herb",1}, {"ItPl_Health_Herb_03",3}}},
      {  21,2,18,5, 0,"itpo_perm_stamina",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"itpl_super_herb",1}}},
      {  29,2,22,5, 0,"itpo_intellect",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_SwampHerb",1}, {"ItPl_Temp_Herb",1}, {"ItAt_Addon_BCKopf",3}}},
      {  11,2,11,5, 0,"ItPo_MegaDrink",{{"ItMi_Flask",1}, {"ItMi_DarkPearl",1}, {"ItMi_Sulfur",1}, {"ItAt_DragonEgg_MIS",9}}},
      {5000,3,12,0, 0,"ItMi_InnosEye_MIS",{{"ItMi_InnosEye_Discharged_Mis",1}, {"ItAt_SwampdragonHeart",1}}},
      {5001,3,12,0, 0,"ItMi_InnosEye_MIS",{{"ItMi_InnosEye_Discharged_Mis",1}, {"ItAt_RockdragonHeart",1}}},
      {5002,3,12,0, 0,"ItMi_InnosEye_MIS",{{"ItMi_InnosEye_Discharged_Mis",1}, {"ItAt_FiredragonHeart",1}}},
      {5003,3,12,0, 0,"ItMi_InnosEye_MIS",{{"ItMi_InnosEye_Discharged_Mis",1}, {"ItAt_IcedragonHeart",1}}},
      {5004,3,12,0, 0,"ItMi_InnosEye_MIS",{{"ItMi_InnosEye_Discharged_Mis",1}, {"itat_reddragonheart",1}}},
      {5005,3,12,0, 0,"ItMi_InnosEye_MIS",{{"ItMi_InnosEye_Discharged_Mis",1}, {"itat_blackdragonheart",1}}},
      {  18,1,15,2, 0,"itpo_speed_02",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPo_Speed",1}}},
      {  19,3,16,5, 0,"ITPO_AMUNSU",{{"ItMi_Flask",1}, {"ItAt_CrawlerMandibles",1}, {"ItAt_WaranFiretongue",1}, {"ItAt_StoneGolemHeart",1}, {"ItAt_SkeletonBone",1}, {"itat_crawlerqueen",1}, {"ItPl_SwampHerb",1}, {"ItPl_Perm_Herb",1}, {"ItPl_Mana_Herb_03",1}, {"ItMi_Aquamarine",1}, {"ItMi_DarkPearl",1}}},
      {  28,1,21,3, 0,"itpo_speed_03",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItFo_Addon_Grog",1}, {"ItPl_Temp_Herb",1}, {"ItAt_BlackSnapperLiver",1}}},
      {  30,3,23,4, 0,"itpo_falldefence",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_Weed",1}, {"ItPl_Temp_Herb",1}, {"ItAt_Wing",15}}},
      {  31,2,24,5, 0,"itpo_firedefence",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_Temp_Herb",1}, {"ItAt_WaranFiretongue",5}, {"ItPl_Mana_Herb_02",2}}},
      {  33,3,26,0, 0,"ItPo_ConstFake",{{"ItMi_Flask",1}, {"ItPl_SwampHerb",1}, {"ItMi_Aquamarine",1}, {"ItFo_Cheese",1}, {"ItPl_Mushroom_01",1}, {"ItMi_SilverRing",1}, {"ItFo_Water",1}, {"ItAt_SkeletonBone",1}}},
      {  34,3,27,3, 0,"itpo_soulriver",{{"ItMi_Flask",1}, {"ItPl_SwampHerb",1}, {"ItPo_Mana_01",1}, {"itat_luzianheart",1}, {"ItMi_Sulfur",1}, {"ItMi_Aquamarine",1}, {"ItMi_DarkPearl",1}}},
      {  35,3,28,2, 0,"ItPo_Addon_Geist_01",{{"ItMi_Flask",1}, {"ItAt_Sting",2}, {"ItPo_Health_01",1}, {"ItPo_Mana_02",1}, {"ItFo_Addon_Pfeffer_01",1}}},
      {  36,3,29,2, 0,"ItPo_Addon_Geist_02",{{"ItMi_Flask",1}, {"ItAt_Sting",2}, {"ItPo_Health_01",1}, {"ItPo_Mana_02",1}, {"ItFo_Addon_Pfeffer_01",1}}},
      {  37,3,30,2, 0,"itmi_prisonsoul_awake",{{"itmi_prisonsoul",1}, {"itmi_orcblood",1}, {"itmi_barlokheart",1}, {"ItMi_Sulfur",5}}},
      {  38,3,31,1, 0,"ItAt_PumaMuscle_Jir",{{"ItMi_Moleratlubric_MIS",1}, {"ItAt_PumaMuscle",1}}},
      {  39,3,32,0, 0,"ItMi_SulfurMuttonRaw",{{"ItFoMuttonRaw",1}, {"ItMi_Sulfur",1}}},
      {  40,4,33,3, 0,"itfo_ealbalzamtwo",{{"ItMi_Flask",1}, {"ItPl_Temp_Herb",3}, {"itfo_wineberrys",3}, {"ItAt_DemonHeart",2}}},
      {  41,4,34,3, 0,"itfo_ealbalzam",{{"ItMi_Flask",1}, {"ItFo_Addon_Rum",1}, {"ItPl_SwampHerb",2}, {"itfo_wineberrys",2}, {"ItAt_WaranFiretongue",1}}},
      {  44,4,37,2, 0,"ItFo_Addon_SchnellerHering",{{"ItMi_Flask",1}, {"ItFo_Addon_Rum",1}, {"ItPl_Speed_Herb_01",1}, {"ItFo_Fish",1}}},
      {  43,4,36,2, 0,"ItFo_Addon_LousHammer",{{"ItMi_Flask",1}, {"ItFo_Addon_Rum",1}, {"ItPl_SwampHerb",1}, {"ItPl_Beet",2}, {"ItAt_SharkTeeth",1}}},
      {  42,4,35,2, 0,"ItFo_Addon_SchlafHammer",{{"ItMi_Flask",1}, {"ItFo_Addon_Rum",2}, {"ItPl_SwampHerb",1}, {"ItPl_Beet",2}, {"ItAt_SharkTeeth",1}}},
      {  45,4,38,1, 0,"itfo_booze_ext",{{"ItFo_Booze",10}}},
      {  46,4,39,1, 0,"ItFo_Alcohol",{{"ItFo_Booze",2}}},
      {  47,4,40,1, 0,"ItFo_Alcohol",{{"ItFo_Addon_Grog",2}}},
      {  48,4,41,1, 0,"ItFo_Alcohol",{{"ItFo_Addon_Rum",2}}},
      {  49,4,42,1, 0,"ItFo_Alcohol",{{"itfo_addon_orcrum",1}}},
      {  50,5,43,1, 0,"ITPO_ANPOIS",{{"ItMi_Flask",1}, {"ItFo_Water",1}, {"ItMi_Salt",1}, {"ItMi_Sulfur",1}, {"ItMi_Coal",1}}},
      {  51,5,44,3, 0,"ITPO_POISON",{{"ItMi_Flask",1}, {"ItPl_SwampHerb",1}, {"ItFo_Water",1}, {"ItAt_Sting",1}}},
      {  52,5,45,3, 0,"ITPO_POISON",{{"ItMi_Flask",1}, {"ItPl_SwampHerb",1}, {"ItFo_Water",1}, {"ItAt_SpiderMandibles",1}}},
      {  53,5,46,1, 0,"ITPO_POISON_01",{{"ItMi_Flask",1}, {"ItPl_SwampHerb",1}, {"ItPl_Health_Herb_01",1}}},
      {  54,5,47,1, 0,"ITPO_POISON_02",{{"ItMi_Flask",1}, {"ItPl_SwampHerb",1}, {"ItPl_Blueplant",1}, {"ItPl_Health_Herb_01",1}}},
      {  55,5,48,1, 0,"ITPO_POISON_03",{{"ItMi_Flask",1}, {"ItPl_Blueplant",2}, {"ItPl_Health_Herb_01",1}, {"ItPl_Mushroom_01",1}}},
      {  56,5,49,1, 0,"ITPO_POISON_04",{{"ItMi_Flask",1}, {"ItPl_Blueplant",2}, {"ItPl_Health_Herb_01",1}, {"ItAt_Sting",1}}},
      {  57,5,50,1, 0,"ITPO_POISON_05",{{"ItMi_Flask",1}, {"ItPl_Blueplant",3}, {"ItPl_Temp_Herb",1}, {"ItAt_Sting",1}}},
      {  58,5,51,2, 0,"ITPO_POISON_ANIMAL",{{"ItMi_Flask",1}, {"ItPl_Temp_Herb",1}, {"ItPl_Health_Herb_01",2}, {"ItAt_Sting",1}}},
      {  59,5,52,2, 0,"ITPO_POISON_ORCS",{{"ItMi_Flask",1}, {"ItPl_Temp_Herb",1}, {"ItPl_Mana_Herb_01",2}, {"ItAt_Sting",1}}},
      {  60,5,53,2, 0,"ITPO_POISON_RALIUS",{{"ItMi_Flask",1}, {"ItPl_Temp_Herb",1}, {"ItPl_Mana_Herb_02",2}, {"ItAt_Sting",1}}},
      {  61,3,54,0, 0,"ItMi_Honigtabak",{{"ItMi_ApfelTabak",1}, {"ItFo_Honey",1}}},
      {  62,3,55,0, 0,"ItMi_SumpfTabak",{{"ItMi_ApfelTabak",1}, {"ItPl_SwampHerb",1}}},
      {  63,3,56,0, 0,"ItMi_PilzTabak",{{"ItMi_ApfelTabak",1}, {"ItPl_Mushroom_01",1}}},
      {  64,3,57,0, 5,"ItMi_Joint",{{"ItPl_SwampHerb",1}}},
      {  65,3,58,0, 5,"ItMi_Addon_Joint_01",{{"ItPl_SwampHerb",2}, {"ItPl_Temp_Herb",1}}},
      {  66,3,59,0,10,"itmi_joint_02",{{"ItPl_SwampHerb",4}, {"ItPl_Temp_Herb",1}}},
      {  67,3,60,0,10,"itmi_joint_03",{{"ItPl_SwampHerb",8}, {"ItPl_Temp_Herb",1}}},
      {  68,2,61,0, 0,"itpo_perm_str",{{"ItMi_Flask",1}, {"ItMi_PotionPermBlank",1}}},
      {  69,2,62,0, 0,"itpo_perm_dex",{{"ItMi_Flask",1}, {"ItMi_PotionPermBlank",1}}},
      {  70,2,63,0, 0,"ItPo_Perm_Mana_Craft",{{"ItMi_Flask",1}, {"ItMi_PotionPermBlank",1}}},
      {  71,2,64,0, 0,"ItPo_Perm_Health_Craft",{{"ItMi_Flask",1}, {"ItMi_PotionPermBlank",1}}},
      {  72,2,65,0, 0,"ItPo_Perm_Stamina",{{"ItMi_Flask",1}, {"ItMi_PotionPermBlank",1}}},
      {  73,5,66,0, 0,"ITPO_POISON_ALL",{{"ITPO_POISON_03",1}, {"ITPO_POISON_ORCS",1}, {"ITPO_POISON_ANIMAL",1}, {"ITPO_POISON_RALIUS",1}}},
      {  74,5,67,0, 0,"ITPO_POISON_ALL",{{"ITPO_POISON_04",1}, {"ITPO_POISON_ORCS",1}, {"ITPO_POISON_ANIMAL",1}, {"ITPO_POISON_RALIUS",1}}},
      {  75,5,68,0, 0,"ITPO_POISON_ALL",{{"ITPO_POISON_05",1}, {"ITPO_POISON_ORCS",1}, {"ITPO_POISON_ANIMAL",1}, {"ITPO_POISON_RALIUS",1}}},
      {  76,0,69,0, 0,"ItPo_Mana_Addon_04",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_Temp_Herb",1}, {"ItPl_Blueplant",1}, {"ItPo_Mana_02",3}}},
      {  77,0,70,0, 0,"ItPo_Mana_Addon_04",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_Temp_Herb",1}, {"ItPl_Blueplant",1}, {"ItPo_Mana_03",2}}},
      {  78,0,71,0, 0,"ItPo_Health_Addon_04",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_Temp_Herb",1}, {"ItPl_Blueplant",1}, {"ItPo_Health_02",3}}},
      {  79,0,72,0, 0,"ItPo_Health_Addon_04",{{"ItMi_Flask",1}, {"ItFo_Alcohol",1}, {"ItPl_Temp_Herb",1}, {"ItPl_Blueplant",1}, {"ItPo_Health_03",2}}},
      {  80,3,73,0, 0,"ITMI_POWDER_FIRE",{{"ItMi_Flask",5}, {"ItMi_Sulfur",1}, {"ItMi_JustTree",10}, {"ItFo_Alcohol",1}, {"ItPl_Mana_Herb_01",1}, {"ItPl_Mana_Herb_02",1}}},
      {  81,3,74,0, 0,"ITMI_POWDER_SMOKE",{{"ItMi_Flask",5}, {"ItMi_Sulfur",1}, {"ItMi_Pitch",1}, {"ItPl_Blueplant",2}, {"ItMi_JustTree",10}}},
      {  82,3,75,0, 0,"ITMI_POWDER_EXPLOSION",{{"ItMi_Flask",5}, {"ItMi_Sulfur",2}, {"ItMi_Pitch",1}, {"ItMiSwordraw",1}, {"ItFo_Alcohol",2}, {"ItPl_Blueplant",2}, {"ItMi_JustTree",10}}},
      {  83,3,76,0, 0,"ITMI_POWDER_ACID",{{"ItMi_Flask",5}, {"ItPl_Blueplant",2}, {"ItPl_Temp_Herb",1}, {"ItPl_Health_Herb_02",1}, {"ItAt_Sting",2}, {"ItFo_Alcohol",1}, {"ItMi_JustTree",10}}},
      {  84,3,77,0, 0,"ITMI_POWDER_MAGIC",{{"ItMi_Flask",3}, {"ItMiSwordraw",1}, {"ItMi_Nugget",3}, {"ItPl_Mana_Herb_01",1}, {"ItMi_Quartz",1}, {"ItMi_JustTree",10}}},
      {  85,3,78,0, 0,"ITMI_POWDER_UNDEAD",{{"ItMi_Flask",3}, {"ItMi_HolyWater",1}, {"ItMi_Rockcrystal",1}, {"ItPl_Blueplant",1}, {"ItMi_JustTree",10}}},
      };
    return list;
    }
  }
