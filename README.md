# ESP32 Night Console (Gece Konsolu)

A handheld game console firmware for an ESP32 DevKit V1, a 1.44" 128x128 colour TFT and six buttons. At power-on an atmospheric menu lets you pick one of six games. The only external library is [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI). An SD card is optional and adds extra levels and tracks.

> All games, graphics and levels are original and made for this project. The Doom-style game is a raycaster written from scratch, not a port of id Software's Doom. No WAD file or original game data is used. The on-screen text is in Turkish.

## Games

| Game | Type | Content |
|---|---|---|
| **DOOM** | First-person raycaster | 5 levels (2 built-in, 3 on SD), 3 enemy types plus a boss, 3 weapons with magazines and reload, doors, coloured keys, secret rooms, stats screen |
| **YILAN** (Snake) | Arcade | Speeds up as you grow, timed bonus food, high score |
| **BLOKLAR** (block puzzle) | Arcade | 10x20 field, next piece and hold, ghost piece, line clear effect, levels that speed up |
| **FLAPPY** | Arcade | A bat in a night cave, parallax background, pillars that start moving as your score rises |
| **YARIS** (Race) | Pseudo-3D racer | 5 tracks (2 built-in, 3 on SD), curves, hills, roadside scenery, 5 rivals, laps, position, nitro, career unlocks |
| **MOTOKROS** (Motocross) | Side-view physics | 5 courses (2 built-in, 3 on SD), ramps, bumps, gaps, flips, crashes, checkpoints, gold/silver/bronze medals |

### Highlights

- **Menu:** flickering title with dripping blood, drifting fog and embers, scrolling game list with an animated icon and the best score or progress for each game, and a **Settings** page (SD status and level counts, FPS overlay toggle, save reset with confirmation).
- **Back to menu from any game:** hold **LEFT + RIGHT for 1.5 s**. A bar fills at the top of the screen, then a confirmation dialog appears.
- **Controls screen:** each game shows its controls before it starts.
- **Saves:** high scores, unlocked levels, best times and medals are stored in flash with `Preferences` (NVS) and survive power-off.
- **Doom atmosphere:** darkness with distance fog in red-black tones, flickering lights, torches that light the walls, blood-stained procedural textures, a red flash and screen shake when hurt, a heartbeat border at low health, and the camera falling to the floor when you die.
- **Doom enemies:** slow and tough zombies, fast zig-zagging demons, floating casters that keep their distance and throw fireballs, and the boss in the last level. Every enemy has walk and attack frames, flashes and shakes when hit, plays a death animation and leaves a corpse.
- **Doom weapons:** pistol, shotgun and machine gun, each with its own damage, fire rate, spread, magazine and reserve ammo, sprite and muzzle flash. An empty magazine reloads automatically (no firing during the reload animation). The HUD shows `magazine/reserve`.

## Hardware

| Part | Qty |
|---|---|
| ESP32 DevKit V1 (ESP32-WROOM-32, no PSRAM) | 1 |
| 1.44" 128x128 SPI TFT display (ST7735) | 1 |
| Tactile button | 6 |
| microSD card module with its own 3.3 V regulator (optional) | 1 |
| microSD card, FAT32 (optional) | 1 |
| Breadboard and jumper wires | — |

There is no sound hardware.

## Wiring Diagram

![Wiring diagram](docs/wiring.svg)

### Display (HSPI)

| Display pin | ESP32 |
|---|---|
| SDA (MOSI) | D13 |
| SCK / SCL | D14 |
| CS | D15 |
| A0 / DC | D27 |
| RESET | D26 |
| VCC | 3V3 |
| LED / BL | 3V3 |
| GND | GND |

> The display is labelled SDA/SCL, but it is an **SPI** display. Do not connect it to the I2C pins. The display wiring is the same as in the original single-game version. The firmware now drives it on the HSPI bus (`USE_HSPI_PORT`) so the VSPI bus is free for the SD card.

### Buttons

Connect one leg of each button to an ESP32 pin and the other leg to GND. The internal pull-ups are used, so no resistors are needed.

