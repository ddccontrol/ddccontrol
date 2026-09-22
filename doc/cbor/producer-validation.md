# Producer validation, 2026-09-22

Source baseline: `ddccontrol-db` commit `c4f616e`, `VERSION=20260922`.
Host: Linux x86_64, Python 3.14.4. This is an unpublished candidate review;
no release was created.

The maintained sources have 470 monitor XML files plus `options.xml.in` and
1,099,739 bytes. Generated `options.xml` adds its existing warning and replaces
`$DATE`, giving 1,099,802 exact manifest-input bytes. Every source profile is
converted. The four profiles added by the final upstream rebase are
`DEL427D`, `DEL427E`, `DEL427F`, and `GBT2709`. The CBOR file is 313,219 bytes and its manifest snapshot is
`dbb9f1542128e579f61dc8147c03c2f72eb5220d0c31e7961c001516942656b9`.

The full-file SHA-256 is
`8315983f0ca09e8230e3cbb92b71e4cb5f0fa780634fa33a47179eb4ce36c813`.

Executed successfully:

* `./configure --prefix=/usr`, GNU `make -j2`, `make check`.
* `make check-controls`: all 470 profiles pass with 30 preexisting grandfathered
  empty-list exceptions; 51 checker regression subtests pass.
* `make check-cbor`: 23 Python test methods pass, including all attributes/order
  for the complete database, 850 distinct truncations, malicious CBOR vectors,
  golden fixtures, numeric grammar, 16-bit values, encoding/prefix compatibility,
  unknown inert data, inactive descendants, namespaces, descriptor type
  validation and guarded XML fallback.
* `make check-db DDCCONTROL=/usr/local/bin/ddccontrol` with the installed
  pre-CBOR ddccontrol 3.3.0: 934 successful integrity checks (options plus 467
  profiles). That existing target skips three `NOCHECKDB` profiles; the new
  converter and whole-source tests include all three. This uses integrity
  mode and sends no commands to a monitor.
* `make dist-xz`; configure/build/staged install from the extracted archive with
  `PYTHON=false MSGFMT=false`: CBOR, source manifest, all 470 XML profiles and
  French translations match. Removing an XML profile then running make without
  Python fails and removes stale CBOR and sidecar outputs.
* BSD bmake 20200710 (Ubuntu package 20200710-17build1, unpacked locally): parallel
  build, `check`, `check-cbor` and staged `install LINGUAS=fr` passed on the
  initial 466-profile baseline before the final source rebase.
* Four locale settings (`C`, `C.UTF-8`, `nb_NO.UTF-8`, `fr_FR.UTF-8`) and an
  unrelated current directory produce both the frozen fixture and the final
  complete 470-profile database byte-for-byte.
  Python's converter does not require those system locales to be installed.

The independent Rust reader consumes these fixtures and compares actual monitor
trees, CAPS and errors across XML/CBOR. Actual Rust memory/timing and s390x
execution results are reported in the companion ddccontrol PR; they must not be
inferred from this producer's file-size observation or Python allocations.

The shared CDDL was independently parsed and used to validate the initial
complete CBOR file and frozen examples with `cddl-cat` 0.7.1 in the companion repository.
The `function-description` entry also validates the standalone frozen descriptor
payload. Remaining source-standard and legacy-encoding gaps are explicit in
[coverage](coverage.md), [sources](sources.md), and
[the producer boundary](../cbor-distribution.md#source-encoding-boundary).
