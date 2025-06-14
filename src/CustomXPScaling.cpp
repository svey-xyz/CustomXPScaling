/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "Configuration/Config.h"

// 		XPSOURCE_KILL = 0,
// 		XPSOURCE_QUEST = 1,
// 		XPSOURCE_QUEST_DF = 2,
// 		XPSOURCE_EXPLORE = 3,
// 		XPSOURCE_BATTLEGROUND = 4

// Add player scripts
class CustomXPScaling : public PlayerScript
{
public:
	CustomXPScaling() : PlayerScript("CustomXPScaling") {
		PLAYERHOOK_ON_LOGIN,
		PLAYERHOOK_ON_GIVE_EXP,
		PLAYERHOOK_ON_UPDATE_GATHERING_SKILL,
		PLAYERHOOK_ON_UPDATE_CRAFTING_SKILL,
		PLAYERHOOK_ON_UPDATE_FISHING_SKILL,
		PLAYERHOOK_ON_ACHI_COMPLETE
	}

	void OnPlayerLogin(Player *player) override
	{
		if (sConfigMgr->GetOption<bool>("CustomXPScaling.Announce", true))
			ChatHandler(player->GetSession()).SendSysMessage("This server is running the |cff4CFF00Custom XP Scaling |rmodule.");
	}

	void OnPlayerGiveXP(Player *player, uint32 &amount, Unit *victim, uint8 xpSource) override
	{
		if (!sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true))
			return;


		// Calculate new XP with floating-point multiplication
		float calculatedXP = static_cast<float>(amount) * GetLevelXPScaling(player);

		if (xpSource == XPSOURCE_QUEST || xpSource == XPSOURCE_QUEST_DF)
			calculatedXP *= GetQuestXPScaling();
		else if (xpSource == XPSOURCE_KILL)
			calculatedXP *= GetKillXPScaling(player, victim);
		else if (xpSource == XPSOURCE_EXPLORE)
			calculatedXP *= GetExploreXPScaling();

