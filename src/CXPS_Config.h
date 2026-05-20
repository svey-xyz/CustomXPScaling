/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef CXPS_CONFIG_H
#define CXPS_CONFIG_H

#include "CXPS_LevelPercentReward.h"
#include "ConfigValueCache.h"
#include "Define.h"

#include <array>
#include <cstdint>

// Cached configuration for the CustomXPScaling module.
//
// Every CustomXPScaling.* key the runtime reads on a hot path is registered
// here at world boot (and re-read on .reload config) via AzerothCore's
// ConfigValueCache, so the per-event XP path does not touch sConfigMgr,
// allocate strings, or run std::stof.
//
// LevelPercent ("<base>:<randomizer>") strings are parsed exactly once per
// load: the raw string is read at BuildConfigCache time and the parsed pair
// is stored in a parallel array keyed by the same enum (_parsedLevelPercents).
// Per-source overrides (Gather / Craft / Fish / Disenchant / Smelt / Lockpick)
// inherit from the global ProfessionsXP.LevelPercent by threading the parsed
// global value in as the ParseLevelPercent defaults -- a missing or malformed
// per-source key silently falls back to the global, same as before.

enum class CXPSConfig : std::uint32_t
{
    // ---- Top level ---------------------------------------------------------
    ENABLE,
    ANNOUNCE,
    ALLOW_PLAYER_TOGGLE,
    LOG_TO_PLAYER,
    LOG_TO_PLAYER_ALLOW_PLAYER_TOGGLE,

    // ---- Level scaling -----------------------------------------------------
    LEVEL_XP_ENABLE,
    LEVEL_XP_SCALING_1_9,
    LEVEL_XP_SCALING_10_19,
    LEVEL_XP_SCALING_20_29,
    LEVEL_XP_SCALING_30_39,
    LEVEL_XP_SCALING_40_49,
    LEVEL_XP_SCALING_50_59,
    LEVEL_XP_SCALING_60_69,
    LEVEL_XP_SCALING_70_79,

    // ---- Source multipliers ------------------------------------------------
    KILL_XP_ENABLE,
    KILL_XP_SCALING,
    RARE_XP_ENABLE,
    RARE_XP_SCALING,
    RARE_XP_RANK_SCALING,
    QUEST_XP_ENABLE,
    QUEST_XP_SCALING,
    EXPLORE_XP_ENABLE,
    EXPLORE_XP_SCALING,

    // ---- Professions -------------------------------------------------------
    PROFESSIONS_ENABLE,
    PROFESSIONS_LEVEL_PERCENT,                  // string, pre-parsed
    PROFESSIONS_DIFFICULTY_GRAY,
    PROFESSIONS_DIFFICULTY_GREEN,
    PROFESSIONS_DIFFICULTY_YELLOW,
    PROFESSIONS_DIFFICULTY_ORANGE,

    // Per-source LevelPercent overrides (strings, pre-parsed). Each inherits
    // from PROFESSIONS_LEVEL_PERCENT when the per-source key is absent or
    // malformed -- the inheritance happens at parse time, not at lookup time.
    PROFESSIONS_GATHER_LEVEL_PERCENT,
    PROFESSIONS_CRAFT_LEVEL_PERCENT,
    PROFESSIONS_FISH_LEVEL_PERCENT,
    PROFESSIONS_DISENCHANT_LEVEL_PERCENT,
    PROFESSIONS_SMELT_LEVEL_PERCENT,
    PROFESSIONS_LOCKPICK_LEVEL_PERCENT,

