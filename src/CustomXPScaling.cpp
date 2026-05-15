/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "CXPS_LevelPercentReward.h"
#include "Chat.h"
#include "Config.h"
#include "DBCStores.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
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

    // Stored value for CXPS_SETTING_LOG. Tri-state so an explicit "off" is
    // distinguishable from "never chose" -- GetPlayerSetting yields 0 when unset,
    // which we treat as "inherit the server default".
    constexpr uint32 CXPS_LOG_OFF = 1; // player forced logging off
    constexpr uint32 CXPS_LOG_ON = 2;  // player forced logging on
}

class CustomXPScaling : public PlayerScript
{
public:
    CustomXPScaling() : PlayerScript("CustomXPScaling") {}

    enum ProfessionDifficulty
    {
        PROF_DIFF_GRAY = 0,
        PROF_DIFF_GREEN,
        PROF_DIFF_YELLOW,
        PROF_DIFF_ORANGE
    };

    void OnPlayerLogin(Player *player) override
    {
        if (sConfigMgr->GetOption<bool>("CustomXPScaling.Announce", true))
        {
            ChatHandler(player->GetSession()).SendSysMessage("This server is running the |cff4CFF00Custom XP Scaling |rmodule.");
        }
        // The per-player log preference is loaded by the core via the
        // PlayerSetting API before this hook fires -- see ShouldLogToPlayer.
        // Nothing module-side to load or cache here.
    }

