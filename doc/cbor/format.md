# ddccontrol-db CBOR format, candidate v1

Status: **unpublished review candidate**. The requirements below define the
contract being implemented and tested. They are not a declaration that every
release gate is complete. See [source evidence and gaps](sources.md),
[coverage](coverage.md), and [decisions](decisions.md). No release is authorized
by this document. The first published v1 freezes every assigned ID, type,
unit, default and semantic rule below permanently.

“MUST”, “MUST NOT”, “SHOULD” and “MAY” express requirements of this format.
The machine-readable [CDDL schema](format.cddl) describes the structural data
model. Validation also MUST enforce the encoding, context, reference, resource
and compatibility rules in this document; CDDL alone does not express them all.

## Scope and compatibility promise

A database is one ordinary uncompressed CBOR file containing common
control definitions and every profile from its XML source snapshot. XML and
gettext translation sources remain the maintained sources. Packages MUST
continue to install XML and translations alongside CBOR; there is no scheduled
XML retirement. Users do not run a database service or a converter.

**Representation** means that the format can retain a description.
**Understanding** means that a reader implements its interpretation.
**Execution** means that a runtime can perform the operation correctly on the
selected device. These are distinct. Storing a descriptor or knowing a numeric
operation ID does not authorize its execution. A v1 reader can use new profiles
and new numeric values within known semantics; it cannot promise to execute all
future hardware functionality. Necessary unknown semantics make the affected
unit unavailable, without changing the meaning of the known fields.

The base semantic contract preserves the current ddccontrol XML database model,
including its historically ignored attributes. Additional descriptions use
extensions. This change does not assign new hardware semantics to existing
controls or add hardware writes to loading.

## Encoding and validation order

The file MUST contain exactly one CBOR map and no trailing bytes. It MUST use
RFC 8949 §4.2.1 **core deterministic encoding**: definite lengths, shortest
integer/length encodings, and bytewise lexicographic ordering of the complete
encoded map keys. This is not the length-first profile in RFC 8949 §4.2.3.
For example unsigned key `24` (hex `1818`) precedes text key `""` (hex `60`),
even though its encoded key is longer. No map order implies event order.

Allowed values are standard CBOR unsigned integers (0 through 2^64−1),
negative integers (−2^64 through −1), byte strings, valid UTF-8 text, arrays,
maps, booleans, and null. Map keys MUST be unsigned integers or text. All tags,
floating-point values, bignums, undefined/other simple values, indefinite
lengths, non-shortest encodings and invalid UTF-8 are forbidden. Text MUST NOT
be normalized, case-folded or translated by the encoder. Binary content MUST
use a byte string, never native-memory or Rust-struct serialization.

A reader MUST check bounds, nesting, types, ordering and duplicate keys before
handing maps to a decoder that might overwrite entries. Strictly increasing
encoded map keys simultaneously rule out duplicate encodings; shortest
encodings and permitted key types rule out alternate encodings of the same
key. The reader MUST then validate the structural schema, manifest/snapshot,
contextual fields and references before exposing a database. Unit-level
required-feature failures follow the isolation policy below. No parse or
validation error may publish a partial monitor tree or partly modified CAPS.

## Permanent root field register

Every field except 7 is required. A required field has no implicit default.
Field 7 is absent-equivalent to an empty array. Null is forbidden in known
root fields. IDs are explicit assignments, never generated from declaration
order, name sorting or Rust enum discriminants.

| ID | Name | Type and meaning |
| --- | --- | --- |
| 0 | identity | Byte string `44 44 43 44 42`, ASCII `DDCDB`. No CBOR tag or external header. |
| 1 | format version | Unsigned integer `1`. Independent of database content and XML version. |
| 2 | revision | Nonempty UTF-8 text supplied by the producer's build/package input; no clock-derived default. |
| 3 | XML semantics | Integer `3`, selecting the legacy rules below. This is not the container version or an MCCS version. |
| 4 | gettext domain | Text `ddccontrol-db`. |
| 5 | common definitions | Options node, kind 0. |
| 6 | profiles | Map from nonempty profile ID to monitor node, kind 5. |
| 7 | extensions | Array of extension envelopes. Array order is retained; it does not authorize operations. |
| 8 | source manifest | Map from exact relative source path to a 32-byte SHA-256 digest. |
| 9 | snapshot ID | 32-byte SHA-256 digest of deterministic CBOR encoding of field 8 alone. |

