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
CustomXPScaling() : PlayerScript("CustomXPScaling") { }

void OnPlayerLogin(Player *player) override
{
	if (sConfigMgr->GetOption<bool>("CustomXPScaling.Announce", true))
		ChatHandler(player->GetSession()).SendSysMessage("This server is running the |cff4CFF00Level Dynamic XP |rmodule.");
}

void OnPlayerGiveXP(Player *player, uint32 &amount, Unit * victim, uint8 /*xpSource*/) override
{
	if (sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true))
	{

		if (sConfigMgr->GetOption<bool>("CustomXPScaling.RareXP.Enable", true) && victim && !victim->IsPlayer() && victim->ToCreature())
		{
			auto creature = victim->ToCreature();
			auto creatureProto = creature->GetCreatureTemplate();

			if (creatureProto->rank != CREATURE_ELITE_RARE && creatureProto->rank != CREATURE_ELITE_RAREELITE)
			{
				return;
			}

			float rareExpMulti = sConfigMgr->GetOption<float>("CustomXPScaling.RareXP.Scaling", 3.0);
			// amount = amount * rareExpMulti;
			// Calculate new XP with floating-point multiplication
			float calculatedXP = static_cast<float>(amount) * rareExpMulti;
			// Round to nearest whole number and convert back to uint32
			amount = static_cast<uint32>(std::round(calculatedXP));
		}

		float rateMultiplier = 1.0f; // Default multiplier

		if (player->GetLevel() <= 9)
				rateMultiplier = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.1-9", 0.2f);
		else if (player->GetLevel() <= 19)
				rateMultiplier = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.10-19", 0.3f);
		else if (player->GetLevel() <= 29)
				rateMultiplier = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.20-29", 0.8f);
		else if (player->GetLevel() <= 39)
				rateMultiplier = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.30-39", 1.0f);
		else if (player->GetLevel() <= 49)
				rateMultiplier = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.40-49", 1.2f);
		else if (player->GetLevel() <= 59)
				rateMultiplier = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.50-59", 1.3f);
		else if (player->GetLevel() <= 69)
				rateMultiplier = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.60-69", 1.3f);
		else if (player->GetLevel() <= 79)
				rateMultiplier = sConfigMgr->GetOption<float>("CustomXPScaling.LevelXP.Scaling.70-79", 1.3f);

		// Calculate new XP with floating-point multiplication
		float calculatedXP = static_cast<float>(amount) * rateMultiplier;
		// Round to nearest whole number and convert back to uint32
		amount = static_cast<uint32>(std::round(calculatedXP));
	}
}

void OnPlayerUpdateGatheringSkill(Player *player, uint32 skillId, uint32 currentLevel, uint32 gray, uint32 green, uint32 yellow, uint32 &gain) override
{
	if (sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true))
	{
		if (currentLevel < gray)
			gain = sConfigMgr->GetOption<uint32>("CustomXPScaling.Rate.Gathering.Gray", 1);
		else if (currentLevel < green)
			gain = sConfigMgr->GetOption<uint32>("CustomXPScaling.Rate.Gathering.Gray", 1);
		else if (currentLevel < yellow)
			gain = sConfigMgr->GetOption<uint32>("CustomXPScaling.Rate.Gathering.Green", 2);
		else
			gain = sConfigMgr->GetOption<uint32>("CustomXPScaling.Rate.Gathering.Yellow", 3);
	}
}

void OnPlayerUpdateCraftingSkill(Player *player, SkillLineAbilityEntry const *skill, uint32 currentLevel, uint32 &gain) override
{
	if (sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true))
	{
		if (currentLevel < skill->Gray)
			gain = sConfigMgr->GetOption<uint32>("CustomXPScaling.Rate.Crafting.Gray", 1);
		else if (currentLevel < skill->Green)
			gain = sConfigMgr->GetOption<uint32>("CustomXPScaling.Rate.Crafting.Green", 2);
		else if (currentLevel < skill->Yellow)
			gain = sConfigMgr->GetOption<uint32>("CustomXPScaling.Rate.Crafting.Yellow", 3);
		else
			gain = sConfigMgr->GetOption<uint32>("CustomXPScaling.Rate.Crafting.Orange", 4);
	}
}

bool OnPlayerUpdateFishingSkill(Player *player, int32 skill, int32 zone_skill, int32 chance, int32 roll) override
{
	if (sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", true))
	{
		if (skill < zone_skill)
			return sConfigMgr->GetOption<bool>("CustomXPScaling.Rate.Fishing.Gray", true);
		else if (skill < chance)
			return sConfigMgr->GetOption<bool>("CustomXPScaling.Rate.Fishing.Green", true);
		else if (skill < roll)
			return sConfigMgr->GetOption<bool>("CustomXPScaling.Rate.Fishing.Yellow", true);
		else
			return sConfigMgr->GetOption<bool>("CustomXPScaling.Rate.Fishing.Orange", true);
	}
	return false;
}

	void OnPlayerAchievementComplete(Player * player, AchievementEntry const * achievement) override
	{
		if (!sConfigMgr->GetOption<bool>("CustomXPScaling.Enable", false) || 
			!sConfigMgr->GetOption<bool>("CustomXPScaling.AchievementXP.Enable", false) ||
			!player || !achievement
		{
			return;
		}

		auto pLevel = player->GetLevel();
		if (pLevel >= sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
		{
			return;
		}

		float expPercent = sConfigMgr->GetOption<float>("CustomXPScaling.AchievementXP.Percent", 1.5f);
		float expMultiplier = (expPercent * achievement->points) / 100;

		if (sConfigMgr->GetOption<bool>("CustomXPScaling.AchievementXP.ScaleLevel", false))
		{
			expMultiplier = ((expMultiplier * 100.0f) * (1.0f - (pLevel / 100.0f))) / 100.0f;
		}

		float xpMax = player->GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
		float xpReward = xpMax * expMultiplier;

		player->GiveXP(xpReward, nullptr);
	};
};

// Add all scripts in one
void AddSC_custom_xp_scaling()
{
    new CustomXPScaling();
}
