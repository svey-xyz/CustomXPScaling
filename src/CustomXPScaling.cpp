/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "Chat.h"
#include "Config.h"
#include "DBCStores.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"

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
	}

	void OnPlayerGiveXP(Player *player, uint32 &amount, Unit *victim, uint8 xpSource) override
	{
		if (!player || !IsEnabled())
			return;

		const bool shouldLog = ShouldLogToPlayer();

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

	bool ShouldLogToPlayer() const
	{
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

	// Parse "base:randomizer" config (e.g. "1.0:0.25").
	// Both values are absolute percentages of the player's next-level XP.
	// Resulting roll: (base + frand(-randomizer, +randomizer)) percent of next-level XP.
	void ParseProfessionLevelPercent(float &basePercent, float &randomizer) const
	{
		const std::string raw = sConfigMgr->GetOption<std::string>("CustomXPScaling.ProfessionsXP.LevelPercent", "1.0:0.25");
		basePercent = 1.0f;
		randomizer = 0.25f;

		const auto colon = raw.find(':');
		if (colon == std::string::npos)
			return;

		try
		{
			basePercent = std::stof(raw.substr(0, colon));
			randomizer = std::stof(raw.substr(colon + 1));
		}
		catch (...)
		{
			// keep defaults on malformed config
		}

		if (randomizer < 0.0f)
			randomizer = 0.0f;
	}

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

		float basePercent = 1.0f;
		float randomizer = 0.25f;
		ParseProfessionLevelPercent(basePercent, randomizer);

		// Roll a random offset in [-randomizer, +randomizer], scale by difficulty.
		const float roll = randomizer > 0.0f ? frand(-randomizer, randomizer) : 0.0f;
		const float effectivePercent = std::max(0.0f, (basePercent + roll) * diffMult);

		const uint32 nextLevelXP = player->GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
		const uint32 xpReward = static_cast<uint32>(std::round(nextLevelXP * (effectivePercent / 100.0f)));

		if (xpReward == 0)
			return;

		if (ShouldLogToPlayer())
		{
			std::stringstream logMsg;
			logMsg << "Profession XP (" << source << ", " << diffLabel << "): " << xpReward
						 << " | " << effectivePercent << "% of next level"
						 << " | base " << basePercent << "% +/- " << randomizer
						 << "% x " << diffMult;
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

	void OnPlayerAchievementComplete(Player *player, AchievementEntry const *achievement) override
	{
		if (!player || !achievement || !sConfigMgr->GetOption<bool>("CustomXPScaling.AchievementXP.Enable", false))
			return;

		const float achievementScaling = sConfigMgr->GetOption<float>("CustomXPScaling.AchievementXP.Scaling", 0.1f);
		const uint32 nextLevelXP = player->GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
		const uint32 xpReward = static_cast<uint32>(std::round(nextLevelXP * achievementScaling));

		if (ShouldLogToPlayer())
		{
			std::stringstream logMsg;
			logMsg << "Achievement XP: " << xpReward << " (Points: " << achievement->points << ")";
			ChatHandler(player->GetSession()).SendSysMessage(logMsg.str());
		}

		player->GiveXP(xpReward, nullptr);
	}
};

void AddCustomXPScalingScripts()
{
	new CustomXPScaling();
}