IDs 10 and above are unassigned. An unknown field is optional descriptive data
and MUST NOT alter selection, initialization, safety, encoding, include effects
or control behavior. Such an effect requires a necessary extension. A consumer
MAY skip unknown fields after validating their complete restricted-CBOR shape
and bounds. An editor MUST preserve them or refuse to rewrite the database.
This same rule applies to unassigned fields of every extensible map.

A profile ID is 1–255 bytes of case-sensitive ASCII `[A-Za-z0-9_-]+`; it is a logical key,
not a filesystem path or a requirement to match a physical PNP ID. `VESA`,
manufacturer `lcd`/`crt` profiles, aliases and named common profiles are valid.
Duplicate profile keys are invalid. Alias profiles retain their own monitor
node and ordered include operations; they are not flattened.

## Nodes and attributes

Every node is a map with required field 0 (unsigned kind ID), field 1
(attributes map), field 2 (children array), and optional field 3 (extension
array, absent means empty). Fields 4 and above are unassigned optional data.
Empty attributes and children are represented by empty containers. Children
MUST remain in document order, including duplicates; a map is not a substitute.

| Kind ID | Meaning |
| --- | --- |
| 0 | options |
| 1 | group |
| 2 | subgroup |
| 3 | control, interpreted in its common-definition or profile context |
| 4 | value, interpreted in its common-definition or profile context |
| 5 | monitor/profile |
| 6 | CAPS patch |
| 7 | include |
| 8 | controls block |
| 255 | inert original unknown XML element; source-metadata extension required |

Kinds 9–254 and 256 upwards are unassigned. An unknown kind in an executable
context is an unknown necessary construct and makes the enclosing independent
unit unavailable. It MUST NOT be treated as kind 255. Kind 255 preserves an
unknown source element but grants it no execution semantics. Its original tag
MUST NOT name one of the registered executable kinds; contextual legacy
unmatched-element diagnostics still apply.

Attribute maps use the following permanently assigned IDs. Attribute presence
is preserved, including empty strings. Null is never permitted in attributes.
Numeric fields are CBOR integers, not numeric text or host-width integers.
Executable textual attributes MUST NOT contain U+0000 because the C ABI uses
NUL-terminated strings; zero bytes in opaque data belong in byte strings.

| ID | Source name | Type | Absence/interpretation |
| --- | --- | --- | --- |
| 0 | name | text | Required only where the legacy resolver consumes a name; a command option value can inherit its control name. |
| 1 | id | text | Logical, case-sensitive control/value reference; omission remains omission. |
| 2 | type | text | Common-control `value`, `command`, or `list`; no executable default. |
| 3 | refresh | text | Common-control `none` or `all`; absent means `none`. |
| 4 | pattern | text | Optional common-subgroup UI pattern; absent differs from empty. |
| 5 | init | text | Monitor `standard` or `samsung`; first encountered definition wins. |
| 6 | address | unsigned 0–255 | VCP code; no enum of known codes and no implicit zero. |
| 7 | delay | signed 32-bit integer | Milliseconds after a control write; absent gives legacy −1 (runtime default). Explicit 0 means zero delay. |
| 8 | value | unsigned 0–65535 | Complete encoded VCP value; absent is not zero. |
| 9 | file | profile-ID text | Include reference; must resolve in this same database. |
| 10 | add | text | Original raw CAPS addition. |
| 11 | remove | text | Original raw CAPS removal. |
| 12 | dbversion | standard CBOR integer | Options resolver requires 3. No default. |
| 13 | date | text | Required source options attribute; not interpreted as a protocol or format version. |
| 14 | caps | text | Deprecated monitor attribute; warning/tolerant behavior preserved, never turned into a patch. |
| 15 | include | text | Deprecated monitor attribute; warning/tolerant behavior preserved, never turned into an include operation. |