    // Per-skill multipliers (default 1.0 = unchanged). Layered on top of the
    // Difficulty.* and Rank.* multipliers so a server can boost individual
    // professions (e.g. Cooking 3x) without touching anything else.
    PROFESSIONS_SKILL_ALCHEMY,
    PROFESSIONS_SKILL_BLACKSMITHING,
    PROFESSIONS_SKILL_COOKING,
    PROFESSIONS_SKILL_DISENCHANTING,
    PROFESSIONS_SKILL_ENCHANTING,
    PROFESSIONS_SKILL_ENGINEERING,
    PROFESSIONS_SKILL_FIRST_AID,
    PROFESSIONS_SKILL_FISHING,
    PROFESSIONS_SKILL_HERBALISM,
    PROFESSIONS_SKILL_INSCRIPTION,
    PROFESSIONS_SKILL_JEWELCRAFTING,
    PROFESSIONS_SKILL_LEATHERWORKING,
    PROFESSIONS_SKILL_LOCKPICKING,
    PROFESSIONS_SKILL_MINING,
    PROFESSIONS_SKILL_SKINNING,
    PROFESSIONS_SKILL_SMELTING,
    PROFESSIONS_SKILL_TAILORING,

    // Skill-rank bracket multipliers (default 1.0). Bracket is chosen from
    // SkillLineAbilityEntry::MinSkillLineRank for crafts, the current skill
    // value for gathering / fishing -- matches the WoW expansion tiering.
    PROFESSIONS_RANK_APPRENTICE,   //   1-75
    PROFESSIONS_RANK_JOURNEYMAN,   //  76-150
    PROFESSIONS_RANK_EXPERT,       // 151-225
    PROFESSIONS_RANK_ARTISAN,      // 226-300
    PROFESSIONS_RANK_MASTER,       // 301-375
    PROFESSIONS_RANK_GRANDMASTER,  // 376-450

    // ---- Achievements ------------------------------------------------------
    ACHIEVEMENT_XP_ENABLE,
    ACHIEVEMENT_XP_LEVEL_PERCENT,               // string, pre-parsed
    ACHIEVEMENT_XP_POINTS_PER_UNIT,
    ACHIEVEMENT_XP_MIN_MULTIPLIER,
    ACHIEVEMENT_XP_MAX_MULTIPLIER,

    // ---- Taxi nodes --------------------------------------------------------
    TAXI_NODE_XP_ENABLE,
    TAXI_NODE_XP_LEVEL_PERCENT,                 // string, pre-parsed

    // ---- Cast speed (profession spell cast time scaling) ------------------
    // Applied once at world startup -- the module rewrites SpellInfo cast
    // times in-place for every profession spell (crafts, gathering, fishing,
    // disenchant, smelt, lockpick). Per-source multiplier is layered with the
    // per-rank multiplier; >= 1.0 slows casts, < 1.0 speeds them up. A
    // global MinCastTimeMs floor keeps any modified cast from collapsing to
    // an instant. Changes to these keys require a worldserver restart --
    // SpellInfo mutation is not undone on .reload config.
    CASTSPEED_ENABLE,
    CASTSPEED_LOG_TO_CONSOLE,
    CASTSPEED_MIN_CAST_TIME_MS,

    CASTSPEED_CRAFT_MULTIPLIER,
    CASTSPEED_GATHER_MULTIPLIER,
    CASTSPEED_FISH_MULTIPLIER,
    CASTSPEED_DISENCHANT_MULTIPLIER,
    CASTSPEED_SMELT_MULTIPLIER,
    CASTSPEED_LOCKPICK_MULTIPLIER,

    // Per-rank brackets, parallel to PROFESSIONS_RANK_* but tuned
    // independently so admins can slow late-tier recipes without touching
    // XP rates. Boundaries are the same: > 75 / > 150 / > 225 / > 300 / > 375.
    CASTSPEED_RANK_APPRENTICE,    //   1- 75
    CASTSPEED_RANK_JOURNEYMAN,    //  76-150
    CASTSPEED_RANK_EXPERT,        // 151-225
    CASTSPEED_RANK_ARTISAN,       // 226-300
    CASTSPEED_RANK_MASTER,        // 301-375
    CASTSPEED_RANK_GRANDMASTER,   // 376-450

    NUM_CONFIGS,
};