| Button | ESP32 |
|---|---|
| FIRE (ATEŞ) | D19 |
| FORWARD (İLERİ) | D4 |
| LEFT (SOLA) | D32 |
| RIGHT (SAĞA) | D25 |
| BACK (GERİ) | D33 |
| RELOAD | D22 |

> On 4-leg buttons the legs are connected internally in pairs. Wire **diagonally opposite** legs, otherwise the button reads as permanently pressed.

Every game can be played with the first five buttons. RELOAD is an extra key: manual reload and weapon switching in Doom, and hold, pause or checkpoint in the other games.

### SD card module (optional, VSPI)

| SD module pin | ESP32 |
|---|---|
| SCK | D18 |
| MOSI | D23 |
| MISO | D21 |
| CS | D5 |
| VCC | VIN (5V) |
| GND | GND |

Pins in use: D4, D5, D13, D14, D15, D18, D19, D21, D22, D23, D25, D26, D27, D32, D33. Leave **D12** (a boot strapping pin; it broke flashing before) and **GPIO6–11** (internal flash) unconnected.

## SD Card

The card is optional. Without a card, or if it cannot be read, every big game still runs with its two built-in levels.

1. Format the card as **FAT32**.
2. Copy the contents of the repository's `sd/` folder to the **root** of the card, so it looks like this:

   ```
   /doom/e1m3.lvl  e1m4.lvl  e1m5.lvl
   /race/track03.trk  track04.trk  track05.trk
   /moto/track03.mtb  track04.mtb  track05.mtb
   ```

3. Insert the card and power on. **Settings** shows the card status and how many files were found per game. Pick the SD card row to rescan the card without rebooting.

Level files are plain text and easy to edit by hand: see **[docs/LEVEL_FORMAT.md](docs/LEVEL_FORMAT.md)** for the Doom map, race track and motocross course formats.

## Setup

