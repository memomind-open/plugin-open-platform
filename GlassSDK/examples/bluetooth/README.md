# Bluetooth text round-trip example

This plugin receives UTF-8 text on channel 1, displays it through the shared
LVGL core drawing table, prefixes it with `[GLASSES]`, converts ASCII lowercase to
uppercase, and sends the modified bytes back to the phone with `bt_send()`.
The Host transports the reply with GM plugin service `0x0F`, glasses-to-phone
command `0x29`; incoming phone messages use command `0x28`.

After changing this example, run `./gm-build` from the SDK root. It creates
`build-host/bluetooth/bluetooth.gmp` and displays the installation QR code.