Attributes 16 upwards are unassigned optional scalar data (`int` or text).
Their absence MUST preserve existing behavior. They do not grant execution
semantics. A future attribute with necessary meaning MUST be guarded by a
required extension; existing IDs MUST NOT be reused even after deprecation.

An assigned attribute is executable only in these contexts:

| Node context | Executable attribute IDs |
| --- | --- |
| options | 12, 13 |
| group | 0 |
| subgroup | 0, 4 |
| common control | 0, 1, 2, 3 |
| common value | 0, 1 |
| monitor | 0, 5, 14, 15 |
| profile control | 1, 6, 7 |
| profile value | 1, 8 |
| CAPS | 10, 11 |
| include | 9 |
| controls | none |

The converter MUST move source attributes outside these contexts into the
source-metadata extension, even if their spelling appears in the register.
For example profile-control `type`, `name`, `min` and `max` remain inert;
common-control `address` and common-value `value` do not become defaults.
The structural node permits missing attributes because the legacy resolver
validates some attributes only when their control is selected. A structural
validator MUST NOT silently fill them. A producer MUST fail for an explicitly
present active numeric attribute that cannot be converted into its declared
range; it MUST report the source location instead of dropping the profile.

Zero is an ordinary integer. Empty text and an empty container are real values.
Absence means only the absence rule assigned to that field. Null is available
inside extension payloads/unknown data as an explicit null value and MUST NOT
be conflated with absence, an empty value or 0. Core known fields never use it.

## Frozen legacy semantic contract

The contract targets the current Rust reader and the relevant pre-migration C
reader (`c695559^:src/lib/monitor_db.c`). Historical documentation is supporting
evidence, not an instruction to activate features those readers ignore.

1. Common definitions provide group/subgroup/control/value display order.
   The reader iterates common groups, subgroups and controls in that order for
   every profile controls block. It finds the first profile control with the
   matching ID, and the first matching profile value for each common value ID.
   Duplicate source IDs MUST remain in arrays. Duplicate address definitions
   are resolved by the first accepted control for that VCP address.
2. The selected profile supplies its name. Initialization uses the first
   encountered recognized `init`, including includes. If none is found,
   tolerant mode uses `standard`; strict mode fails. An unknown active init
   value is an error. Loading selects an init type but sends no command.
3. Monitor children execute sequentially. A CAPS element applies `remove`
   before `add`; at least one attribute must exist. An include visits the
   target at that exact position with the current CAPS and defined-address
   state. A controls block observes CAPS at that position. A second controls
   block in the same profile is an error. A profile requires controls or an
   include; a CAPS-only profile is not accepted by the legacy resolver.
4. A control is available only if its address is in the current CAPS. The
   current reader filters controls by address, not the profile's listed values
   by advertised value subsets. A later CAPS change does not retrospectively
   add or remove an already built control. Do not “improve” this as part of
   changing the container.
5. Lists retain complete 16-bit values. A command with no matched values gets
   legacy value 1 and its control name. A command value without a common name
   inherits the common control name; non-command values require their name.
   Missing address/value fields are not obtained from common defaults.
6. Missing delay yields −1; the runtime applies its existing default. Negative
   source delays are retained as signed values. The converter uses the
   legacy base-0 integer grammar for address/value/dbversion (decimal, `0x`
   hexadecimal, leading-zero octal; optional sign/leading ASCII whitespace)
   and decimal delay grammar. Out-of-range numbers are rejected, never wrapped.
7. Unknown/unmatched profile control/value elements cause a diagnostic and a
   strict failure; tolerant mode ignores them. Deprecated monitor `caps` and
   `include` attributes fail strictly and warn tolerantly. Unknown elements
   outside these diagnostic contexts remain inert. Empty groups/subgroups are
   pruned after successful construction. Commented XML controls stay inactive.
8. Includes MUST resolve in this container. Missing references and cycles make
   the referring profiles and their dependants unavailable; unaffected profiles
   remain usable. The producer rejects a source graph containing such errors.
   Never open a path outside the selected snapshot. Graph validation
   applies the resource bounds below before expansion. Legacy semantic loading
   retains its historical maximum of 16 visited profile levels (root level 0,
   deepest allowed level 15); exceeding it returns the legacy loading error.
   The larger wire graph bound is not permission to change legacy behavior.
