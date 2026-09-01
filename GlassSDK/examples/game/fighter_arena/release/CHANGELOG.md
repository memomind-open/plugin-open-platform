# Changelog

## 0.1.0-beta.22

- Corrected per-frame hurt-sheet orientation so each fighter's head and torso
  fall away from the attacker throughout recoil, knockdown, and KO animations.
- Replaced both human fighter identities with original music-and-cosmos mascot
  characters while keeping the existing 12 combat poses, four reaction poses,
  collision spacing, and 112 x 98 runtime frame format.
- Preserved the user-requested soft star head molds as the primary silhouette
  feature while differentiating faces, clothing, footwear, accessories, and
  props from the physical references.
- Extended the asset builder to regenerate either fighter, preserve real alpha,
  remove baked transparency previews, and filter detached cross-cell fragments.
- Regenerated the review PNGs and all runtime sprite headers. The packaged game
  remains below the 80,000-byte beta budget.

## 0.1.0-beta.21

- Removed the visible `GD` row and all guard depletion, recovery and guard-break
  behavior. Stand and crouch blocking remain, with chip damage and block stun.
- Increased health-bar height by 50 percent and rearranged the compact HUD to
  use the space freed by the guard row.
- Reduced every direct move's damage to approximately half, rounding small
  integer hits to useful values. The six-hit `O` rush now deals 12 total damage.
- Extended a deterministic full round from roughly 9-10 heavy attacks to 19 in
  the current scenario.
- Replaced the fight loop with tapatilorenzo's CC0 `MIDI battle theme` from
  OpenGameArt and recorded its URL, license and downloaded-file hash.
- Rebuilt original swings, hits and blocks with shorter envelopes, harder
  square-wave transients and brighter arcade-style cracks.

## 0.1.0-beta.20

- Replaced the difficult-to-read ground grab with an original classic tracking
  rush: high-speed approach, alternating punches, knee and heavy kick, then a
  launching double-palm finisher. Cost and total damage remain 35 energy and
  24 damage across six hits.
- Made the opening rush visibly chase for 200 ms and up to roughly 160 logical
  pixels, with three short speed trails attached behind the attacker.
- Kept connection-dependent follow-ups: a 400-pixel test whiffs cleanly with no
  later damage, while a 220-pixel test advances 152 pixels and connects.
- Removed the dedicated grab and ground-pound frames from the runtime package,
  restoring the ordinary combat poses and freeing package space.
- Retained Beta 19 guard pressure: three blocked heavy punches break full guard,
  with 900 ms break stun and slow delayed recovery.

## 0.1.0-beta.19

- Added dedicated original grapple-entry and kneeling ground-pound frames for
  both fighters instead of assembling the `O` sequence from ordinary punches.
- Corrected the captured fighter's ground orientation, depth order, downward
  offset, and binding distance so the head faces the attacker, the body stays
  grounded, and the attacker remains visibly on top during the barrage.
- Kept `O` free of projectiles and detached energy shapes; connected ground
  hits now retain a stronger local contact spark.
- Made guard pressure consequential: three blocked heavy punches break a full
  guard, guard-break stun is 900 ms, recovery waits 1.6 seconds and restores
  5 points per second, and critical guard flashes at 25 or below.
- Expanded deterministic combat coverage to 45 scenarios with a full-guard
  three-heavy pressure test. Complete combat and lifecycle tests pass.

## 0.1.0-beta.18

- Replaced the `O` four-stage energy rush with an original 35-energy six-hit
  close-range finisher: forward grab, slam, three ground strikes, and a
  launching final blow. The sequence deals 24 total damage.
- Made the grab unblockable but connection-dependent. A missed opening grab
  cannot deal any later sequence damage or create a projectile.
- Removed all circular energy rendering from `O`; only the `L` projectile keeps
  the layered wave visual.
- Bound the grabbed fighter in front of the attacker, forced a grounded hurt
  pose during the barrage, moved contact sparks to floor height, then released
  the target for the finisher launch.
- Returned title fighters to their front-facing idle frames while keeping the
  subtle alternating vertical breathing motion and explicit preview-state
  initialization.
- Expanded deterministic combat coverage to 44 scenarios with grab-connect,
  slam, ground-barrage, finisher, and whiff-without-follow-up checks.

## 0.1.0-beta.17

- Replaced the filled circular feedback on ordinary melee hits with a compact,
  connected contact spark. Circular energy visuals are now reserved for the
  `L` energy wave and the `O` four-stage energy rush.
- Fully initialized title-screen fighter previews and moved them to the more
  upright walk stance with a subtle alternating vertical idle motion.
