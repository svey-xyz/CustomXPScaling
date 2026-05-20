/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef CXPS_CAST_SPEED_H
#define CXPS_CAST_SPEED_H

// CustomXPScaling cast-speed subsystem.
//
// Globally rewrites SpellInfo cast times for profession spells -- crafts,
// gathering, fishing, disenchant, smelt, lockpick -- once at world startup,
// using the per-source / per-rank multipliers from cxpsConfig.
//
// Apply mode: WORLDHOOK_ON_STARTUP. DBC + config are both loaded by the time
// the hook fires, so we can read SkillLineAbility data and cxpsConfig in the
// same pass. The mutation is one-shot per process: per-modified-spell we
// allocate a synthesized SpellCastTimesEntry, store it in a process-lifetime
// container, and rebind SpellInfo::CastTimeEntry to it. SpellMgr never
// reloads the spell store at runtime, so this stays valid until shutdown.
//
// Config changes to the multipliers DO require a worldserver restart. The
// per-spell rebind is not reverted on .reload config (the original pointers
// aren't tracked). This is documented in conf/custom_xp_scaling.conf.dist.
//
// Inspiration: mod-craftspeed (Day36512) -- this module generalizes the same
// trick across the six CXPS profession sources and adds per-rank brackets.

namespace CXPS
{
namespace CastSpeed
{
    // Entry point called from the CXPS_CastSpeedScript WorldScript on
    // WORLDHOOK_ON_STARTUP. No-op on subsequent invocations -- iterating the
    // spell store twice would compound multipliers and over-scale every
    // affected cast. Safe to call before sSpellMgr is fully populated; the
    // function bails if GetSpellInfoStoreSize() returns 0.
    void Apply();
}
}

#endif // CXPS_CAST_SPEED_H
