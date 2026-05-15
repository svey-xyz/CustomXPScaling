/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "CXPS_LevelPercentReward.h"
#include "Config.h"
#include "Player.h"
#include "Random.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace CXPS
{
    LevelPercent ParseLevelPercent(char const* configKey, float defaultBase, float defaultRand)
    {
        LevelPercent result{ defaultBase, defaultRand };

        const std::string raw = sConfigMgr->GetOption<std::string>(
            configKey, std::to_string(defaultBase) + ":" + std::to_string(defaultRand));

        const auto colon = raw.find(':');
        if (colon != std::string::npos)
        {
            try
            {
                result.base = std::stof(raw.substr(0, colon));
                result.randomizer = std::stof(raw.substr(colon + 1));
            }
            catch (...)
            {
                // Either half non-numeric -- keep both defaults rather than a
                // half-parsed pair, matching the documented fallback behavior.
                result.base = defaultBase;
                result.randomizer = defaultRand;
            }
        }

        if (result.randomizer < 0.0f)
            result.randomizer = 0.0f;

        return result;
    }

    float RollEffectivePercent(LevelPercent const& levelPercent, float multiplier)
    {
        const float roll = levelPercent.randomizer > 0.0f
            ? frand(-levelPercent.randomizer, levelPercent.randomizer)
            : 0.0f;
        return std::max(0.0f, (levelPercent.base + roll) * multiplier);
    }

    uint32 LevelPercentToXP(Player const* player, float effectivePercent)
    {
        if (!player)
            return 0;

        const uint32 nextLevelXP = player->GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
        return static_cast<uint32>(std::round(nextLevelXP * (effectivePercent / 100.0f)));
    }
}
