# Bluetooth text round-trip example

This plugin receives UTF-8 text on channel 1, displays it through the shared
LVGL core drawing table, prefixes it with `[GLASSES]`, converts ASCII lowercase to
uppercase, and sends the modified bytes back to the phone with `bt_send()`.
Incoming messages arrive as `GM_PLUGIN_EVENT_BT_MESSAGE`; the phone plugin
uses the public PhoneSDK Bluetooth APIs through the official App. Channel 1 is
this example's application channel; device transport framing is managed by the Host.

### Bluetooth wire mapping

Application messages use GM service `0x0F`: phone-to-glasses command `0x28`
with `INT16 channel` and `BYTES data`; its acknowledgement also uses `0x28`.
The plugin sends application replies/events with `bt_send()` through command
`0x29`, carrying the same channel/data TLV layout. See
[the binary protocol](../../docs/PROTOCOL.md#plugin-application-service-0x0f)
for framing, byte order and acknowledgement semantics. This is business data,
not executable-plugin package transfer.
