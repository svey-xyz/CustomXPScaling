/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "CXPS_Config.h"
#include "SharedDefines.h"

#include <string>

CXPSConfigData cxpsConfig;

namespace
{
    // Validator helpers. The cache requires both a predicate and a human-
    // readable description so a misconfigured key produces a sensible startup
    // warning. We use a couple of stock ones and keep them out of the header.
    auto AnyBool   = [](bool  const& /*v*/) { return true; };
    auto AnyFloat  = [](float const& /*v*/) { return true; };
    auto AnyString = [](std::string const& /*v*/) { return true; };
    auto NonNegFloat = [](float const& v) { return v >= 0.0f; };
}

void CXPSConfigData::BuildConfigCache()
{
    using Reload = ConfigValueCache::Reloadable;

    // -- Top-level toggles --------------------------------------------------
    SetConfigValue<bool>(CXPSConfig::ENABLE,
        "CustomXPScaling.Enable", true, Reload::Yes, AnyBool, "any");
    SetConfigValue<bool>(CXPSConfig::ANNOUNCE,
        "CustomXPScaling.Announce", true, Reload::Yes, AnyBool, "any");
    SetConfigValue<bool>(CXPSConfig::ALLOW_PLAYER_TOGGLE,
        "CustomXPScaling.AllowPlayerToggle", true, Reload::Yes, AnyBool, "any");
    SetConfigValue<bool>(CXPSConfig::LOG_TO_PLAYER,
        "CustomXPScaling.LogToPlayer", false, Reload::Yes, AnyBool, "any");
    SetConfigValue<bool>(CXPSConfig::LOG_TO_PLAYER_ALLOW_PLAYER_TOGGLE,
        "CustomXPScaling.LogToPlayer.AllowPlayerToggle", true, Reload::Yes, AnyBool, "any");

    // -- Level scaling brackets --------------------------------------------
    SetConfigValue<bool>(CXPSConfig::LEVEL_XP_ENABLE,
        "CustomXPScaling.LevelXP.Enable", true, Reload::Yes, AnyBool, "any");
    SetConfigValue<float>(CXPSConfig::LEVEL_XP_SCALING_1_9,
        "CustomXPScaling.LevelXP.Scaling.1-9", 0.2f, Reload::Yes, NonNegFloat, ">= 0");
    SetConfigValue<float>(CXPSConfig::LEVEL_XP_SCALING_10_19,
        "CustomXPScaling.LevelXP.Scaling.10-19", 0.3f, Reload::Yes, NonNegFloat, ">= 0");
    SetConfigValue<float>(CXPSConfig::LEVEL_XP_SCALING_20_29,
        "CustomXPScaling.LevelXP.Scaling.20-29", 0.8f, Reload::Yes, NonNegFloat, ">= 0");
    SetConfigValue<float>(CXPSConfig::LEVEL_XP_SCALING_30_39,
        "CustomXPScaling.LevelXP.Scaling.30-39", 1.0f, Reload::Yes, NonNegFloat, ">= 0");
    SetConfigValue<float>(CXPSConfig::LEVEL_XP_SCALING_40_49,
        "CustomXPScaling.LevelXP.Scaling.40-49", 1.2f, Reload::Yes, NonNegFloat, ">= 0");
    SetConfigValue<float>(CXPSConfig::LEVEL_XP_SCALING_50_59,
        "CustomXPScaling.LevelXP.Scaling.50-59", 1.3f, Reload::Yes, NonNegFloat, ">= 0");
    SetConfigValue<float>(CXPSConfig::LEVEL_XP_SCALING_60_69,
        "CustomXPScaling.LevelXP.Scaling.60-69", 1.3f, Reload::Yes, NonNegFloat, ">= 0");
    SetConfigValue<float>(CXPSConfig::LEVEL_XP_SCALING_70_79,
        "CustomXPScaling.LevelXP.Scaling.70-79", 1.3f, Reload::Yes, NonNegFloat, ">= 0");

    // -- Source-specific multipliers ---------------------------------------
    SetConfigValue<bool>(CXPSConfig::KILL_XP_ENABLE,
        "CustomXPScaling.KillXP.Enable", false, Reload::Yes, AnyBool, "any");
    SetConfigValue<float>(CXPSConfig::KILL_XP_SCALING,
        "CustomXPScaling.KillXP.Scaling", 1.0f, Reload::Yes, NonNegFloat, ">= 0");
    SetConfigValue<bool>(CXPSConfig::RARE_XP_ENABLE,
        "CustomXPScaling.RareXP.Enable", false, Reload::Yes, AnyBool, "any");
    SetConfigValue<float>(CXPSConfig::RARE_XP_SCALING,
        "CustomXPScaling.RareXP.Scaling", 1.0f, Reload::Yes, NonNegFloat, ">= 0");
    SetConfigValue<bool>(CXPSConfig::RARE_XP_RANK_SCALING,
        "CustomXPScaling.RareXP.RankScaling", true, Reload::Yes, AnyBool, "any");
    SetConfigValue<bool>(CXPSConfig::QUEST_XP_ENABLE,
        "CustomXPScaling.QuestXP.Enable", true, Reload::Yes, AnyBool, "any");
    SetConfigValue<float>(CXPSConfig::QUEST_XP_SCALING,
        "CustomXPScaling.QuestXP.Scaling", 1.0f, Reload::Yes, NonNegFloat, ">= 0");
    SetConfigValue<bool>(CXPSConfig::EXPLORE_XP_ENABLE,
        "CustomXPScaling.ExploreXP.Enable", true, Reload::Yes, AnyBool, "any");
    SetConfigValue<float>(CXPSConfig::EXPLORE_XP_SCALING,
        "CustomXPScaling.ExploreXP.Scaling", 1.0f, Reload::Yes, NonNegFloat, ">= 0");

    // -- Professions (toggles + difficulty multipliers) --------------------
    SetConfigValue<bool>(CXPSConfig::PROFESSIONS_ENABLE,
        "CustomXPScaling.ProfessionsXP.Enable", true, Reload::Yes, AnyBool, "any");
    SetConfigValue<std::string>(CXPSConfig::PROFESSIONS_LEVEL_PERCENT,
        "CustomXPScaling.ProfessionsXP.LevelPercent", std::string("1.0:0.25"),
        Reload::Yes, AnyString, "any");
    SetConfigValue<float>(CXPSConfig::PROFESSIONS_DIFFICULTY_GRAY,
        "CustomXPScaling.ProfessionsXP.Difficulty.Gray", 0.0f, Reload::Yes, NonNegFloat, ">= 0");
    SetConfigValue<float>(CXPSConfig::PROFESSIONS_DIFFICULTY_GREEN,
        "CustomXPScaling.ProfessionsXP.Difficulty.Green", 0.5f, Reload::Yes, NonNegFloat, ">= 0");
    SetConfigValue<float>(CXPSConfig::PROFESSIONS_DIFFICULTY_YELLOW,
        "CustomXPScaling.ProfessionsXP.Difficulty.Yellow", 1.0f, Reload::Yes, NonNegFloat, ">= 0");
    SetConfigValue<float>(CXPSConfig::PROFESSIONS_DIFFICULTY_ORANGE,
        "CustomXPScaling.ProfessionsXP.Difficulty.Orange", 1.5f, Reload::Yes, NonNegFloat, ">= 0");

    // Per-source LevelPercent override strings. Register first, parse below.
    SetConfigValue<std::string>(CXPSConfig::PROFESSIONS_GATHER_LEVEL_PERCENT,
        "CustomXPScaling.ProfessionsXP.Gather.LevelPercent", std::string(""),
        Reload::Yes, AnyString, "any");
    SetConfigValue<std::string>(CXPSConfig::PROFESSIONS_CRAFT_LEVEL_PERCENT,
        "CustomXPScaling.ProfessionsXP.Craft.LevelPercent", std::string(""),
        Reload::Yes, AnyString, "any");
    SetConfigValue<std::string>(CXPSConfig::PROFESSIONS_FISH_LEVEL_PERCENT,
        "CustomXPScaling.ProfessionsXP.Fish.LevelPercent", std::string(""),
        Reload::Yes, AnyString, "any");
    SetConfigValue<std::string>(CXPSConfig::PROFESSIONS_DISENCHANT_LEVEL_PERCENT,
        "CustomXPScaling.ProfessionsXP.Disenchant.LevelPercent", std::string(""),
        Reload::Yes, AnyString, "any");
    SetConfigValue<std::string>(CXPSConfig::PROFESSIONS_SMELT_LEVEL_PERCENT,
        "CustomXPScaling.ProfessionsXP.Smelt.LevelPercent", std::string(""),
        Reload::Yes, AnyString, "any");
    SetConfigValue<std::string>(CXPSConfig::PROFESSIONS_LOCKPICK_LEVEL_PERCENT,
        "CustomXPScaling.ProfessionsXP.Lockpick.LevelPercent", std::string(""),
        Reload::Yes, AnyString, "any");

    // -- Per-skill multipliers (default 1.0 = no per-skill change) ----------
    auto setSkill = [this](CXPSConfig key, char const* name)
    {
        SetConfigValue<float>(key, name, 1.0f, Reload::Yes, NonNegFloat, ">= 0");
    };
    setSkill(CXPSConfig::PROFESSIONS_SKILL_ALCHEMY,        "CustomXPScaling.ProfessionsXP.Skill.Alchemy.Multiplier");
    setSkill(CXPSConfig::PROFESSIONS_SKILL_BLACKSMITHING,  "CustomXPScaling.ProfessionsXP.Skill.Blacksmithing.Multiplier");
    setSkill(CXPSConfig::PROFESSIONS_SKILL_COOKING,        "CustomXPScaling.ProfessionsXP.Skill.Cooking.Multiplier");
    setSkill(CXPSConfig::PROFESSIONS_SKILL_DISENCHANTING,  "CustomXPScaling.ProfessionsXP.Skill.Disenchanting.Multiplier");
    setSkill(CXPSConfig::PROFESSIONS_SKILL_ENCHANTING,     "CustomXPScaling.ProfessionsXP.Skill.Enchanting.Multiplier");
    setSkill(CXPSConfig::PROFESSIONS_SKILL_ENGINEERING,    "CustomXPScaling.ProfessionsXP.Skill.Engineering.Multiplier");
    setSkill(CXPSConfig::PROFESSIONS_SKILL_FIRST_AID,      "CustomXPScaling.ProfessionsXP.Skill.FirstAid.Multiplier");
    setSkill(CXPSConfig::PROFESSIONS_SKILL_FISHING,        "CustomXPScaling.ProfessionsXP.Skill.Fishing.Multiplier");
    setSkill(CXPSConfig::PROFESSIONS_SKILL_HERBALISM,      "CustomXPScaling.ProfessionsXP.Skill.Herbalism.Multiplier");
    setSkill(CXPSConfig::PROFESSIONS_SKILL_INSCRIPTION,    "CustomXPScaling.ProfessionsXP.Skill.Inscription.Multiplier");
    setSkill(CXPSConfig::PROFESSIONS_SKILL_JEWELCRAFTING,  "CustomXPScaling.ProfessionsXP.Skill.Jewelcrafting.Multiplier");
    setSkill(CXPSConfig::PROFESSIONS_SKILL_LEATHERWORKING, "CustomXPScaling.ProfessionsXP.Skill.Leatherworking.Multiplier");
    setSkill(CXPSConfig::PROFESSIONS_SKILL_LOCKPICKING,    "CustomXPScaling.ProfessionsXP.Skill.Lockpicking.Multiplier");
    setSkill(CXPSConfig::PROFESSIONS_SKILL_MINING,         "CustomXPScaling.ProfessionsXP.Skill.Mining.Multiplier");
    setSkill(CXPSConfig::PROFESSIONS_SKILL_SKINNING,       "CustomXPScaling.ProfessionsXP.Skill.Skinning.Multiplier");
    setSkill(CXPSConfig::PROFESSIONS_SKILL_SMELTING,       "CustomXPScaling.ProfessionsXP.Skill.Smelting.Multiplier");
    setSkill(CXPSConfig::PROFESSIONS_SKILL_TAILORING,      "CustomXPScaling.ProfessionsXP.Skill.Tailoring.Multiplier");

    // -- Skill-rank brackets ------------------------------------------------
    auto setRank = [this](CXPSConfig key, char const* name)
    {
        SetConfigValue<float>(key, name, 1.0f, Reload::Yes, NonNegFloat, ">= 0");
    };
    setRank(CXPSConfig::PROFESSIONS_RANK_APPRENTICE,  "CustomXPScaling.ProfessionsXP.Rank.Apprentice");
    setRank(CXPSConfig::PROFESSIONS_RANK_JOURNEYMAN,  "CustomXPScaling.ProfessionsXP.Rank.Journeyman");
    setRank(CXPSConfig::PROFESSIONS_RANK_EXPERT,      "CustomXPScaling.ProfessionsXP.Rank.Expert");
    setRank(CXPSConfig::PROFESSIONS_RANK_ARTISAN,     "CustomXPScaling.ProfessionsXP.Rank.Artisan");
    setRank(CXPSConfig::PROFESSIONS_RANK_MASTER,      "CustomXPScaling.ProfessionsXP.Rank.Master");
    setRank(CXPSConfig::PROFESSIONS_RANK_GRANDMASTER, "CustomXPScaling.ProfessionsXP.Rank.GrandMaster");

    // -- Achievement XP -----------------------------------------------------
    SetConfigValue<bool>(CXPSConfig::ACHIEVEMENT_XP_ENABLE,
        "CustomXPScaling.AchievementXP.Enable", false, Reload::Yes, AnyBool, "any");
    SetConfigValue<std::string>(CXPSConfig::ACHIEVEMENT_XP_LEVEL_PERCENT,
        "CustomXPScaling.AchievementXP.LevelPercent", std::string("1.0:0.25"),
        Reload::Yes, AnyString, "any");
    SetConfigValue<float>(CXPSConfig::ACHIEVEMENT_XP_POINTS_PER_UNIT,
        "CustomXPScaling.AchievementXP.PointsPerUnit", 10.0f, Reload::Yes, AnyFloat, "any");
    SetConfigValue<float>(CXPSConfig::ACHIEVEMENT_XP_MIN_MULTIPLIER,
        "CustomXPScaling.AchievementXP.MinMultiplier", 0.5f, Reload::Yes, AnyFloat, "any");
    SetConfigValue<float>(CXPSConfig::ACHIEVEMENT_XP_MAX_MULTIPLIER,
        "CustomXPScaling.AchievementXP.MaxMultiplier", 5.0f, Reload::Yes, AnyFloat, "any");

    // -- Taxi node XP -------------------------------------------------------
    SetConfigValue<bool>(CXPSConfig::TAXI_NODE_XP_ENABLE,
        "CustomXPScaling.TaxiNodeXP.Enable", true, Reload::Yes, AnyBool, "any");
    SetConfigValue<std::string>(CXPSConfig::TAXI_NODE_XP_LEVEL_PERCENT,
        "CustomXPScaling.TaxiNodeXP.LevelPercent", std::string("2.0:0.5"),
        Reload::Yes, AnyString, "any");

    // -- Cast speed --------------------------------------------------------
    // SpellInfo mutation is applied at WORLDHOOK_ON_STARTUP and is *not*
    // reversed on .reload config, so the Enable / multiplier values can be
    // marked Reload::Yes safely (they only take effect on the next restart
    // for the cast-time mutation; the cache itself still updates so future
    // restarts pick up edits without a code change).
    SetConfigValue<bool>(CXPSConfig::CASTSPEED_ENABLE,
        "CustomXPScaling.CastSpeed.Enable", false, Reload::Yes, AnyBool, "any");
    SetConfigValue<bool>(CXPSConfig::CASTSPEED_LOG_TO_CONSOLE,
        "CustomXPScaling.CastSpeed.LogToConsole", false, Reload::Yes, AnyBool, "any");
    SetConfigValue<float>(CXPSConfig::CASTSPEED_MIN_CAST_TIME_MS,
        "CustomXPScaling.CastSpeed.MinCastTimeMs", 250.0f, Reload::Yes, NonNegFloat, ">= 0");

    auto setCastSpeedSource = [this](CXPSConfig key, char const* name)
    {
        SetConfigValue<float>(key, name, 1.0f, Reload::Yes, NonNegFloat, ">= 0");
    };
    setCastSpeedSource(CXPSConfig::CASTSPEED_CRAFT_MULTIPLIER,
        "CustomXPScaling.CastSpeed.Craft.Multiplier");
    setCastSpeedSource(CXPSConfig::CASTSPEED_GATHER_MULTIPLIER,
        "CustomXPScaling.CastSpeed.Gather.Multiplier");
    setCastSpeedSource(CXPSConfig::CASTSPEED_FISH_MULTIPLIER,
        "CustomXPScaling.CastSpeed.Fish.Multiplier");
    setCastSpeedSource(CXPSConfig::CASTSPEED_DISENCHANT_MULTIPLIER,
        "CustomXPScaling.CastSpeed.Disenchant.Multiplier");
    setCastSpeedSource(CXPSConfig::CASTSPEED_SMELT_MULTIPLIER,
        "CustomXPScaling.CastSpeed.Smelt.Multiplier");
    setCastSpeedSource(CXPSConfig::CASTSPEED_LOCKPICK_MULTIPLIER,
        "CustomXPScaling.CastSpeed.Lockpick.Multiplier");

    auto setCastSpeedRank = [this](CXPSConfig key, char const* name)
    {
        SetConfigValue<float>(key, name, 1.0f, Reload::Yes, NonNegFloat, ">= 0");
    };
    setCastSpeedRank(CXPSConfig::CASTSPEED_RANK_APPRENTICE,
        "CustomXPScaling.CastSpeed.Rank.Apprentice");
    setCastSpeedRank(CXPSConfig::CASTSPEED_RANK_JOURNEYMAN,
        "CustomXPScaling.CastSpeed.Rank.Journeyman");
    setCastSpeedRank(CXPSConfig::CASTSPEED_RANK_EXPERT,
        "CustomXPScaling.CastSpeed.Rank.Expert");
    setCastSpeedRank(CXPSConfig::CASTSPEED_RANK_ARTISAN,
        "CustomXPScaling.CastSpeed.Rank.Artisan");
    setCastSpeedRank(CXPSConfig::CASTSPEED_RANK_MASTER,
        "CustomXPScaling.CastSpeed.Rank.Master");
    setCastSpeedRank(CXPSConfig::CASTSPEED_RANK_GRANDMASTER,
        "CustomXPScaling.CastSpeed.Rank.GrandMaster");

    // ---- Pre-parse all LevelPercent strings ------------------------------
    //
    // The cache now holds the raw "<base>:<randomizer>" strings; this pass
    // turns each into a CXPS::LevelPercent so the runtime never re-parses.
    // Per-source overrides inherit from the global by threading the parsed
    // global value in as the ParseLevelPercent defaults -- empty / malformed
    // per-source keys silently fall back, just like before the cache existed.
    _parsedLevelPercents[static_cast<std::size_t>(CXPSConfig::PROFESSIONS_LEVEL_PERCENT)] =
        CXPS::ParseLevelPercent(
            "CustomXPScaling.ProfessionsXP.LevelPercent", 1.0f, 0.25f);

    CXPS::LevelPercent const profGlobal =
        _parsedLevelPercents[static_cast<std::size_t>(CXPSConfig::PROFESSIONS_LEVEL_PERCENT)];

    auto parseOverride = [&](CXPSConfig slot, char const* key)
    {
        _parsedLevelPercents[static_cast<std::size_t>(slot)] =
            CXPS::ParseLevelPercent(key, profGlobal.base, profGlobal.randomizer);
    };
    parseOverride(CXPSConfig::PROFESSIONS_GATHER_LEVEL_PERCENT,
        "CustomXPScaling.ProfessionsXP.Gather.LevelPercent");
    parseOverride(CXPSConfig::PROFESSIONS_CRAFT_LEVEL_PERCENT,
        "CustomXPScaling.ProfessionsXP.Craft.LevelPercent");
    parseOverride(CXPSConfig::PROFESSIONS_FISH_LEVEL_PERCENT,
        "CustomXPScaling.ProfessionsXP.Fish.LevelPercent");
    parseOverride(CXPSConfig::PROFESSIONS_DISENCHANT_LEVEL_PERCENT,
        "CustomXPScaling.ProfessionsXP.Disenchant.LevelPercent");
    parseOverride(CXPSConfig::PROFESSIONS_SMELT_LEVEL_PERCENT,
        "CustomXPScaling.ProfessionsXP.Smelt.LevelPercent");
    parseOverride(CXPSConfig::PROFESSIONS_LOCKPICK_LEVEL_PERCENT,
        "CustomXPScaling.ProfessionsXP.Lockpick.LevelPercent");

    _parsedLevelPercents[static_cast<std::size_t>(CXPSConfig::ACHIEVEMENT_XP_LEVEL_PERCENT)] =
        CXPS::ParseLevelPercent(
            "CustomXPScaling.AchievementXP.LevelPercent", 1.0f, 0.25f);

    _parsedLevelPercents[static_cast<std::size_t>(CXPSConfig::TAXI_NODE_XP_LEVEL_PERCENT)] =
        CXPS::ParseLevelPercent(
            "CustomXPScaling.TaxiNodeXP.LevelPercent", 2.0f, 0.5f);
}

