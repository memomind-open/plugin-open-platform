# Button input

Displays the latest stable plugin button/action value on the glass screen.
Single-click, double-click, triple-click, and long-press events each show a distinct status
message and are also written to the Host log.

## Firmware dependency

**Upgrade the glasses firmware before testing triple click on a physical device.**
The firmware must include plugin triple-click routing and Host event mapping.
Installing GM Input v4 or updating the SDK/Studio alone is not sufficient:
older firmware does not deliver this action to the plugin, even when the same
plugin displays `TRIPLE CLICK` successfully in Studio.

No minimum released firmware version is specified yet; confirm plugin
triple-click support in the firmware release notes. Existing single/double-click
and long-press controls remain available. See [Input documentation](../../docs/INPUT.md#primary-glasses-button-triple-click).

## Verification

Use the Desktop Studio primary-button control to click once, twice, three times, or hold for one
second. The event remains visible until the next supported button event.

After upgrading the glasses, install GM Input v4 and triple-click the physical
button. Confirm that `TRIPLE CLICK` is displayed and action `7` is logged.
