# ddccontrol-db

`ddccontrol-db` parses the ddccontrol XML monitor database and exports the C ABI
used by `libddccontrol`.

CAPS string parsing lives in the sibling `ddccontrol-caps` crate.

## C ABI Ownership

This crate deliberately preserves the existing C ABI while moving the XML
database parser to Rust.

Rust allocates returned C data with the process C allocator through `malloc`.
The C side must release that data with the matching ddccontrol free functions:

- VCP entries created by `ddccontrol_caps_parse` are owned by the caller's
  `struct caps` and are released by the existing C caps cleanup paths.
- Monitor databases returned by `ddcci_create_db` must be released with
  `ddcci_free_db`.

Do not allocate these structs with Rust-owned containers and expose their
internal pointers to C. Do not release Rust-created database structs with
anything other than the documented C cleanup function.

The Rust mirror structs are `#[repr(C)]`. Keep the Rust layout tests and the C
`test_abi_layout` test in sync with `src/lib/ddcci.h`, `src/lib/monitor_db.h`,
and `src/lib/monitor_db_internal.h`.
