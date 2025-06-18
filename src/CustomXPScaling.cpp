/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "Chat.h"
#include "Config.h"
#include "Player.h"
#include "ScriptMgr.h"

// 		XPSOURCE_KILL = 0,
// 		XPSOURCE_QUEST = 1,
// 		XPSOURCE_QUEST_DF = 2,
// 		XPSOURCE_EXPLORE = 3,
// 		XPSOURCE_BATTLEGROUND = 4

// Add player scripts
class CustomXPScaling : public PlayerScript
{
public:
	CustomXPScaling() : PlayerScript("CustomXPScaling") { }

	void OnPlayerLogin(Player *player) override
	{
		if (sConfigMgr->GetOption<bool>("CustomXPScaling.Announce", true))
			ChatHandler(player->GetSession()).SendSysMessage("This server is running the |cff4CFF00Custom XP Scaling |rmodule.");
	}

	void OnPlayerGiveXP(Player *player, uint32 &amount, Unit *victim, uint8 xpSource) override
	{
		if (!sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true))
			return;


		std::stringstream XPScalingSources;

		// Calculate new XP with floating-point multiplication
		float calculatedXP = static_cast<float>(amount) * GetLevelXPScaling(player, XPScalingSources);

		if (xpSource == XPSOURCE_QUEST || xpSource == XPSOURCE_QUEST_DF)
			calculatedXP *= GetQuestXPScaling(XPScalingSources);
		else if (xpSource == XPSOURCE_KILL)
			calculatedXP *= GetKillXPScaling(player, victim, XPScalingSources);
		else if (xpSource == XPSOURCE_EXPLORE)
			calculatedXP *= GetExploreXPScaling(XPScalingSources);