9. Common names are gettext msgids translated at profile construction using
   the root domain and the caller's locale. Monitor names and pattern strings
   retain existing untranslated behavior. The converter MUST NOT call gettext
   or depend on its locale. Translations are compiled and installed from the
   same package source snapshot; no generated localized text is put in CBOR.

The XML reader accepts encoding declarations recognized by its `encoding_rs`
path and existing leading-comment/whitespace-before-declaration normalization.
Conversion MUST produce the same Unicode strings for accepted input or reject
unsupported input explicitly. It MUST NOT guess another encoding silently.
Comments and formatting whitespace may be omitted. Ignored non-whitespace text,
tails and unknown attributes MUST be retained in inert source metadata.

## Extension envelope and feature negotiation

Every extension is a map with required fields:

| ID | Meaning |
| --- | --- |
| 0 | Nonempty absolute URI identifying an immutable semantic contract under an authority-controlled namespace, or a globally unique URN. |
| 1 | Boolean `required`; never defaulted and never inferred from whether a payload is parseable. |
| 2 | Payload using the restricted CBOR data model. |

Fields 3 upwards are unassigned optional descriptive data. The same identity
MUST always mean the same contract. A change of meaning requires a new identity,
not a changed payload interpreted under the old identity. Each extension array
MUST contain at most one envelope per identity, including unknown optional
identities. The same identity MAY occur at separate scopes. An extension needing
repeated declarations MUST represent them inside its single payload according
to that identity's contract. Extensions are data, not a script engine,
code-loading mechanism or general executable bytecode.

The envelopes at root, profile and control levels declare the features used
there and which are necessary. A reader's supported set consists of identities
whose complete applicable semantics it implements. Recognition of the URI or
ability to retain its payload is not sufficient. The first reader supports no
new identity as necessary executable semantics; the base legacy behavior is
selected by root field 3. Routine profile additions MUST NOT add a global
minimum-feature requirement.

| Location of unknown required semantics | Required response |
| --- | --- |
| Root/common options or a shared grouping affecting all interpretations | Reject the database/session with a diagnostic. |
| Monitor, profile matcher, initialization, CAPS patch, controls block, include effect, or another condition affecting the remaining profile | Make that profile and profiles that include it unavailable. |
| Independent control or one of its values/encodings | Make that VCP address unavailable throughout the assembled profile: remove earlier definitions and block later definitions/includes. If the address is missing/invalid and isolation cannot be proved, reject the profile. |
| Optional description with no behavior effect | Validate its generic payload and skip understanding it. |

A necessary matcher or safety condition MUST NOT be demoted to metadata.
A necessary control requirement that cannot be isolated to one control
(e.g. changes initialization or other controls) MUST be placed at profile
scope and reject that profile. Other unaffected profiles remain usable.
No reader may apply known operations using altered meanings after skipping
an unknown required encoding, condition, operation or include effect.

A rejected profile MUST NOT silently fall back to its XML representation or to
a generic profile that bypasses the same requirement. A failed independent
control MUST NOT reappear through another include. Consumers may skip unknown
optional fields; converters and editors MUST round-trip unknown fields,
identities, payloads and ordering without loss or reject the rewrite. The
XML-to-CBOR producer is not a lossy CBOR-to-XML editor.

### Source metadata identity

`https://ddccontrol.sourceforge.net/cbor/ext/source-metadata/1` is always
optional. Its payload fields are 0: ignored-attribute map (text names to text
values, required), 1: original unknown tag name (optional except required for
kind 255), 2: original non-whitespace element text (optional), 3: original
non-whitespace tail text (optional). Missing text differs from empty text.
These data are preserved for maintainers and diagnostics and MUST NOT affect
hardware operations. Comments are outside this retained data model.

### Function description identity

