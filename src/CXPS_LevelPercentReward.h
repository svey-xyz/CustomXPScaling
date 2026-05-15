/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef CXPS_LEVEL_PERCENT_REWARD_H
#define CXPS_LEVEL_PERCENT_REWARD_H

#include "Define.h"

class Player;

// Shared "percent of next-level XP" reward math used by every XP source that
// pays a rolled fraction of the player's next level rather than scaling a
// core-supplied amount: professions, taxi nodes, and achievements.
//
// The "<base>:<randomizer>" config string semantics live here so all three
// callers stay in lockstep -- both halves are absolute percentage points of
// PLAYER_NEXT_LEVEL_XP, and a tick rolls
//     (base + frand(-randomizer, +randomizer)) * multiplier
// where the multiplier is the caller's difficulty/points weight (1.0 = none).
namespace CXPS
{
    // A parsed "<base>:<randomizer>" pair. Both are absolute percentage points
    // of next-level XP. randomizer is always >= 0 after parsing.
    struct LevelPercent
    {
        float base;
        float randomizer;
    };

    // Parse a "base:randomizer" config string (e.g. "1.0:0.25").
    // Falls back to the supplied defaults on a missing key or malformed value
    // (either half non-numeric). A negative randomizer is clamped to 0.
    LevelPercent ParseLevelPercent(char const* configKey, float defaultBase, float defaultRand);

    // Roll the effective percent for one tick:
    //     max(0, (base + frand(-randomizer, +randomizer)) * multiplier)
    // multiplier is the caller's difficulty/points weight; pass 1.0f for none.
    float RollEffectivePercent(LevelPercent const& levelPercent, float multiplier);

    // Convert an effective percent of next-level XP into a concrete XP amount
    // for this player. Returns 0 at the level cap (PLAYER_NEXT_LEVEL_XP == 0)
    // or when the result rounds down to 0 -- callers must short-circuit on 0
    // so they never feed zero XP into the core.
    uint32 LevelPercentToXP(Player const* player, float effectivePercent);
}

#endif // CXPS_LEVEL_PERCENT_REWARD_H
