# ddccontrol-dbgen

An offline Rust producer, validator and lossless inspector for the unpublished
candidate-v1 database contract in [format.md](../../doc/cbor/format.md). This
crate has no C ABI, hardware, GUI, gettext or installed-database dependency.

```sh
cargo build --locked --release -p ddccontrol-dbgen
./target/release/ddccontrol-dbgen convert /path/to/ddccontrol-db/db database.cbor \
  --snapshot database.snapshot
./target/release/ddccontrol-dbgen validate database.cbor
./target/release/ddccontrol-dbgen dump database.cbor
./target/release/ddccontrol-dbgen rewrite database.cbor rewritten.cbor \
  --snapshot rewritten.snapshot
```

`--revision REV` overrides the `options.xml` date; otherwise that date must be
nonempty. Neither timestamps, locale, checkout location nor architecture enter
the output. All XML files are collected and checked again after generation;
changes during collection fail generation. Output files use temporary files and
rename. A failed conversion removes old output and snapshot artifacts so a
package build cannot silently reuse stale CBOR. Output, snapshot and input XML
paths must be distinct. `rewrite` requires a distinct output and retains all
unknown fields and extensions, including required semantics unsupported by the
runtime. Input read or validation errors leave existing rewrite destinations
unchanged. It emits the permanent `false` fallback guard when necessary. The
snapshot sidecar and CBOR must be staged and installed together by the packager.

The wire decoder, deterministic encoder, field/element registers, contextual
attribute roles, identity checks, snapshot hashing, structural validation and
legacy XML decoding live in `ddccontrol-db-format`; both the runtime reader and
this producer use that crate. Legacy integer notation uses `ddccontrol-xml`.
The producer rejects malformed source encodings, while the runtime retains its
historical tolerant XML decoding. Decoding a descriptor never executes it.

`dump` represents every CBOR map as `{"$map": [[key, value], ...]}` and every byte
string as `{"$bytes": "lowercase hex"}`. Standard CBOR integers retain their full
range, including -18446744073709551616 and 18446744073709551615; they never pass
through floating point. UTF-8, two-space indentation and final LF match the
independently committed JSON fixtures exactly.

## Validation

```sh
cargo test --locked -p ddccontrol-db-format -p ddccontrol-dbgen
cargo clippy --locked -p ddccontrol-db-format -p ddccontrol-dbgen --all-targets -- -D warnings
```

The integration suite checks byte-identical production and JSON against the
existing base/newer/descriptive fixtures, independently fixed RFC vectors,
every truncated base-file prefix, malformed types and restricted-CBOR limits,
legacy encodings including both UTF-16 byte orders, namespace dispatch,
inactive branches, source text and comments, numeric grammar/bounds/long
literals, include cycles and expansion, snapshot integrity, identity and
metadata contracts, typed descriptors, required-extension guards, lossless
future-data rewrite, locale/cwd reproducibility and stale-artifact/path-alias
handling. `ddccontrol-db` additionally compares generated databases with XML
through the real reader, including CAPS changes, translation and tree ownership;
see [testing.md](../../doc/cbor/testing.md).

The `fixtures/source`, `fixtures/newer-source` XML and the two snapshot files
were copied unchanged from ddccontrol-db PR #441, commit
`6aac4a9`, where they predate this implementation. Existing CBOR/JSON binaries
remain in `../ddccontrol-db/fixtures/cbor`; tests do not regenerate them. The
frozen first reader remains separately checksummed, with unchanged source and
decoding semantics, so shared production code is not the only compatibility
oracle.

Release workflows build a static Linux executable for each published target,
execute fixture checks, and package it with license/provenance and SHA-256
checksums. Use a pinned version and verify its checksum. Offline Debian package
builds can instead build this tool from source using Debian-accepted dependencies.
