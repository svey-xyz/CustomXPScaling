/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "CXPS_CastSpeed.h"
#include "CXPS_Config.h"
#include "CXPS_LevelPercentReward.h"
#include "Chat.h"
#include "Config.h"
#include "DBCStores.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "World.h"
#include <algorithm>

namespace
{
    // Per-player log preference is stored through AzerothCore's PlayerSetting
    // API (the core-owned character_settings table) under this source namespace.
    // The core handles load on login, save, and cleanup on character delete --
    // no module-side query, cache, or OnPlayerDelete hook needed. Requires the
    // core's PlayerSettings.Enable option for the choice to persist.
    constexpr char const *CXPS_SETTINGS_SOURCE = "mod-cxps";
    constexpr uint8 CXPS_SETTING_LOG = 0;
    constexpr uint8 CXPS_SETTING_ENABLE = 1;

    // Stored values for the CXPS_SETTING_* slots. Tri-state so an explicit "off"
    // is distinguishable from "never chose" -- GetPlayerSetting yields 0 when
    // unset, which we treat as "inherit the server default".
    constexpr uint32 CXPS_LOG_OFF = 1; // player forced logging off
    constexpr uint32 CXPS_LOG_ON = 2;  // player forced logging on

    constexpr uint32 CXPS_ENABLE_OFF = 1; // player opted out of XP scaling
    constexpr uint32 CXPS_ENABLE_ON = 2;  // player opted in to XP scaling

    // Disenchant lives under SKILL_ENCHANTING in SkillLineAbility -- the only
    // way to distinguish it from a real Enchant cast is the spell id. 13262
    // is the canonical Disenchant spell across all 3.3.5 builds.
    constexpr uint32 CXPS_SPELL_DISENCHANT = 13262;
}

class CustomXPScaling : public PlayerScript
{
public:
    // Modern hook-list constructor -- the core skips this script for any hook
    // not listed here. Names verified against
    // src/server/game/Scripting/ScriptDefines/PlayerScript.h: the enum drops
    // the Player infix from the method name (so OnPlayerLogin -> ON_LOGIN)
    // but uses ON_GIVE_EXP for OnPlayerGiveXP and ON_ACHI_COMPLETE for
    // OnPlayerAchievementComplete. Two abbreviations to watch out for.
    CustomXPScaling() : PlayerScript("CustomXPScaling", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_GIVE_EXP,
        PLAYERHOOK_ON_UPDATE_GATHERING_SKILL,
        PLAYERHOOK_ON_UPDATE_CRAFTING_SKILL,
        PLAYERHOOK_ON_UPDATE_FISHING_SKILL,
        PLAYERHOOK_ON_ACHI_COMPLETE,
        PLAYERHOOK_ON_LEARN_TAXI_NODE,
    }) {}

    enum ProfessionDifficulty
    {
        PROF_DIFF_GRAY = 0,
        PROF_DIFF_GREEN,
        PROF_DIFF_YELLOW,
        PROF_DIFF_ORANGE
    };

    void OnPlayerLogin(Player *player) override
    {
        if (cxpsConfig.GetBool(CXPSConfig::ANNOUNCE))
        {
            ChatHandler(player->GetSession()).SendSysMessage(
                "This server is running the |cff4CFF00Custom XP Scaling|r module. "
                "For more info, type .xpscaling help or .xpscaling about.");
        }
        // The per-player log/enable preferences are loaded by the core via the
        // PlayerSetting API before this hook fires -- see ShouldLogToPlayer and
        // IsEnabledFor. Nothing module-side to load or cache here.
    }

    void OnPlayerGiveXP(Player *player, uint32 &amount, Unit *victim, uint8 xpSource) override
    {
        if (!player || !IsEnabledFor(player))
            return;

        const bool shouldLog = ShouldLogToPlayer(player);

        std::stringstream scalingSources;
        float scalingFactor = 1.0f;

        // Apply base level scaling
        scalingFactor *= GetLevelScalingFactor(player, shouldLog ? &scalingSources : nullptr);

        // Apply type-specific scaling
        switch (xpSource)
        {
        case XPSOURCE_QUEST:
        case XPSOURCE_QUEST_DF:
            scalingFactor *= GetQuestScalingFactor(shouldLog ? &scalingSources : nullptr);
            break;
        case XPSOURCE_KILL:
            scalingFactor *= GetKillScalingFactor(victim, shouldLog ? &scalingSources : nullptr);
            break;
        case XPSOURCE_EXPLORE:
            scalingFactor *= GetExploreScalingFactor(shouldLog ? &scalingSources : nullptr);
            break;
        default:
            break; // No additional scaling
        }

        const float calculatedXP = static_cast<float>(amount) * scalingFactor;
        const uint32 newAmount = static_cast<uint32>(std::round(calculatedXP));

        if (shouldLog)
        {
            LogXPDetails(player, xpSource, amount, newAmount, scalingSources);
        }

        amount = newAmount;
    }

