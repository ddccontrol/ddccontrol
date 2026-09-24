# Frozen first v1 reader

This independent build freezes the first candidate decoder, monitor interpreter,
CAPS parser and C ABI probe. `SHA256SUMS` protects the checked-in source and lock
file. Tests never copy the current reader over these files. The production reader
may evolve without changing this historical consumer. This is a pre-publication
compatibility baseline, not a declaration that v1 has been released.

Run `./scripts/check_cbor_frozen.sh` from the repository root. The harness builds
only the frozen library, then checks the independent Python producer's frozen
binary fixtures. The same TST0001 tree survives a newer database revision;
NEW0001 uses a new VCP selector 254 and value 65535; unknown optional fields and
extensions are ignored. Unknown required matcher/operation descriptions reject
TST0001 with status 3 while VESA remains usable. Every successful tree is read
after releasing the database session.

The source files were copied once from this candidate. `ciborium`, `sha2` and
other crates are pinned in the lock file. The CAPS source is copied locally
because CAPS semantics are part of the contract. EDID, user-profile and protocol
crates are linked from the workspace to satisfy unrelated ABI exports; the
probe does not call them. There are no display-device accesses or commands.

The frozen source includes test-only modules from its original location, but
this standalone package is intentionally **built**, not run with `cargo test`.
Its compatibility tests are the external C probe and golden output files.

Rebase onto the scanmonitor integration added the workspace-only
`ddccontrol-xml` dependency of `ddccontrol-profile` to the lock file. No registry
version, frozen decoder, monitor interpreter, CAPS parser or probe changed.
The lock-file checksum was updated for that build dependency only.