// Logical source for a profession spell cast-time bucket. Parallels
// CXPSProfessionSource but kept separate so the two subsystems can evolve
// independently (e.g. CastSpeed may someday add channel-only sources).
enum CXPSCastSpeedSource : std::uint8_t
{
    CXPS_CAST_SPEED_CRAFT = 0,
    CXPS_CAST_SPEED_GATHER,
    CXPS_CAST_SPEED_FISH,
    CXPS_CAST_SPEED_DISENCHANT,
    CXPS_CAST_SPEED_SMELT,
    CXPS_CAST_SPEED_LOCKPICK,
};

// Logical XP-source labels used by GiveProfessionXP. Maps to a per-source
// LevelPercent override key and to a human-readable label for the chat log.
enum CXPSProfessionSource : std::uint8_t
{
    CXPS_PROF_SRC_GATHER = 0,
    CXPS_PROF_SRC_CRAFT,
    CXPS_PROF_SRC_FISH,
    CXPS_PROF_SRC_DISENCHANT,
    CXPS_PROF_SRC_SMELT,
    CXPS_PROF_SRC_LOCKPICK,
};

class CXPSConfigData : public ConfigValueCache<CXPSConfig>
{
public:
    CXPSConfigData() : ConfigValueCache(CXPSConfig::NUM_CONFIGS) { }

    // Called from the WORLDHOOK_ON_BEFORE_CONFIG_LOAD wrapper. Registers every
    // cached key with the base class and parses every LevelPercent string into
    // _parsedLevelPercents. Re-runs cleanly on .reload config because the
    // values are all flagged Reloadable::Yes.
    void BuildConfigCache() override;

    // Typed accessors. Marked inline + const so the hot path is a single
    // array indexed read after templated dispatch.
    bool        GetBool(CXPSConfig key)  const { return GetConfigValue<bool>(key); }
    float       GetFloat(CXPSConfig key) const { return GetConfigValue<float>(key); }

    // The parsed LevelPercent for a key registered as a string. Returns the
    // pre-parsed pair (filled in BuildConfigCache); never reparses on access.
    CXPS::LevelPercent const& GetLevelPercent(CXPSConfig key) const
    {
        return _parsedLevelPercents[static_cast<std::size_t>(key)];
    }

    // Resolves a profession source to its LevelPercent cache key.
    static CXPSConfig SourceLevelPercentKey(CXPSProfessionSource source);

    // Human-readable label for the source -- used in the per-event chat log.
    static char const* SourceLabel(CXPSProfessionSource source);

    // Returns the per-skill multiplier for a profession event. `skillId` is
    // the SkillLineAbilityEntry::SkillLine (crafts) or the hook-supplied
    // skill id (gathering/fishing). The two override flags route the lookup
    // to the dedicated buckets that share an underlying SKILL_* id with a
    // different profession (Mining gather vs. Smelting craft, Enchanting
    // craft vs. Disenchanting).
    float GetSkillMultiplier(std::uint32_t skillId, bool isDisenchant, bool isSmelt) const;

    // Returns the rank-bracket multiplier (Apprentice...GrandMaster) for the
    // supplied skill rank. Bracket boundaries match the WoW expansion tiers
    // exactly: > 75 / > 150 / > 225 / > 300 / > 375 -- otherwise apprentice.
    float GetRankMultiplier(std::uint32_t skillRank) const;

    // CastSpeed counterpart to GetRankMultiplier -- same bracket boundaries,
    // but reads the CASTSPEED_RANK_* slots so cast-time scaling and XP scaling
    // can be tuned independently.
    float GetCastSpeedRankMultiplier(std::uint32_t skillRank) const;

    // Resolves a CastSpeed source to its per-source multiplier slot.
    float GetCastSpeedSourceMultiplier(CXPSCastSpeedSource source) const;

    // Human-readable label for a CastSpeed source -- used in the boot log.
    static char const* CastSpeedSourceLabel(CXPSCastSpeedSource source);

private:
    // Parallel to the float/bool cache; slot reused only for *_LEVEL_PERCENT
    // keys. Other slots stay default-initialized and are never read.
    std::array<CXPS::LevelPercent, static_cast<std::size_t>(CXPSConfig::NUM_CONFIGS)>
        _parsedLevelPercents{};
};

extern CXPSConfigData cxpsConfig;

#endif // CXPS_CONFIG_H
