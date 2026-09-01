# Beta Validation Status

Date: 2026-08-21

## Completed

- The RISC-V build completed with `-Wall -Wextra -Werror`.
- `fighter_arena.gmp` validated at 75,920 bytes, below the 80,000-byte beta
  budget.
- Both mascot combat and reaction sheets were visually inspected after
  normalization. Their head molds remain consistent across poses, detached
  cross-cell fragments are removed, and hands and feet stay inside the
  112 x 98 runtime frames.
- Simulator captures of the title, neutral fight, heavy recoil, knockdown,
  stage, HUD, effects, result, and lifecycle states were inspected.
- Deterministic combat tests passed 49/49 scenarios, including movement,
  blocking, damage, combos, skills, hit reactions, side switching, CPU pacing,
  event recovery, and complete-round behavior.
- Hidden-menu, pause, disconnect, replay, and lifecycle tests passed 24/24.
- The paired Web controller protocol and audio event path passed automated
  checks and Desktop Studio integration smoke testing.
- Release-file and ZIP SHA-256 checks passed.

## Required before public upload

- Install and run on the target glasses and record model, firmware, display
  geometry, and refresh rate.
- Complete a 30-minute play session covering all three difficulties,
  pause/focus loss, disconnect/reconnect, double KO, timeout, blocking, and
  best-of-three replay.
- Perform at least 50 melee hits and 10 projectile hits while checking that
  connected impact shapes disappear completely and leave no optical residue.
- Verify synchronized sound and event ordering between the Fighter Controller
  Web plugin and the glasses over a real App connection.
- Launch and play the published package pair through a clean supported App and
  through each officially distributed Desktop Studio platform build.
