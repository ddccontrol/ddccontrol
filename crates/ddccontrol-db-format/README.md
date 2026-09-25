# Shared database format

Pure Rust implementation of the candidate-v1 CBOR contract used by the
`ddccontrol-db` runtime and the `ddccontrol-dbgen` producer. It depends only on
`ciborium`, `sha2` and `encoding_rs`; it does not depend on libddccontrol, gettext,
GTK, a monitor, or an installed database.

`value` preflights bounded deterministic CBOR before library decoding. `encode`
uses RFC 8949 section 4.2.1 bytewise encoded-key order and rejects unsupported
values and duplicate keys. Explicit ID/name tables and contextual attribute
rules are shared with XML conversion. `manifest_digest` hashes deterministic
manifest bytes, never the digest field itself.

`decode` builds owned runtime nodes and negotiates required semantics, isolating
invalid reference graphs at profile scope. `validate` returns the original
lossless CBOR value after checking the contract, including descriptive payloads
and stricter producer reference-expansion limits; unknown required features
remain representable. `fallback_manifest` prevents XML from bypassing those
features. Required semantics are never an implicit execution capability.

XML normalization/encoding helpers preserve the runtime's legacy tolerant API
and add a strict entry point for source production. The maintained source of
normative requirements is [format.md](../../doc/cbor/format.md), not Rust layouts
or dependency-specific serialization.
