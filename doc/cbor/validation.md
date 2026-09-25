# Candidate validation record

The following measurements and original implementation results are historical.
The shared Rust producer introduced on 2026-09-25 is covered separately in
[producer validation](producer-validation.md#rust-producer-2026-09-25); the
performance measurements below were not repeated for that refactoring.

Executed on 2026-09-22. Final application baseline: `23b0f83`; database XML baseline:
`c4f616e` (`VERSION=20260922`). Initial audits also covered `a38bcb8`/`c0b1a51`. The database contains 470 profiles plus common options; all were
converted, including the three profiles without inherited initialization.
No connected display was read or written. The contract remains an unpublished
review candidate; [standards evidence gaps](sources.md) must be considered
before a permanent public freeze.

## Executed compatibility checks

| Check | Result |
| --- | --- |
| Full conversion | All 470 profiles; 313,219-byte uncompressed CBOR; 1,099,802 XML source bytes. Ignored attributes/text remain metadata, comments remain inactive. |
| Semantic equivalence | All 470 profiles × all/empty/filtered CAPS × strict/tolerant mode: 2,820 scenarios for each format. Complete C monitor trees, resulting CAPS and failure/required-feature status match. |
| Rust checks | Workspace tests with all features on stable and MSRV 1.77 (also 1.70 before the upstream monitor-list rebase); clippy all targets/features with warnings denied; formatting. 52 database tests, plus existing sibling-crate suites. |
| Application build | Autoreconf/configure, complete C/GTK/Rust build, `make check` and `make distcheck`; eight library C tests and the daemon test pass. |
| Frozen reader | Hash-checked first decoder/interpreter/CAPS implementation reads base and newer fixtures unchanged; new profile, code 254, value 65535, unknown optional fields/extensions. Required future matcher rejects only affected profile. |
| Invalid input | All prefixes of independent 850-byte reference file; non-shortest/indefinite encodings, tags, floating values, duplicate/out-of-order keys, invalid UTF-8, extra bytes, nesting and size errors. Required database/profile/control/value/include/CAPS effects tested. |
| Lifetime | C trees used after database release; simultaneous callers; reinitialization; file replacement; staged CAPS rollback; FFI panic containment. |
| Distribution/fallback | XML-only, CBOR-only, dual with conflicting XML, malformed/present CBOR, mismatched XML manifest, guarded missing CBOR, explicit selected directories. |
| Translations | Production `gettext` library, real French catalog, `LC_ALL=en_US.UTF-8 LANGUAGE=fr`: VESA output identical for XML/CBOR (7,992 bytes), different from C locale (7,477 bytes). Trees inspected after session release. |
| Independent schema | Unmodified cddl-cat 0.7.1 parses CDDL and validates the complete database, six frozen DB fixtures and future-description payload. Wrong magic and invalid discovery quantity rejected. |
| Big-endian execution | Actual s390x Rust reader under QEMU 10.2.1: 52 tests, including the full 2,820-scenario corpus comparison. See [exact environment](architecture-validation.md). |
| Producer/package | 23 Python tests; GNU make and BSD bmake; 51 existing control-check subtests; archive builds/installs with Python and msgfmt disabled. Failed/deleted source generation removes stale CBOR and stops build. |
| Old program | Installed ddccontrol 3.3.0 XML integrity mode succeeds with the dual package. Repository's legacy target covers 467 profiles, excludes its three preexisting no-init profiles; producer and new-reader checks cover all 470. |

The first frozen reader is checked by `scripts/check_cbor_frozen.sh` in CI. Full
external-corpus and locale checks are documented in [testing.md](testing.md).
Fixtures are not regenerated during reader tests. Producer tests independently
compare frozen encoded bytes, source snapshot hashes, locale and working
directory reproducibility. The s390x tests read those exact same binary vectors;
they do not rely on cross-compilation alone.

Malformed reference graphs deliberately differ from historic XML recovery:
the CBOR validator marks dependent profiles unavailable and prohibits generic
fallback; the XML-only legacy path retains ordinary missing/cyclic-include
failure. This conservative policy is not claimed to preserve every malformed
XML document's fallback behavior.

## Measured reader costs

Hardware-free release build on x86_64 Linux, Rust 1.98.1, 470 simultaneously
retained C profiles, synthetic all-code CAPS. Commands:

```
cargo run --release --locked -p ddccontrol-db --example database_bench -- DB_DIRECTORY
```

The comparison XML directory contains the exact same `options.xml` and
`monitor/` files, with CBOR absent. One observed run, warm filesystem cache:

| Metric | CBOR | XML |
| --- | ---: | ---: |
| File read alone | 113 µs | not separately measured |
| Strict validation + CBOR decode + owned model | 13,183 µs | not separately measured |
| Complete session initialization, subsequent measurement | 10,641 µs | 16,721 µs |
| Construct all 470 active C profiles | 59,123 µs | 59,752 µs |
| Peak requested Rust allocations above harness baseline | 10,502,866 B | 4,433,772 B |
| Process RSS high-water mark | 14,780 KiB | 14,032 KiB |
| Allocator heap delta with all C trees after session release | 6,121,408 B | 6,154,480 B |
| Allocator heap delta after freeing all C trees | 197,680 B | 229,296 B |
| Requested Rust allocation delta after freeing all trees | 1,082 B | 1,133 B |
| New session initialization and release | 11,545 µs | 16,325 µs |

The benchmark uses a counting Rust allocator, Linux `/proc/self/status`, and
glibc `mallinfo2` (`uordblks + hblkhd`). Rust totals exclude direct C `malloc`
allocations; heap deltas include them and allocator caching/overhead. Remaining
Rust bytes include the harness's C path and stdout buffer. RSS stays at its
high-water mark after freeing the session and trees because the allocator may
retain pages. These are observed process/allocator costs, not file-size RAM
estimates, a leak proof, or a promised performance improvement. The CBOR
preflight plus generic decoding creates a larger temporary allocation peak
than this XML implementation. Timings are single measurements and not a
statistically controlled speed comparison.

## Explicit limits of evidence

No physical monitor protocol execution was added or tested. Future descriptors
are representable, not executable by this reader. ARM, ppc64el and riscv64
runtime execution were not performed locally; portable CBOR vectors and real
s390x execution cover the endian-sensitive path. The [standards record](sources.md)
lists inaccessible MCCS updates and the absence of an exhaustive historical
per-code/EDID/DisplayID semantic audit. Debian dependency availability is checked
there; no Debian package architecture list or shared-library package layout was
changed in this work, so no new `dh_shlibdeps` layout is introduced.

## Validation after the scanmonitor rebase

Rebasing onto application `e22b769` retained the shared scanner options types,
XML decoding and local monitor override API. Overrides own their parsed root;
includes resolve only from the pinned database session, and installed source
changes require reinitialization. Added coverage compares a local XML override
against XML-only and CBOR sessions and checks required include rejection without
replacing an existing valid override. Upstream override tests now explicitly
check snapshot retention and reinitialization after installed source changes.

The workspace suites passed with Rust 1.77 and the current toolchain, including
**60 database tests** and all 470 profiles. Clippy with warnings denied, the
frozen consumer probe, full C/GTK/Rust build, CLI/GUI preview checks and
`make distcheck` passed. Actual s390x execution passed the same 60 tests in
55.44 seconds. Earlier performance measurements above remain measurements of
the pre-rebase implementation; they were not rerun as part of this rebase.
