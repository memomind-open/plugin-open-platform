# Bidirectional Plugin Messaging

Use generic plugin messages to exchange application-defined binary data between
a Web plugin on the phone and the currently paired plugin on the glasses.

This API is intended for controls, game events, sensor-derived events, status
updates, and other plugin-specific data. Continue to use the `display.*` APIs
for Scene rendering.

## Quick reference

| Direction | Sender API | Receiver API |
| --- | --- | --- |
| Web to glasses | `gm.plugin.sendMessage(channel, data)` | `GM_PLUGIN_EVENT_BT_MESSAGE` |
| Glasses to Web | `host->bt_send(channel, data, length)` | `gm.plugin.onMessage(listener)` |

Both directions use the same application message contract:

- `channel` is an application-defined unsigned 16-bit integer (`0..65535`).
- The payload is binary, must not be empty, and can contain up to `81,901`
  bytes.
- The platform preserves the channel and payload bytes.
- The transport automatically fragments and reassembles large logical
  messages. Plugin code receives only complete messages.
- Messages are delivered only between the active paired Web and glasses
  plugins. There is no offline queue or cross-plugin broadcast.
- No Web plugin manifest permission or `device.subscribeEvents()` subscription
  is required.

## Send a message from Web to glasses

Call `gm.plugin.sendMessage()` with a channel and a non-empty `Uint8Array`:

```js
const CONTROL_CHANNEL = 0x4201;

const payload = Uint8Array.of(
  1, // schema version
  7, // sequence
  2, // application opcode
  1, // application value
);

try {
  const result = await gm.plugin.sendMessage(CONTROL_CHANNEL, payload);
  console.log(`Sent ${result.payloadBytes} bytes on channel ${result.channel}`);
} catch (error) {
  console.error("Unable to send plugin message", error);
}
```

The glasses plugin receives the complete message as a
`GM_PLUGIN_EVENT_BT_MESSAGE` event:

```c
#include "gm_plugin.h"

#define CONTROL_CHANNEL UINT16_C(0x4201)

static bool plugin_on_event(void *context, const gm_plugin_event_t *event)
{
    const uint8_t *payload;
    uint32_t length;
    (void)context;

    if (event == NULL ||
        event->type != GM_PLUGIN_EVENT_BT_MESSAGE ||
        event->data.bt.channel != CONTROL_CHANNEL)
        return false;

    payload = event->data.bt.data;
    length = event->data.bt.length;
    if (payload == NULL || length != 4U || payload[0] != 1U)
        return false;

    /* Decode and handle the application payload here. */
    return true;
}
```

`event` and `event->data.bt.data` are borrowed Host memory and are valid only
until `on_event()` returns. Copy the bytes during the callback if they are
needed later.

A successful `gm.plugin.sendMessage()` call means that the device acknowledged
delivery to the running glasses plugin. It does not mean that the plugin has
completed its application logic or updated the display.

## Send a message from glasses to Web

The glasses plugin sends a binary payload through the Host `bt_send()` API:

```c
#include "gm_plugin.h"

#define EVENT_CHANNEL UINT16_C(0x4202)

static const gm_plugin_host_api_t *s_host;
static uint8_t s_sequence;

static gm_plugin_result_t send_status(uint8_t status)
{
    uint8_t payload[] = {
        1U,            /* schema version */
        s_sequence++,  /* wrapping application sequence */
        1U,            /* status opcode */
        status,
    };

    return s_host->bt_send(EVENT_CHANNEL, payload, sizeof(payload));
}
```

In this example, `s_host` is the validated Host pointer saved by
`gm_plugin_entry()` during plugin initialization.

The Host copies the payload before `bt_send()` returns, so the plugin may reuse
or release its source buffer immediately afterwards. Always check the returned
`gm_plugin_result_t`:

- `GM_PLUGIN_OK`: accepted by the Host transport.
- `GM_PLUGIN_EINVAL`: invalid pointer, empty payload, or oversized payload.
- `GM_PLUGIN_ENOMEM`: the Host could not construct the transport packet.
- `GM_PLUGIN_EIO`: the transport rejected the packet.

`GM_PLUGIN_OK` does not confirm that the WebView listener consumed or processed
the message. Add an application-level response only when the feature needs that
confirmation.

The Web plugin subscribes with `gm.plugin.onMessage()`:

```js
const EVENT_CHANNEL = 0x4202;

const unsubscribe = gm.plugin.onMessage(({ channel, data }) => {
  if (channel !== EVENT_CHANNEL) return;
  if (data.length < 4 || data[0] !== 1) return;

  const sequence = data[1];
  const opcode = data[2];
  const value = data[3];
  console.log({ sequence, opcode, value });
});

// Call this when the page or feature is destroyed.
unsubscribe();
```

The callback receives:

- `channel`: the original unsigned 16-bit channel.
- `data`: the original payload as a `Uint8Array`.

Multiple listeners may be registered. Each call returns an independent
unsubscribe function.

## Design your application protocol

The platform treats the payload as opaque bytes. Define its format as part of
the contract shared by the Web and glasses plugins.

Recommended practices:

1. Give every channel a descriptive constant on both sides.
2. Start the payload with a schema version and reject unsupported versions.
3. Add an opcode when one channel carries multiple message types.
4. Add a sequence or request ID when duplicate detection, ordering, or a reply
   is important.
5. Define integer byte order explicitly. Network byte order (big-endian) is a
   reasonable default for multi-byte values.
6. Validate the complete payload length before reading fields.
7. Keep high-frequency events compact and avoid blocking either plugin callback.

Direction is selected by the sending API, not by the channel value. A channel
may be reused in both directions, but separate channel constants are usually
clearer when the two directions use different payload schemas.

## Transport reference

Most plugin developers do not need to encode the underlying GM packet. For
debugging or Host integration, generic messages use service `0x0F`:

| Direction | Command | Payload |
| --- | --- | --- |
| Phone to glasses | `0x28` (`GM_PLUGIN_COMMAND_PHONE_TO_GLASSES`) | `INT16 channel`, `BYTES data` |
| Glasses to phone | `0x29` (`GM_PLUGIN_COMMAND_GLASSES_TO_PHONE`) | `INT16 channel`, `BYTES data` |

Command `0x28` is also reused by the transport acknowledgement for a
phone-to-glasses request. That acknowledgement contains status and next-offset
fields, not a plugin message payload. Command `0x29` is an unsolicited
glasses-to-phone event and must not be treated as a response to `0x28`.

## Test in Studio

Start the default dual-ended Studio from the repository root:

```sh
npm run dev
```

Pair a Web `.mmpkg` or development directory with a compatible glasses `.gmp`,
then verify both directions:

- Web `sendMessage()` reaches `GM_PLUGIN_EVENT_BT_MESSAGE` with identical bytes.
- Glasses `bt_send()` reaches `onMessage()` with identical bytes.
- Unknown channels and unsupported payload versions are ignored safely.
- Listeners are unsubscribed when their feature or page is destroyed.
- Boundary payloads are tested if the plugin sends large messages.

## Related documentation and examples

- [Web plugin API reference](../WebSDK/docs/web-plugin/api-reference.md)
- [Glasses Host API](../GlassSDK/include/gm_plugin.h)
- [Low-level plugin protocol](../GlassSDK/PROTOCOL.md)
- [Bluetooth round-trip example](../GlassSDK/examples/bluetooth/README.md)
- [Fighter Controller Web example](../WebSDK/examples/fighter-controller/README.md)
