# Install an `.mmpkg` over a LAN

Desktop Studio can package the selected Web plugin as an `.mmpkg` and display
a QR code for the phone App's debugging entry point. This channel is intended
for development-time LAN sideloading and does not provide server
authentication.

## QR code payload

The QR code contains compact JSON:

```json
{"v":1,"scheme":"mmpkg+tcp","host":"192.168.1.8","port":18765,"name":"counter-0.1.0.mmpkg"}
```

- `v`: protocol version, currently `1`.
- `scheme`: always `mmpkg+tcp`; do not treat it as the firmware plugin scheme
  `gmp+tcp`.
- `host` and `port`: Studio's LAN TCP address.
- `name`: suggested download filename.

Studio prefers TCP port `18765`. If that port is busy, it selects a random
available port and writes the actual value into the QR code.

## Download protocol

After connecting to the TCP address in the QR code, the App sends:

```text
MMPKG/1 GET\n
```

A successful response is one ASCII metadata line followed immediately by the
package body:

```text
MMPKG/1 OK <size> <sha256>\n<mmpkg bytes>
```

`size` is a decimal byte count and `sha256` is a 64-character lowercase
hexadecimal digest. The App must enforce the 10 MB package limit, read exactly
`size` bytes, verify SHA-256, and then run the normal `.mmpkg` manifest, file
hash, and installation validation.

An invalid request returns:

```text
MMPKG/1 ERROR invalid-request\n
```

If the package was deleted, corrupted, or exceeds the limit before download,
the server returns:

```text
MMPKG/1 ERROR invalid-package\n
```

## Security boundary

This protocol has no TLS, authentication, or signature and is suitable only
for Debug installation on a trusted LAN. Size and SHA-256 checks detect
truncation or transmission inconsistency, but they do not prevent an active
attacker from replacing both the package and digest. The App's product security
policy determines whether Release builds may use this channel and whether
`.mmpkg` packages must be signed.
