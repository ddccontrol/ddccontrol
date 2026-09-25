# ddccontrol-db

`ddccontrol-db` loads the ddccontrol CBOR or XML monitor database and exports the
C ABI used by `libddccontrol`. The [candidate-v1 contract](../../doc/cbor/format.md)
and [validation report](../../doc/cbor/validation.md) describe the wire format,
extension requirements, measured costs and remaining standards evidence gaps.

Initialization prefers `ddccontrol-db.cbor` in the selected data directory.
Missing CBOR selects an immutable XML snapshot; present invalid or unsupported
CBOR fails initialization with a diagnostic. It never silently falls back to
XML or mixes revisions. An installed `ddccontrol-db.snapshot` verifies the XML
snapshot or explicitly prohibits XML fallback for necessary CBOR semantics.
Explicit test directories use only their own files. Required unknown profile
semantics also prevent manufacturer/VESA fallback in the C monitor-open path.
This protection includes required controls and included profiles when another
loading error occurs before their requirements can be checked or isolated.

One shared, immutable session holds the decoded database. Concurrent calls
retain it with `Arc`; `ddcci_release_db` releases the global reference. Each
returned monitor tree owns its data through the existing C allocator, so it
survives session release. A new initialization starts a fresh session. Loading
and the compatibility tests send no display commands.

The `options` module also exposes a normal Rust API, used by both the C database
bridge and `ddccontrol-scanmonitor`. `options::load` reads `options.xml` using the
database's encoding rules; `options::parse` accepts decoded XML. Both return the
same typed vocabulary, including optional scanner address/value hints. These
hints do not change the addresses or values loaded from monitor definitions.
The crate produces both an `rlib` for Rust callers and the existing C static
library; the C ABI and allocation rules are unchanged.

The wire codec, field registry, snapshot hashing and XML byte decoding live in
`ddccontrol-db-format`, shared with the standalone [`ddccontrol-dbgen`](../ddccontrol-dbgen/README.md)
producer. Profile interpretation and the C ABI remain here. The producer does
not link the C library, GUI or hardware code.

CAPS string parsing lives in the sibling `ddccontrol-caps` crate. EDID parsing
lives in `ddccontrol-edid`; this crate includes both parsers in the existing
Rust static library and exposes their C ABI entry points.

## C ABI Ownership

This crate deliberately preserves the existing C ABI while moving the XML
database parser to Rust.

The static library also provides the C ABI bridge for local user-profile XML.
Parsing and serialization live in the separate `ddccontrol-profile` crate;
monitor reads, writes, retries, and profile-list management remain in C.

Cached monitor-list XML uses the separate `ddccontrol-monitorlist` parser and
serializer. Its bridge in `src/monitor_list.rs` keeps partial C allocations in
a Rust cleanup guard and catches unwinding panics at both entry points. Load
returns 0 on success and writes the output pointer only then (NULL for an empty
cache); on failure it returns -1 and leaves the output unchanged. Save borrows
an acyclic C list and validates it before opening the file, returning 0 or -1.
Paths preserve Unix filename bytes. HOME and config-directory policy remain in C.

Rust allocates returned C data with the process C allocator through `malloc`.
The C side must release that data with the matching ddccontrol free functions:

- VCP entries created by `ddccontrol_caps_parse` are owned by the caller's
  `struct caps` and are released by the existing C caps cleanup paths.
- `ddccontrol_edid_parse` writes into caller-owned fixed-size storage and does
  not allocate or transfer ownership across the ABI.
- Monitor databases returned by `ddcci_create_db` must be released with
  `ddcci_free_db`.
- Cached monitor lists returned by `ddccontrol_monitorlist_load` use C `malloc`
  for every node and string and must be released with `ddcci_free_list`.

All four monitor-database entry points catch unwinding Rust panics. Initialization
returns `0` and creation returns null on failure; the void cleanup functions
contain panics as well. This does not make invalid C pointers safe. A poisoned
database mutex is recovered because contexts are fully built before publication
and are never mutated in place under the lock. Existing monitor trees remain
caller-owned across database release or reinitialization.

Do not allocate these structs with Rust-owned containers and expose their
internal pointers to C. Do not release Rust-created database structs with
anything other than the documented C cleanup function.

The Rust mirror structs are `#[repr(C)]`. Keep the Rust layout tests and the C
`test_abi_layout` test in sync with `src/lib/ddcci.h`, `src/lib/rust_ffi.h`,
`src/lib/monitor_db.h`, and `src/lib/monitor_db_internal.h`.

## Compatibility Tests

The normal test suite includes a golden monitor database fixture under
`fixtures/compat-db`. To smoke-test a real `ddccontrol-db` checkout as well,
first generate its `db/options.xml` from `db/options.xml.in` with
`make -C /path/to/ddccontrol-db db/options.xml`. Then set
`DDCCONTROL_DB_TEST_DATADIR` to either the checkout root or the `db`
directory that contains `options.xml` and `monitor/` before running
`cargo test`.

By default the real database test loads the first 25 monitor profiles in sorted
order. Set `DDCCONTROL_DB_TEST_ALL=1` to load every monitor profile, or set
`DDCCONTROL_DB_TEST_PROFILES` to a comma-separated profile list to select
specific profiles. These two variables are mutually exclusive. The all-profiles
mode fails on the first profile that cannot be parsed or loaded.

The separate XML/CBOR differential test always covers all source profiles,
three CAPS inputs and both strict and tolerant loading. Build `ddccontrol-dbgen`
and select its executable with `DDCCONTROL_DB_CONVERTER` (or put it on `PATH`).
See [reproduction commands](../../doc/cbor/testing.md) for absolute paths and
the independent fixture, frozen-reader and production gettext checks.
