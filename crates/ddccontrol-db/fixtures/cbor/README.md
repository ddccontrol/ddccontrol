# Immutable wire fixtures

These binary files are checked in, never generated as part of reader tests.
`reference-v1.cbor` comes from the independent Python reference producer's
`tests/cbor/fixtures/base-v1.cbor`; it contains TST0001 and VESA, ordered CAPS and
include operations, an accented name, delay zero versus missing, inert source
attributes and a complete 16-bit value 65443. Its `.json` is a tagged diagnostic
dump: `$map` contains key/value pairs and `$bytes` encodes hexadecimal bytes.
It cannot confuse integer keys with text keys or binary data with text.

`compat.cbor` and `compat.snapshot` were generated once from `../compat-db`,
revision `2026-06-26`, to exercise the preexisting XML golden tree through the
independent wire format. `newer-v1.cbor` adds NEW0001, VCP254/value65535 and unknown
optional information. `descriptions-v1.cbor` represents every declared future
function category and requires an unknown matcher for TST0001, keeping VESA
usable. Corresponding tagged JSON files document their exact decoded contents.
The producer repository preserves the XML fixture sources and generation
provenance. `.tree` files are hand-reviewed expected C ABI output.

`tests/frozen-v1` consumes these fixtures with frozen decoder and interpreter
source. Changes to current production code must not regenerate these files.
