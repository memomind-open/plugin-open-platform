# Beta Validation Status

Date: 2026-08-21

## Completed

- RISC-V build completed with `-Wall -Wextra -Werror`.
- `fighter_arena.gmp` validated at 75,920 bytes, below the 80,000-byte beta
  budget.
- Both mascot combat sheets and reaction sheets were visually inspected after
  normalization. Their requested head molds remain consistent across poses,
  detached cross-cell fragments are removed, and all hands and feet stay inside
  their 112 x 98 runtime frames.
- Fresh simulator captures of the title, neutral fight, heavy recoil, and late
  knockdown states were inspected. Both mascots remain readable against the
  stage and retain their head silhouettes at runtime scale.
- All 24 GMPluginWindows protocol and Fighter Arena application unit tests
  passed for this release scope.
- The GMPluginWindows help text was rebuilt to describe the six-hit tracking
  rush and double-palm finisher.
- Fighter Arena simulator smoke test passed.
- The nighttime city stage was visually inspected in the lifecycle simulator;
  its moon, skyline, rooftop, and foreground layers render without obscuring
  fighters, the HUD, or combat feedback.
- The six-hit tracking rush, including its attached speed trails, alternating
  punch/kick poses and double-palm finisher, both normal target combos, the enlarged
  energy wave, and a complete live-combat round were inspected in
  simulator captures; all fighters, impacts, combo text, HUD, and result text
  remained visible against the stage.
- The extra-thick HP bars, compact EN row, removed GD row, difficulty
  descriptions, brighter RIVAL, defender-centered combo text, block effects, unobscured
  round prompt, match score, and maximum-hit result were inspected in fresh
  simulator captures.
- Ordinary melee hits and both normal target combos were re-inspected after
  replacing the filled circular contact effect with a compact spark. Simulator
  assertions confirm they create no projectile objects; circular energy remains
  limited to the `L` wave.
- Two title frames 500 ms apart were inspected: both fighters use the
  front-facing stance and alternate a subtle vertical idle motion from fully initialized
  preview state.
- Left/right knockdown captures were visually inspected at initial recoil and
  later hurt-sheet frames; both fighters' heads and torsos consistently fall
  away from the hit.
- Deterministic combat tests passed 49/49 scenarios, including hurt-sheet
  source orientation, shoe-ground visibility, light/heavy kick near-hit and
  far-whiff behavior, distinct kick
  frames, 152-pixel tracking advance, six-hit rush, launching palm finisher,
  distant-rush lockout, large-wave projectile lifecycle, both normal target combos,
  temporary event
  send failure/recovery, stand guard, multi-pose recoil, side-switch landing
  recovery, attack lockout, delayed turning, corrected attack direction, full
  body separation, backward movement and threat-triggered guard on both sides,
  paced CPU pursuit, player disengagement, post-attack CPU retreat, bidirectional
  ZEN/RIVAL hit reactions, a complete live-combat round, and walk-to-idle
  transitions, repeated blocks without a guard break, half-damage move totals,
  and a complete 19-heavy-attack round.
- Hidden menu, pause, disconnect, replay, and lifecycle tests passed 24/24.
- Python sources passed bytecode compilation.
- PyInstaller produced a single-file Windows executable containing 27 generated
  WAV resources, the CC0 MP3 fight loop, its license record, and the
  multi-channel SDL mixer.
- A dummy audio-device test confirmed the CC0 MP3 loop and a generated arcade
  hit effect remained active together.
- A user-reported SPP session produced continuous jump, swing, hit, block,
  special, KO, result, menu, and music log events on the target setup.
- The packaged executable started as a responsive Windows process in a local
  smoke test.
- Release-file and ZIP SHA-256 checks passed.

## Required before public upload

- Install and run on the target glasses and record model, firmware, display
  geometry, and refresh rate.
- Complete a 30-minute SPP play session covering all three difficulties,
  pause/focus loss, disconnect/reconnect, double KO, timeout, blocking, and
  best-of-three replay.
- Perform at least 50 melee hits and 10 projectile hits while checking that the
  connected impact shape disappears completely and leaves no optical residue.
- Verify synchronized sound and event ordering over a real SPP connection.
- Launch and play from the bundle on clean Windows 10 and Windows 11 systems
  without Python installed.