`https://ddccontrol.sourceforge.net/cbor/ext/function-description/1` supplies
stable general descriptive types. Its exact payload is `function-description`
in the CDDL. All fields are optional; omission means “not described”, never a
hardware default. Empty collections mean no declarations in that collection.
Null is not a valid known field. Unknown fields are retained as descriptive
restricted-CBOR data. The identity can be optional only when the entire
description is informational and does not change behavior. It MUST be required
if any operation, matcher, access restriction, encoding, condition or quirk
must be understood to operate safely/correctly. The initial reader does not
execute any description from this extension.

| Payload ID | Meaning, type and units |
| --- | --- |
| 0 | Category unsigned ID: 1 continuous, 2 discrete, 3 command, 4 bitfield/composite, 5 table, 6 block. |
| 1 | Access unsigned ID: 0 unspecified, 1 read, 2 write, 3 read/write. This is separate from operation type. |
| 2 | Ordered array of declarative operation maps; order retains author intent but does not cause automatic execution. |
| 3 | Vendor/standard namespace URI; no implicit vendor inferred from a code. |
| 4 | VCP code, unsigned 0–255; no closed enum of standardized codes. |
| 5 | Unit URI, or absent for unspecified; an identified unit's meaning is immutable. |
| 6, 7, 8, 9 | Minimum, maximum, step and default respectively, each a quantity described below. |
| 10 | Ordered encoded-choice array; each choice has field 0 datum and optional field 1 gettext msgid. |
| 11 | Ordered bitfield array: field 0 unsigned least-significant bit offset, field 1 width 1–64 bits, optional field 2 choices, optional field 3 msgid. No host endianness is implied. |
| 12 | Array of protocol-version maps, field 0 namespace URI and field 1 version text. MCCS and DDC/CI are separate namespaces; versions have no global ordering. |
| 13 | Author-declared raw capabilities text, preserved without assuming parser completeness. |
| 14 | Author-declared command-code array, unsigned 0–255 in the described protocol's namespace. |
| 15 | Conditions array; each map contains field 0 immutable condition URI and field 1 restricted-CBOR payload. |
| 16 | Opaque descriptor array; each map contains field 0 namespace URI, field 1 encoding URI and field 2 bytes. Suitable for author-declared EDID/DisplayID data; no decoder is implied. |
| 17 | Gettext msgid, translated in the root domain when understood. |
| 18 | Array of namespaced quirk envelopes, with the same necessity rules. |
| 19 | Matchers array using the condition shape; model, firmware, protocol, connection and version matching require their own implemented contracts. |

A quantity is either `{0: 1, 1: datum}` (database-author declaration) or
`{0: 2}` (must be discovered at runtime). The latter MUST NOT contain an
observed value. A datum is an integer, UTF-8 text, bytes, or the exact rational
map `{0: numerator, 1: positive-denominator}`. Integers use CBOR's full integer
range; denominator is 1 through 2^64−1. These general data are not all forced
into 16-bit VCP values. Units come from field 5/identified encodings; absence
never implies percent or another guessed unit. Distributed profiles MUST NOT
incorporate observations of an individual user's connected monitor.

An operation map contains field 0 operation ID, optional field 1 unsigned
0–255 selector, optional field 2 declarative parameters, optional field 3
encoding URI, and optional field 4 conditions array. Fields 5 upwards are
unassigned optional descriptive data. No missing field grants an executable
default. The permanent operation register is:

| ID | Operation class |
| --- | --- |
| 1 | Read a VCP feature. |
| 2 | Write a VCP feature. |
| 3 | Save current settings. |
| 4 | Read table data. |
| 5 | Write table data. |
| 6 | Explicit request/reply transaction described by its encoding identity. |

Zero and IDs 7 upwards are unassigned, retained as open numeric values and
never executed without support. These are ddccontrol descriptor IDs, not the
DDC/CI wire command byte. [Source evidence](sources.md) identifies the relevant
standards and sections. Declarative parameters cannot contain an expression
language whose evaluation is implicitly required. New operations require an
implemented immutable extension contract, not an expanded general interpreter.

## Limits and growth

These are normative wire bounds. Byte counts are octets; container lengths
are item counts (map length counts pairs). Readers MUST check them before
allocation or indexing, using checked arithmetic.

