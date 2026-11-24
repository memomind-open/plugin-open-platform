# Extension discovery

Queries the published random and LZ4 extension tables through `extension_get`,
validates representative function pointers, and calls them. Use this pattern
for post-1.0 optional modules without changing the frozen core Host table.

It also uses `gm_plugin_ble_get()` to query optional native BLE GATT extension
ID `6` and log its capabilities. Missing support is explicitly logged and does
not prevent the original example from running. This query does not scan, open
a client, pair, write, subscribe, or unbond any device. The example is
glasses-only; disable the phone component in Studio.

See [native BLE GATT](../../docs/ABI.md#optional-native-ble-gatt) for the full
interface, lifecycle, completion events, and HOGP/ANCS coexistence boundaries.
Current Desktop Studio support for ID `6` is not established by this SDK update.