CXPSConfig CXPSConfigData::SourceLevelPercentKey(CXPSProfessionSource source)
{
    switch (source)
    {
    case CXPS_PROF_SRC_GATHER:     return CXPSConfig::PROFESSIONS_GATHER_LEVEL_PERCENT;
    case CXPS_PROF_SRC_CRAFT:      return CXPSConfig::PROFESSIONS_CRAFT_LEVEL_PERCENT;
    case CXPS_PROF_SRC_FISH:       return CXPSConfig::PROFESSIONS_FISH_LEVEL_PERCENT;
    case CXPS_PROF_SRC_DISENCHANT: return CXPSConfig::PROFESSIONS_DISENCHANT_LEVEL_PERCENT;
    case CXPS_PROF_SRC_SMELT:      return CXPSConfig::PROFESSIONS_SMELT_LEVEL_PERCENT;
    case CXPS_PROF_SRC_LOCKPICK:   return CXPSConfig::PROFESSIONS_LOCKPICK_LEVEL_PERCENT;
    }
    return CXPSConfig::PROFESSIONS_LEVEL_PERCENT;
}

char const* CXPSConfigData::SourceLabel(CXPSProfessionSource source)
{
    switch (source)
    {
    case CXPS_PROF_SRC_GATHER:     return "Gather";
    case CXPS_PROF_SRC_CRAFT:      return "Craft";
    case CXPS_PROF_SRC_FISH:       return "Fish";
    case CXPS_PROF_SRC_DISENCHANT: return "Disenchant";
    case CXPS_PROF_SRC_SMELT:      return "Smelt";
    case CXPS_PROF_SRC_LOCKPICK:   return "Lockpick";
    }
    return "Unknown";
}

