# Level, Track and Course Formats

Doom levels, race tracks and motocross courses are plain text files you can edit in any text editor. The built-in levels in the firmware use exactly the same format, so you can use them as examples: `src/games/doom/levels.cpp`, `src/games/race/tracks.cpp` and `src/games/moto/tracks.cpp`.

## Common rules

- Copy the repository's `sd/` folder to the **root** of a FAT32 card. You should end up with `/doom`, `/race` and `/moto` on the card.
- File extensions: `.lvl` (Doom), `.trk` (race), `.mtb` (motocross). File names may be at most 31 characters long.
- Files are loaded in **file name order** after the built-in ones, which is why the bundled files are named `e1m3.lvl`, `track03.trk` and so on. Each game shows up to 16 entries and reads files up to 16 KB.
- Lines starting with `#` are comments. Empty lines are ignored. Leading and trailing spaces are trimmed.
- Everything before the first `[section]` line is the **header**: `key=value` lines. Unknown keys are ignored, and missing keys get the default value.
- `name=` is the title shown in the game. Best times and medals are stored by this name, so renaming a level resets its records.
- Progress unlocks levels in list order. If you add files with names that sort between existing ones, the unlocked count stays the same but now points at different entries.
- A file that cannot be read or parsed is skipped (Doom shows "BOLUM YUKLENEMEDI"). The built-in levels always work, with or without a card.

## Doom (`/doom/*.lvl`)

```
name=Atik Tunelleri
par=180
light=150
fog=60
fogcolor=20,10,30
flicker=1
angle=0
[map]
666666666
6@..z...6
6...D...X
666666666
```

### Header

| Key | Default | Meaning |
|---|---|---|
| `name` | file name | Level title |
| `par` | 120 | Target time in seconds, shown on the stats screen |
| `light` | 200 | Base brightness, 0–255 (lower is darker) |
| `fog` | 45 | Fog density, 0–100 (higher means the dark closes in sooner) |
| `fogcolor` | `40,0,0` | Colour that distant surfaces fade into (`r,g,b`) |
| `ceil` / `floor` | `40,28,28` / `70,40,32` | Ceiling and floor colours |
| `flicker` | 0 | `1` makes the lights flicker |
| `angle` | 0 | Player's start direction in degrees: 0 east, 90 south, 180 west, 270 north |

### `[map]` section

One character per cell, at most **32×32**. Short rows are padded with wall. The outer edge must be closed (walls, doors or the exit).

| Char | Meaning |
|---|---|
| `.` or space | Floor |
| `1`–`6` | Wall: 1 bloody brick, 2 rusty metal, 3 flesh, 4 stone with skull, 5 hell rock (lava cracks), 6 tech panel |
| `X` | Exit switch (a wall). Walk into it to finish the level. |
| `D` | Door. It opens when you walk into it and closes again after 4 s. |
| `R` `Y` `B` | Door that needs the red / yellow / blue key |
| `S` | Secret wall. It looks like the wall next to it, opens when pushed and counts towards secret %. |
| `@` | Player start (exactly one) |
| `z` | Zombie: slow, tough, melee |
| `d` | Demon: fast, zig-zags towards you, melee |
| `f` | Fire caster: floats, keeps its distance, throws fireballs |
| `k` | Boss (Kasap Lord): 1000 HP, triple fireballs and a charge, faster below half health |
| `h` | Medkit (+25 health) |
| `a` | Armour (+50, absorbs a third of the damage) |
| `m` | Bullet box (pistol +20, machine gun +40 reserve) |
| `e` | Shotgun shells (+8) |
| `s` / `g` | Shotgun / machine gun (switches to it automatically) |
| `r` `y` `b` | Red / yellow / blue key |
| `t` | Torch (decoration that lights the walls around it) |
| `c` | Corpse (decoration) |

Limits: 48 enemies, 80 items, 32 doors. Extra ones are ignored, and extra doors become walls. Enemies and items always stand in the centre of their cell.

Check a level before copying it to the card. The checker verifies the size, a closed border, a single start, and that the exit can be reached with the keys found in the level:

