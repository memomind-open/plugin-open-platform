# LZ4 extension example

Queries the optional Host LZ4 extension, compresses one raw LZ4 block, safely
decompresses it, and verifies the round trip.

```sh
./gm-build build --example lz4
```

The output is `build-host/lz4/lz4.gmp`. Raw LZ4 blocks do not contain the
original byte count, so applications must transfer or store that metadata
separately.