float CXPSConfigData::GetSkillMultiplier(std::uint32_t skillId,
                                         bool isDisenchant, bool isSmelt) const
{
    // Same-skill-id collisions resolved by the override flags: Mining-the-
    // gather and Smelting-the-craft both report SKILL_MINING; Enchanting and
    // Disenchanting both report SKILL_ENCHANTING. The hook decides which one.
    if (isDisenchant)
        return GetFloat(CXPSConfig::PROFESSIONS_SKILL_DISENCHANTING);
    if (isSmelt)
        return GetFloat(CXPSConfig::PROFESSIONS_SKILL_SMELTING);

    switch (skillId)
    {
    case SKILL_ALCHEMY:        return GetFloat(CXPSConfig::PROFESSIONS_SKILL_ALCHEMY);
    case SKILL_BLACKSMITHING:  return GetFloat(CXPSConfig::PROFESSIONS_SKILL_BLACKSMITHING);
    case SKILL_COOKING:        return GetFloat(CXPSConfig::PROFESSIONS_SKILL_COOKING);
    case SKILL_ENCHANTING:     return GetFloat(CXPSConfig::PROFESSIONS_SKILL_ENCHANTING);
    case SKILL_ENGINEERING:    return GetFloat(CXPSConfig::PROFESSIONS_SKILL_ENGINEERING);
    case SKILL_FIRST_AID:      return GetFloat(CXPSConfig::PROFESSIONS_SKILL_FIRST_AID);
    case SKILL_FISHING:        return GetFloat(CXPSConfig::PROFESSIONS_SKILL_FISHING);
    case SKILL_HERBALISM:      return GetFloat(CXPSConfig::PROFESSIONS_SKILL_HERBALISM);
    case SKILL_INSCRIPTION:    return GetFloat(CXPSConfig::PROFESSIONS_SKILL_INSCRIPTION);
    case SKILL_JEWELCRAFTING:  return GetFloat(CXPSConfig::PROFESSIONS_SKILL_JEWELCRAFTING);
    case SKILL_LEATHERWORKING: return GetFloat(CXPSConfig::PROFESSIONS_SKILL_LEATHERWORKING);
    case SKILL_LOCKPICKING:    return GetFloat(CXPSConfig::PROFESSIONS_SKILL_LOCKPICKING);
    case SKILL_MINING:         return GetFloat(CXPSConfig::PROFESSIONS_SKILL_MINING);
    case SKILL_SKINNING:       return GetFloat(CXPSConfig::PROFESSIONS_SKILL_SKINNING);
    case SKILL_TAILORING:      return GetFloat(CXPSConfig::PROFESSIONS_SKILL_TAILORING);
    default: break;
    }
    return 1.0f;
}