		LogToPlayer(player, String("XP Scaling: ") + std::to_string(calculatedXP) + " (Original: " + std::to_string(amount) + ")");
		// Round to nearest whole number and convert back to uint32
		amount = static_cast<uint32>(std::round(calculatedXP));
	}

	void LogToPlayer(Player *player, String msg)
	{
		if (!sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true) ||
			!sConfigMgr->GetOption<bool>("CustomXPScaling.LogToPlayer", false) || !player)
			return;

			ChatHandler(player->GetSession()).SendSysMessage(msg);
	}

	float GetLevelXPScaling(Player *player)
	{

		float levelXPScaling = 1.0f; // Default multiplier

		if (player->GetLevel() <= 9)
			levelXPScaling = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.1-9", 0.2f);
		else if (player->GetLevel() <= 19)
			levelXPScaling = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.10-19", 0.3f);
		else if (player->GetLevel() <= 29)
			levelXPScaling = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.20-29", 0.8f);
		else if (player->GetLevel() <= 39)
			levelXPScaling = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.30-39", 1.0f);
		else if (player->GetLevel() <= 49)
			levelXPScaling = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.40-49", 1.2f);
		else if (player->GetLevel() <= 59)
			levelXPScaling = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.50-59", 1.3f);
		else if (player->GetLevel() <= 69)
			levelXPScaling = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.60-69", 1.3f);
		else if (player->GetLevel() <= 79)
			levelXPScaling = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.70-79", 1.3f);

		return levelXPScaling;
	}

	float GetKillXPScaling(Player *player, Unit *victim)
	{
		if (!sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true) ||
				!player || !victim || !victim->IsCreature() || victim->IsPlayer() || !victim->ToCreature())
			return 1.0f;

		float killXPScaling = 1.0f; // Default multiplier

		if (sConfigMgr->GetOption<bool>("CustomXPScaling.KillXP.Enable", false))
		{
			killXPScaling = killXPScaling * sConfigMgr->GetOption<float>("CustomXPScaling.KillXP.Scaling", 1.0f);
		}

		if (sConfigMgr->GetOption<bool>("CustomXPScaling.RareXP.Enable", false))
		{
			auto creature = victim->ToCreature();
			auto creatureProto = creature->GetCreatureTemplate();

			if (creatureProto->rank == CREATURE_ELITE_NORMAL || creatureProto->rank == CREATURE_ELITE_RARE ||
					creatureProto->rank == CREATURE_ELITE_RAREELITE || creatureProto->rank == CREATURE_ELITE_ELITE)
				killXPScaling = killXPScaling * sConfigMgr->GetOption<float>("CustomXPScaling.RareXP.Scaling", 1.0);
		}
		
		return killXPScaling;
	}

	float GetExploreXPScaling()
	{
		if (!sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true) ||
				!sConfigMgr->GetOption<bool>("CustomXPScaling.ExploreXP.Enable", true))
			return 1.0f;

		float exploreXPScaling = sConfigMgr->GetOption<float>("CustomXPScaling.ExploreXP.Scaling", 1.0);
		return exploreXPScaling;
	}

	float GetQuestXPScaling()
	{
		if (!sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true) ||
				!sConfigMgr->GetOption<bool>("CustomXPScaling.QuestXP.Enable", true))
			return 1.0f;

		float questXPScaling = sConfigMgr->GetOption<float>("CustomXPScaling.QuestXP.Scaling", 1.0);
		return questXPScaling;
	}

	void GivePlayerXP(Player *player, float xpReward)
	{
		if (!sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true) || !player)
			return;

		auto pLevel = player->GetLevel();
		if (pLevel >= sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
			return;

		// Round to nearest whole number and convert back to uint32
		uint xpToGive = static_cast<uint32>(std::round(xpReward));

		player->GiveXP(xpToGive, nullptr);
	}

	void OnPlayerUpdateGatheringSkill(Player *player, uint32 /*skillId*/, uint32 /*currentLevel*/, uint32 /*gray*/, uint32 /*green*/, uint32 /*yellow*/, uint32 &gain) override
	{
		if (!sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true) || !sConfigMgr->GetOption<bool>("CustomXPScaling.ProfessionsXP.Enable", true))
			return;

		float expPercent = sConfigMgr->GetOption<float>("CustomXPScaling.ProfessionsXP.Scaling", 1.5f);
		float expMultiplier = (expPercent * gain) / 100;

		if (sConfigMgr->GetOption<bool>("CustomXPScaling.ProfessionsXP.ScaleLevel", false))
			expMultiplier = ((expMultiplier * 100.0f) * (1.0f - (player->GetLevel() / 100.0f))) / 100.0f;

		float xpMax = player->GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
		float xpReward = xpMax * expMultiplier;

		LogToPlayer(player, String("Gathering Skill XP: ") + std::to_string(xpReward) + " (Gain: " + std::to_string(gain) + ")");
		GivePlayerXP(player, xpReward);
	}

	void OnPlayerUpdateCraftingSkill(Player *player, SkillLineAbilityEntry const */*const *skill*/, uint32 /*currentLevel*/, uint32 &gain) override
	{
		if (!sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true) || !sConfigMgr->GetOption<bool>("CustomXPScaling.ProfessionsXP.Enable", true))
			return;

		float expPercent = sConfigMgr->GetOption<float>("CustomXPScaling.ProfessionsXP.Scaling", 1.5f);
		float expMultiplier = (expPercent * gain) / 100;

		if (sConfigMgr->GetOption<bool>("CustomXPScaling.ProfessionsXP.ScaleLevel", false))
			expMultiplier = ((expMultiplier * 100.0f) * (1.0f - (player->GetLevel() / 100.0f))) / 100.0f;

		float xpMax = player->GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
		float xpReward = xpMax * expMultiplier;

		LogToPlayer(player, String("Crafting Skill XP: ") + std::to_string(xpReward) + " (Gain: " + std::to_string(gain) + ")");
		GivePlayerXP(player, xpReward);
	}

	bool OnPlayerUpdateFishingSkill(Player *player, int32 /*skill*/, int32 /*zone_skill*/, int32 /*chance*/, int32 roll) override
	{
		if (!sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true) || !sConfigMgr->GetOption<bool>("CustomXPScaling.ProfessionsXP.Enable", true))
			return true; // Continue with the default fishing skill update logic

		float expPercent = sConfigMgr->GetOption<float>("CustomXPScaling.ProfessionsXP.Scaling", 1.5f);
		float expMultiplier = (expPercent * roll) / 100;

		if (sConfigMgr->GetOption<bool>("CustomXPScaling.ProfessionsXP.ScaleLevel", false))
			expMultiplier = ((expMultiplier * 100.0f) * (1.0f - (player->GetLevel() / 100.0f))) / 100.0f;

		float xpMax = player->GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
		float xpReward = xpMax * expMultiplier;

		LogToPlayer(player, String("Fishing Skill XP: ") + std::to_string(xpReward) + " (Roll: " + std::to_string(roll) + ")");
		GivePlayerXP(player, xpReward);

		return true; // Continue with the default fishing skill update logic
	}

	void OnPlayerAchievementComplete(Player *player, AchievementEntry const *achievement) override
	{
		if (!sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", false) || 
			!sConfigMgr->GetOption<bool>("CustomXPScaling.AchievementXP.Enable", false) ||
			!player || !achievement) return;
 
		float expPercent = sConfigMgr->GetOption<float>("CustomXPScaling.AchievementXP.Scaling", 1.5f);
		float expMultiplier = (expPercent * achievement->points) / 100;

		auto pLevel = player->GetLevel();
		if (sConfigMgr->GetOption<bool>("CustomXPScaling.AchievementXP.ScaleLevel", false))
			expMultiplier = ((expMultiplier * 100.0f) * (1.0f - (pLevel / 100.0f))) / 100.0f;
 
		float xpMax = player->GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
		float xpReward = xpMax * expMultiplier;

		LogToPlayer(player, String("Achievement XP: ") + std::to_string(xpReward) + " (Achievement Points: " + std::to_string(achievement->points) + ")");
		GivePlayerXP(player, xpReward);
	};
};

// Add all scripts in one
void AddSC_custom_xp_scaling()
{
	new CustomXPScaling();
}
