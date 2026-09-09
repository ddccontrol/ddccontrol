# ddccontrol-protocol

Pure DDC/CI request framing, length validation, frame checksums, and VCP/CAPS
reply parsing. This crate has no dependencies, unsafe code, heap allocation,
I/O, logging, timing, retries, or monitor state. Frames use fixed-size storage;
parsed replies borrow the supplied byte slice.

`build_frame` takes a seven-bit I2C address and a payload. `frame_length`
rejects payloads above 127 bytes before adding the three framing bytes.
`parse_frame` takes the actual received bytes and a caller-supplied payload
limit; it validates the source, declared length and checksum before returning
the payload. The maximum frame size is 130 bytes, including the checksum.

The C transport calls this crate through the existing `ddccontrol-db` static
library and the internal `src/lib/ddcci_protocol.h` interface. Buffers remain
owned by C and error returns leave output buffers unchanged. FFI uses C integer
types and pointers; no Rust layouts cross the ABI. Multi-byte wire values are
explicitly big-endian, including on big-endian hosts such as Debian s390x.

Compatibility behavior is deliberate and covered by tests:

- Valid empty (null) replies are accepted at the frame layer.
- Padding after the declared checksum is ignored, as required by fixed-size reads.
- The missing length flag seen on Fujitsu Siemens P19-2 and NEC LCD 1970NX is
  accepted and reported to the caller for the existing diagnostic.
- Any nonzero VCP result means unsupported, the VCP type byte remains ignored,
  and values are returned even for unsupported controls, matching the C API.
- CAPS fragments preserve embedded NULs. An empty fragment is a terminator only
  when the reply command and requested 16-bit offset match.

The existing C code still owns I2C ioctls, Linux/FreeBSD return conventions,
delays, retries, CAPS assembly, and monitor-specific initialization.

## Tests and fuzzing

Run the pure protocol tests without hardware:

```sh
cargo test -p ddccontrol-protocol
```

`make check` also runs C ABI and in-memory transport regression tests, including
a full 130-byte reply and output preservation after invalid replies. The ABI
tests run even when I2C support is disabled.

With `cargo-fuzz` and a nightly Rust toolchain installed, run from this directory:

```sh
cargo +nightly fuzz run protocol -- -max_len=256 -max_total_time=60
```

The target exercises arbitrary frames and payloads, bounds on output lengths,
request checksums, and generated valid replies. It needs no device access.
The fuzz package has its own workspace; `libfuzzer-sys` is an optional developer
dependency and is not part of the normal workspace build or Debian dependencies.
Fuzz corpus, artifacts and build output are ignored by Git.