| Resource | Maximum |
| --- | --- |
| Entire CBOR file | 268,435,456 bytes (256 MiB) |
| One text string | 1,048,576 UTF-8 bytes (1 MiB) |
| One byte string | 16,777,216 bytes (16 MiB) |
| CBOR nesting depth | 64, root at depth 0 |
| Total CBOR items including keys | 4,000,000 |
| Entries in one array or map | 1,000,000 |
| Profile records | 65,536 |
| Include graph depth | 256 profiles on a path |
| Include visits during one expansion | 1,000,000 |

The inventoried baseline has 466 profiles, approximately 1.03 MiB of source
XML, and a largest source file of 52,121 bytes. The profile bound is over 140
times this baseline, the file bound over 249 times the raw source size, and
the single-text bound allows about twenty times the entire largest source
file. These are independent bounds, not a claim that every maximum can fit
simultaneously. See the coverage inventory for exact source identity.

A deployment MAY impose a lower tunable memory/work budget, but MUST identify
a resource-limit rejection distinctly from an invalid format. A lower budget
is not a new wire restriction. Implementations SHOULD permit operators to
raise it within the normative bounds. Normal database growth MUST NOT be
handled by reducing the v1 bounds or globally requiring new semantics.
The legacy depth-16 resolver behavior above is a separate semantic constraint.

## Snapshot identity, generation and dual distribution

Field 8 contains exactly `options.xml` plus `monitor/<profile-id>.xml` for
every profile, no absolute paths or `.`/`..` components, and no other entries.
Each digest is SHA-256 of the exact source bytes, before Unicode decoding,
comment removal or semantic conversion. Field 9 is SHA-256 of deterministic
CBOR encoding of this manifest map alone; it does not include itself. The
revision field is independently supplied; it is not substituted for the
snapshot digest. This is content identity, not an authenticity signature.

The producer MUST emit byte-identical output for identical source bytes and
revision regardless of current directory, clock, locale and CPU architecture.
It MUST sort only unordered maps/files, never source child arrays. Source
filenames enter the manifest only as the prescribed relative paths. Source
dates are copied, not regenerated from the build clock. XML numeric spelling
may normalize to the same integer while the manifest still records exact
source-byte identity.

Generation MUST fail the package build on any conversion/validation failure.
It MUST NOT leave an older successful CBOR file paired with newer XML as a
successful build result. Output is written to a temporary file and atomically
replaced only after complete validation. Source distribution archives SHOULD
include generated CBOR and its snapshot manifest so installing a release does
not require generator development dependencies. All XML, CBOR and compiled
gettext catalogs MUST be built from the same package source snapshot.

The installed files are `ddccontrol-db.cbor` and `ddccontrol-db.snapshot`.
Every dual-format package MUST install the sidecar. Its permanent type is
`source-manifest / false`:

- A manifest map is the deterministic CBOR encoding of field 8 **and an
  affirmative producer assertion that every installed definition has equivalent
  executable behavior under legacy XML semantic contract 3**. Hash agreement
  alone does not establish that assertion; the producer must establish it before
  authorizing XML fallback. The current XML converter creates only legacy
  definitions and can make that assertion.
- The CBOR value `false` (the single byte `f4`) means **XML fallback is
  prohibited: this snapshot requires its CBOR database**. A producer MUST use
  this guard whenever a necessary matcher, initialization, operation, coding,
  safety condition or other behavior lacks an equivalent legacy XML meaning.
  The conservative reference producer uses the guard for any required
  extension. Future producers MUST preserve this rule, even when the required
  extension affects only one profile. They MUST NOT omit the sidecar or write
  a manifest-only authorization for such a bundle.

When CBOR is missing, a reader MUST reject a guarded sidecar before publishing
any XML session. The original manifest-only validator also rejects this
non-map value, so the guard does not depend on a later decoder update. A valid
CBOR session continues to isolate unsupported features by profile/control;
this global guard only governs the exceptional XML fallback path. Invalid
sidecars also fail initialization. Neither installed file may be searched for
outside the explicitly selected database directory.

