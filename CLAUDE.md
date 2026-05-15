# CustomXPScaling — module guide for Claude

AzerothCore (WotLK) module that rewrites XP gains across every source the
core fires hooks for: combat, quests, exploration, professions, and
achievements. All behavior is gated by `worldserver.conf` options under the
`CustomXPScaling.*` namespace.

## Repo layout

```
src/
  CXPS_loader.cpp              Module entry point (Addmod_custom_xp_scaling_Scripts).
  CustomXPScaling.cpp          Single PlayerScript implementing every hook.
  CXPS_LevelPercentReward.*    Shared "<base>:<randomizer>" parsing and the
                               percent-of-next-level roll (.h + .cpp), used by
                               professions, taxi nodes, and achievements.
conf/
  custom_xp_scaling.conf.dist  Documented config; AC copies this to .conf.
apps/ci/                 Codestyle script consumed by the GitHub workflow.
.github/workflows/       core-build + core_codestyle CI (mirror AC's checks).
include.sh               Empty stub; AC's module loader sources it.
```

Only one PlayerScript class (`CustomXPScaling`) — every override lives there.
`AddCustomXPScalingScripts()` is called from `Addmod_custom_xp_scaling_Scripts`
in `CXPS_loader.cpp`, which is the symbol AC's module system invokes at boot.

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

## Profession XP (the recent rework)

The whole profession path was rebuilt to (a) make the percent-of-level
semantics explicit, (b) reward harder tasks more, and (c) add deterministic
variance per tick.

### Difficulty classification

`enum ProfessionDifficulty { GRAY, GREEN, YELLOW, ORANGE }` plus a single
helper `ClassifyDifficulty(currentLevel, yellow, green, gray)` that mirrors
AC's own skill-up thresholds from `PlayerUpdates.cpp`:

- `currentLevel >= gray` → Gray (trivial).
- `currentLevel >= green` → Green.
- `currentLevel >= yellow` → Yellow.
- otherwise → Orange (hardest).

Per-source threshold sourcing:

- **Gathering** — the hook already passes `gray/green/yellow` as skill values
  (AC sets them to `RedLevel+100 / +50 / +25`), so we pass them straight in.
- **Crafting** — `SkillLineAbilityEntry::TrivialSkillLineRankHigh` is gray,
  `TrivialSkillLineRankLow` is yellow, green is the midpoint. Same formula
  AC uses for craft skill-up odds.
- **Fishing** — no DBC trivial fields exist, so we derive from
  `delta = skill - zone_skill`: ≥100 gray, ≥50 green, ≥0 yellow, else orange.
  Keep this approximation in mind when tuning fishing — it's heuristic,
  not data-driven.

### Reward formula

The roll math is **shared** — it lives in `CXPS_LevelPercentReward.{h,cpp}`
(`namespace CXPS`) and is used identically by professions, taxi nodes, and
achievements. Three free functions:

```
CXPS::ParseLevelPercent(key, defBase, defRand)  → LevelPercent{ base, randomizer }
CXPS::RollEffectivePercent(levelPercent, mult)  → max(0, (base + frand(-rand,+rand)) * mult)
CXPS::LevelPercentToXP(player, effectivePct)    → round(PLAYER_NEXT_LEVEL_XP * effectivePct / 100)
```

Each caller supplies its own `mult` (profession Difficulty.*, achievement
points multiplier, or a flat `1.0` for taxi nodes) and its own logging.

Both `base` and `randomizer` are **absolute percent points** of next-level XP,
parsed from a single `LevelPercent = "<base>:<rand>"` string. With the
profession defaults (`"1.0:0.25"` and `Difficulty.Yellow = 1.0`) a Yellow tick
lands between **0.75% and 1.25%** of next-level XP; Orange (mult 1.5) lands
between **1.125% and 1.875%**; Gray (mult 0.0) returns early and grants
nothing. Every caller short-circuits on `xpReward == 0` (also the level-cap
case, where `PLAYER_NEXT_LEVEL_XP` is 0) so we never feed zero into the core.

### Config keys (profession section)

```
CustomXPScaling.ProfessionsXP.Enable           = 1
CustomXPScaling.ProfessionsXP.LevelPercent     = "1.0:0.25"
CustomXPScaling.ProfessionsXP.Difficulty.Gray   = 0.0
CustomXPScaling.ProfessionsXP.Difficulty.Green  = 0.5
CustomXPScaling.ProfessionsXP.Difficulty.Yellow = 1.0
CustomXPScaling.ProfessionsXP.Difficulty.Orange = 1.5
```

`LevelPercent` is parsed by `CXPS::ParseLevelPercent`. Malformed strings or
non-numeric halves silently fall back to the caller's defaults; negative
randomizers are clamped to `0`.

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

## Config conventions

- All keys live under `[worldserver]` in `custom_xp_scaling.conf.dist`.
- Code defaults are duplicated alongside `sConfigMgr->GetOption<...>` calls;
  the dist file is the source of truth for shipped values, but the in-code
  default keeps the module functional if a server admin forgets a key.
- `bool` keys use `1`/`0`. `float` keys are plain decimals. `LevelPercent`
  is the only string-shaped key in the module.

## Player-facing logging

`LogToPlayer = 1` is enabled by default in the dist conf and is the primary
debug surface (no `LOG_*` calls). Combat/quest/explore use `LogXPDetails`;
profession ticks emit their own line via `GiveProfessionXP` with format:

```
Profession XP (<Source>, <Color>): <xp> | <pct>% of next level | base X% +/- Y% x Z
```

ASCII punctuation only — multi-byte glyphs render unreliably in the WoW chat
frame, so don't switch back to `±`/`×`.

## Persistence (per-character log preference)

The only state the module persists is each player's `.xpscaling log on/off`
choice, handled by `CXPS_CommandScript`. It goes through AzerothCore's
**`PlayerSetting` API** — `player->UpdatePlayerSetting(...)` to write,
`player->GetPlayerSetting(...)` to read — under the source namespace
`"mod-cxps"`, index `CXPS_SETTING_LOG` (0). The core owns the lifecycle: it
loads `character_settings` before `OnPlayerLogin`, flushes on its normal save
cadence, and purges the row on character delete. That's why the module has no
DB query, no in-memory cache, no `OnPlayerLogout`, and no `OnPlayerDelete` —
adding any of those would duplicate work the core already does.

The stored value is **tri-state**, because `GetPlayerSetting` returns `0` for
an unset key and that must stay distinct from an explicit "off": `1` = player
forced off, `2` = player forced on, `0`/absent = inherit the server-wide
`CustomXPScaling.LogToPlayer` default. `ShouldLogToPlayer` resolves this.

Persistence depends on the **core's `PlayerSettings.Enable`** option being on
(it ships off in stock AC). With it off, a toggle still works for the live
session but is never written to the DB — `SetLogPref` detects this via
`CONFIG_PLAYER_SETTINGS_ENABLED` and warns the player. Don't reintroduce a
direct `character_settings` write to work around this; the API is the
supported path and a raw write fights the core's load/save ordering.

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
- Headers in use: `Chat.h`, `Config.h`, `DBCStores.h`, `Player.h`, `Random.h`,
  `ScriptMgr.h`, `World.h`, plus the module-local `CXPS_LevelPercentReward.h`.
  Add new ones explicitly — the module is built with PCH on by default but CI
  exercises `NOPCH`.

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
