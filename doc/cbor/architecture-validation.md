# Executed big-endian validation

On 2026-09-22 the Rust database tests actually executed as Linux s390x
instructions under QEMU user emulation on the x86_64 development host. This
was not only a cross-compilation. `file` identified the test executable as
`ELF 64-bit MSB pie executable, IBM S/390`, interpreter `/lib/ld64.so.1`.

After synchronizing the application onto upstream `23b0f83` and the database
onto release baseline `c4f616e` (20260922), the final database-crate run passed
**52 tests**. These cover the frozen CBOR reference, independent-producer byte
comparison, 16-bit values, strict decoder errors, FFI layouts, extension
isolation, staged CAPS, guarded XML fallback, Samsung initialization and
profile lifetime, plus three upstream monitor-list tests. With the full source
directory configured, the same run compared all **470 profiles × three CAPS
inputs × strict/tolerant modes (2,820 comparisons)**. The synchronized corpus
contains **313,219 bytes** and independently passes the unchanged CDDL schema.
The complete run finished in **56.04 seconds** under emulation.

Before that synchronization, the 466-profile source snapshot also passed all
49 then-current tests, including 2,796 XML/CBOR comparisons, in 52.66 seconds.
Its 311,775-byte generated corpus passed the same schema. The four newly added
profiles are `DEL427D`, `DEL427E`, `DEL427F` and `GBT2709`; the final run includes
them rather than relying on the earlier snapshot's result.

These are test execution times, not production performance measurements. An
unconfigured whole-database test returns early; that alone was not counted as
corpus proof.

Environment:

- Rust 1.98.1, commit `48a229cea`, compiler date 2026-09-01;
  `rust-std` s390x component from the 2026-09-03 distribution manifest.
- Component SHA-256:
  `2b17f570083cc6dac5c3bdb762e64f75813f62e3668c5dfaefee546d10380d4b`.
- QEMU user 10.2.1, Ubuntu package `1:10.2.1+ds-1ubuntu3.2`.
- Cross GCC 15.2.0 (`15.2.0-16ubuntu1cross1`), binutils 2.46-3ubuntu2,
  cross glibc 2.43-2ubuntu2cross1.

The tools were extracted without privileges into
`/tmp/ddccontrol-s390x/sysroot`. The linker wrapper invokes cross GCC with
`--sysroot=/tmp/ddccontrol-s390x/sysroot` and sets its host library search path
to that prefix's `usr/lib/x86_64-linux-gnu` for cross-binutils. The QEMU loader
prefix is the extracted `usr/s390x-linux-gnu` directory. No Docker architecture
assumption or host-native struct serialization was involved.

The commands used, with the repository path abbreviated only here:

```sh
export CARGO_TARGET_S390X_UNKNOWN_LINUX_GNU_LINKER=/tmp/ddccontrol-s390x/linker
export CARGO_TARGET_S390X_UNKNOWN_LINUX_GNU_RUNNER='/tmp/ddccontrol-s390x/sysroot/usr/bin/qemu-s390x -L /tmp/ddccontrol-s390x/sysroot/usr/s390x-linux-gnu'
cargo test -p ddccontrol-db --target s390x-unknown-linux-gnu
DDCCONTROL_DB_TEST_DATADIR=../ddccontrol-db/db \
DDCCONTROL_DB_TEST_ALL=1 \
  cargo test -p ddccontrol-db --target s390x-unknown-linux-gnu
```

The actual corpus invocation used the absolute ddccontrol-db source directory
because the test process resolves a relative path against its crate directory.
Use an absolute path when reproducing it. The post-build cleanliness script
reported the expected in-progress source changes, not generated build outputs;
the formatting script reported every targeted file unchanged. Final clean-tree
verification belongs after the reviewed implementation is committed.

No actual arm64, ppc64el or riscv64 execution is claimed here. No hardware
commands were sent. The final integration record must repeat affected
architecture checks if further code changes alter the behavior covered here.

## Scanmonitor rebase validation

After rebasing onto `e22b769` (scanmonitor and local XML preview), the same
s390x/QEMU setup executed all **60 database tests**, including all 470 profiles
and the new local XML override / CBOR include / required-extension test.
All passed in **55.44 seconds**. The corpus and frozen decoder were unchanged.

## Shared Rust producer validation, 2026-09-25

The refactored reader executed 56 tests under s390x/QEMU in 55.29 seconds,
including all 2,820 XML/CBOR comparisons. Seven decoder tests moved unchanged
to the shared format crate and also passed. Seventeen producer tests, including
the complete source attribute/order audit and fixed integer/endian vectors,
passed under emulation in 5.23 seconds. Five integration tests which directly
spawn the CLI were excluded from that s390x test executable because this setup
uses an explicit QEMU runner rather than a system binfmt handler; the complete
22-test producer suite ran natively on x86_64.

The s390x producer executable was separately invoked through QEMU to convert
all 470 XML profiles. Both its 313,219-byte CBOR and snapshot file compare
byte-for-byte with the native Rust output and original independent producer.
`file` identifies the executable as `ELF 64-bit MSB pie executable, IBM S/390`.
The compiler, QEMU and cross compiler versions are the same as above; tools
were extracted under `/tmp/ddccontrol-rust-s390x/sysroot`. The previously
installed s390x Rust standard library component was reused.

The native static release artifact was built and executed on x86_64 with
Rust 1.85.0 and musl. The PR workflow also executes that artifact check on a
native arm64 runner. No local ppc64el/riscv64 execution or new performance
measurement is claimed by these checks.