A package missing both CBOR and its mandatory sidecar is incomplete, not a
conforming new XML-only package. Its remaining files can be indistinguishable
from an old XML-only package, which remains supported; no reader can recover
removed requirements from absent metadata. Package integrity must preserve
these files. Pre-v1 XML-only applications do not inspect the sidecar at all.
Future producers therefore MUST also keep the installed legacy XML safe for
those applications, for example by leaving unsupported new operations inactive
in the legacy view, or reject dual publication. The guard does not add feature
understanding to an older XML executable.

## Selection, errors and session lifetime

The reader uses the caller-selected data directory, or its existing default
only when the caller supplied none. An explicitly selected XML test directory
MUST NOT be overridden by an installed CBOR database elsewhere.

1. A present CBOR file is preferred and fully validated. Its common definitions
   and profile records are one immutable snapshot. It does not require XML to
   remain readable after loading; its manifest identifies its source snapshot.
2. If CBOR is absent, inspect the sidecar authorization above. `false` prohibits
   fallback and fails initialization; a manifest authorizes fallback only after
   its complete exact-byte inventory verifies, including missing and extra
   files. Older XML-only packages without a sidecar remain supported using the
   existing XML semantic version. Pin the complete XML set; the session MUST
   NOT reopen individual profiles during later construction.
3. A present malformed/unsupported CBOR file fails initialization with a clear
   diagnostic. **There is no automatic XML fallback for this condition.**
   This conservative policy avoids bypassing unknown requirements or choosing
   XML from a different revision. Unsupported root features also fail the
   whole session. Profile/control feature failures follow the narrower
   isolation table; no per-profile XML retry is allowed.
4. XML snapshot acquisition checks both complete inventories and bytes across
   capture and verification. A concurrent change, read failure or manifest
   mismatch fails acquisition; no old options/new monitor mix is published.
   This protects ordinary package updates. A hostile writer able to replace
   and restore arbitrary files between checks is outside the consistency
   guarantee; package authenticity is not provided by a content hash.
5. In a successful session, original CBOR bytes are read once. Active calls
   retain shared ownership of the immutable decoded snapshot. Releasing the
   global session cannot free storage still used by a call. New scans/reopens
   can start a new session. Reinitialization does not invalidate previously
   constructed monitor trees.
6. Completed C monitor trees independently own every name, pattern, control
   and 16-bit value. They retain no pointers into the CBOR buffer, decoded
   objects or XML storage. Existing C layouts and malloc/free ownership rules
   remain unchanged. Releasing the last session reference frees the database;
   monitor trees remain usable until their existing free function is called.
7. CAPS construction is staged privately. Only a successfully completed tree
   commits CAPS to the caller; failure returns no partial tree and leaves the
   caller's CAPS unchanged. Panics MUST NOT cross the C ABI. Loading itself
   MUST never send hardware commands.

Fallback from a normally missing profile to manufacturer/VESA profiles remains
an application behavior within the pinned session. A profile rejected because
of unknown necessary semantics is a different error and MUST prevent generic
fallback from bypassing that requirement. Explicitly selecting an unrelated,
unaffected profile in another operation remains permitted.

The explicit `--monitor-file` developer override validates and owns its XML root
before publication. It replaces only the matching top-level profile; every
include still resolves from the pinned database, including an include of that
same profile ID. The override is not an automatic fallback. Required semantics
in included CBOR profiles and shared controls retain their normal rejection or
isolation rules. Changes to installed include files become visible only after
starting a new database session and validating the override again.

## Lossless diagnostic JSON

A human-readable JSON dump MUST represent types explicitly: a map as an
ordered `{"$map": [[key, value], ...]}` pair array and bytes as
`{"$bytes": "lowercase hexadecimal"}`; keys are recursively encoded values,
not JSON object member names. Integers use JSON integer tokens without
floating conversion. Text, booleans, null and arrays preserve their native
JSON shapes. Thus integer key 1 differs from text key `"1"`, and a byte string
cannot be mistaken for user text. JSON tooling that cannot preserve the full
CBOR integer range MUST reject the operation or use a documented typed-integer
wrapper; it MUST NOT round through IEEE-754 numbers. A dump is diagnostic,
not a substitute normative file format.
