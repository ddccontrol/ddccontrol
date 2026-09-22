# Coverage and compatibility audit

This matrix separates source preservation, current interpretation and hardware
execution. A test obligation is not a claim that a test passed. Actual commands,
fixtures, result counts, timings and remaining gates belong in the validation
report and test output. All loading tests use synthetic CAPS and no monitor
writes.

## Inventory and evidence

The database inventory covers all 466 monitor/profile XML files and
`options.xml`, including generic profiles and aliases. The initial audited source
snapshot at `c0b1a51` contains 1,077,652 bytes; the largest file is `options.xml` at 52,121
bytes. The maintained options.xml.in is 52,058 bytes; configure adds a
warning comment and substitutes the source date when producing options.xml.
This count is over actual generated input bytes, not a CBOR-size or RAM estimate.
There are 7 groups, 37 subgroups, 170 common controls, 594 common values,
1,574 profile controls, 3,132 profile values, 454 includes, 313 CAPS patches and
291 controls blocks. The longest include chain has 5 nodes (4 edges); all
current targets resolve and no cycle was found. Counts must be regenerated
and compared when the source snapshot changes.

The inspected sources are the current Rust database/CAPS parsers,
`src/lib/ddcci.c`, the C ABI definitions, the pre-migration C database reader
at `c695559^`, `doc/techdocs.xml`, and the full XML inventory. Common control
IDs `sharpness`, `fengine` and `colortemp` repeat. Such source duplicates are
intentional input to precedence rules, not duplicate CBOR map keys.

There are 50 profile values above 255; the largest is 65,443 (`0xFFA3` in
`DEL4206`). There are 388 explicit standard initializations and 78 absent
initializations, with no active Samsung initializer in this snapshot. The
Samsung path remains implemented and is covered by a synthetic fixture.

At least 105 comment blocks contain inactive controls. Non-whitespace ignored
tails occur in `AOC3279.xml`, `BNQ8301.xml` (literal `-->`) and `DEL40F3.xml`
(an apostrophe). These are not activated. Source metadata retains the tails.
One document declares UTF-8 explicitly; the remaining documents use the
implicit UTF-8 default. Foreign encodings and declaration-after-comment forms
are covered by synthetic compatibility input, not claimed present in this
source snapshot. The converter currently supports UTF-8, BOM-marked UTF-16,
Windows-1252 and WHATWG-compatible ISO-8859-1/ASCII aliases. It explicitly
rejects other encoding labels accepted by the broader encoding_rs reader;
this is a converter coverage gap until equivalent decoding has been tested.

## Existing behavior matrix

| Data or rule | Stored source / current understanding | Representation and required checks |
| --- | --- | --- |
| Physical IDs, generic IDs, alias names | Filename identity is independent of monitor name; `VESA`, vendor profiles and includes are accepted. | Preserve case-sensitive profile keys; generic and alias fixtures; no PNP-only key grammar. |
| Monitor name | Required for selected profile; not gettext-translated. | Own UTF-8 name after session release; empty and missing remain distinct. |
| Initialization | First encountered standard/Samsung mode; includes can provide it. Tolerant missing mode becomes standard; strict missing mode fails. | Ordered includes and synthetic Samsung case; rejecting unknown required init prevents generic fallback. |
| Common grouping and labels | Options group/subgroup/control/value order drives UI; subgroup pattern is copied. | Compare complete trees, including empty-group pruning, names and pattern absent/empty distinction. |
| Control ID versus address | ID resolves common description; numeric address is an open 8-bit VCP selector. | First ID match and first accepted address win; test duplicates and previously unused numeric codes. |
| Values and command defaults | Full 16-bit values; command fallback value 1 only when no matched value. | 0, 255, 256, 0x1234, 0xFFA3, 65535 and rejected overflow; independent decoded vectors. |
| Delay | Decimal milliseconds, missing −1; explicit 0 differs from missing. | Preserve signed 32-bit value; reject overflow rather than truncate. |
| Type and refresh | Common-control type is value/command/list; refresh absent/none or all. Profile overrides are ignored. | Do not activate profile type/name/refresh or infer new access restrictions. |
| Includes and precedence | Ordered, stateful, shared defined-address set; no pre-expansion independent of CAPS. | Multiple input CAPS sets, control-before/after patch and include-before/after local controls. |
| CAPS additions/removals | Remove then add within one patch; later patches do not revise already built controls. | Compare output CAPS and trees, including failed-load atomicity. |
| CAPS filtering | Runtime tests address presence. Profile value choices are not filtered by the hardware value list. | Preserve this behavior; test limited advertised value lists without “correcting” the database. |
| Strict/tolerant behavior | Unmatched control/value diagnostics; strict fails, tolerant ignores. Deprecated root attributes warn/fail. | Unknown source elements preserved inertly but remain visible to applicable strict diagnostics. |
| Missing references/cycles | Load errors; historical semantic recursion limit remains 16 profile levels. | Structural reference checks, cycles, repeated DAG includes and bounded work; no partial result. |
| Generic fallback | C open path tries vendor LCD/CRT then VESA after normal failure. | Required semantics rejection must prevent this path from bypassing rejection. |
| Gettext | Domain ddccontrol-db, common label msgids translated at load; converter has no translation step. | Same locale/domain/catalog on XML and CBOR paths; compare non-English output and byte-identical producer output across locales. |
| Encoding and old XML forms | Rust uses encoding_rs and normalizes leading comments/whitespace before declarations. | ISO-8859-1 plus UTF-8 fixtures; require same decoded Unicode or explicit converter failure. |
| ABI ownership | repr(C), C ABI scalars, C malloc/free trees, private 16-bit value storage behind unchanged public layout. | Access all strings/values after releasing raw file and decoded database; multiple profiles and reinitialization; FFI panic containment. |

