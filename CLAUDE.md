# CustomXPScaling — module guide for Claude

AzerothCore (WotLK) module that rewrites XP gains across every source the
core fires hooks for: combat, quests, exploration, professions, and
achievements. All behavior is gated by `worldserver.conf` options under the
`CustomXPScaling.*` namespace.

## Repo layout

```
src/
  CXPS_loader.cpp              Module entry point (Addmod_custom_xp_scaling_Scripts).
  CustomXPScaling.cpp          PlayerScript with every XP hook, the chat
                               CommandScript, and two WorldScripts:
                               CXPS_LoadConfigScript (config cache) and
                               CXPS_CastSpeedScript (boot-time cast-speed pass).
  CXPS_Config.{h,cpp}          ConfigValueCache wrapper (cxpsConfig global).
                               Owns the CXPSConfig enum, the per-skill lookup,
                               the rank-bracket lookup, and the CastSpeed
                               source/rank accessors. Runtime never touches
                               sConfigMgr -- every accessor reads a typed
                               cached value.
  CXPS_LevelPercentReward.*    Shared "<base>:<randomizer>" parsing and the
                               percent-of-next-level roll. Strings are parsed
                               once during cxpsConfig.BuildConfigCache(); the
                               parsed CXPS::LevelPercent struct is stored
                               alongside the cache for runtime use.
  CXPS_CastSpeed.{h,cpp}       Profession-spell cast-time rewriter. Iterates
                               the SpellInfo store once at
                               WORLDHOOK_ON_STARTUP, classifies each spell
                               via SkillLineAbility + spell-effect inspection,
                               and rebinds SpellInfo::CastTimeEntry to a
                               synthesized struct so unrelated spells sharing
                               the original DBC CastingTimeIndex stay intact.
conf/
  custom_xp_scaling.conf.dist  Documented config; AC copies this to .conf.
apps/ci/                 Codestyle script consumed by the GitHub workflow.
.github/workflows/       core-build + core_codestyle CI (mirror AC's checks).
include.sh               Empty stub; AC's module loader sources it.
```

`AddCustomXPScalingScripts()` is called from `Addmod_custom_xp_scaling_Scripts`
in `CXPS_loader.cpp`, which is the symbol AC's module system invokes at boot.
It registers four scripts: the `CustomXPScaling` PlayerScript (every XP hook),
the `CXPS_CommandScript` (`.xpscaling` family), the `CXPS_LoadConfigScript`
WorldScript (populates `cxpsConfig` on every `WORLDHOOK_ON_BEFORE_CONFIG_LOAD`,
which fires at world boot and on `.reload config`), and the
`CXPS_CastSpeedScript` WorldScript (runs the cast-speed pass once on
`WORLDHOOK_ON_STARTUP` after DBC + config are loaded).

All scripts use the modern hook-list constructor so the core only iterates
this module for the hooks listed in the constructor (see the `PLAYERHOOK_*` /
`WORLDHOOK_*` lists at the top of each class). Split the two WorldScripts
intentionally: the cast-speed pass should NOT re-run on `.reload config`
(cast-time mutation is restart-only), and the config cache should NOT pay
the cost of being iterated on every `OnStartup` hook fan-out.

## Hooks used

| Hook | Source | Notes |
|------|--------|-------|
| `OnPlayerLogin` | — | One-shot announce on login if `CustomXPScaling.Announce`. |
| `OnPlayerGiveXP` | Quest/Kill/Explore/BG | Mutates `amount&` in place; never zeroes it (AC drops the XP if 0). |
| `OnPlayerUpdateGatheringSkill` | Mining/Herb/Skinning | Uses the hook's `gray/green/yellow` skill thresholds directly. |
| `OnPlayerUpdateCraftingSkill` | All tradeskill crafts | Derives thresholds from `SkillLineAbilityEntry`. |
| `OnPlayerUpdateFishingSkill` | Fishing | No DBC thresholds — approximates from `skill - zone_skill`. |
| `OnPlayerAchievementComplete` | Achievement award | Rolls a `LevelPercent` reward; achievement `points` act as the difficulty multiplier. |
| `OnPlayerLearnTaxiNode` | Flight-path discovery | Rolls a `LevelPercent` reward, no difficulty tiers (flat 1.0x). |