```bash
python3 tools/check_levels.py sd/doom/*.lvl
```

## Race (`/race/*.trk`)

```
name=Karli Gecit
laps=3
sky=day
theme=snow
skill=1.0
deco=pine,rock
density=0.55
[road]
# length curve hill
30 0 0
40 0 1500
30 -4 0
[objects]
# segment side type
50 -2.4 house
```

### Header

| Key | Default | Meaning |
|---|---|---|
| `laps` | 3 | Number of laps |
| `sky` | `day` | `day`, `dusk` or `night` (night adds street lamps automatically) |
| `theme` | `grass` | Ground colours: `grass`, `desert`, `snow`, `city` |
| `skill` | 0.9 | Rival speed multiplier. About 0.85 is easy and 1.1 is hard. Each later track in the list adds 3 %. |
| `deco` | `tree,bush` | Up to 4 roadside object types scattered at random (see the table below) |
| `density` | 0.35 | Chance per segment of a random roadside object, 0–1 |

### `[road]` section

Each line adds one section: `length curve hill`.

- `length`: number of segments (one segment is a short piece of road; 50 segments take about one second at top speed).
- `curve`: −8 to 8. Negative turns left, positive turns right, 0 is straight. Sections ease in and out of the curve on their own.
- `hill`: height change over the section (−3000 to 3000). Positive goes uphill, negative downhill.

A 12-segment straight with the start line is added in front automatically. If the hills don't add up to zero, a closing slope is added at the end so the lap joins up. Curves of 3 or more get warning signs on the outside of the bend. A whole track can have at most 1500 segments.

### `[objects]` section

`segment side type`: segment number counted from the start of the `[road]` sections, side in road widths (−1 is the left edge, `-2.5` is well off the road on the left). Up to 24 objects.

Object types: `tree`, `pine`, `palm`, `cactus`, `rock`, `bush`, `signl`, `signr`, `post`, `billboard`, `lamp`, `house`. Hitting anything except `bush` and `post` almost stops the car.

## Motocross (`/moto/*.mtb`)

```
name=Gece Kosusu
theme=night
gold=36
silver=43
bronze=54
[terrain]
# command parameters (metres)
flat 20
bumps 24 1 4
ramp 10 3
flat 14
gap 4 3
[objects]
# x type
100 checkpoint
```

### Header

| Key | Default | Meaning |
|---|---|---|
| `theme` | `forest` | `forest`, `desert`, `night`, `snow` |
| `gold` / `silver` / `bronze` | estimated from length | Medal times in seconds. Bronze or better unlocks the next course. |

The time is the clock minus 1 s for every full flip.

### `[terrain]` section

The ground is built left to right from commands. Each command continues from where the previous one ended. All values are in metres, and the screen is about 10 m wide.

| Command | Meaning |
|---|---|
| `flat L` | Flat ground of length L |
| `up L H` / `down L H` | Straight slope rising / falling H over L |
| `hill L H` / `valley L H` | Smooth bump / dip of height H |
| `bumps L H N` | N small bumps of height H spread over L |
| `ramp L H` | Concave kick ramp up to height H, then a steep drop back down |
| `gap L D` | Small kicker, then a pit L wide and D deep. Jump over it. |
| `pt DX DY` | Single point DX ahead and DY up |

40 m of flat ground is added after the last command, with the finish line 6 m into it. Lengths are capped at 200 and heights at ±20. A course can have at most 700 points.

Tips for rideable courses:

- Keep gaps 4 m wide or less.
- Put at least 14 m of flat ground before a gap.
- Don't put a checkpoint right before a gap: after a crash you restart at the checkpoint at about 6 m/s.

### `[objects]` section

`x type`: position in metres from the start. Up to 40 objects.

| Type | Meaning |
|---|---|
| `checkpoint` | Restart point after a crash, or when you press RELOAD |
| `flag`, `sign`, `tree`, `cactus`, `rock`, `tire` | Decoration only |

The repository's simulator has a test rider you can use to check that a new course can be finished. See "Simulator" in the README.
