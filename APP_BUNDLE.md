# Developer App ZIP

MemoMind imports and shares one developer app as a standard `.zip` file. The
ZIP contains the selected phone and glasses plugins directly at its root:

```text
weather.zip
|-- weather-0.1.0.mmpkg   # optional phone component (A)
`-- weather.gmp           # optional glasses component (B)
```

The allowed compositions are A, B, and A+B. A ZIP must contain one or two
non-empty files, at most one `.mmpkg` and at most one `.gmp`. Directories,
other extensions, duplicate component types, symbolic links, placeholder
files, and bundle metadata such as `manifest.json` are not allowed. Each
component keeps its own existing format and validation rules.

Local import and system sharing always use a complete snapshot ZIP. Desktop
Studio also displays one QR code for the complete current selection.

## QR and incremental update protocol

The QR payload is:

```text
mmapp+tcp://<host>:<port>
```

For a new import, the App requests both components:

```text
MMAPP/1 GET - -\n
```

When updating an existing local app, the two arguments are the App's current
lowercase SHA-256 values for A and B; `-` means that side is absent locally:

```text
MMAPP/1 GET <a-sha256-or-> <b-sha256-or->\n
```

The server returns a ZIP containing only changed components:

```text
MMAPP/1 OK <size> <zip-sha256> <A|B|AB> <percent-encoded-name>\n
<zip bytes>
```

If no component bytes changed, it returns:

```text
MMAPP/1 NOT_MODIFIED <A|B|AB> <percent-encoded-name>\n
```

The composition field describes the complete target, not the contents of an
incremental ZIP. The final token is the percent-encoded `.zip` application
name. This keeps the QR address short while giving the App a stable identity
for its per-application disclaimer. The App validates received component files
and then atomically merges them with unchanged local components.
