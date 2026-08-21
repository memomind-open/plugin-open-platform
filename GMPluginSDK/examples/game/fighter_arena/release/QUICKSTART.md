# Fighter Arena Beta Quick Start

Version: `0.1.0-beta.21`
Plugin manifest version: `29`

## Requirements

- GM glasses with the GM Plugin Host ABI 1.0
- Windows 10 or 11
- A paired outgoing Bluetooth SPP COM port
- The bundled `GMPluginWindows.exe`

Use the EXE from this same Beta 21 package. Older Windows tools do not contain
the current multi-channel audio event handling.

## Install and play

1. Pair the glasses in Windows and note the outgoing Bluetooth SPP COM port.
2. Start `GMPluginWindows.exe`, choose that port, and connect over SPP.
3. Select `fighter_arena.gmp`, then install and start it.
4. Open **Extensions > Fighter Arena...** to show the dedicated game-control
   window.
5. Press `J` to leave the title screen. Select difficulty with `A`/`D` and
   confirm with `J` or `Enter`.
6. Use `A`/`D` to move, `W` to jump, `S` to crouch, `J` for light punch, `K`
   for heavy punch, `U` for light kick, and `I` for heavy kick.
7. Press `L` for a large 50-energy wave or `O` for a 35-energy six-hit
   tracking rush. It chases toward the opponent with speed trails, then chains
   punches, a knee, and a heavy kick into a launching double-palm finisher.
   The opening strike must connect; a very distant whiff cannot deal later hits.
   Normal target combos are `J, J, K` and `J, U, I`. Hold away to retreat;
   when an attack reaches its active range, the
   same input becomes stand-block. Combine it with `S` to crouch-block. Press
   `Esc` to pause or resume.

Blocking no longer has a separate `GD` meter or guard break. A successful block
still causes chip damage and brief block stun. Direct attack damage is roughly
half of Beta 20, so rounds last substantially longer.

The PC mixes four original looping tracks plus the CC0 `MIDI battle theme` with
23 original game effects,
including attack swings, distinct hits, blocks, special charge/launch, jump,
menu, round start, KO, draw, victory, and defeat cues. Keep GMPluginWindows
connected and focused while playing. Losing focus or the SPP connection pauses
the match and its music.