float CXPSConfigData::GetRankMultiplier(std::uint32_t skillRank) const
{
    // Bracket boundaries (> 75 / > 150 / > 225 / > 300 / > 375) match the
    // expansion tiers and PE's MultApprentice...MultGrandMaster grouping.
    if (skillRank > 375) return GetFloat(CXPSConfig::PROFESSIONS_RANK_GRANDMASTER);
    if (skillRank > 300) return GetFloat(CXPSConfig::PROFESSIONS_RANK_MASTER);
    if (skillRank > 225) return GetFloat(CXPSConfig::PROFESSIONS_RANK_ARTISAN);
    if (skillRank > 150) return GetFloat(CXPSConfig::PROFESSIONS_RANK_EXPERT);
    if (skillRank > 75)  return GetFloat(CXPSConfig::PROFESSIONS_RANK_JOURNEYMAN);
    return GetFloat(CXPSConfig::PROFESSIONS_RANK_APPRENTICE);
}

float CXPSConfigData::GetCastSpeedRankMultiplier(std::uint32_t skillRank) const
{
    // Mirrors GetRankMultiplier with the CASTSPEED_RANK_* slots. Kept as a
    // separate function so the XP rank ladder and the cast-speed rank ladder
    // can diverge without one accidentally widening the other.
    if (skillRank > 375) return GetFloat(CXPSConfig::CASTSPEED_RANK_GRANDMASTER);
    if (skillRank > 300) return GetFloat(CXPSConfig::CASTSPEED_RANK_MASTER);
    if (skillRank > 225) return GetFloat(CXPSConfig::CASTSPEED_RANK_ARTISAN);
    if (skillRank > 150) return GetFloat(CXPSConfig::CASTSPEED_RANK_EXPERT);
    if (skillRank > 75)  return GetFloat(CXPSConfig::CASTSPEED_RANK_JOURNEYMAN);
    return GetFloat(CXPSConfig::CASTSPEED_RANK_APPRENTICE);
}