- Added simulator assertions that light attacks, the four-stage rush, and both
  normal target combos do not create projectile objects, plus a second title
  screenshot for animation review.

## 0.1.0-beta.16

- Labeled the monochrome HUD meters as HP, EN, and GD, and increased RIVAL's
  display shade so both fighters remain readable against the night stage.
- Added distinct connected hit, block, and guard-break shapes plus an explicit
  `GUARD BREAK` callout.
- Moved combo feedback toward the defender and placed round prompts on a black
  central panel so fighters cannot obscure the next action.
- Expanded the difficulty and final-result screens with difficulty behavior,
  match score, and maximum-hit information.
- Synchronized the Windows control guide with the Beta 15 keys, buffered
  target combos, large energy wave, and four-hit rush.

## 0.1.0-beta.15

- Enlarged `L` into a layered, pulsing energy wave with a connected body,
  brighter leading core, larger collision width, and a launch point in front
  of the fighter's hands.
- Upgraded `O` to a 35-energy four-hit rush dealing 24 total damage, with four
  distinct attack poses, staged lunges, energy effects, impact sounds, and a
  launching finisher.
- Added normal-attack cancel buffering for `J, J, K` and `J, U, I`; the latter
  includes a short advancing step so its close-range kick connects reliably.
- Kept all projectile and combo effects connected and fully redrawn each frame
  to avoid detached residue-like pixels on the optical display.
- Added production-simulator checks and screenshots for the large wave, the
  four-hit skill, and both normal target combos.

## 0.1.0-beta.14

- Decoupled hit-reaction direction from the fighter's normal facing direction.
- Locked recoil direction at contact so light/heavy hurt, knockdown, guard
  break, and KO animations consistently fall away from the attacker.
- Normalized the opposite source orientations of the ZEN and RIVAL reaction
  sheets during rendering instead of assuming both sheets face the same way.
- Preserved the locked reaction direction across the initial combat recoil and
  the later multi-frame hurt sheet, including after side switching.
- Added simulator checks and visual captures for both fighters being struck
  from the left and right.

## 0.1.0-beta.13

- Replaced the CPU's continuous frame-by-frame pursuit with a paced approach,
  allowing the faster player walk to open distance by retreating.
- Added a post-attack retreat window of 400 ms on Easy, 300 ms on Normal, and
  200 ms on Hard so the CPU creates space instead of immediately sticking to
  the player again.
- Allowed the CPU to launch its projectile from long range rather than walking
  into point-blank distance before considering the move.
- Preserved reaction-delayed defense and difficulty-specific attack timing; the
  CPU still does not read and counter the current input instantly.
- Added simulator checks for paced pursuit, successful player disengagement,
  and automatic CPU retreat after a completed attack.

## 0.1.0-beta.12

- Restored backward movement: holding away now retreats while there is no
  immediate attack or projectile threat.
- Changed back-to-block so it engages only as an incoming hit enters active
  range, including after the fighters switch sides.
- Replaced the `O` rising strike with a 25-energy three-hit rush combo using
  distinct punch, heavy strike, and launching finisher phases.
- Added separate damage, reach, knockback, hit reaction, impact, and sound
  events for all three combo hits; the full sequence deals 17 damage.
- Limited combo hit-energy gain to 11 total, so a landed 25-energy combo cannot
  refund its entire cost or become self-sustaining.
- Added simulator coverage for retreating on both sides, threat-triggered
  blocking, every combo phase, and a complete round played through live combat.

## 0.1.0-beta.11

- Replaced the sparse training-stage backdrop with an original nighttime city
  scene drawn directly by the plugin.
- Added a cratered moon, layered building silhouettes, lit windows, rooftop
  details, and foreground pavement geometry without using external artwork.
- Kept the background dark and low-detail around the fighters so combat poses,
  projectiles, impact feedback, and the HUD remain readable.
- Made no changes to Beta 10 combat balance or controls.

## 0.1.0-beta.10

- Rebalanced light kick to 6 damage, 38 reach, 100 ms startup, and a 50 ms
  active window; it now uses a compact knee/short-kick pose and whiffs outside
  close range.
- Rebalanced heavy kick to 10 damage, 44 reach, 200 ms startup, and a 50 ms
  active window; only it uses the fully extended kick and knockdown reaction.
- Reduced CPU kick selection frequency so punches remain the primary neutral
  attacks.
- Reworked `O` from a grounded pseudo-uppercut into a 25-energy rising strike:
  the attacker now rises, changes to an airborne pose, and launches the target.
- Kept `L` as a dedicated 50-energy projectile with separately verified charge,
  launch, travel, hit, and energy behavior.
- Added explicit near-hit and far-whiff simulator cases plus visual-frame and
  vertical-motion assertions for both kicks and both skills.

