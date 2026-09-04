# Fighter Arena plugin

`fighter_arena` is a focused single-player fighting game for the GM plugin
runtime. It uses the Bluetooth and sliced GRAY_4 framebuffer paths and ships
one polished fixed original-mascot matchup for the public beta.

## Game flow

1. Title screen
2. `EASY`, `NORMAL`, or `HARD` difficulty selection
3. Fixed `ZEN` versus `RIVAL` best-of-three match
4. Win, loss, or draw result, then replay

`ZEN` is a star-crown rhythm fighter and `RIVAL` is a soft five-point
star fighter; each has an original twelve-frame sprite set. Every normal
attack now has a readable wind-up, contact and recovery sequence: jab, heavy
cross and roundhouse use separate anticipation and impact silhouettes, while
the special uses a dedicated two-hand energy-release pose. Hit stun, walking,
jumping, crouch-blocking and neutral guard also have distinct frames. Grounded
fighters retain body separation and use the real rendered sprite width for edge
spacing. An airborne fighter can pass over the opponent, land on the other
side, enter a 150 ms landing recovery, and then turn to face the opponent.
Grounded bodies maintain one full fighter width of separation.
Both fighters launch a visible energy projectile with their special and start
each round with 50 energy so it can be tested immediately.

The CPU approaches in short movement pulses instead of matching every player
step. After completing an attack it retreats for 400 ms on Easy, 300 ms on
Normal, or 200 ms on Hard, creating a punish and repositioning window. At long
range it may spend energy on a projectile instead of always walking forward.

Normal attacks are defined in one move table with damage, chip damage, startup,
active and recovery timing, hit/block stun, reach, knockback, guard damage,
energy cost and attack level. Blocking only works against attacks arriving from
the front. Holding away walks backward while there is no immediate threat and
becomes a stand-block as an attack enters its active range; combine it with `S`
to crouch-block. The low kick must be crouch-blocked, the heavy
overhead must be stand-blocked, and light or projectile attacks can be blocked
either way. Extra-thick top HUD bars show health, with a smaller energy row
below. Both tracks use bright outlines with black empty regions so only the
remaining amount is filled. Blocking still causes chip damage and block stun,
but there is no guard meter or guard-break state.

Hits use a brief simulation pause and one filled, connected impact shape that
touches the defender. Detached rays, screen shake and projectile trails remain
omitted because isolated bright pixels look like display residue on the
monochrome optical surface. Every frame clears the complete Host-provided row
stride before redrawing current objects.

## Controls

Run `PhoneSDK/examples/fighter-controller` together with this `.gmp` in Desktop
Studio. The controller provides:

- movement joystick: move, jump, crouch, and retreat/block;
- `J` / **LIGHT**: light punch and continue after a round;
- `K` / **HEAVY**: heavy punch;
- `U` / **KICK**: light kick;
- `I` / **BLOCK**: heavy kick or explicit block modifier;
- **SKILL 1**: large energy-wave projectile at 50 energy;
- **SKILL 2**: six-hit tracking rush at 35 energy;
- **START**: confirm menus and skip interstitial screens; and
- **PAUSE**: pause or resume.

Normal target combos are `J, J, K` and `J, U, I`. Hold the glasses primary
button to show a two-second exit countdown; release it to cancel.

The Fighter Controller Web plugin sends a four-byte v2 snapshot on channel `0x4647`:
`version, sequence, buttons_hi, buttons_lo`. Bit 9 is an explicit pause state.
Legacy v1 snapshots are rejected. A disconnect or 300 ms without a valid
snapshot pauses the match instead of leaving the CPU active.

Best-effort sound events are sent back to the paired Web plugin on channel `0x4648` as
`version, sequence, event, value`. Version 1 defines hit, block, guard-break,
special-launch, round-end, attack, jump, round-start, menu, KO, and music-state
events. Fighter Controller maps current events to original layered effects and
music loops. Its browser mixer keeps
music and overlapping combat effects audible together. The legacy guard-break
event remains reserved but is not emitted by the current game. Audio plays on
the phone or computer because the plugin ABI does not expose glasses-side audio.

Events enter a small non-blocking queue. Temporary Bluetooth-busy results are
retried for a bounded number of game loops, preventing ordinary attack and hit
sounds from being lost while the Web controller is also sending 50 ms input snapshots.

The release-ready encoded sheets are stored in `zen_combat_sprites.h`,
`rival_combat_sprites.h`, `zen_hurt_sprites.h`, and
`rival_hurt_sprites.h`. Source-art production intermediates are intentionally
excluded from the public SDK. Combat sheets use a fixed per-character scale in
a 112 x 98 action canvas, so wide attacks preserve fighter scale.
Rendering treats source row 95 as the ground contact and enforces a minimum
brightness across the final foot rows, preventing dark shoe pixels from fading
out or hovering above the stage line.

Damage reactions combine the four-frame reaction sheet with the previously
unused rearward-recoil combat frame. Light punches, heavy punches, kicks,
specials, guard breaks, and KO now follow distinct multi-pose timelines; a KO
remains down during the result screen. Stand guard uses a raised-arm combat pose
and no longer shares any hurt animation.

Every hit records its knockback direction independently from the fighter's
normal facing. Initial recoil, airborne knockdown, recovery, and KO frames keep
falling away from the attacker, including after either fighter changes sides.

The animation state priority is hurt, guard, attack, landing, airborne, crouch,
turn, walk, then idle. Walk frames advance on a stable timer and stop on the
same logic frame as movement.

See [`release/QUICKSTART.md`](release/QUICKSTART.md) for the paired Desktop
Studio and physical-device verification paths.