## XP flow at a glance

`OnPlayerGiveXP` composes a single `scalingFactor` by multiplying:

1. **Base level bracket** (`LevelXP.Scaling.<range>`) — applied to every source.
2. **Source-specific multiplier** — exactly one of `KillXP.Scaling`,
   `RareXP.Scaling` (stacked on top of Kill when victim has `rank > 0`),
   `QuestXP.Scaling`, or `ExploreXP.Scaling`.

Battleground XP falls through the `default` branch and only gets level scaling.
All `*.Enable` toggles default true *except* `KillXP.Enable` and `RareXP.Enable`
in code (the dist conf turns them on — code defaults are the safety net if a
key is missing).

`LogToPlayer = 1` sends a per-event system chat line with the original amount,
the result, and a `|`-delimited breakdown of every multiplier that fired.

## Profession XP

Every profession event passes through `GiveProfessionXP(player, ctx)`, where
`ctx` is a `ProfessionContext` filled in by the hook. The roll is:

```
xp = round(PLAYER_NEXT_LEVEL_XP * effective% / 100)
effective% = max(0, (base ± randomizer) * Difficulty * SkillMult * RankMult)
```

`base` and `randomizer` come from the source's `LevelPercent` (pre-parsed at
config load — see "Config cache" below). The three multipliers are
independent axes; all default to 1.0 so a fresh install behaves identically
to the pre-axes version.

### Sources

The profession path has six logical sources, each with its own optional
`LevelPercent` override and its own chat-log label:

| `CXPSProfessionSource` | Hook                            | Routed when                                  |
|------------------------|---------------------------------|----------------------------------------------|
| `CXPS_PROF_SRC_GATHER`     | `OnPlayerUpdateGatheringSkill` | any gather skill except `SKILL_LOCKPICKING` |
| `CXPS_PROF_SRC_LOCKPICK`   | `OnPlayerUpdateGatheringSkill` | `skillId == SKILL_LOCKPICKING`              |
| `CXPS_PROF_SRC_CRAFT`      | `OnPlayerUpdateCraftingSkill`  | any craft except disenchant / smelt         |
| `CXPS_PROF_SRC_DISENCHANT` | `OnPlayerUpdateCraftingSkill`  | `skill->Spell == 13262`                     |
| `CXPS_PROF_SRC_SMELT`      | `OnPlayerUpdateCraftingSkill`  | `skill->SkillLine == SKILL_MINING`          |
| `CXPS_PROF_SRC_FISH`       | `OnPlayerUpdateFishingSkill`   | always                                      |

Add a new source by extending the enum, the two static helpers in
`CXPSConfigData` (`SourceLevelPercentKey` and `SourceLabel`), and registering
its `LevelPercent` cache slot in `BuildConfigCache`.

### Difficulty (color tier)

`enum ProfessionDifficulty { GRAY, GREEN, YELLOW, ORANGE }`; `ClassifyDifficulty`
mirrors AC's skill-up thresholds:

- Gathering — the hook supplies `gray/green/yellow` directly.
- Crafting — `SkillLineAbilityEntry::TrivialSkillLineRankHigh` is gray,
  `TrivialSkillLineRankLow` is yellow, green is the midpoint.
- Fishing — heuristic: `delta = skill - zone_skill`, `≥100` gray, `≥50` green,
  `≥0` yellow, else orange. Approximation only; no DBC trivial fields exist.

`Difficulty.Gray = 0` (the default) short-circuits the path before any roll,
so gray ticks pay nothing.

### Per-skill multipliers

Independent axis layered on top of difficulty. Routed by
`CXPSConfigData::GetSkillMultiplier(skillId, isDisenchant, isSmelt)`:

- `isDisenchant` → `Skill.Disenchanting.Multiplier` (disenchant rides on
  `SKILL_ENCHANTING`; the spell id 13262 is the only reliable signal).
