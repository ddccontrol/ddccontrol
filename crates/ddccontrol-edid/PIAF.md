# PIAF and ddccontrol EDID parsing

This document compares the internal `ddccontrol-edid` parser with
[PIAF](https://docs.rs/piaf/latest/piaf/), and evaluates what PIAF's additional
functionality could contribute to a desktop monitor control application.

The comparison was originally made against PIAF 0.4.1. PIAF is evolving, so
its current API and supported EDID extensions should be checked before making
an integration decision.

## Summary

`ddccontrol-edid` and PIAF solve different-sized problems:

- `ddccontrol-edid` is a small compatibility parser for the fields used by the
  existing ddccontrol C API.
- PIAF is a general EDID decoder that produces a rich, typed display
  capability model.

PIAF would add useful monitor identification, diagnostics, video mode, color,
HDR, audio, and extension-block information. Most of that information is not
needed to read or write DDC/CI controls, however. EDID does not replace MCCS,
the monitor capability string, VCP operations, the monitor database, or
monitor-specific quirks.

For the current ddccontrol C API, the internal parser remains an appropriate
implementation. PIAF becomes more compelling if ddccontrol grows into a wider
display information and diagnostics tool.

## Current `ddccontrol-edid` scope

The internal parser preserves the behavior of the historical
`ddcci_parse_edid_buf()` implementation. It extracts:

- manufacturer and product code, formatted as the existing seven-character
  PNP ID;
- numeric serial number;
- manufacture week and year;
- EDID version and revision;
- the digital/analog input flag;
- physical width and height;
- monitor-name and ASCII-serial descriptors; and
- at most the first 128 bytes as raw EDID data.

The minimum accepted input is 23 bytes because the last fixed field read by
ddccontrol is at zero-based offset `0x16`. This is a compatibility threshold,
not the size of a complete EDID base block. A complete base block is 128 bytes.

With a partial input, only the fixed fields are decoded. Descriptor text is
decoded only when a complete base block is available.

For compatibility, the parser validates the EDID header but does not enforce
the checksum. It does not decode extension blocks and ignores input beyond the
first 128 bytes.

## Comparison with PIAF

| Area | `ddccontrol-edid` | PIAF |
|---|---|---|
| Primary goal | Preserve ddccontrol behavior | General EDID capability decoding |
| Minimum input | 23-byte compatibility input | Complete 128-byte base block |
| Validation | Header only | Header, length, checksums, and structured diagnostics |
| Base-block coverage | Fields used by ddccontrol | Broad specification-defined field coverage |
| Extension blocks | Not decoded | Handler-based CEA-861 and DisplayID decoding |
| Video modes | Not decoded | Standard, detailed, and extension-provided modes |
| Color and HDR | Not decoded | Chromaticity, gamma, colorimetry, HDR metadata, and related data |
| Audio | Not decoded | HDMI/DisplayPort sink audio capabilities |
| Result model | Small `Edid` and `EdidInfo` types | `ParsedEdidRef` plus `DisplayCapabilities` |
| Memory model | Owned `String` and `Vec` values | Zero-copy parsing plus owned and `no_std` alternatives |
| C integration | Fixed-size, caller-owned C ABI result | Would require a ddccontrol-specific adapter |
| Dependency policy | Internal crate with no third-party dependencies | External MPL-2.0 Rust crate |

PIAF separates byte-level parsing from capability extraction:

```text
EDID bytes
    -> ParsedEdidRef
    -> DisplayCapabilities
```

This separation allows callers to inspect the validated block structure or
derive a consumer-oriented capability model using standard or custom extension
handlers. See the [PIAF crate documentation](https://docs.rs/crate/piaf/latest)
for the current data model and supported handlers.

## Potential value for desktop monitor control

### More reliable monitor identification

PIAF can expose a richer set of identity fields than the current PNP ID alone:

- manufacturer;
- product code;
- numeric and textual serial numbers;
- display name;
- manufacture date; and
- identity information carried by newer extension formats.

This could improve database matching and help distinguish multiple monitors of
the same model. These fields must still be treated as imperfect identifiers:
some displays use zero or duplicated serial numbers, while docks, KVM switches,
and virtual displays may expose synthetic EDIDs.

### Better diagnostics and issue reports

PIAF distinguishes hard parse failures from non-fatal warnings and validates
complete block checksums. A future diagnostic command could report:

```text
EDID base block: valid
Checksum: valid
Extension blocks: 2
CEA-861: decoded
DisplayID: not present
Warning: unknown vendor-specific data block
```

Combining this with the raw EDID, DDC/CI capability string, supported VCP codes,
and database profile would make monitor compatibility reports substantially
more useful.

### Richer CLI and GUI information

PIAF could support an information view containing:

- preferred and supported resolutions and refresh rates;
- physical dimensions and preferred image size;
- digital interface type and color bit depth;
- gamma, chromaticity, and supported color encodings;
- HDR metadata and colorimetry;
- HDMI or DisplayPort audio capabilities; and
- DisplayID or tiled-display information.

This information is valuable for users and diagnostics, but most of it is
informational from ddccontrol's perspective. Changing video modes, HDR state,
or color-management configuration normally belongs to DRM, the compositor, or
a color-management service rather than DDC/CI.

### Profile and policy selection

A future display-management layer could use richer EDID information to:

- choose a monitor database or quirk profile;
- associate an ICC profile with a physical monitor;
- distinguish SDR and HDR workflows;
- retain a monitor identity when its I2C bus number changes; and
- produce clearer names for CLI and GUI display selectors.

The application should not assume that every EDID field is unique, truthful,
or stable across connection paths.

### Advanced display support

CEA-861 and DisplayID decoding is relevant to HDR displays, professional
monitors, ultrawide displays, tiled panels, and devices with extensive HDMI or
audio capability data. PIAF's handler system also provides a possible route for
vendor-specific extensions without placing those decoders in ddccontrol's core
parser.

## What PIAF would not provide

EDID describes the display as a video sink. It does not describe the complete
DDC/CI control interface and cannot tell ddccontrol reliably:

- which VCP features are supported;
- which values a VCP feature accepts;
- whether an advertised control actually works;
- how brightness, contrast, power, or input selection should be changed;
- how many physical input connectors the monitor exposes; or
- which monitor-specific DDC/CI quirks are required.

Those responsibilities remain with MCCS definitions, the monitor's capability
string, VCP reads and writes, the ddccontrol database, and tested quirks.

In particular, the interface described by EDID is the active video connection.
It should not be interpreted as a complete inventory of all physical inputs on
the monitor.

## Compatibility concerns

PIAF's stricter validation is useful, but a direct replacement could reject
inputs accepted by historical ddccontrol:

- partial buffers containing only the first 23 bytes;
- complete base blocks with an invalid checksum; and
- malformed EDIDs from real monitors that still provide usable identity data.

Any integration must define whether strict validation is required, optional,
or followed by a compatibility fallback. Changing this behavior silently would
risk regressions in monitor detection and database matching.

PIAF would also add an external Rust dependency. Before adoption, the project
should review API stability, the MPL-2.0 licensing and redistribution
requirements, minimum supported Rust version, Debian availability or vendoring
policy, and support across Debian architectures.

## Recommended architecture

If PIAF is adopted, `ddccontrol-edid` should remain the project-owned API layer:

```text
libddccontrol / future Rust consumers
                  |
          ddccontrol-edid
                  |
                PIAF
```

This keeps PIAF types out of the public C ABI and allows the dependency to be
upgraded or replaced without changing every consumer.

A conservative implementation strategy would be:

1. Keep the existing parser for the legacy C-compatible result.
2. Add an optional rich parse path for complete EDID data.
3. Use PIAF for complete, checksum-valid base and extension blocks.
4. Preserve an explicit compatibility fallback for partial or checksum-invalid
   input if existing ddccontrol behavior is still required.
5. Convert PIAF output into project-owned Rust types before exposing it to the
   rest of ddccontrol.
6. Do not expose PIAF-owned or Rust-native containers through the C ABI.
7. Validate the behavior against a corpus of real EDIDs from desktop monitors,
   docks, KVM switches, and virtual displays.

## Adoption threshold

PIAF is probably unnecessary if ddccontrol only needs to identify a monitor and
control brightness, contrast, power, and input selection.

It becomes worthwhile if the project plans features such as:

- `ddccontrol info` with comprehensive display information;
- `ddccontrol diagnose` with structured EDID warnings;
- richer issue-report generation;
- video mode, HDR, color, or audio visibility in the GUI;
- robust monitor/profile association; or
- broader display-management functionality beyond DDC/CI.

The strongest near-term benefits are improved identity, diagnostics, and issue
reports. Rich video, HDR, audio, and color data are useful primarily if
ddccontrol deliberately expands beyond its current DDC/CI control scope.
