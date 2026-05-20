/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "CXPS_CastSpeed.h"
#include "CXPS_Config.h"

#include "DBCStores.h"
#include "DBCStructure.h"
#include "Log.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace
{
    // Disenchant rides on SKILL_ENCHANTING; the spell id is the only reliable
    // distinction. Same constant used in CustomXPScaling.cpp -- kept in sync
    // by convention (the runtime never compares the two values directly).
    constexpr std::uint32_t CXPS_SPELL_DISENCHANT = 13262;

    // Owns the synthesized SpellCastTimesEntry structs we point modified
    // SpellInfo::CastTimeEntry slots at. Reserved for process lifetime --
    // SpellMgr never reloads its store, and we apply exactly once.
    std::vector<std::unique_ptr<SpellCastTimesEntry>> g_synthesizedCastTimes;

    // Latched so a second Apply() (e.g. someone wires the hook twice by
    // mistake) is a no-op rather than compounding multipliers.
    bool g_applied = false;

    struct ClassifiedSpell
    {
        CXPSCastSpeedSource source;
        std::uint32_t       rankSkill; // chooses the Apprentice...GrandMaster bucket
    };

    // Returns nullopt for any spell we shouldn't touch:
    //   * no cast time entry, or cast time <= 0 (instant)
    //   * no SkillLineAbility entry (not a profession spell)
    //   * profession skill we don't model
    //
    // Classification order is significant: signature spells (disenchant) win
    // over generic skill-line rules; lockpicking/fishing are recognized by
    // skill alone; mining is split into smelt vs. gather by checking for a
    // create-item effect (smelt recipes produce bars; the gather cast does
    // not).
    std::optional<ClassifiedSpell> Classify(SpellInfo const* spellInfo)
    {
        if (!spellInfo)
            return std::nullopt;
        if (!spellInfo->CastTimeEntry || spellInfo->CastTimeEntry->CastTime <= 0)
            return std::nullopt;

        SkillLineAbilityMapBounds bounds =
            sSpellMgr->GetSkillLineAbilityMapBounds(spellInfo->Id);
        if (bounds.first == bounds.second)
            return std::nullopt;

        // Multiple SkillLineAbility rows can reference the same spell (e.g.
        // recipes taught by different trainers). They share SkillLine and
        // MinSkillLineRank for our purposes -- the first match is enough.
        SkillLineAbilityEntry const* sla = bounds.first->second;
        if (!sla)
            return std::nullopt;

        std::uint32_t const skillLine = sla->SkillLine;
        std::uint32_t const rank      = sla->MinSkillLineRank; // 0 for raw skill-line abilities

        // 1) Disenchant -- signature spell id wins over SKILL_ENCHANTING.
        if (spellInfo->Id == CXPS_SPELL_DISENCHANT)
            return ClassifiedSpell{CXPS_CAST_SPEED_DISENCHANT, rank};

        // 2) Lockpick / Fish -- single-spell skill lines, no recipe rank.
        if (skillLine == SKILL_LOCKPICKING)
            return ClassifiedSpell{CXPS_CAST_SPEED_LOCKPICK, rank};
        if (skillLine == SKILL_FISHING)
            return ClassifiedSpell{CXPS_CAST_SPEED_FISH, rank};

        // 3) Does this spell produce an item? Smelt + craft both do; gather
        //    casts (Mining/Herb Gathering/Skinning) do not. CREATE_ITEM_2 is
        //    rare in 3.3.5 but present -- match it for safety.
        bool hasCreateItem = false;
        for (auto const& effect : spellInfo->Effects)
        {
            if (effect.Effect == SPELL_EFFECT_CREATE_ITEM ||
                effect.Effect == SPELL_EFFECT_CREATE_ITEM_2)
            {
                hasCreateItem = true;
                break;
            }
        }

        // 4) Mining is the ambiguous one: gather-mining and smelt-recipes
        //    both report SKILL_MINING. Use the create-item flag to split.
        if (skillLine == SKILL_MINING)
        {
            return hasCreateItem
                ? ClassifiedSpell{CXPS_CAST_SPEED_SMELT,  rank}
                : ClassifiedSpell{CXPS_CAST_SPEED_GATHER, rank};
        }

        // 5) Herbalism / Skinning -- gather casts. No create-item.
        if (skillLine == SKILL_HERBALISM || skillLine == SKILL_SKINNING)
            return ClassifiedSpell{CXPS_CAST_SPEED_GATHER, rank};

        // 6) Everything else: real crafting recipe (alchemy, blacksmithing,
        //    cooking, etc.). We require a create-item effect AND at least one
        //    reagent so we don't accidentally slow the "Cooking" /
        //    "Blacksmithing" *learn* spells, which sit on the same SkillLine
        //    but produce nothing.
        if (!hasCreateItem)
            return std::nullopt;

        bool hasReagent = false;
        for (auto const& reagent : spellInfo->Reagent)
        {
            if (reagent > 0)
            {
                hasReagent = true;
                break;
            }
        }
        if (!hasReagent)
            return std::nullopt;

        return ClassifiedSpell{CXPS_CAST_SPEED_CRAFT, rank};
    }

    // Allocates a synthesized SpellCastTimesEntry seeded from the original
    // and rebinds the SpellInfo to it. SpellInfo::CastTimeEntry is declared
    // as `SpellCastTimesEntry const*` on a non-const class, so we const_cast
    // the SpellInfo (legal: we own the data via SpellMgr) and assign a
    // pointer to a struct we own. The original DBC entry is left untouched
    // so unrelated spells that share its CastingTimeIndex are unaffected.
    void RebindCastTime(SpellInfo const* spellInfo, std::int32_t newCastTimeMs)
    {
        auto synth = std::make_unique<SpellCastTimesEntry>(*spellInfo->CastTimeEntry);
        synth->CastTime = newCastTimeMs;

        SpellCastTimesEntry const* synthPtr = synth.get();
        g_synthesizedCastTimes.push_back(std::move(synth));

        const_cast<SpellInfo*>(spellInfo)->CastTimeEntry = synthPtr;
    }
} // anonymous namespace