    void OnPlayerGiveXP(Player *player, uint32 &amount, Unit *victim, uint8 xpSource) override
    {
        if (!player || !IsEnabled())
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
        return sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true);
    }

    bool ShouldLogToPlayer(Player *player) const
    {
        // A per-character override only applies if the server allows toggling.
        if (player && sConfigMgr->GetOption<bool>("CustomXPScaling.LogToPlayer.AllowPlayerToggle", true))
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
        return sConfigMgr->GetOption<bool>("CustomXPScaling.LogToPlayer", false);
    }

    float GetLevelScalingFactor(const Player *player, std::stringstream *logStream) const
    {
        if (!sConfigMgr->GetOption<bool>("CustomXPScaling.LevelXP.Enable", true))
            return 1.0f;

        const uint8 level = player->GetLevel();
        float scaling = 1.0f;

        if (level <= 9)
            scaling = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.1-9", 0.2f);
        else if (level <= 19)
            scaling = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.10-19", 0.3f);
        else if (level <= 29)
            scaling = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.20-29", 0.8f);
        else if (level <= 39)
            scaling = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.30-39", 1.0f);
        else if (level <= 49)
            scaling = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.40-49", 1.2f);
        else if (level <= 59)
            scaling = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.50-59", 1.3f);
        else if (level <= 69)
            scaling = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.60-69", 1.3f);
        else if (level <= 79)
            scaling = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.70-79", 1.3f);

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
        if (sConfigMgr->GetOption<bool>("CustomXPScaling.KillXP.Enable", false))
        {
            const float killScaling = sConfigMgr->GetOption<float>("CustomXPScaling.KillXP.Scaling", 1.0f);
            scaling *= killScaling;

            if (logStream)
            {
                *logStream << " Kill: " << killScaling * 100.0f << "% |";
            }
        }

        // Apply rare scaling
        if (victim && victim->IsCreature() && sConfigMgr->GetOption<bool>("CustomXPScaling.RareXP.Enable", false))
        {
            const Creature *creature = victim->ToCreature();
            const CreatureTemplate *creatureProto = creature->GetCreatureTemplate();

            if (creatureProto && creatureProto->rank > 0)
            {
                float rareScaling = sConfigMgr->GetOption<float>("CustomXPScaling.RareXP.Scaling", 1.0f);

                if (sConfigMgr->GetOption<bool>("CustomXPScaling.RareXP.RankScaling", true))
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
        if (!sConfigMgr->GetOption<bool>("CustomXPScaling.ExploreXP.Enable", true))
            return 1.0f;

        const float scaling = sConfigMgr->GetOption<float>("CustomXPScaling.ExploreXP.Scaling", 1.0f);

        if (logStream)
        {
            *logStream << " Explore: " << scaling * 100.0f << "% |";
        }

        return scaling;
    }

    float GetQuestScalingFactor(std::stringstream *logStream) const
    {
        if (!sConfigMgr->GetOption<bool>("CustomXPScaling.QuestXP.Enable", true))
            return 1.0f;

        const float scaling = sConfigMgr->GetOption<float>("CustomXPScaling.QuestXP.Scaling", 1.0f);

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

    float GetDifficultyMultiplier(ProfessionDifficulty diff, const char *&label) const
    {
        switch (diff)
        {
        case PROF_DIFF_GRAY:
            label = "Gray";
            return sConfigMgr->GetOption<float>("CustomXPScaling.ProfessionsXP.Difficulty.Gray", 0.0f);
        case PROF_DIFF_GREEN:
            label = "Green";
            return sConfigMgr->GetOption<float>("CustomXPScaling.ProfessionsXP.Difficulty.Green", 0.5f);
        case PROF_DIFF_YELLOW:
            label = "Yellow";
            return sConfigMgr->GetOption<float>("CustomXPScaling.ProfessionsXP.Difficulty.Yellow", 1.0f);
        case PROF_DIFF_ORANGE:
            label = "Orange";
            return sConfigMgr->GetOption<float>("CustomXPScaling.ProfessionsXP.Difficulty.Orange", 1.5f);
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

    void GiveProfessionXP(Player *player, ProfessionDifficulty diff, const char *source) const
    {
        if (!player || !sConfigMgr->GetOption<bool>("CustomXPScaling.ProfessionsXP.Enable", true))
            return;

        const char *diffLabel = "Unknown";
        const float diffMult = GetDifficultyMultiplier(diff, diffLabel);

        // A non-positive multiplier means "no XP at this difficulty" (gray by default).
        if (diffMult <= 0.0f)
            return;

        const CXPS::LevelPercent levelPercent =
            CXPS::ParseLevelPercent("CustomXPScaling.ProfessionsXP.LevelPercent", 1.0f, 0.25f);
        const float effectivePercent = CXPS::RollEffectivePercent(levelPercent, diffMult);
        const uint32 xpReward = CXPS::LevelPercentToXP(player, effectivePercent);

        if (xpReward == 0)
            return;

        if (ShouldLogToPlayer(player))
        {
            std::stringstream logMsg;
            logMsg << "Profession XP (" << source << ", " << diffLabel << "): " << xpReward
                         << " | " << effectivePercent << "% of next level"
                         << " | base " << levelPercent.base << "% +/- " << levelPercent.randomizer
                         << "% x " << diffMult;
            ChatHandler(player->GetSession()).SendSysMessage(logMsg.str());
        }

        player->GiveXP(xpReward, nullptr);
    }

    void GiveTaxiNodeXP(Player *player) const
    {
        if (!player || !sConfigMgr->GetOption<bool>("CustomXPScaling.TaxiNodeXP.Enable", true))
            return;

        // No difficulty tiers for node discovery -- every node pays the same
        // rolled percent, so the multiplier is a flat 1.0.
        const CXPS::LevelPercent levelPercent =
            CXPS::ParseLevelPercent("CustomXPScaling.TaxiNodeXP.LevelPercent", 2.0f, 0.5f);
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
    void OnPlayerUpdateGatheringSkill(Player *player, uint32 /*skillId*/, uint32 currentLevel, uint32 gray, uint32 green, uint32 yellow, uint32 & /*gain*/) override
    {
        if (!IsEnabled())
            return;

        const ProfessionDifficulty diff = ClassifyDifficulty(currentLevel, yellow, green, gray);
        GiveProfessionXP(player, diff, "Gather");
    }

    void OnPlayerUpdateCraftingSkill(Player *player, SkillLineAbilityEntry const *skill, uint32 currentLevel, uint32 & /*gain*/) override
    {
        if (!IsEnabled())
            return;

        // SkillLineAbilityEntry exposes high/low trivial ranks; AC's UpdateCraftSkill
        // uses high = gray, low = yellow, midpoint = green for its skill-up odds.
        ProfessionDifficulty diff = PROF_DIFF_YELLOW;
        if (skill)
        {
            const uint32 gray = skill->TrivialSkillLineRankHigh;
            const uint32 yellow = skill->TrivialSkillLineRankLow;
            const uint32 green = (gray + yellow) / 2;
            diff = ClassifyDifficulty(currentLevel, yellow, green, gray);
        }

        GiveProfessionXP(player, diff, "Craft");
    }

    bool OnPlayerUpdateFishingSkill(Player *player, int32 skill, int32 zone_skill, int32 /*chance*/, int32 /*roll*/) override
    {
        if (!IsEnabled())
            return true;

        // Fishing has no DB-defined trivial thresholds — approximate from skill
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

        GiveProfessionXP(player, diff, "Fish");
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
        const float perUnit = sConfigMgr->GetOption<float>("CustomXPScaling.AchievementXP.PointsPerUnit", 10.0f);
        const float minMult = sConfigMgr->GetOption<float>("CustomXPScaling.AchievementXP.MinMultiplier", 0.5f);
        const float maxMult = sConfigMgr->GetOption<float>("CustomXPScaling.AchievementXP.MaxMultiplier", 5.0f);

        const float raw = perUnit > 0.0f ? (static_cast<float>(points) / perUnit) : 1.0f;
        return std::max(minMult, std::min(maxMult, raw));
    }

    void OnPlayerAchievementComplete(Player *player, AchievementEntry const *achievement) override
    {
        if (!player || !achievement || !sConfigMgr->GetOption<bool>("CustomXPScaling.AchievementXP.Enable", false))
            return;

        // Achievement points act as the difficulty multiplier here, the same
        // role Difficulty.* plays for professions.
        const float pointsMult = GetAchievementPointsMultiplier(achievement->points);
        if (pointsMult <= 0.0f)
            return;

        const CXPS::LevelPercent levelPercent =
            CXPS::ParseLevelPercent("CustomXPScaling.AchievementXP.LevelPercent", 1.0f, 0.25f);
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

    // The hook provides Player const* so we const_cast to call GiveXP.
    // This is safe — the core does not rely on the player being unmodified
    // after this hook fires, and GiveXP has no lasting side-effects here
    // beyond the standard level-up path.
    void OnPlayerLearnTaxiNode(Player const *player, uint32 /*nodeId*/) override
    {
        if (!player || !IsEnabled())
            return;

        GiveTaxiNodeXP(const_cast<Player *>(player));
    }
};

// Command: .xpscaling log [on|off]
// Lets players enable or disable their own XP scaling log messages.
// Persisted through the core's PlayerSetting API (character_settings) -- the
// core owns load/save/delete, so no schema migration or cleanup hook is needed.
// Gated by CustomXPScaling.LogToPlayer.AllowPlayerToggle.
class CXPS_CommandScript : public CommandScript
{
public:
    CXPS_CommandScript() : CommandScript("CXPS_CommandScript") {}

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> xpScalingLogTable =
        {
            { "on",  SEC_PLAYER, false, &HandleLogOnCommand,  "" },
            { "off", SEC_PLAYER, false, &HandleLogOffCommand, "" },
        };

        static std::vector<ChatCommand> xpScalingTable =
        {
            { "log", SEC_PLAYER, false, nullptr, "", xpScalingLogTable },
        };

        static std::vector<ChatCommand> commandTable =
        {
            { "xpscaling", SEC_PLAYER, false, nullptr, "", xpScalingTable },
        };

        return commandTable;
    }

    static bool HandleLogOnCommand(ChatHandler *handler, const char * /*args*/)
    {
        return SetLogPref(handler, true);
    }

    static bool HandleLogOffCommand(ChatHandler *handler, const char * /*args*/)
    {
        return SetLogPref(handler, false);
    }

private:
    static bool SetLogPref(ChatHandler *handler, bool enabled)
    {
        if (!sConfigMgr->GetOption<bool>("CustomXPScaling.LogToPlayer.AllowPlayerToggle", true))
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

        handler->PSendSysMessage("XP scaling log %s.", enabled ? "|cff4CFF00enabled|r" : "|cffFF4040disabled|r");

        // PlayerSetting writes are only persisted when the core's PlayerSettings
        // system is enabled; warn so the choice isn't silently lost on logout.
        if (!sWorld->getBoolConfig(CONFIG_PLAYER_SETTINGS_ENABLED))
            handler->SendSysMessage("Note: PlayerSettings is disabled on this server -- this choice will not persist after logout.");

        return true;
    }
};

void AddCustomXPScalingScripts()
{
    new CustomXPScaling();
    new CXPS_CommandScript();
}