## 0.1.0-beta.9

- Anchored all combat and hurt sheets to source row 95 and raised the minimum
  brightness of the final eight foot rows, keeping shoes visible and grounded.
- Adopted a six-button layout: `J/K` light/heavy punch, `U/I` light/heavy kick,
  and `L/O` projectile/rising-uppercut skills.
- Replaced the dedicated guard button with facing-aware back-to-block and
  down-back crouch guard, including correct behavior after side switching.
- Added direct 50-energy projectile and 25-energy rising-uppercut skills, plus
  distinct uppercut and heavy-kick movement/impact audio.
- Replaced gray-filled HUD tracks with bright outlines and black empty regions;
  remaining health, energy, and guard are the only filled areas.
- Retained v1 Pixel Fighter key encoding while extending the v2 Fighter Arena
  input mask for the complete six-button layout.

## 0.1.0-beta.8

- Added explicit moving, turning, airborne, landing-recovery, crouching,
  blocking, attacking, hurt, and idle state priorities.
- Added 150 ms landing recovery so jumping cannot lead directly into an attack.
- Added a short turn state after side switching; attacks, specials, and front
  guard use the corrected direction after the turn completes.
- Reworked walking animation timing and restored idle immediately when movement
  stops instead of leaving fighters frozen on a walk frame.
- Increased grounded body separation to a full fighter width.
- Added inset hurt-box visualization plus pose, hurt, landing, turning, and
  facing diagnostics to the Windows simulator overlay.

## 0.1.0-beta.7

- Queued fight events and retried temporary Bluetooth-send failures without
  blocking the game loop, fixing ordinary attack/hit sounds disappearing while
  frequent input snapshots are active.
- Added received sound/music entries to the Windows log for device diagnosis.
- Corrected stand guard to use a real raised-arm combat pose instead of the
  unused rearward hurt frame.
- Added separate kick hurt state and expanded light, heavy, kick, special,
  guard-break, and KO reaction timelines across five available poses.
- Allowed airborne pass-through, side switching, dynamic facing, and movement
  to the right of the opponent while preserving grounded body separation.

## 0.1.0-beta.6

- Added original looping title, difficulty-select, battle, victory, and defeat
  background tracks.
- Added screen-driven music events so the soundtrack follows the game flow.
- Replaced single-channel Windows playback with a 16-channel mixer, allowing
  music, attack movement, impacts, and result cues to overlap naturally.
- Paused and resumed background music with remote pause, focus loss, and SPP
  disconnect state.

## 0.1.0-beta.5

- Increased health-bar height and made the energy and guard meters four times
  the UI scale, with labels and round markers moved clear of the larger HUD.
- Expanded the event channel with attack, jump, round-start, menu, and KO cues.
- Rebuilt the original sound set at 44.1 kHz with 20 layered effects covering
  swings, distinct impacts, blocking, special charge/launch, navigation, round
  flow, KO, draw, victory, and defeat.
- Delayed round-result audio briefly so it does not cut off the KO impact.

## 0.1.0-beta.4

- Distinguished remote pause, lost input, and disconnected states on screen.
- Replaced title portraits with the real fighter sprites and rendered the full
  stage, HUD, and both fighters during the round intro.
- Added `J CONTINUE` to round results and `J REPLAY` to champion/game-over
  screens.
- Cleared connected impact feedback when a round ends.
- Added numeric pause-reason diagnostics to the Windows simulator overlay.

## 0.1.0-beta.3

- Added four original damage frames for each character: light stagger, heavy
  recoil, knockdown fall, and one-knee recovery.
- Added separate light, heavy, special knockdown, guard-break, and KO reaction
  timelines instead of displaying one shared hurt pose.
- Continued KO animation during the round-result screen and kept defeated
  fighters down.

## 0.1.0-beta.2

- Rebuilt both combat sheets with one fixed scale per character and a wider
  transparent action canvas, eliminating pose-dependent size pulsing.
- Added torso anchoring so extended punches and kicks do not drag the fighter's
  body toward the center of each frame.
- Changed the legacy desktop companion control help to Chinese.

## 0.1.0-beta.1

- Added title, difficulty selection, best-of-three results, and replay flow.
- Added reaction-delayed Easy, Normal, and Hard CPU profiles.
- Added high, low, overhead, and mid guard rules with guard break and recovery.
- Added explicit pause state, focus-loss and disconnect safety, double-KO draw,
  normalized timeout scoring, and grounded-only attacks.
- Added connected hit feedback and best-effort fight events for synchronized
  Windows-side sound.
- Added original twelve-frame ZEN and RIVAL art with cell-leak cleanup.