- `isSmelt` → `Skill.Smelting.Multiplier` (rides on `SKILL_MINING`, but only
  when reported by the crafting hook — mining-the-gather routes to `Mining`).
- otherwise `skillId` switch to the matching `Skill.<Name>.Multiplier`.

Default 1.0 for every skill. Adding a new bucket means a new
`PROFESSIONS_SKILL_*` enum, a `setSkill(...)` line, and a `case` in the
switch.

### Skill-rank brackets

Second multiplicative axis, derived from a "rank skill" value chosen per
hook:

- Crafting — `skill->MinSkillLineRank` (the recipe's required skill, so a
  450-rank recipe pays GrandMaster XP regardless of how over-skilled the
  player is). Falls back to `currentLevel` when `MinSkillLineRank == 0`.
- Gathering / Fishing — the player's current skill.

`CXPSConfigData::GetRankMultiplier(rank)` picks the bucket with the same
`> 75 / > 150 / > 225 / > 300 / > 375` boundaries PE uses.

### Reward math

Lives in `CXPS_LevelPercentReward.{h,cpp}` (`namespace CXPS`):

```
CXPS::ParseLevelPercent(key, defBase, defRand)  → LevelPercent{ base, randomizer }
CXPS::RollEffectivePercent(levelPercent, mult)  → max(0, (base + frand(-rand,+rand)) * mult)
CXPS::LevelPercentToXP(player, effectivePct)    → round(PLAYER_NEXT_LEVEL_XP * effectivePct / 100)
```

`ParseLevelPercent` is only called from `CXPSConfigData::BuildConfigCache`
during load / reload — the runtime always reads pre-parsed pairs from
`cxpsConfig.GetLevelPercent(...)`. Don't call `ParseLevelPercent` from a hot
path; it allocates a string and runs `try`/`stof`.

`base` and `randomizer` are **absolute percent points** of next-level XP. With
the dist defaults (`"1.0:0.25"`, `Difficulty.Yellow = 1.0`, all Skill/Rank
multipliers `1.0`) a Yellow tick lands between **0.75% and 1.25%** of next-
level XP; Orange (mult 1.5) lands between **1.125% and 1.875%**. Every caller
short-circuits on `xpReward == 0` (level cap, where `PLAYER_NEXT_LEVEL_XP` is
0, also any all-zero multiplier).

### Per-source LevelPercent override mechanism

Each of the six sources has its own optional `LevelPercent` key. The
inheritance happens **once at parse time** inside
`CXPSConfigData::BuildConfigCache`: the global `ProfessionsXP.LevelPercent`
is parsed first, then each per-source key is parsed with the parsed global's
values threaded in as the `ParseLevelPercent` defaults. A missing, empty, or
malformed per-source string silently falls back to the global — same as
before the cache existed. Don't add a separate empty-string check or
hard-coded fallback at lookup time; the default-threading at parse time is
the whole mechanism.

## Achievement XP

`OnPlayerAchievementComplete` uses the same shared `LevelPercent` roll as
professions. The difference is the multiplier: instead of a Gray/Green/Yellow/
Orange tier it derives a **points multiplier** from `achievement->points` via
`GetAchievementPointsMultiplier` —

```
mult = clamp(points / PointsPerUnit, MinMultiplier, MaxMultiplier)
```

so a higher-point achievement pays proportionally more. `PointsPerUnit <= 0`
disables points weighting (every achievement uses `1.0`, still clamped to the
band). Config keys:

```
CustomXPScaling.AchievementXP.Enable         = 1
CustomXPScaling.AchievementXP.LevelPercent   = "1.0:0.25"
CustomXPScaling.AchievementXP.PointsPerUnit  = 10.0
CustomXPScaling.AchievementXP.MinMultiplier  = 0.5
CustomXPScaling.AchievementXP.MaxMultiplier  = 5.0
```

The older `AchievementXP.Scaling` / `AchievementXP.ScaleLevel` keys were
removed in this rework — don't reintroduce them.

## Cast Speed (profession spell cast-time mutation)

