# Independent CDDL validation

This optional developer tool uses unmodified `cddl-cat` 0.7.1 to parse the
actual CDDL and validate CBOR. It is separate from the production reader and
shared Rust reader/producer validator, and is not a runtime or Debian package dependency.
It does not replace deterministic-wire, manifest, reference or semantic tests.

Run from the ddccontrol repository root:

```sh
cargo run --locked --manifest-path doc/cbor/schema-check/Cargo.toml -- \
  doc/cbor/format.cddl database \
  crates/ddccontrol-db/fixtures/cbor/reference-v1.cbor \
  crates/ddccontrol-db/fixtures/cbor/compat.cbor \
  ../ddccontrol-db/tests/cbor/fixtures/base-v1.cbor \
  ../ddccontrol-db/tests/cbor/fixtures/descriptions-v1.cbor \
  ../ddccontrol-db/tests/cbor/fixtures/newer-v1.cbor \
  ../ddccontrol-db/db/ddccontrol-db.cbor

cargo run --locked --manifest-path doc/cbor/schema-check/Cargo.toml -- \
  doc/cbor/format.cddl function-description \
  ../ddccontrol-db/tests/cbor/fixtures/description-payload.cbor
```

These inputs passed on 2026-09-22. The `xml-fallback-authorization` entry
also accepted both the full source manifest and the single-byte `false` guard. Independent negative vectors with an
incorrect magic byte string, and a discover-at-runtime quantity containing an
observed value, were rejected. The exact magic constraint was retained in the
schema. `zcbor` 0.9.1 and `cddl` 0.10.7 were also tried but encountered upstream
byte-string-literal handling defects; neither was used to claim successful
schema validation. `cddl-cat` required no local patches or schema weakening.
