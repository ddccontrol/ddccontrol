# Format decisions

| Decision | Reason and consequence |
| --- | --- |
| Keep XML as maintained source and installed format indefinitely. | Existing applications and developer XML workflows remain usable. Packaging owns generation; users do not initialize a database. |
| One uncompressed CBOR value, with common definitions and all profiles. | A session pins one data identity; package compression remains independent of the format. |
| Preserve an ordered contextual source model. | Includes and CAPS edits are runtime-dependent. Flattening a profile would change precedence/filtering and lose strict diagnostics. Inert source attributes preserve auditability. |
| Explicit numeric field/kind/operation registers. | IDs survive implementation refactors and new standard functions. They are not generated from source order or names. |
| RFC 8949 core deterministic ordering. | Exact encoded-key lexical comparison is independently testable. A library's ambiguous “canonical” option is not the specification. |
| Restricted CBOR with a strict preflight before ciborium. | Tags, floats and indefinite lengths add no required value here. Duplicate rejection and limits cannot be delegated to a map that overwrites duplicates. |
| Shared Rust format library and XML decoder for reader and producer; ciborium 0.2.2. | One implementation maintains validation, wire IDs and legacy decoding. Independent CDDL validation, fixed RFC vectors and frozen original-reader fixtures remain separate checks against shared mistakes. Debian stable packages the codec dependency for the required architectures. |
| SHA-256 of a separately defined source manifest. | The snapshot has no circular hash. Exact source bytes, including ignored text, determine identity. It is reproducibility/consistency metadata, not a signature. |
| Separate container, legacy semantics, database revision and protocol versions. | XML dbversion does not become the wire version, and MCCS versions are not assumed to form a universal monotonic feature level. |
| Necessary semantics use immutable extension identities. | Old readers can retain new descriptions and use unaffected profiles while rejecting unknown behavior at its smallest independent scope. |
| Declarative future operation descriptions only. | Representation does not imply hardware support. There is no script engine or promise to execute arbitrary future functions. |
| Reject present invalid/incompatible CBOR instead of automatically retrying XML. | This avoids semantic bypass and revision mixing without guessing whether the XML reader would understand a requirement. Missing CBOR still supports old XML-only packages. |
| A manifest sidecar authorizes XML fallback; the permanent false guard prohibits it. | Future required semantics cannot be bypassed merely by removing CBOR. New dual packages must retain the sidecar; pre-v1 XML executables still require a safe legacy view. |
| Pin the complete XML fallback snapshot. | Later profile creation cannot read a newer file into older common definitions. Sidecars verify new packages; repeated capture detects normal updates for old packages. |
| Stage CAPS and own completed C trees. | Failure does not mutate the caller or expose a partial profile. Session buffers can be released while monitor objects survive. |
| Unpublished candidate status. | Actual standards updates/historical review, compatibility gates and architecture/measurement evidence must be assessed before a permanent public freeze. Creating a PR is not publishing a stable format release. |

Remaining evidence gaps are listed in [sources.md](sources.md) and
[coverage.md](coverage.md). Known legacy errors are preserved or reported rather
than hidden by omitting profiles. Packaging-side source generators and the
reader-side contract must agree before either side can be released. Future
changes must use the registered extension mechanism instead of changing the
meaning of an existing field.
