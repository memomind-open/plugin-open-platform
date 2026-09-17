# GM Plugin Security

## Package integrity

GMP v2 uses a 120-byte header and whole-package CRC32. The cache also verifies
SHA-256 against the canonical package identity sent by the phone. Installed
instruction bindings are checked before canonicalization; corrupt executable
bytes cannot be hidden by replacing them during the hash check.

Format, exact ABI, bounds, RAM sizes and relocations are validated before
activation. CRC32 and SHA-256 detect content changes; neither authenticates the
publisher without a trusted signature. Name/version/ABI are embedded in the
package; the full manifest is not.

## Trust boundary

A GMP contains native RV32 code. The current target does not isolate plugin
code with an MPU. The Host function table is an ABI boundary, not a security
sandbox: crafted native code can address firmware memory without using a Host
entry.

Function-table and extension discovery are therefore compatibility mechanisms,
not authorization or isolation boundaries. Until package signature verification
and a production trust policy are implemented, install only `.gmp` files from a
trusted source through an official, trusted Studio or App build.

Production authenticity, if required, must be implemented as a separate
signing and verification layer rather than treating CRC32 as a security check.

See [PROTOCOL.md](PROTOCOL.md) for transport validation and [ABI.md](ABI.md) for
the runtime compatibility contract.