float CXPSConfigData::GetCastSpeedSourceMultiplier(CXPSCastSpeedSource source) const
{
    switch (source)
    {
    case CXPS_CAST_SPEED_CRAFT:      return GetFloat(CXPSConfig::CASTSPEED_CRAFT_MULTIPLIER);
    case CXPS_CAST_SPEED_GATHER:     return GetFloat(CXPSConfig::CASTSPEED_GATHER_MULTIPLIER);
    case CXPS_CAST_SPEED_FISH:       return GetFloat(CXPSConfig::CASTSPEED_FISH_MULTIPLIER);
    case CXPS_CAST_SPEED_DISENCHANT: return GetFloat(CXPSConfig::CASTSPEED_DISENCHANT_MULTIPLIER);
    case CXPS_CAST_SPEED_SMELT:      return GetFloat(CXPSConfig::CASTSPEED_SMELT_MULTIPLIER);
    case CXPS_CAST_SPEED_LOCKPICK:   return GetFloat(CXPSConfig::CASTSPEED_LOCKPICK_MULTIPLIER);
    }
    return 1.0f;
}

char const* CXPSConfigData::CastSpeedSourceLabel(CXPSCastSpeedSource source)
{
    switch (source)
    {
    case CXPS_CAST_SPEED_CRAFT:      return "Craft";
    case CXPS_CAST_SPEED_GATHER:     return "Gather";
    case CXPS_CAST_SPEED_FISH:       return "Fish";
    case CXPS_CAST_SPEED_DISENCHANT: return "Disenchant";
    case CXPS_CAST_SPEED_SMELT:      return "Smelt";
    case CXPS_CAST_SPEED_LOCKPICK:   return "Lockpick";
    }
    return "Unknown";
}