private:
    bool IsEnabled() const
    {
        return cxpsConfig.GetBool(CXPSConfig::ENABLE);
    }

    // Server enable + per-character opt-in/out. Mirrors ShouldLogToPlayer:
    // tri-state PlayerSetting where 0/unset inherits the server default and an
    // explicit on/off overrides it, gated by an admin-side AllowPlayerToggle.
    // Takes Player * (non-const) because AC's GetPlayerSetting is not marked
    // const -- keeps the signature honest and avoids casts in callers.
    bool IsEnabledFor(Player *player) const
    {
        if (!IsEnabled())
            return false;

        if (player && cxpsConfig.GetBool(CXPSConfig::ALLOW_PLAYER_TOGGLE))
        {
            switch (player->GetPlayerSetting(CXPS_SETTINGS_SOURCE, CXPS_SETTING_ENABLE).value)
            {
            case CXPS_ENABLE_OFF:
                return false;
            case CXPS_ENABLE_ON:
                return true;
            default:
                break; // unset -- fall through to the server-wide default (on)
            }
        }
        return true;
    }

    bool ShouldLogToPlayer(Player *player) const
    {
        // A per-character override only applies if the server allows toggling.
        if (player && cxpsConfig.GetBool(CXPSConfig::LOG_TO_PLAYER_ALLOW_PLAYER_TOGGLE))
        {
            switch (player->GetPlayerSetting(CXPS_SETTINGS_SOURCE, CXPS_SETTING_LOG).value)
            {
            case CXPS_LOG_OFF:
                return false;
            case CXPS_LOG_ON:
                return true;
            default:
                break; // unset -- fall through to the server-wide default
            }
        }
        return cxpsConfig.GetBool(CXPSConfig::LOG_TO_PLAYER);
    }

    float GetLevelScalingFactor(const Player *player, std::stringstream *logStream) const
    {
        if (!cxpsConfig.GetBool(CXPSConfig::LEVEL_XP_ENABLE))
            return 1.0f;

        const uint8 level = player->GetLevel();
        float scaling = 1.0f;

        if (level <= 9)
            scaling = cxpsConfig.GetFloat(CXPSConfig::LEVEL_XP_SCALING_1_9);
        else if (level <= 19)
            scaling = cxpsConfig.GetFloat(CXPSConfig::LEVEL_XP_SCALING_10_19);
        else if (level <= 29)
            scaling = cxpsConfig.GetFloat(CXPSConfig::LEVEL_XP_SCALING_20_29);
        else if (level <= 39)
            scaling = cxpsConfig.GetFloat(CXPSConfig::LEVEL_XP_SCALING_30_39);
        else if (level <= 49)
            scaling = cxpsConfig.GetFloat(CXPSConfig::LEVEL_XP_SCALING_40_49);
        else if (level <= 59)
            scaling = cxpsConfig.GetFloat(CXPSConfig::LEVEL_XP_SCALING_50_59);
        else if (level <= 69)
            scaling = cxpsConfig.GetFloat(CXPSConfig::LEVEL_XP_SCALING_60_69);
        else if (level <= 79)
            scaling = cxpsConfig.GetFloat(CXPSConfig::LEVEL_XP_SCALING_70_79);

        if (logStream)
        {
            *logStream << " Level: " << scaling * 100.0f << "% |";
        }

        return scaling;
    }

    float GetKillScalingFactor(const Unit *victim, std::stringstream *logStream) const
    {
        float scaling = 1.0f;

        // Apply kill scaling
        if (cxpsConfig.GetBool(CXPSConfig::KILL_XP_ENABLE))
        {
            const float killScaling = cxpsConfig.GetFloat(CXPSConfig::KILL_XP_SCALING);
            scaling *= killScaling;

            if (logStream)
            {
                *logStream << " Kill: " << killScaling * 100.0f << "% |";
            }
        }

        // Apply rare scaling
        if (victim && victim->IsCreature() && cxpsConfig.GetBool(CXPSConfig::RARE_XP_ENABLE))
        {
            const Creature *creature = victim->ToCreature();
            const CreatureTemplate *creatureProto = creature->GetCreatureTemplate();

            if (creatureProto && creatureProto->rank > 0)
            {
                float rareScaling = cxpsConfig.GetFloat(CXPSConfig::RARE_XP_SCALING);

                if (cxpsConfig.GetBool(CXPSConfig::RARE_XP_RANK_SCALING))
                {
                    rareScaling *= creatureProto->rank;
                }

                scaling *= rareScaling;

                if (logStream)
                {
                    *logStream << " Rare: " << rareScaling * 100.0f << "% |";
                }
            }
        }

        return scaling;
    }

    float GetExploreScalingFactor(std::stringstream *logStream) const
    {
        if (!cxpsConfig.GetBool(CXPSConfig::EXPLORE_XP_ENABLE))
            return 1.0f;

        const float scaling = cxpsConfig.GetFloat(CXPSConfig::EXPLORE_XP_SCALING);

        if (logStream)
        {
            *logStream << " Explore: " << scaling * 100.0f << "% |";
        }

        return scaling;
    }

    float GetQuestScalingFactor(std::stringstream *logStream) const
    {
        if (!cxpsConfig.GetBool(CXPSConfig::QUEST_XP_ENABLE))
            return 1.0f;

        const float scaling = cxpsConfig.GetFloat(CXPSConfig::QUEST_XP_SCALING);

        if (logStream)
        {
            *logStream << " Quest: " << scaling * 100.0f << "% |";
        }

        return scaling;
    }

    void LogXPDetails(Player *player, uint8 xpSource, uint32 originalXP, uint32 calculatedXP, const std::stringstream &scalingSources) const
    {
        std::string sourceStr;
        switch (xpSource)
        {
        case XPSOURCE_KILL:
            sourceStr = "Kill";
            break;
        case XPSOURCE_QUEST:
            sourceStr = "Quest";
            break;
        case XPSOURCE_QUEST_DF:
            sourceStr = "Quest (DF)";
            break;
        case XPSOURCE_EXPLORE:
            sourceStr = "Explore";
            break;
        case XPSOURCE_BATTLEGROUND:
            sourceStr = "Battleground";
            break;
        default:
            sourceStr = "Unknown";
        }

        std::stringstream logMsg;
        logMsg << "XP Source: " << sourceStr
                     << " | Original: " << originalXP
                     << " | Calculated: " << calculatedXP
                     << " | Scaling: " << scalingSources.str();

        ChatHandler(player->GetSession()).SendSysMessage(logMsg.str());
    }

    // "<base>:<randomizer>" parsing and the percent-of-next-level roll live in
    // CXPS_LevelPercentReward.{h,cpp} -- shared by professions, taxi nodes, and
    // achievements so the rolled-percent semantics stay identical across them.
    // The parsing only runs at config-load time now; runtime reads come from
    // the parallel array on cxpsConfig (see GetLevelPercent).

    float GetDifficultyMultiplier(ProfessionDifficulty diff, const char *&label) const
    {
        switch (diff)
        {
        case PROF_DIFF_GRAY:
            label = "Gray";
            return cxpsConfig.GetFloat(CXPSConfig::PROFESSIONS_DIFFICULTY_GRAY);
        case PROF_DIFF_GREEN:
            label = "Green";
            return cxpsConfig.GetFloat(CXPSConfig::PROFESSIONS_DIFFICULTY_GREEN);
        case PROF_DIFF_YELLOW:
            label = "Yellow";
            return cxpsConfig.GetFloat(CXPSConfig::PROFESSIONS_DIFFICULTY_YELLOW);
        case PROF_DIFF_ORANGE:
            label = "Orange";
            return cxpsConfig.GetFloat(CXPSConfig::PROFESSIONS_DIFFICULTY_ORANGE);
        }
        label = "Unknown";
        return 1.0f;
    }

    // Classifies a profession action by its skill-color relative to the player's
    // current skill, matching the gray/green/yellow/orange thresholds AzerothCore
    // uses for skill-up rolls. See PlayerUpdates.cpp (UpdateGatherSkill / UpdateCraftSkill).
    static ProfessionDifficulty ClassifyDifficulty(uint32 currentLevel, uint32 yellow, uint32 green, uint32 gray)
    {
        if (currentLevel >= gray)
            return PROF_DIFF_GRAY;
        if (currentLevel >= green)
            return PROF_DIFF_GREEN;
        if (currentLevel >= yellow)
            return PROF_DIFF_YELLOW;
        return PROF_DIFF_ORANGE;
    }

    // Bundle of inputs the profession path needs to roll an XP reward. Packed
    // here so the three hooks all call GiveProfessionXP the same way; each
    // hook fills in the source label, the difficulty, the skill id (for the
    // per-skill multiplier lookup), and the rank value used to pick a
    // bracket (current skill for gather/fish, recipe MinSkillLineRank for crafts).
    struct ProfessionContext
    {
        CXPSProfessionSource source;
        ProfessionDifficulty difficulty;
        uint32 skillId;          // SKILL_* -- routes the per-skill multiplier
        uint32 rankSkill;        // chooses the Apprentice...GrandMaster bracket
        bool   isDisenchant;     // overrides skillId-based lookup -> Disenchanting
        bool   isSmelt;          // overrides skillId-based lookup -> Smelting
    };

    void GiveProfessionXP(Player *player, ProfessionContext const& ctx) const
    {
        if (!player || !cxpsConfig.GetBool(CXPSConfig::PROFESSIONS_ENABLE))
            return;

        const char *diffLabel = "Unknown";
        const float diffMult = GetDifficultyMultiplier(ctx.difficulty, diffLabel);

        // A non-positive multiplier means "no XP at this difficulty" (gray by default).
        if (diffMult <= 0.0f)
            return;

        // Per-skill and per-rank multipliers stack on top of difficulty. Both
        // default to 1.0 so a fresh install behaves identically to the pre-axes
        // version.
        const float skillMult = cxpsConfig.GetSkillMultiplier(
            ctx.skillId, ctx.isDisenchant, ctx.isSmelt);
        const float rankMult  = cxpsConfig.GetRankMultiplier(ctx.rankSkill);

        // Combined multiplier passed to the roll. Order of multiplication is
        // commutative; the breakdown is preserved in the chat log below.
        const float totalMult = diffMult * skillMult * rankMult;
        if (totalMult <= 0.0f)
            return;

        // Per-source LevelPercent override (Gather / Craft / Fish / Disenchant /
        // Smelt / Lockpick). The cache pre-parses each override at config load
        // and substitutes the global ProfessionsXP.LevelPercent when the
        // per-source key is absent or malformed -- so this lookup never
        // re-parses strings or touches sConfigMgr.
        const CXPS::LevelPercent levelPercent =
            cxpsConfig.GetLevelPercent(
                CXPSConfigData::SourceLevelPercentKey(ctx.source));

        const float effectivePercent = CXPS::RollEffectivePercent(levelPercent, totalMult);
        const uint32 xpReward = CXPS::LevelPercentToXP(player, effectivePercent);

        if (xpReward == 0)
            return;

        if (ShouldLogToPlayer(player))
        {
            std::stringstream logMsg;
            logMsg << "Profession XP (" << CXPSConfigData::SourceLabel(ctx.source)
                         << ", " << diffLabel << "): " << xpReward
                         << " | " << effectivePercent << "% of next level"
                         << " | base " << levelPercent.base << "% +/- " << levelPercent.randomizer
                         << "% x diff " << diffMult
                         << " x skill " << skillMult
                         << " x rank " << rankMult;
            ChatHandler(player->GetSession()).SendSysMessage(logMsg.str());
        }

        player->GiveXP(xpReward, nullptr);
    }

    void GiveTaxiNodeXP(Player *player) const
    {
        if (!player || !cxpsConfig.GetBool(CXPSConfig::TAXI_NODE_XP_ENABLE))
            return;

        // No difficulty tiers for node discovery -- every node pays the same
        // rolled percent, so the multiplier is a flat 1.0.
        const CXPS::LevelPercent levelPercent =
            cxpsConfig.GetLevelPercent(CXPSConfig::TAXI_NODE_XP_LEVEL_PERCENT);
        const float effectivePercent = CXPS::RollEffectivePercent(levelPercent, 1.0f);
        const uint32 xpReward = CXPS::LevelPercentToXP(player, effectivePercent);

        if (xpReward == 0)
            return;

        if (ShouldLogToPlayer(player))
        {
            std::stringstream logMsg;
            logMsg << "Taxi Node XP: " << xpReward
                   << " | " << effectivePercent << "% of next level"
                   << " | base " << levelPercent.base << "% +/- " << levelPercent.randomizer << "%";
            ChatHandler(player->GetSession()).SendSysMessage(logMsg.str());
        }

        player->GiveXP(xpReward, nullptr);
    }

    // Profession skill handlers
    void OnPlayerUpdateGatheringSkill(Player *player, uint32 skillId, uint32 currentLevel, uint32 gray, uint32 green, uint32 yellow, uint32 & /*gain*/) override
    {
        if (!IsEnabledFor(player))
            return;

        // Lockpicking is reported through the gathering hook; route it to its
        // own source so admins can tune (or zero) rogue lockpicking XP
        // independently of mining / herbalism / skinning.
        const CXPSProfessionSource src =
            (skillId == SKILL_LOCKPICKING) ? CXPS_PROF_SRC_LOCKPICK : CXPS_PROF_SRC_GATHER;

        ProfessionContext ctx{};
        ctx.source       = src;
        ctx.difficulty   = ClassifyDifficulty(currentLevel, yellow, green, gray);
        ctx.skillId      = skillId;
        ctx.rankSkill    = currentLevel;
        ctx.isDisenchant = false;
        ctx.isSmelt      = false;

        GiveProfessionXP(player, ctx);
    }

    void OnPlayerUpdateCraftingSkill(Player *player, SkillLineAbilityEntry const *skill, uint32 currentLevel, uint32 & /*gain*/) override
    {
        if (!IsEnabledFor(player))
            return;

        // SkillLineAbilityEntry exposes high/low trivial ranks; AC's UpdateCraftSkill
        // uses high = gray, low = yellow, midpoint = green for its skill-up odds.
        ProfessionDifficulty diff = PROF_DIFF_YELLOW;
        uint32 skillId   = 0;
        uint32 rankSkill = currentLevel;
        bool   isDisench = false;
        bool   isSmelt   = false;

        if (skill)
        {
            skillId = skill->SkillLine;

            const uint32 gray = skill->TrivialSkillLineRankHigh;
            const uint32 yellow = skill->TrivialSkillLineRankLow;
            const uint32 green = (gray + yellow) / 2;
            diff = ClassifyDifficulty(currentLevel, yellow, green, gray);

            // The recipe's required skill is the right bracket signal for
            // crafts (a Grandmaster recipe should pay grand-master XP even if
            // the player is over-skilled). Fall back to currentLevel if absent.
            if (skill->MinSkillLineRank > 0)
                rankSkill = skill->MinSkillLineRank;

            // Disenchant rides on SKILL_ENCHANTING; the spell id is the only
            // reliable distinction. Smelting rides on SKILL_MINING but in the
            // crafting hook (mining-the-gather hits the gathering hook).
            if (skill->Spell == CXPS_SPELL_DISENCHANT)
                isDisench = true;
            else if (skill->SkillLine == SKILL_MINING)
                isSmelt = true;
        }

        ProfessionContext ctx{};
        ctx.source       = isDisench ? CXPS_PROF_SRC_DISENCHANT
                         : isSmelt   ? CXPS_PROF_SRC_SMELT
                         :             CXPS_PROF_SRC_CRAFT;
        ctx.difficulty   = diff;
        ctx.skillId      = skillId;
        ctx.rankSkill    = rankSkill;
        ctx.isDisenchant = isDisench;
        ctx.isSmelt      = isSmelt;

        GiveProfessionXP(player, ctx);
    }

    bool OnPlayerUpdateFishingSkill(Player *player, int32 skill, int32 zone_skill, int32 /*chance*/, int32 /*roll*/) override
    {
        if (!IsEnabledFor(player))
            return true;

        // Fishing has no DB-defined trivial thresholds -- approximate from skill
        // vs. zone requirement so high-level fishing in low-level zones decays to gray.
        const int32 delta = skill - zone_skill;
        ProfessionDifficulty diff;
        if (delta >= 100)
            diff = PROF_DIFF_GRAY;
        else if (delta >= 50)
            diff = PROF_DIFF_GREEN;
        else if (delta >= 0)
            diff = PROF_DIFF_YELLOW;
        else
            diff = PROF_DIFF_ORANGE;

        ProfessionContext ctx{};
        ctx.source       = CXPS_PROF_SRC_FISH;
        ctx.difficulty   = diff;
        ctx.skillId      = SKILL_FISHING;
        ctx.rankSkill    = skill > 0 ? static_cast<uint32>(skill) : 0;
        ctx.isDisenchant = false;
        ctx.isSmelt      = false;

        GiveProfessionXP(player, ctx);
        return true; // Continue with default handling
    }

    // Maps an achievement's point value to a difficulty-style multiplier on the
    // base level-percent roll -- the achievement analogue of the profession
    // Difficulty.* tiers. The raw multiplier is points / PointsPerUnit, clamped
    // to a configurable [Min, Max] band so a 5-point feat and a 100-point feat
    // don't pay wildly different amounts. A non-positive PointsPerUnit disables
    // point weighting entirely (every achievement uses 1.0x, still clamped).
    float GetAchievementPointsMultiplier(uint32 points) const
    {
        const float perUnit = cxpsConfig.GetFloat(CXPSConfig::ACHIEVEMENT_XP_POINTS_PER_UNIT);
        const float minMult = cxpsConfig.GetFloat(CXPSConfig::ACHIEVEMENT_XP_MIN_MULTIPLIER);
        const float maxMult = cxpsConfig.GetFloat(CXPSConfig::ACHIEVEMENT_XP_MAX_MULTIPLIER);

        const float raw = perUnit > 0.0f ? (static_cast<float>(points) / perUnit) : 1.0f;
        return std::max(minMult, std::min(maxMult, raw));
    }

    void OnPlayerAchievementComplete(Player *player, AchievementEntry const *achievement) override
    {
        if (!player || !achievement || !IsEnabledFor(player) ||
            !cxpsConfig.GetBool(CXPSConfig::ACHIEVEMENT_XP_ENABLE))
            return;

        // Achievement points act as the difficulty multiplier here, the same
        // role Difficulty.* plays for professions.
        const float pointsMult = GetAchievementPointsMultiplier(achievement->points);
        if (pointsMult <= 0.0f)
            return;

        const CXPS::LevelPercent levelPercent =
            cxpsConfig.GetLevelPercent(CXPSConfig::ACHIEVEMENT_XP_LEVEL_PERCENT);
        const float effectivePercent = CXPS::RollEffectivePercent(levelPercent, pointsMult);
        const uint32 xpReward = CXPS::LevelPercentToXP(player, effectivePercent);

        if (xpReward == 0)
            return;

        if (ShouldLogToPlayer(player))
        {
            std::stringstream logMsg;
            logMsg << "Achievement XP (" << achievement->points << " pts): " << xpReward
                         << " | " << effectivePercent << "% of next level"
                         << " | base " << levelPercent.base << "% +/- " << levelPercent.randomizer
                         << "% x " << pointsMult;
            ChatHandler(player->GetSession()).SendSysMessage(logMsg.str());
        }

        player->GiveXP(xpReward, nullptr);
    }

    // The hook provides Player const* so we const_cast once at the top and
    // pass the non-const through to IsEnabledFor / GiveTaxiNodeXP. Safe -- the
    // core does not rely on the player being unmodified after this hook fires,
    // and GiveXP has no lasting side-effects beyond the standard level-up path.
    void OnPlayerLearnTaxiNode(Player const *constPlayer, uint32 /*nodeId*/) override
    {
        if (!constPlayer)
            return;

        Player *player = const_cast<Player *>(constPlayer);
        if (!IsEnabledFor(player))
            return;

        GiveTaxiNodeXP(player);
    }
};

