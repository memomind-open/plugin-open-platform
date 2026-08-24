# Asset Provenance

## Character art

ZEN and RIVAL are original grayscale mascot fighters generated for this project
with OpenAI's built-in image generation tool. User-supplied photos of two
physical figures established only the high-level head molds: ZEN has a soft,
asymmetrical star crown and RIVAL has a broad, rounded five-point star. Their
faces, bodies, clothing, accessories, poses, and sprite rendering were newly
designed for this game. ZEN uses a crescent visor, cropped jacket, scarf, dark
trousers, mittens, and light high-tops. RIVAL uses capsule eyes, a diamond nose,
neckerchief, cuff rings, fitted shorts and leggings, and two-tone ankle boots.
The prompts explicitly excluded the reference figures' facial marks and props,
commercial fighting-game characters, logos, costumes, and stages.

The generated source, alpha-cleaned sheet, normalized preview, and encoded
header are retained beside the plugin for review and regeneration. Beta 2 uses
a fixed scale derived from each character's neutral frame, a torso anchor, and
a wider transparent action canvas so attacks preserve body size. Beta 22
replaces both character identities and extends the deterministic build tool to
handle either fighter, preserve real alpha, remove baked preview backgrounds,
and filter small detached cell leaks.

Beta 3 adds two original 2 x 2 damage sheets derived from the same identity
references. They contain only light stagger, heavy recoil, knockdown and
recovery poses; no commercial animation frames were used. Beta 22 regenerates
these sheets for the new mascot identities.

Beta 19 adds original two-character grapple-entry and kneeling ground-pound
source sheets. Beta 22 replaces both figures in those retained review sheets
with poses from the new character sources. The green-screen sources, normalized
per-character frames, combined 12-frame previews, encoded headers, and
deterministic rebuild script are retained with the example. No commercial
fighting-game frames were used.

## Stage art

The Beta 11 nighttime city stage is original geometry drawn directly in the
plugin. Its moon, craters, skyline, windows, rooftop details, and foreground
pavement use only code-defined primitives; no external stage image or
commercial game asset is included.

## Audio

The 23 effects and four non-fight music loops are synthesized locally at
44.1 kHz by `GMPluginWindows/tools/build_fighter_sfx.py`. Layered chirps,
harmonics, square-wave transients, filtered seeded noise, envelopes, and short
original note sequences produce the attack, impact, movement, menu and
match-flow cues. They contain no recorded samples and do not reproduce audio
from The King of Fighters or another commercial game.

Beta 21 uses `MIDI battle theme` by tapatilorenzo for the fight loop. The
OpenGameArt source page marks it CC0 and describes it as an original, unused,
loopable BeepBox composition:
https://opengameart.org/content/midi-battle-theme

The downloaded original is retained as `bgm_fight_cc0.mp3`; its URL, license
and SHA-256 are also recorded in `GMPluginWindows/assets/sfx/BGM_LICENSES.md`.
