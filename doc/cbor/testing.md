# Reproducing compatibility checks

Tests construct profiles from synthetic CAPS. They do not open monitor devices
or perform monitor operations.

Run these commands from the application repository root. Resolve the producer
checkout to an absolute path before passing it to Cargo: test executables run
from their crate directory, not the directory in which Cargo was invoked.
Use a producer checkout containing `scripts/cbor-db.py` and generated
`db/options.xml`; adjust the checkout path below if it is elsewhere.

Run the Rust reader suite and the optional full-source differential check:

```sh
cbor_source=$(cd ../ddccontrol-db && pwd)
cargo test -p ddccontrol-db --locked
DDCCONTROL_DB_TEST_DATADIR="$cbor_source/db" \
DDCCONTROL_DB_CONVERTER="$cbor_source/scripts/cbor-db.py" \
  cargo test -p ddccontrol-db whole_database_xml_cbor_semantics_match_when_configured --locked -- --nocapture
./scripts/check_cbor_frozen.sh
```

The differential test converts all source profiles with the independent Python
producer, then compares complete trees, final CAPS and rejection status in
strict and tolerant modes with three CAPS inputs. It accepts
`DDCCONTROL_DB_CONVERTER` if the producer is elsewhere. The frozen-reader check
verifies stored source hashes and tests newer files without changing that
reader's decoder or resolver. It explicitly uses `CARGO_TARGET_DIR` when set,
otherwise its own `tests/frozen-v1/target/`, for both building and linking.

## Production gettext path

Rust unit tests deliberately use untranslated labels, so they do not establish
production gettext behavior. Build the current production library with gettext
and link the hardware-free C driver against that library. The driver source is
shared with the historical compatibility probe; the library under test here is
the current reader.

```sh
cbor_source=$(cd ../ddccontrol-db && pwd)
cbor_target=${CARGO_TARGET_DIR:-target}
cargo build -p ddccontrol-db --release --locked --features gettext --target-dir "$cbor_target"
cc -Wall -Wextra -Werror crates/ddccontrol-db/tests/frozen-v1/driver.c \
  "$cbor_target/release/libddccontrol_db.a" \
  -ldl -lpthread -lm -o "$cbor_target/ddccontrol-production-gettext-reader"
./scripts/check_cbor_gettext.sh "$cbor_target/ddccontrol-production-gettext-reader" "$cbor_source" en_US.UTF-8
```

These commands use `CARGO_TARGET_DIR` when set, otherwise the local `target/`
directory. The separate `check_cbor_frozen.sh` test continues to build and link
the unchanged historical library; its success cannot establish current
production gettext behavior.
The database checkout must already contain generated `db/options.xml` and
`db/ddccontrol-db.cbor`; its existing `po/fr.po` remains the translation source.
The script accepts either the checkout root or its `db` directory. It needs
`msgfmt` and an installed UTF-8 locale, creates isolated XML-only and CBOR-only
views, compiles the French catalog, and compares every printed tree field after
session release. It also requires that French output differ from C-locale
output, preventing an untranslated false-positive result. An existing snapshot
sidecar is retained in the XML view, including any fallback prohibition.

The validation environment has `en_US.UTF-8`; `LANGUAGE=fr` selects French
without requiring an installed French locale. `C.UTF-8` suppressed gettext in
this environment and is not used for the translated comparison. The recorded
VESA run produced identical 7,992-byte French outputs from XML and CBOR;
C-locale output was 7,477 bytes. Labels included `Luminosité` and `Contraste`.
These lengths are observations for the tested database snapshot, not assertions
about later source revisions.