Five control elements occur directly under monitor, outside a controls block:
`DELA125` inputsource, `BNQ7F31` inputsource/dpms and `SAM00A3` fine/coarse.
They and their values were ignored by the reader. The total of 1,574 profile
control elements includes these five; only 1,569 potentially participate in
legacy resolution. Conversion must retain the misplaced elements inertly,
without validating or activating their unused numeric attributes.

The inventory found these ignored attributes: 128 common-control `address`
attributes, 269 common-value `value` attributes, 67 profile-control `name`,
450 profile-control `type`, 17 profile-control `refresh`, 4 each profile-control
`min`/`max`, and 162 profile-value `name`. They MUST remain source metadata.
The documentation's statement that common address/value attributes provide
defaults does not match either inspected reader and is not adopted.

One historical reader difference is explicit: the C reader ignored value
children for continuous controls, while the current Rust reader parses them
for every type. `AUS28B1` sharpness has 11 such values. XML/CBOR equivalence is
against the current Rust behavior; old installed programs keep their existing
XML behavior. The format conversion does not revert current functionality.

`SAMcp10`, `SAMg9` and `SAMmb6` have no resolved initialization mode; current
tolerant loading supplies standard mode. This existing strict-mode failure is
not repaired by dropping them or editing their controls. Whole-database tests
must retain every profile and report any other pre-existing strict/tolerant
failure explicitly.

## Future descriptions and standards scope

The source references and exact editions/sections are in [sources.md](sources.md).
This table describes what the model can retain, not a new VESA conformance claim.

| Category | v1 representation | Reader understanding / execution |
| --- | --- | --- |
| New numeric VCP code or 16-bit enum value within known behavior | Base integer address/value fields, stable shared IDs. | Existing resolver/runtime paths can use them without adding a format enum. |
| Continuous, discrete and command controls | Existing common-control kinds; richer category and quantity descriptors are optional or necessary as appropriate. | Existing behavior only; a descriptor does not change it. |
| Bitfield/composite, table and block controls | Function-description categories, bitfields, encoded choices, bytes and named encodings. | Descriptor executor absent; reject affected controls when understanding is required. |
| Read/write direction, request/reply and save-settings | Separate access and operation IDs; declarative parameters. | No implicit operation dispatch from file data; existing C operations retain their behavior. |
| Bounds, steps, units, defaults | Typed quantities with declared/discover-at-runtime provenance, integer/rational/byte/text data. | Unknown necessary encoding or safety bound rejects its unit. No current hardware values written into distribution data. |
| Protocol versions and raw capabilities | Namespaced version text, raw text and announced numeric command array. | No assumption that current CAPS parser understands all tokens or version differences. |
| Model/firmware/connection conditions | Immutable namespaced condition/matcher contracts. | Necessary selection/init conditions reject entire affected profile if unsupported. |
| Vendor quirks | Namespaced extension envelopes, open numeric codes and opaque payloads. | No guessed vendor mapping and no silently ignored necessary quirk. |
| EDID/DisplayID-related future data | Namespace plus encoding identity and byte string. | Opaque retention only; no new field decoder or matcher is claimed. |

## Release gates and evidence ownership

The published-v1 gate requires frozen binary fixtures with reviewed expected
contents, an independent encoder/decoder check, an unchanged first-reader
compatibility harness, XML/CBOR differential tests on the full snapshot and
relevant CAPS scenarios, extension-scope tests, malformed-input tests,
concurrent lifetime checks, distribution combinations, reproducibility, actual
big-endian execution, and measurements in the Rust reader. A fixture generated
anew by the code under test is not frozen evidence. A generic CBOR roundtrip is
not monitor-tree equivalence. A cross-build is not big-endian execution.

The producer's source inventory and schema validation do not prove semantic
coverage by themselves. The exact verified subset and open gates must be
stated in the PR validation record; this matrix must not be used to claim an
unrun test passed. Standards review gaps in sources.md remain explicit even
when implementation tests pass.


## Final producer rebase inventory

The detailed initial inventory above remains tied to `c0b1a51`. Before PR
creation the producer was rebased onto `c4f616e` (`VERSION=20260922`). That source
adds `DEL427D`, `DEL427E`, `DEL427F`, and `GBT2709`, giving **470 monitor
profiles**, 1 options document, 7 groups, 37 subgroups, 1,755 control elements,
3,737 value elements, 459 includes, 315 CAPS elements and 293 controls blocks.
Maintained XML sources total 1,099,739 bytes; generated manifest inputs total
1,099,802 bytes. Every final profile was converted and checked by the producer.
The 313,219-byte CBOR has source snapshot
`dbb9f1542128e579f61dc8147c03c2f72eb5220d0c31e7961c001516942656b9`.
See [producer validation](producer-validation.md) for exact executed checks;
this source update does not change the shared format contract.