Counterweight to the boosted profession / gather / fish XP: optionally
**slows** the cast time of every profession spell so the rate of XP per
real-world second stays in the admin's intended band. Disabled by default
(`CustomXPScaling.CastSpeed.Enable = 0`); turning it on at multiplier 1.0
across the board is a no-op. Inspired by `Day36512/mod-craftspeed`, which
performs the same global rewrite for crafting spells with a single
multiplier — this module generalizes the idea across six CXPS sources and
adds per-rank brackets.

### Where it lives

The whole subsystem is in `CXPS_CastSpeed.{h,cpp}` plus the
`CXPS_CastSpeedScript` WorldScript registered in `CustomXPScaling.cpp`. The
config keys (`CASTSPEED_*` in the `CXPSConfig` enum, `CXPSCastSpeedSource`
in the source enum) and the source/rank accessors
(`GetCastSpeedSourceMultiplier`, `GetCastSpeedRankMultiplier`,
`CastSpeedSourceLabel`) sit on the existing `cxpsConfig` global so the
boot pass never touches `sConfigMgr`.

### When it runs

Exactly once per worldserver process, on `WORLDHOOK_ON_STARTUP`. By that
point `sSpellMgr` has finished `LoadSpellInfoStore()` and `cxpsConfig` has
been populated by the earlier `WORLDHOOK_ON_BEFORE_CONFIG_LOAD` pass, so
both are safe to read. `CXPS::CastSpeed::Apply()` latches on a static
`g_applied` flag so a misconfigured second registration is a no-op
instead of compounding multipliers.

### The math

```
final = max(MinCastTimeMs, round(original * SourceMult * RankMult))
```

Multiplier convention is `>= 1.0` SLOWS the cast (1.5 = 50% slower);
`< 1.0` speeds it up; `0.0` is treated as "skip this spell" rather than
"make it instant" because zero cast times misbehave on the client. The
floor (`CustomXPScaling.CastSpeed.MinCastTimeMs`, default 250ms) is the
right place to cap fast values.

### Classification rules

`Classify(SpellInfo const*)` returns a source + rank for each modifiable
spell. Routing is intentionally ordered so the more specific rule wins:

1. **Disenchant** — `spellId == 13262`. Same signature the XP path uses,
   for the same reason: there is no DBC distinction between Enchant and
   Disenchant beyond this single id.
2. **Lockpick** — `SkillLine == SKILL_LOCKPICKING`. Rogue / key spells.
3. **Fish** — `SkillLine == SKILL_FISHING`. The wait-for-bite channel.
4. **Smelt vs. Mining gather** — both are `SkillLine == SKILL_MINING`.
   Split by whether the spell has `SPELL_EFFECT_CREATE_ITEM` /
   `SPELL_EFFECT_CREATE_ITEM_2`. Smelt recipes produce bars, the gather
   cast doesn't.
5. **Gather (non-mining)** — `SkillLine` is `SKILL_HERBALISM` or
   `SKILL_SKINNING` and no create-item effect.
6. **Craft** — any other profession spell with a create-item effect *and*
   at least one `Reagent[i] > 0`. The reagent check is the same filter
   mod-craftspeed uses to exclude trade-skill *learn* spells (e.g. the
   "Cooking" parent ability), which sit on a profession SkillLine but
   produce nothing.

A spell with no `SkillLineAbility` row or a non-positive cast time is
skipped immediately. Multiple SkillLineAbility rows for the same spell
(recipes taught by multiple trainers) share `SkillLine` and
`MinSkillLineRank` for our purposes, so the first one wins.

### Rank brackets

Identical bracket boundaries to the XP `Rank.*` ladder
(`> 75 / > 150 / > 225 / > 300 / > 375`), but kept on **separate** config
slots (`CASTSPEED_RANK_*`) so admins can slow late-tier recipes without
also amplifying their XP yield. Bracket is chosen from the SLA's
`MinSkillLineRank`. Single-spell skill lines (Fishing, Lockpick, raw
gather casts) typically have `MinSkillLineRank == 0`, landing in the
Apprentice bucket — tune that slot if you want to scale them.

### How the mutation works

For each modified spell, we:

