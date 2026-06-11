# ddccontrol-caps

`ddccontrol-caps` is a minimal Rust parser for the subset of monitor capability
strings that the current C-based `ddccontrol` code uses.

This crate is not a full ACCESS.bus or MCCS capability parser. It intentionally
starts with the behavior needed by the existing `ddccontrol` database and CLI
paths, so the Rust port has a small and testable compatibility surface.

## Ported Behavior

The previous C parser was implemented in `src/lib/ddcci.c` as
`ddcci_parse_caps`. Its primary consumers used two pieces of parsed data:

- monitor type, used to choose LCD/CRT/VESA fallback profiles
- the VCP support table, used to decide whether controls from the monitor
  database should be kept or discarded

This crate currently ports that behavior.

## Monitor Type

The parser recognizes:

- `type(lcd)`
- `type(crt)`

Matching is case-insensitive, so `type(LCD)` and `type(CRT)` are accepted. An
unknown type is ignored and does not overwrite an existing parsed type.

## VCP Controls

The parser recognizes `vcp(...)` sections and records supported VCP feature
codes as 8-bit values.

Both styles used by real monitors are supported:

```text
vcp(10 12 14)
vcp(101214)
```

In both cases, the result is equivalent to the C code's `caps.vcp[code] != NULL`
check.

## VCP Value Lists

The parser supports value lists attached to a VCP code:

```text
vcp(14(05 08 0B) 60(01 03))
```

Values are stored as `u16`, matching the C parser's `unsigned short` value
storage. A VCP entry distinguishes between:

- a supported control with no explicit value list
- a supported control with a parsed list of allowed values

Nested value groups are tolerated and skipped. This matches the practical
behavior of the C parser, which only records values at the direct VCP value-list
level.

## Add And Remove

The C parser accepts an `add` flag and is also used by monitor database
`<caps add="..." remove="..."/>` profile patches.

This crate exposes equivalent operations:

- `Caps::parse(...)` for initial add-style parsing
- `Caps::apply_add(...)` for adding controls or value lists
- `Caps::apply_remove(...)` for removing controls or values

Remove behavior follows the C code:

- `vcp(10)` removes the whole VCP control
- `vcp(10(01))` removes only value `0x01`
- if the last explicit value is removed, the VCP control is removed
- removing a nonexistent control is allowed

## Ignored Sections

Unknown sections are skipped so they do not prevent later `vcp(...)` or
`type(...)` sections from being parsed.

Examples of ignored sections include:

- `prot(...)`
- `cmds(...)`
- `model(...)`
- `mccs_ver(...)`
- `mswhql(...)`
- `edid bin(...)`
- `vdif bin(...)`

## Validation

The parser reports errors for the invalid input cases that matter for the
current port:

- invalid VCP feature codes
- invalid value-list entries
- value lists without a preceding VCP code
- unbalanced or unexpected parentheses

## Not Implemented Yet

The following capability data is intentionally not parsed yet:

- protocol information from `prot(...)`
- command support from `cmds(...)`
- model names from `model(...)`
- MCCS version from `mccs_ver(...)`
- EDID or VDIF binary payloads
- VCP names or localized value names
- full ACCESS.bus grammar coverage

These can be added later when Rust code needs them directly.
