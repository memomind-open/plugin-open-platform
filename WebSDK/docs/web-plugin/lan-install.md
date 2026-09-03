# Install a developer app over a LAN

The prebuilt Desktop Studio packages its selected phone and glasses plugins as
one developer-app ZIP and displays one QR code for the phone App's debugging
entry point. This channel is intended for development-time LAN sideloading and
does not provide server authentication.

## QR code payload

The QR payload contains only the direct Studio server address:

```text
mmapp+tcp://192.168.1.8:18765
```

The QR does not contain JSON or package metadata. Studio serves a ZIP with at
most one phone `.mmpkg` and at most one glasses `.gmp`; both files are placed
directly at the ZIP root. ZIP composition and incremental updates are handled
internally by Desktop Studio and the App.

The scheme is always `mmapp+tcp`. The authority contains Studio's LAN host and
port. The server returns the percent-encoded application ZIP name in its
response metadata, keeping the QR short while preserving application identity.

Studio prefers TCP port `18765`. If that port is busy, it selects a random
available port and writes the actual value into the QR code.

## Download protocol

After connecting to the TCP address in the QR code, the App sends:

```text
MMAPP/1 GET - -\n
```

A successful response is one ASCII metadata line followed immediately by the
package body:

```text
MMAPP/1 OK <size> <sha256> <A|B|AB> <percent-encoded-name>\n<zip bytes>
```

`size` is a decimal byte count and `sha256` is a 64-character lowercase
hexadecimal digest. The final token is the percent-encoded `.zip` application
name used by the App for the per-application disclaimer and local identity. The
App enforces the combined package limit, reads exactly `size` bytes, verifies
SHA-256, and then applies each component's normal validation rules.

An invalid request returns:

```text
MMAPP/1 ERROR invalid-request\n
```

If the package was deleted, corrupted, or exceeds the limit before download,
the server returns:

```text
MMAPP/1 ERROR invalid-package\n
```

## Security boundary

This protocol has no TLS, authentication, or signature and is suitable only
for Debug installation on a trusted LAN. Size and SHA-256 checks detect
truncation or transmission inconsistency, but they do not prevent an active
attacker from replacing both the package and digest. The App's product security
policy determines whether Release builds may use this channel and whether
`.mmpkg` packages must be signed.