1. Allocate a fresh `SpellCastTimesEntry` seeded from the original
   (`std::make_unique<SpellCastTimesEntry>(*spellInfo->CastTimeEntry)`).
2. Overwrite `synth->CastTime` with the new value.
3. `const_cast` the `SpellInfo` and reassign `CastTimeEntry` to point at
   the synthesized struct.

The synthesized entries are owned by `g_synthesizedCastTimes`
(`std::vector<std::unique_ptr<SpellCastTimesEntry>>`) which stays alive
for the process lifetime. Critically, this **does not** mutate the
shared `sSpellCastTimesStore` DBC entry — that entry's
`CastingTimeIndex` is reused by unrelated spells, so editing it in place
would scale them too. The per-spell `CastTimeEntry` rebind is the only
correct surgical edit.

### Reload behavior

There is none, by design. The cast-time mutation is one-shot and never
reverted. Editing any `CastSpeed.*` key and running `.reload config`
updates `cxpsConfig` (so the in-memory values are correct), but the
already-rebound `SpellInfo::CastTimeEntry` pointers stay where they are.
A worldserver restart is required for cast-speed config changes to take
effect. This is documented in `conf/custom_xp_scaling.conf.dist` and is
the same behavior mod-craftspeed has — adding revert-on-reload would mean
tracking originals on a 50k-spell store and walking them all on every
reload, which isn't worth it for an admin-tuning knob.

### Adding a new CastSpeed source

If you ever need a seventh source:

1. Add an enum value to `CXPSCastSpeedSource` in `CXPS_Config.h`.
2. Add a `CASTSPEED_<NAME>_MULTIPLIER` slot to `CXPSConfig` in the same
   header.
3. Register it in `BuildConfigCache` (via the `setCastSpeedSource`
   lambda) and add the conf key + docstring to `custom_xp_scaling.conf.dist`.
4. Extend `GetCastSpeedSourceMultiplier` and `CastSpeedSourceLabel` in
   `CXPS_Config.cpp` with the new case.
5. Add a routing rule to `Classify` in `CXPS_CastSpeed.cpp`. Keep the
   classification order specific → generic, same as the existing rules.

### Boot-time logging

`CustomXPScaling.CastSpeed.LogToConsole = 1` emits one `LOG_INFO("module",
...)` line per modified spell with the original / new cast time and the
multipliers that fired. The summary count line at the end always logs
regardless. There is intentionally no per-cast log (the runtime is silent;
the only cost is the boot pass).

## Config cache

Every CustomXPScaling key the runtime reads is registered once in
`CXPSConfigData::BuildConfigCache` and accessed thereafter via cached typed
getters on the `cxpsConfig` global. **The hot path must not call
`sConfigMgr`** — there are no per-event config reads, no string allocations,
no `try`/`stof`.

The cache is repopulated on every `WORLDHOOK_ON_BEFORE_CONFIG_LOAD` (boot and
`.reload config`). When adding a new key:

1. Add a value to the `CXPSConfig` enum in `CXPS_Config.h` (right above
   `NUM_CONFIGS`).
2. Register it with the matching `SetConfigValue<T>(...)` call in
   `BuildConfigCache`. Use the helper lambdas (`AnyBool`, `NonNegFloat`,
   etc.) for the validator, and pass `Reload::Yes` unless you have a reason
   not to.
3. For a `LevelPercent` string, also add a parse line in the pre-parse pass
   at the bottom of `BuildConfigCache` and use
   `cxpsConfig.GetLevelPercent(KEY)` at the call site — not
   `CXPS::ParseLevelPercent`.
4. Read it from the runtime as `cxpsConfig.GetBool(...)` /
   `cxpsConfig.GetFloat(...)` / `cxpsConfig.GetLevelPercent(...)`.

Conventions:

- All keys live under `[worldserver]` in `custom_xp_scaling.conf.dist`.
- The dist file is the source of truth for shipped values; the in-code
  default passed to `SetConfigValue` keeps the module functional if a server
  admin forgets a key.
- `bool` keys use `1`/`0`. `float` keys are plain decimals. `LevelPercent`
  strings are the only string-shaped keys in the module.

## Player-facing logging