[PlatformIO](https://platformio.org/) is required. On Debian/Ubuntu:

```bash
# PlatformIO
sudo apt install python3-venv
curl -fsSL -o get-platformio.py https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py
python3 get-platformio.py
echo 'export PATH="$PATH:$HOME/.platformio/penv/bin"' >> ~/.bashrc && source ~/.bashrc

# Serial port permission (log out and back in afterwards)
sudo usermod -aG dialout $USER
```

Build and upload:

```bash
git clone https://github.com/FredJones35/ESP32-Night-Console.git
cd ESP32-Night-Console
pio run -t upload        # or ./flash-game.sh
pio device monitor       # FPS and free heap, printed once per second
```

All TFT_eSPI settings are in the `build_flags` of `platformio.ini`, so there is no need to edit the library's `User_Setup.h`. The firmware uses the `huge_app.csv` partition table (3 MB app).

The serial monitor prints one line per second in this format:

```
FPS <frames> | heap <free bytes> (min <lowest free>, en buyuk blok <largest block>) | <game>
```

## Controls

Every game starts with a controls screen. In all games: **hold LEFT + RIGHT for 1.5 s** → confirm with FIRE → back to the menu.

**Menu:** FORWARD/BACK move, FIRE selects, RELOAD goes back (in Settings).

### DOOM

| Button | Action |
|---|---|
| FORWARD / BACK | Walk forward / backward; walking into a door or the exit switch opens it |
| LEFT / RIGHT | Turn |
| FIRE | Shoot (hold for automatic fire) |
| RELOAD | Manual reload |
| RELOAD twice within 0.3 s | Cancel the reload and switch weapon (pistol → shotgun → machine gun → pistol) |

A weapon you pick up for the first time is selected automatically. When a weapon is completely out of ammo, the game switches to one that still has ammo. Locked doors need the matching red, yellow or blue key. Walls that look odd may be secret rooms: walk into them.

### YILAN (Snake)

| Button | Action |
|---|---|
| FORWARD / BACK / LEFT / RIGHT | Up / down / left / right |
| FIRE (or RELOAD) | Pause |

### BLOKLAR (block puzzle)

| Button | Action |
|---|---|
| LEFT / RIGHT | Move (hold to repeat) |
| FIRE | Rotate |
| BACK | Soft drop |
| FORWARD | Hard drop |
| RELOAD | Hold piece (optional) |

### FLAPPY

| Button | Action |
|---|---|
| FIRE or FORWARD | Flap |
| BACK (or RELOAD) | Pause |

### YARIS (Race)

| Button | Action |
|---|---|
| FORWARD | Throttle |
| BACK | Brake |
| LEFT / RIGHT | Steer |
| FIRE | Nitro (3 per race) |
| RELOAD | Pause |

Finish in the top 3 to unlock the next track. Leaving the road slows you down, hitting roadside objects almost stops you, and running into a rival drops you to its speed.

### MOTOKROS (Motocross)

| Button | Action |
|---|---|
| FORWARD | Throttle |
| BACK | Brake / reverse |
| LEFT | Lean back (nose up, backflip in the air) |
| RIGHT | Lean forward (nose down, front flip in the air) |
| FIRE | Turbo (3 per run) |
| RELOAD | Return to the last checkpoint (optional) |

Land on both wheels. If your head or the frame hits the ground you crash and restart at the last checkpoint. Every full flip takes 1 s off your time. Bronze or better unlocks the next course.

## Troubleshooting

| Symptom | Fix |
|---|---|
| Screen is completely white | Wiring error. Check SDA→D13 and SCK→D14. If `FPS` lines appear in the serial monitor, the code is running. |
| Image is shifted by 32 pixels or has a snowy strip at the top | Change the driver in `platformio.ini`: `ST7735_GREENTAB3` → `GREENTAB128` / `GREENTAB2` / `BLACKTAB` |
| 1–2 pixel coloured line at the edge | Try `ST7735_GREENTAB2` or `ST7735_BLACKTAB` |
| Red and blue are swapped | Enable the `-D TFT_RGB_ORDER=TFT_BGR` line |
| Image is garbled or snowy | Set `SPI_FREQUENCY` to `20000000` or `10000000` |
| FIRE fires continuously without being pressed | Button legs are wired wrong; use diagonally opposite legs. Also make sure `TFT_MISO=-1` is still in `platformio.ini`. |
| Settings shows "SD kart: YOK" | Check the SD wiring (MISO is D21, CS is D5), power the module from VIN (5V), format the card as FAT32 (exFAT cards formatted by a phone are not supported). Pick the SD row in Settings to rescan. |
| Serial log shows `f_mount failed: (1) A hard error occurred in the low level disk I/O layer` | The card answers but sector reads fail at high SPI clock. Found on the test hardware at 20 MHz; the firmware now mounts at 4 MHz (`src/core/sd.cpp`). If it still fails, shorten the SD jumper wires. |
| Serial log shows `open(): /sd/doom does not exist` | The card is mounted but the `doom`/`race`/`moto` folders are missing. Copy the contents of `sd/` to the card root. |
| An SD level is missing from the list | File name longer than 31 characters, wrong extension or folder, or the file is larger than 16 KB. Check the serial log. |
| Upload fails with `Unable to verify flash chip connection` | Something is connected to D12 or to the CMD/SD0–SD3/CLK pins. Leave those pins free. |
| Serial monitor shows only garbage after the USB cable was re-plugged or after an upload | The game keeps running; only the log is unreadable. Press EN (reset) on the board, or reopen the port with a reset, and the log is clean again. |
| Backflips in MOTOKROS never show "TAKLA!" | Fixed in the firmware: rotation is now measured from both wheels leaving the ground until both touch again, so backflips landed on the rear wheel count. Update the firmware. |

## Measured Performance

Measured on the real hardware (ESP32 DevKit V1, no PSRAM, ST7735 at 27 MHz SPI, SD card inserted) from the serial log while playing. Full results are in [TEST_REPORT.md](TEST_REPORT.md).

| Game | Average FPS | Lowest FPS | Free heap in game | Largest block |
|---|---|---|---|---|
| DOOM (E1M1–E1M5, incl. boss fight and red damage flash) | 66 | 39 | 245 856 | 110 580 |
| YARIS (Race) | 75 | 57 | 266 508 | 110 580 |
| MOTOKROS | 65 | 59 | 273 832 | 110 580 |
| YILAN (Snake) | 90 | 84 | 279 700 | 110 580 |
| BLOKLAR | 89 | 84 | 280 692 | 110 580 |
| FLAPPY | 88 | 83 | 280 748 | 110 580 |
| Menu | 82–87 | — | 281 256 (308 736 without SD card) | 110 580 |

Free heap returns to the same value after leaving every game, no matter how often a game is entered.

## Project Structure

```
ESP32-Night-Console/
├── platformio.ini            # board, partition table, display and SPI settings
├── flash-game.sh             # build + upload shortcut
├── src/
│   ├── main.cpp              # setup()/loop() → app
│   ├── core/                 # shared code
│   │   ├── app.cpp           # menu → controls → game loop, return-to-menu dialog, FPS/heap log
│   │   ├── game.h            # common Game interface
│   │   ├── display.*         # TFT_eSPI + the shared 128x128 framebuffer sprite (32 KB)
│   │   ├── gfx.*, font5x7.h  # drawing and text helpers
│   │   ├── input.*           # debounce, down/pressed/released, hold time
│   │   ├── save.*            # Preferences (NVS)
│   │   ├── sd.*              # optional SD card on VSPI
│   │   ├── parse.*, levels.* # text level reader, built-in + SD level lists
│   │   └── ui.*              # panels, lists, dialogs, controls screen
│   ├── menu/                 # splash screen, game list, settings
│   └── games/
│       ├── registry.cpp      # list of games shown in the menu
│       ├── doom/  snake/  tetris/  flappy/  race/  moto/
├── sd/                       # copy this folder's contents to the SD card root
├── docs/
│   ├── wiring.svg            # wiring diagram
│   └── LEVEL_FORMAT.md       # level / track / course formats
└── tools/
    ├── check_levels.py       # Doom map checker
    └── sim/                  # desktop simulator (renders frames to images)
```

## How It Works

- **One firmware, one loop:** every game implements the same interface (`name`, `icon`, `scoreText`, `controls`, `enter`, `update`, `render`, `exit`) inside its own namespace. A game allocates its memory with `calloc` in `enter` and frees it in `exit`, so only one game's data is ever in RAM. The largest is Doom at about 35 KB (textures, map, enemies), then the race at about 15 KB and motocross at about 7 KB.
- **Display:** every game draws into one shared 128x128 RGB565 `TFT_eSprite` framebuffer (32 KB), which is pushed to the display in a single SPI transfer per frame.
- **Memory:** sprites, levels, fonts and other large tables are `const` and stay in flash. No large arrays live on the stack.
- **Doom:** DDA raycasting per screen column with rising doors (rays continue past half-open doors), distance light, fog and torch light per column, and z-buffered billboard sprites drawn from ASCII art stored in flash. Enemies find their way with a breadth-first search over the map grid.
- **Race:** segment-based pseudo-3D road. Each segment has a curve and a height, is projected from the camera, and is drawn row by row from near to far. Roadside objects and cars are clipped behind hill crests.
- **Motocross:** a rigid-body bike on two sprung wheels over a height-field terrain, integrated in four sub-steps per frame. The drive force is limited by tyre grip, and leaning adds torque.

## Simulator

`tools/sim` builds the core, the menu and all the games for the desktop with stubbed hardware. It is useful for checking screens and levels without a board:

```bash
tools/sim/build.sh
# Buttons: F=FIRE U=FORWARD L=LEFT R=RIGHT D=BACK X=RELOAD, '-' none; *N = hold for N frames (30 fps)
tools/sim/sim "-*20 F*2 -*3 F*2 -*3 shot:controls F*2 -*30 shot:doom"
python3 tools/sim/montage.py out.png controls doom     # PPM screenshots → one PNG
```

Environment variables: `SIM_SD=<folder>` (SD root, default `sd`), `SIM_NOSD=1` (no card), `SIM_SAVE=dm_unl=5,snk_hi=100` (preload saves), `SIM_BOT=moto` (a test rider for checking that a new course can be finished), `SIM_QUIET=1`.
