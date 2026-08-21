# GM Plugin Security

## Package integrity

GMP v1 uses a fixed 28-byte header and one whole-package CRC32. The Host also
validates package format, ABI compatibility, bounds, image layout, relocations,
and runtime memory size before activation.

CRC32 detects accidental transport corruption. It does not authenticate the
publisher and is not a digital signature. Manifest text, SHA-256, a second
header CRC, duplicated capability declarations, and derivable offsets are not
transferred in the runtime package.

## Trust boundary

A GMP contains native RV32 code. The current target does not isolate plugin
code with an MPU. The Host function table is an ABI boundary, not a security
sandbox: crafted native code can address firmware memory without using a Host
entry.

Function-table and extension discovery are therefore compatibility mechanisms,
not authorization or isolation boundaries. Until package signature verification
and a production trust policy are implemented, install only `.gmp` files from a
trusted source through a trusted GMPluginWindows or GMPluginPhoneApp build.

Production authenticity, if required, must be implemented as a separate
signing and verification layer rather than treating CRC32 as a security check.

See [PROTOCOL.md](PROTOCOL.md) for transport validation and [ABI.md](ABI.md) for
the runtime compatibility contract.
