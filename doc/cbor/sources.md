# Source evidence and standards limits

Research date: 2026-09-22. This is a review candidate, not a declaration of
DDC/CI or MCCS conformance. The implementation does not add hardware operations.

## Documents inspected

| Source | Edition and inspected sections | Evidence used |
| --- | --- | --- |
| [RFC 8949](https://www.rfc-editor.org/rfc/rfc8949.html) | December 2020, §§1.2, 3, 3.1, 4.2.1, 4.2.3, 5.3.1, 5.6.1, 6, 10 | CBOR types, byte order, deterministic encoding, map-key validity and resource checks. |
| [RFC 8610](https://www.rfc-editor.org/rfc/rfc8610.html) | June 2019, §§2, 3.5, 3.6, 3.8, 3.9 | CDDL notation, ranges, extensibility, control operators and CBOR types. |
| [VESA catalog](https://vesa.org/vesa-standards/) | Accessed on the research date | Lists DDC/CI 1.1, MCCS 2.2a and MCCS Updates. The catalog alone supplies no protocol semantics. |
| [VESA DDC/CI specification, mirrored PDF](https://manuals.plus/m/a0df82d979045b2d6656a50b7ecd2aca14ef580da7444937b16e3dfcd2e8a572.pdf) | Version 1.1, 29 October 2004, 42 pages | VESA-authored primary document inspected through a third-party mirror; no assertion that the mirror is VESA's current distribution. |
| [VESA MCCS specification, mirrored PDF](https://milek7.pl/ddcbacklight/mccs.pdf) | Version 2.2a, 13 January 2011, 131 pages | VESA-authored primary document inspected through a third-party mirror. |

DDC/CI 1.1 defines communication (§1 and §4). Get VCP and its reply are
§4.3; Set VCP is §4.4; Save Current Settings is a separate operation (§4.5).
Capabilities request/reply is §4.6; table read/write is §4.8. The revision
history, page 3, records addition of table-class commands in 1.1. These
references justify separate declarative operation IDs; they do not establish
that ddccontrol implements every operation.

MCCS 2.2a describes control semantics separately from transport (cover summary).
Access is RO/WO/RW (§1.4.2); functions include continuous, non-continuous and
table (§1.4.3, §4.3). Two-byte and four-byte data are distinguished (§1.4.6,
§4.1). Manufacturer codes occupy E0–FF (§4.3.4, §8.8). Capability strings
include version, model and command information (§6), can depend on input,
and do not fully describe bitfield support (page 24). Version-dependent
interpretation is required (§5). The cover and revision history (page 11)
explain that 2.2a incorporates changes from 3.0 while preserving 2.1
compatibility. Therefore version labels cannot be treated as a globally
increasing feature level. These are representational requirements, not a
claim that all VCP definitions have been audited.

## Concrete evidence gaps

VESA's [official download route](https://fs16.formsite.com/VESA/form714826558/secure_index.html)
requires a registration form with a person's name, company, country and email.
No identity was submitted and no account was created. The mirrored documents
were readable, including the relevant tables, but were not compared byte for
byte with files supplied by VESA. The current MCCS Updates document was not
obtained. Standalone editions MCCS 1, 2.0, 2.1 and 3.0, DDC/CI 1.0, and the
relevant EDID/DisplayID editions were not fully reviewed. There is no
per-code/version compliance matrix or audit of every operation in MCCS §8.
The extension model preserves opaque descriptions while those definitions are
researched. No concrete EDID/DisplayID field decoder, universal matcher,
bitfield executor, table executor or firmware quirk is standardized by this
change.

The missing update and historical-version review prevents claiming full
standard coverage or treating this candidate as release-ready. Before a
published v1 freeze, maintainers must close the applicable gaps or explicitly
narrow the supported normative scope with evidence. No later extension may
reinterpret the already assigned wire fields.

## Dependency evidence

[Debian trixie lists ciborium 0.2.2-2](https://packages.debian.org/trixie/rust/librust-ciborium-dev)
on amd64, arm64, ppc64el, riscv64 and s390x. Its dependency list names the
ciborium-io, ciborium-ll and serde source packages. Package-site links labelled
“Package not available” for feature-qualified virtual packages are not proof
that those virtual dependencies cannot be satisfied; an actual Debian build
remains the packaging check.

The [ciborium 0.2.2 low-level API](https://docs.rs/ciborium-ll/0.2.2/ciborium_ll/)
provides explicit CBOR headers and container lengths. The selected reader uses
ciborium 0.2.2 after an application preflight: a generic decoder alone does not
enforce this contract's shortest encodings, lexical key ordering, duplicate
rejection, forbidden types or limits. Do not deserialize into an ordinary map
before rejecting duplicates. The producer has an independent Python codec.

[Debian sid lists minicbor 2.3.0-1](https://packages.debian.org/sid/rust/librust-minicbor-dev)
on the required architectures, but that does not establish availability of
that version in stable. Choosing ciborium 0.2.2 avoids requiring this newer
package. No Rust struct layout or derive-field ordering defines wire IDs.
Actual cross-architecture execution results belong in the validation report;
a package listing or successful cross-compilation is not such a result.

The transitive codec packages were checked separately:
[ciborium-ll 0.2.2-2](https://packages.debian.org/uk/trixie/rust/librust-ciborium-ll-dev)
and [ciborium-io 0.2.2-1](https://packages.debian.org/uk/trixie/rust/librust-ciborium-io-dev)
also list all five architectures. For hashing,
[trixie packages sha2 0.10.8-1](https://packages.debian.org/stable/rust/librust-sha2-dev).
The crate constraint `sha2 = "0.10"` admits that version. A Debian build must
resolve against Debian's packaged crates; a newer upstream Cargo.lock entry
alone does not establish that the same lockfile can be used offline in trixie.
The optional independent CDDL checker under schema-check is a developer tool
and is not added to runtime/packaging dependencies.