`LogToPlayer = 1` is enabled by default in the dist conf and is the primary
debug surface (no `LOG_*` calls). Combat/quest/explore use `LogXPDetails`;
profession ticks emit their own line via `GiveProfessionXP` with format:

```
Profession XP (<Source>, <Color>): <xp> | <pct>% of next level | base X% +/- Y% x diff D x skill S x rank R
```

ASCII punctuation only — multi-byte glyphs render unreliably in the WoW chat
frame, so don't switch back to `±`/`×`.

## Persistence (per-character preferences)

The module persists two per-character preferences via AzerothCore's
**`PlayerSetting` API** — `player->UpdatePlayerSetting(...)` to write,
`player->GetPlayerSetting(...)` to read — both under the source namespace
`"mod-cxps"`:

| Index | Constant              | Purpose                                          | Resolver           |
|-------|-----------------------|--------------------------------------------------|--------------------|
| `0`   | `CXPS_SETTING_LOG`    | Show/hide per-event XP log lines                 | `ShouldLogToPlayer`|
| `1`   | `CXPS_SETTING_ENABLE` | Opt the character in/out of XP scaling entirely  | `IsEnabledFor`     |

The core owns the lifecycle: it loads `character_settings` before
`OnPlayerLogin`, flushes on its normal save cadence, and purges rows on
character delete. That's why the module has no DB query, no in-memory cache,
no `OnPlayerLogout`, and no `OnPlayerDelete` — adding any of those would
duplicate work the core already does.

Stored values are **tri-state** because `GetPlayerSetting` returns `0` for an
unset key and that must stay distinct from an explicit "off". For both slots:
`1` = player forced off, `2` = player forced on, `0`/absent = inherit the
server-wide default (`CustomXPScaling.LogToPlayer` for log,
`CustomXPScaling.Enable` for scaling). Each slot has its own admin gate —
`CustomXPScaling.LogToPlayer.AllowPlayerToggle` and
`CustomXPScaling.AllowPlayerToggle` — that disables the command entirely
and forces the server default when off.

`IsEnabledFor(player)` is the gate every XP-awarding hook uses
(`OnPlayerGiveXP`, the three profession hooks, `OnPlayerLearnTaxiNode`,
`OnPlayerAchievementComplete`). Don't add new XP-granting code paths without
routing them through it — the previous `IsEnabled()` server-only helper is
still around for non-XP code, but hooks should always use the per-player
form so the toggle stays effective.

Persistence depends on the **core's `PlayerSettings.Enable`** option being on
(it ships off in stock AC). With it off, a toggle still works for the live
session but is never written to the DB — both `SetLogPref` and
`SetScalingPref` route through `WarnIfPlayerSettingsDisabled`, which checks
`CONFIG_PLAYER_SETTINGS_ENABLED` and warns the player. Don't reintroduce a
direct `character_settings` write to work around this; the API is the
supported path and a raw write fights the core's load/save ordering.

## Chat commands

Every command is `SEC_PLAYER`, in-game only (`Console::No`).

| Command                              | Behavior |
|--------------------------------------|----------|
| `.xpscaling on` / `.xpscaling off`   | Opt this character in/out of all XP scaling. Gated by `CustomXPScaling.AllowPlayerToggle`. |
| `.xpscaling log on` / `.xpscaling log off` | Show/hide the per-event XP log lines. Gated by `CustomXPScaling.LogToPlayer.AllowPlayerToggle`. |
| `.xpscaling help`                    | Lists the commands above. |
| `.xpscaling about`                   | Short module summary. |

The login announce ("…running the Custom XP Scaling module…") also points
players at `.xpscaling help` / `.xpscaling about`.

Two formatting notes for new command output: AC's `PSendSysMessage` is
**fmtlib-based** (`{}` placeholders, not `%s`), and chat strings must stay
ASCII — see the player-facing logging section above.

## Build / install

1. Drop the repo into `azerothcore/modules/mod-custom-xp-scaling/`.
2. Re-run CMake (`cmake ..` from `build/`); AC picks up the module via
   `include.sh` + `CXPS_loader.cpp` automatically.
