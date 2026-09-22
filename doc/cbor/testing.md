# Reproducing compatibility checks

Tests construct profiles from synthetic CAPS. They do not open monitor devices
or perform monitor operations.

Run the Rust reader suite and the optional full-source differential check:

```sh
cargo test -p ddccontrol-db
DDCCONTROL_DB_TEST_DATADIR=../ddccontrol-db/db \
  cargo test -p ddccontrol-db whole_database_xml_cbor_semantics_match_when_configured -- --nocapture
./scripts/check_cbor_frozen.sh
```

The differential test converts all source profiles with the independent Python
producer, then compares complete trees, final CAPS and rejection status in
strict and tolerant modes with three CAPS inputs. It accepts
`DDCCONTROL_DB_CONVERTER` if the producer is elsewhere. The frozen-reader check
verifies stored source hashes and tests newer files without changing that
reader's decoder or resolver.

## Production gettext path

Rust unit tests deliberately use untranslated labels, so they do not establish
production gettext behavior. Build the separate frozen library with gettext
and link its hardware-free C driver:

```sh
frozen=crates/ddccontrol-db/tests/frozen-v1
cargo build --manifest-path "$frozen/Cargo.toml" --release --locked --features gettext
cc -Wall -Wextra -Werror "$frozen/driver.c" \
  "$frozen/target/release/libddccontrol_db_frozen_v1.a" \
  -ldl -lpthread -lm -o /tmp/ddccontrol-frozen-gettext-reader
./scripts/check_cbor_gettext.sh /tmp/ddccontrol-frozen-gettext-reader ../ddccontrol-db en_US.UTF-8
```

If `CARGO_TARGET_DIR` is set, use its `release` directory for the archive path.
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
