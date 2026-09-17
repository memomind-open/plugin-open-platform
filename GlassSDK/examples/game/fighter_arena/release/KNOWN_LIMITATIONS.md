# Known Limitations

- This is a developer-preview beta, not a compatibility guarantee for every
  GM Host ABI 1.0 device or firmware build.
- Play requires the paired Fighter Controller Web plugin and a compatible App
  or Desktop Studio host. Installing only `fighter_arena.gmp` does not provide
  the complete control and audio path.
- The beta contains one fixed matchup and one stage. There is no character
  selection, story mode, throw, aerial attack, save data, or online play.
- Sound is produced by the Web plugin host on the phone or computer, not by the
  glasses.
- Current firmware caches complete plugin images in Flash. A reboot clears runtime
  state; a matching valid cache can be reused when the App launches the plugin.
  Evicted or invalid images are supplied again by the App.
- Display residue must be assessed on real hardware; Desktop Studio cannot
  prove optical behavior.