void CXPS::CastSpeed::Apply()
{
    if (g_applied)
        return;
    g_applied = true;

    if (!cxpsConfig.GetBool(CXPSConfig::CASTSPEED_ENABLE))
        return;

    std::uint32_t const storeSize = sSpellMgr->GetSpellInfoStoreSize();
    if (storeSize == 0)
        return;

    bool const logToConsole = cxpsConfig.GetBool(CXPSConfig::CASTSPEED_LOG_TO_CONSOLE);
    float const floorMs     = std::max(0.0f,
        cxpsConfig.GetFloat(CXPSConfig::CASTSPEED_MIN_CAST_TIME_MS));

    // Pre-reserve a generous capacity. 3.3.5 has ~50k spells, of which a few
    // thousand qualify -- reserving avoids reallocation churn while iterating.
    g_synthesizedCastTimes.reserve(4096);

    std::uint32_t modified = 0;

    for (std::uint32_t spellId = 1; spellId < storeSize; ++spellId)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            continue;

        auto classification = Classify(spellInfo);
        if (!classification)
            continue;

        float const sourceMult = cxpsConfig.GetCastSpeedSourceMultiplier(classification->source);
        float const rankMult   = cxpsConfig.GetCastSpeedRankMultiplier(classification->rankSkill);
        float const totalMult  = sourceMult * rankMult;

        // No-op when both multipliers leave the cast unchanged. Skipping
        // avoids spurious heap allocations for the majority case where the
        // admin only tweaks one or two sources.
        if (std::fabs(totalMult - 1.0f) < 0.0001f)
            continue;
        if (totalMult <= 0.0f)
            continue; // would zero or negate cast time; treat as opt-out for this spell

        std::int32_t const original = spellInfo->CastTimeEntry->CastTime;
        float const scaled = static_cast<float>(original) * totalMult;
        // scaled is always >= 0 here (we guarded totalMult > 0 above and the
        // original DBC cast time is > 0). max-with-floor is safe.
        std::int32_t const clamped = static_cast<std::int32_t>(
            std::round(std::max(floorMs, scaled)));

        if (clamped == original)
            continue;

        RebindCastTime(spellInfo, clamped);
        ++modified;

        if (logToConsole)
        {
            LOG_INFO("module",
                "[CXPS][CastSpeed] spell {} ({}) cast {} -> {} ms "
                "(source={} x{:.3f}, rank={} x{:.3f})",
                spellId,
                CXPSConfigData::CastSpeedSourceLabel(classification->source),
                original, clamped,
                CXPSConfigData::CastSpeedSourceLabel(classification->source),
                sourceMult,
                classification->rankSkill, rankMult);
        }
    }

    LOG_INFO("module",
        "[CXPS][CastSpeed] modified {} profession spell cast times "
        "(crafts/gather/fish/disenchant/smelt/lockpick).", modified);
}