		std::stringstream ss;
		ss << "XP Source: ";
		switch (xpSource)
		{
			case XPSOURCE_KILL: ss << "Kill"; break;
			case XPSOURCE_QUEST: ss << "Quest"; break;
			case XPSOURCE_QUEST_DF: ss << "Quest (DF)"; break;
			case XPSOURCE_EXPLORE: ss << "Explore"; break;
			case XPSOURCE_BATTLEGROUND: ss << "Battleground"; break;
			default: ss << "Unknown";
		}
		ss << " | Original XP: " << amount << " | Calculated XP: " << std::round(calculatedXP) << " | Scaling Sources: " << XPScalingSources.str();
		LogToPlayer(player, ss.str());
		// Round to nearest whole number and convert back to uint32
		amount = static_cast<uint32>(std::round(calculatedXP));
	}

	void LogToPlayer(Player *player, std::string msg)
	{
		if (!sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true) ||
			!sConfigMgr->GetOption<bool>("CustomXPScaling.LogToPlayer", false) || !player)
			return;

		ChatHandler(player->GetSession()).SendSysMessage(msg);
	}

	float GetLevelXPScaling(Player *player, std::stringstream &XPScalingSources)
	{

		float levelXPScaling = 1.0f; // Default multiplier

		if (!sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true) || !player ||
				!sConfigMgr->GetOption<bool>("CustomXPScaling.LevelXP.Enable", true))
			return levelXPScaling;

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

		// std::stringstream ss;
		// ss << "Level XP Scaling Percent: " << levelXPScaling * 100.0f << "% (Level: " << player->GetLevel() << ")";
		// LogToPlayer(player, ss.str());
		XPScalingSources << " Level: " << levelXPScaling * 100.0f << "% |";

		return levelXPScaling;
	}

	float GetKillXPScaling(Player *player, Unit *victim, std::stringstream &XPScalingSources)
	{
		if (!sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true) ||
				!player || !victim || !victim->IsCreature() || victim->IsPlayer() || !victim->ToCreature())
			return 1.0f;

		float killXPScaling = 1.0f; // Default multiplier

		if (sConfigMgr->GetOption<bool>("CustomXPScaling.KillXP.Enable", false))
		{
			killXPScaling = killXPScaling * sConfigMgr->GetOption<float>("CustomXPScaling.KillXP.Scaling", 1.0f);
			XPScalingSources << " Kill: " << killXPScaling * 100.0f << "% |";
		}

		if (sConfigMgr->GetOption<bool>("CustomXPScaling.RareXP.Enable", false))
		{
			auto creature = victim->ToCreature();
			auto creatureProto = creature->GetCreatureTemplate();

			if (creatureProto->rank == CREATURE_ELITE_NORMAL || creatureProto->rank == CREATURE_ELITE_RARE ||
				creatureProto->rank == CREATURE_ELITE_RAREELITE || creatureProto->rank == CREATURE_ELITE_ELITE)
			{
				float rareXPScaling = sConfigMgr->GetOption<float>("CustomXPScaling.RareXP.Scaling", 1.0f);
				killXPScaling = killXPScaling * rareXPScaling;
				XPScalingSources << " Rare: " << rareXPScaling * 100.0f << "% |";
			}
		}
		
		return killXPScaling;
	}

	float GetExploreXPScaling(std::stringstream &XPScalingSources)
	{
		if (!sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true) ||
				!sConfigMgr->GetOption<bool>("CustomXPScaling.ExploreXP.Enable", true))
			return 1.0f;

		float exploreXPScaling = sConfigMgr->GetOption<float>("CustomXPScaling.ExploreXP.Scaling", 1.0);
		XPScalingSources << " Explore: " << exploreXPScaling * 100.0f << "% |";
		return exploreXPScaling;
	}

	float GetQuestXPScaling(std::stringstream &XPScalingSources)
	{
		if (!sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true) ||
				!sConfigMgr->GetOption<bool>("CustomXPScaling.QuestXP.Enable", true))
			return 1.0f;

		float questXPScaling = sConfigMgr->GetOption<float>("CustomXPScaling.QuestXP.Scaling", 1.0);
		XPScalingSources << " Quest: " << questXPScaling * 100.0f << "% |";
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

	void GiveProfessionXP(Player *player)
	{
		if (!sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true) ||
				!sConfigMgr->GetOption<bool>("CustomXPScaling.ProfessionsXP.Enable", true))
			return;

		float professionXPScaling = sConfigMgr->GetOption<float>("CustomXPScaling.ProfessionsXP.Scaling", 0.01f);

		float xpMax = player->GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
		float xpReward = xpMax * professionXPScaling;

		std::stringstream ss;
		ss << "Profession XP: " << xpReward;
		LogToPlayer(player, ss.str());

		GivePlayerXP(player, xpReward);
	}

	void OnPlayerUpdateGatheringSkill(Player *player, uint32 /*skillId*/, uint32 /*currentLevel*/, uint32 /*gray*/, uint32 /*green*/, uint32 /*yellow*/, uint32 &gain) override
	{
		GiveProfessionXP(player);
	}

	void OnPlayerUpdateCraftingSkill(Player *player, SkillLineAbilityEntry const */*const *skill*/, uint32 /*currentLevel*/, uint32 &gain) override
	{
		GiveProfessionXP(player);
	}

	bool OnPlayerUpdateFishingSkill(Player *player, int32 /*skill*/, int32 /*zone_skill*/, int32 /*chance*/, int32 roll) override
	{
		GiveProfessionXP(player);
		return true; // Continue with the default fishing skill update logic
	}

	void OnPlayerAchievementComplete(Player *player, AchievementEntry const *achievement) override
	{
		if (!sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", false) || 
			!sConfigMgr->GetOption<bool>("CustomXPScaling.AchievementXP.Enable", false) ||
			!player || !achievement) return;
 
		float achievementXPScaling = sConfigMgr->GetOption<float>("CustomXPScaling.AchievementXP.Scaling", 0.1f);
 
		float xpMax = player->GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
		float xpReward = xpMax * achievementXPScaling;

		std::stringstream ss;
		ss <<  "Achievement XP: " << xpReward << " (Points: " << achievement->points << ")";
		LogToPlayer(player, ss.str());

		GivePlayerXP(player, xpReward);
	};
};

// Add all scripts in one
void AddCustomXPScalingScripts()
{
	new CustomXPScaling();
}
