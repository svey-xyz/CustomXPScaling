# Custom XP Scaling

An [AzerothCore](https://www.azerothcore.org/) (WotLK 3.3.5a) module that
revisits experience gain. The xp modification in worldserver.conf is nice, but can be
limiting. This module adds scoped level experience modification, per xp source scaling,
and new xp sources like professions and flight paths! Every behaviour is opt-in and
tunable, so you can run it as a light XP-rate tweak or a full progression overhaul
without touching the core. Tune existing XP sources and make new ways to level
and play more viable. 

## What it does

**Combat, quest, and exploration XP** — `OnPlayerGiveXP` composes a single
multiplier from a per-level-bracket factor (`LevelXP.Scaling.<range>`, applied
to everything) and exactly one source-specific factor (`KillXP`, `RareXP`,
`QuestXP`, or `ExploreXP`). The per-bracket factor is what reshapes the
leveling curve — prefer spending time in Elwynn Forest? Slow down the early
levels and speed-up leveling through Outlands, the choice of curve is yours! —
while the source-specific factors tune individual activities on top. Rare/elite
kills can stack an extra multiplier scaled by the creature's rank. Battleground
XP only receives the level-bracket scaling.

**Profession XP** — gathering (mining/herbalism/skinning), crafting, and
fishing each grant XP on every skill tick, turning trade skills into a viable
way to level. The award uses a **randomized level-percent scaling**: a
`LevelPercent = "<base>:<randomizer>"` config string sets a base percentage of
the XP needed for the player's next level, and every tick rolls a random
offset within `±randomizer` of that base. The rolled percentage is then scaled
by the action's difficulty color (gray / green / yellow / orange — the same
thresholds AzerothCore uses for skill-up rolls), so harder tasks pay more and
trivial (gray) tasks can be set to grant nothing.

**Achievement XP** — completing an achievement awards XP using the same
**randomized level-percent scaling** as professions — a
`LevelPercent = "<base>:<randomizer>"` base percentage of next-level XP plus a
random per-achievement offset. The achievement's point value acts as the
difficulty multiplier (the role the gray/green/yellow/orange colors play for
professions), so higher-point achievements pay proportionally more, clamped to
a configurable multiplier band.

**Taxi node XP** — discovering a new flight path grants XP, making exploration
its own form of progress. It uses the same **randomized level-percent scaling**
as professions — a `LevelPercent = "<base>:<randomizer>"` base percentage of
next-level XP plus a random per-discovery offset — but with no difficulty
tiers, so every node pays the same rolled amount.

**In-game logging** — with `LogToPlayer` on, the module sends each player a
system-chat breakdown of every XP event: the original amount, the scaled
result, and which multipliers fired. It's the module's primary debugging
surface. Players can toggle their own log on or off with a chat command (see
below).

## Installation

1. Clone or copy this repository into your AzerothCore source tree:

   ```
   azerothcore/modules/mod-custom-xp-scaling/
   ```

2. Re-run CMake from your `build/` directory — AzerothCore discovers the
   module automatically:

   ```
   cd build && cmake ..
   ```

3. Build and install as usual (`make -jN && make install`).

4. Copy the config template into your worldserver config directory and edit
   to taste:

   ```
   cp modules/mod-custom-xp-scaling/conf/custom_xp_scaling.conf.dist \
      <your-config-dir>/custom_xp_scaling.conf
   ```

5. Restart `worldserver`.

## Required core setting: `PlayerSettings.Enable`

The module lets each player toggle their own XP log with
`.xpscaling log on|off`. That preference is stored through AzerothCore's
built-in **PlayerSetting** system, which the core only persists to the
database when `PlayerSettings` is enabled.

In your **`worldserver.conf`** (the core config, *not* this module's config),
make sure:

```
PlayerSettings.Enable = 1
```

It ships **disabled** in stock AzerothCore. With it left off, a player's
`.xpscaling log` choice still takes effect for their current session but is
**not saved** — it resets to the server default on logout. The command warns
the player in chat when this is the case. Everything else in the module works
regardless of this setting; it only affects whether the per-player log toggle
survives a relog.

## In-game command

```
.xpscaling log on     Enable XP scaling log messages for yourself
.xpscaling log off    Disable them
```

Available to all players. It can be disabled server-wide by setting
`CustomXPScaling.LogToPlayer.AllowPlayerToggle = 0`, in which case the
server-wide `CustomXPScaling.LogToPlayer` default is enforced for everyone and
the command reports that it is unavailable.

## Configuration

All options live in `conf/custom_xp_scaling.conf.dist`, which is fully
commented. Highlights:

| Key | Purpose |
|-----|---------|
| `CustomXPScaling.Enable` | Master on/off switch for the module. |
| `CustomXPScaling.LevelXP.Scaling.<range>` | Per-level-bracket multiplier applied to all XP sources. |
| `CustomXPScaling.KillXP.*` / `RareXP.*` | Kill and rare/elite kill multipliers. |
| `CustomXPScaling.QuestXP.*` / `ExploreXP.*` | Quest and exploration multipliers. |
| `CustomXPScaling.ProfessionsXP.*` | Profession XP percentage, per-tick variance, and difficulty multipliers. |
| `CustomXPScaling.AchievementXP.*` | Achievement XP level-percent roll plus a points-based multiplier band. |
| `CustomXPScaling.TaxiNodeXP.*` | Flight-path discovery XP percentage. |
| `CustomXPScaling.LogToPlayer` | Server-wide default for the in-game XP log. |
| `CustomXPScaling.LogToPlayer.AllowPlayerToggle` | Whether players may override that default via the chat command. |

Each key also has an in-code default, so the module stays functional if a key
is missing from the config — but the `.conf.dist` file is the source of truth
for shipped values.

## License

Released under the GNU AGPL v3, consistent with AzerothCore. See the license
header in the source files.