// Commands:
//   .xpscaling on  | off       -- per-character opt-in/out of XP scaling
//   .xpscaling log on | off    -- per-character per-event log messages
//   .xpscaling help            -- list available commands
//   .xpscaling about           -- short module summary
//
// On/off prefs are persisted through the core's PlayerSetting API
// (character_settings) -- the core owns load/save/delete, so no schema
// migration or cleanup hook is needed. The two toggles are gated independently
// by CustomXPScaling.AllowPlayerToggle (scaling) and
// CustomXPScaling.LogToPlayer.AllowPlayerToggle (log).
using namespace Acore::ChatCommands;

class CXPS_CommandScript : public CommandScript
{
public:
    CXPS_CommandScript() : CommandScript("CXPS_CommandScript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable xpScalingLogTable =
        {
            { "on",  HandleLogOnCommand,  SEC_PLAYER, Console::No },
            { "off", HandleLogOffCommand, SEC_PLAYER, Console::No },
        };

        static ChatCommandTable xpScalingTable =
        {
            { "on",    HandleScalingOnCommand,  SEC_PLAYER, Console::No },
            { "off",   HandleScalingOffCommand, SEC_PLAYER, Console::No },
            { "log",   xpScalingLogTable },
            { "help",  HandleHelpCommand,       SEC_PLAYER, Console::No },
            { "about", HandleAboutCommand,      SEC_PLAYER, Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "xpscaling", xpScalingTable },
        };

        return commandTable;
    }