3. Copy `conf/custom_xp_scaling.conf.dist` →
   `<worldserver-conf-dir>/custom_xp_scaling.conf` and edit.
4. Compile with `-DNOPCH=1` at least once before shipping — required header
   hygiene check per AzerothCore module guidelines.

## Style

- `.editorconfig`: UTF-8, **4-space soft tabs**, trim trailing whitespace,
  final newline, 80-col soft cap. The existing source uses tab-indented
  lines (legacy) — match the surrounding file when editing rather than
  reformatting wholesale.
- CI runs `apps/ci/ci-codestyle.sh` via `.github/workflows/core_codestyle.yml`
  — keep new files consistent with the rest of the module so it doesn't trip.
- Headers in use: `Chat.h`, `Config.h`, `ConfigValueCache.h`, `DBCStores.h`,
  `DBCStructure.h`, `Log.h`, `Player.h`, `Random.h`, `ScriptMgr.h`,
  `SharedDefines.h`, `SpellInfo.h`, `SpellMgr.h`, `World.h`, plus the
  module-local `CXPS_CastSpeed.h`, `CXPS_Config.h`, and
  `CXPS_LevelPercentReward.h`. Add new ones explicitly — the module is built
  with PCH on by default but CI exercises `NOPCH`.

## Gotchas

- `OnPlayerGiveXP` runs *before* the core applies XP, so mutating `amount`
  is the supported path. Don't call `player->GiveXP` from inside it — you'll
  double-count.
- `OnPlayerUpdateFishingSkill` must return `true` to keep AC's default
  fishing skill-up roll. Returning `false` suppresses it.
- The `XPSOURCE_BATTLEGROUND` case is intentionally not scaled by a source
  multiplier — only base level scaling applies. Add a `BGXP` block if a
  separate knob is needed.
- `PLAYER_NEXT_LEVEL_XP` returns 0 once a player hits the cap (80). The
  profession path guards via `xpReward == 0`; if you add new percent-of-level
  rewards elsewhere, mirror that check.
- The hook-list constructor on `PlayerScript` / `WorldScript` is load-
  bearing: if you add a hook (e.g. `OnPlayerLogout`), you must also list
  its `PLAYERHOOK_*` value in the constructor, or the core will silently
  skip your override.
- Disenchant rides on `SKILL_ENCHANTING`. The only reliable signal is the
  spell id (`13262`), so don't try to detect it via `SkillLine` alone.
  Smelting rides on `SKILL_MINING` *through the crafting hook* — the gather
  hook still sees mining as plain mining. Lockpicking comes through the
  gathering hook with `skillId == SKILL_LOCKPICKING`.
- The cache stores `LevelPercent` strings as pre-parsed `CXPS::LevelPercent`
  pairs in a parallel array on `CXPSConfigData`. Don't call
  `CXPS::ParseLevelPercent` from a hot path; it allocates and runs `stof`.
  Read from `cxpsConfig.GetLevelPercent(KEY)` instead.
- Cast-speed mutation runs ONCE on `WORLDHOOK_ON_STARTUP` and is not undone
  on `.reload config`. Editing `CustomXPScaling.CastSpeed.*` keys updates
  `cxpsConfig` in memory but the already-rebound `SpellInfo::CastTimeEntry`
  pointers stay put — a worldserver restart is required for cast-time
  changes to apply. Don't add a SpellScript or per-cast hook to "fix" this;
  the boot-time rebind is the entire performance argument for the feature
  (mod-craftspeed's original justification).
- The cast-speed pass mutates each affected `SpellInfo::CastTimeEntry` to
  point at a *freshly allocated* `SpellCastTimesEntry` we own (stored in
  `g_synthesizedCastTimes`). Do NOT edit the original DBC entry in place
  — its `CastingTimeIndex` is shared across unrelated spells, so an
  in-place edit would scale them too.
- The cast-speed classifier shares the disenchant signature
  (`spellId == 13262`) with the XP path but the two `13262` literals are
  intentionally separate. If you ever change one, change both, or hoist
  the constant into a shared header — don't add a cross-`.cpp` extern.
