# ddccontrol-monitorlist

Pure parsing and serialization of `~/.ddccontrol/monitorlist`. The C ABI and
file access live in `ddccontrol-db`; HOME/config-directory policy stays in C.
No monitor hardware is needed to run `cargo test -p ddccontrol-monitorlist`.

The existing XML schema is preserved: a `monitorlist` root with an exact
`ddccontrolversion` match and direct `monitor` children with `filename`,
`supported`, `name`, and `digital` attributes. Order and duplicates are kept;
unknown children and attributes are ignored. Empty lists and empty strings
are valid. XML entities and whitespace in strings survive a save/load cycle.

Flag values accept decimal, octal, and hexadecimal syntax with leading ASCII
whitespace and an optional plus sign. Empty, negative, overflowing, or otherwise
invalid values are rejected instead of truncated into C byte fields. Every
byte value is preserved, including the digital-input flag `0x80`.

Input supports UTF-8, UTF-16 in either byte order, and legacy encodings known to
the existing `encoding_rs` dependency. Output is UTF-8. Invalid XML characters,
decoding errors, unsupported encodings, and DTDs are rejected. Parsing is
all-or-nothing, so an invalid later monitor never returns a partial list.