    static bool HandleLogOnCommand(ChatHandler *handler)      { return SetLogPref(handler, true); }
    static bool HandleLogOffCommand(ChatHandler *handler)     { return SetLogPref(handler, false); }
    static bool HandleScalingOnCommand(ChatHandler *handler)  { return SetScalingPref(handler, true); }
    static bool HandleScalingOffCommand(ChatHandler *handler) { return SetScalingPref(handler, false); }

    static bool HandleHelpCommand(ChatHandler *handler)
    {
        handler->SendSysMessage("|cff4CFF00Custom XP Scaling|r commands:");
        handler->SendSysMessage("  .xpscaling on       -- enable scaling for your character");
        handler->SendSysMessage("  .xpscaling off      -- disable scaling for your character");
        handler->SendSysMessage("  .xpscaling log on   -- show per-event XP log messages");
        handler->SendSysMessage("  .xpscaling log off  -- hide per-event XP log messages");
        handler->SendSysMessage("  .xpscaling about    -- about this module");
        handler->SendSysMessage("  .xpscaling help     -- this help");
        handler->SendSysMessage("Some toggles may be locked by the server admin.");
        return true;
    }

    static bool HandleAboutCommand(ChatHandler *handler)
    {
        handler->SendSysMessage("|cff4CFF00Custom XP Scaling|r");
        handler->SendSysMessage("Reshapes XP gains across the server; scaling factors are configured by the admin");
        handler->SendSysMessage(" and can vary by level and activity. Check your server's about or wiki for details.");
        handler->SendSysMessage("Preferences are per-character and optionally player-toggleable,");
        handler->SendSysMessage("with live XP gain breakdowns in chat if logging is enabled.");
        handler->SendSysMessage("Prefer standard XP gains? Type |cff4CFF00.xpscaling off|r to disable for your character.");
        handler->SendSysMessage("Type .xpscaling help for the command list.");
        return true;
    }

private:
    static bool SetLogPref(ChatHandler *handler, bool enabled)
    {
        if (!cxpsConfig.GetBool(CXPSConfig::LOG_TO_PLAYER_ALLOW_PLAYER_TOGGLE))
        {
            handler->SendSysMessage("XP scaling log toggle is not available on this server.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player *player = handler->GetPlayer();
        if (!player)
            return false;

        // Stored through the core's PlayerSetting API; the core flushes it to
        // character_settings on save and purges it on character delete.
        player->UpdatePlayerSetting(CXPS_SETTINGS_SOURCE, CXPS_SETTING_LOG,
            enabled ? CXPS_LOG_ON : CXPS_LOG_OFF);

        handler->PSendSysMessage("XP scaling log {}.", enabled ? "|cff4CFF00enabled|r" : "|cffFF4040disabled|r");

        WarnIfPlayerSettingsDisabled(handler);
        return true;
    }

    static bool SetScalingPref(ChatHandler *handler, bool enabled)
    {
        if (!cxpsConfig.GetBool(CXPSConfig::ALLOW_PLAYER_TOGGLE))
        {
            handler->SendSysMessage("XP scaling toggle is not available on this server.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player *player = handler->GetPlayer();
        if (!player)
            return false;

        player->UpdatePlayerSetting(CXPS_SETTINGS_SOURCE, CXPS_SETTING_ENABLE,
            enabled ? CXPS_ENABLE_ON : CXPS_ENABLE_OFF);

        handler->PSendSysMessage("Custom XP scaling {} for your character.",
            enabled ? "|cff4CFF00enabled|r" : "|cffFF4040disabled|r");

        WarnIfPlayerSettingsDisabled(handler);
        return true;
    }

    // PlayerSetting writes are only persisted when the core's PlayerSettings
    // system is enabled; warn so the choice isn't silently lost on logout.
    static void WarnIfPlayerSettingsDisabled(ChatHandler *handler)
    {
        if (!sWorld->getBoolConfig(CONFIG_PLAYER_SETTINGS_ENABLED))
            handler->SendSysMessage("Note: PlayerSettings is disabled on this server -- this choice will not persist after logout.");
    }
};

// WorldScript whose only job is to populate cxpsConfig at world boot and on
// every .reload config. Listed on the hook-list constructor so the core only
// iterates this script for WORLDHOOK_ON_BEFORE_CONFIG_LOAD.
class CXPS_LoadConfigScript : public WorldScript
{
public:
    CXPS_LoadConfigScript() : WorldScript("CXPS_LoadConfigScript", {
        WORLDHOOK_ON_BEFORE_CONFIG_LOAD,
    }) {}

    void OnBeforeConfigLoad(bool reload) override
    {
        cxpsConfig.Initialize(reload);
    }
};

// Separate WorldScript for the cast-speed pass: it must run *after* DBCs and
// configs are loaded (so SpellMgr / cxpsConfig are populated), which is what
// WORLDHOOK_ON_STARTUP gives us. Splitting it out from CXPS_LoadConfigScript
// keeps the hook lists tight -- the config-load script doesn't pay the cost
// of being invoked on startup, and the cast-speed script doesn't run on every
// .reload config (cast-time mutation is intentionally restart-only).
class CXPS_CastSpeedScript : public WorldScript
{
public:
    CXPS_CastSpeedScript() : WorldScript("CXPS_CastSpeedScript", {
        WORLDHOOK_ON_STARTUP,
    }) {}

    void OnStartup() override
    {
        CXPS::CastSpeed::Apply();
    }
};

void AddCustomXPScalingScripts()
{
    new CustomXPScaling();
    new CXPS_CommandScript();
    new CXPS_LoadConfigScript();
    new CXPS_CastSpeedScript();
}
