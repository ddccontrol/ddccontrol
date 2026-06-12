# TODO

## gddccontrol

- Support automatic GDK monitor to DDC/CI device mapping in `gddccontrol` for
  more than two monitors.

  Current probing/listing already handles an arbitrary monitor list, but
  `src/gddccontrol/main.c:window_changed()` only auto-switches the selected
  monitor when there are exactly two GDK monitors and two DDC/CI monitors.

  A full fix needs a user-visible mapping from each physical GDK monitor to a
  DDC/CI device such as `dev:/dev/i2c-N`, stored in configuration and used by
  `window_changed()` instead of the current two-monitor toggle.

## VCP Handling

- Decide how to expose engineering VCP controls.

  Treat engineering/vendor-specific VCP codes as expert controls: keep parsing
  support, but hide or mark them clearly by default unless the database has safe
  metadata for exposing them.

## Build System

- Improve `--enable-doc` handling.

  Make the documentation build option explicit about required and optional
  tools. `--enable-doc` should fail clearly when required DocBook/xsltproc
  dependencies are missing, while optional tools such as `tidy`, `aspell`,
  `fop`, `ssh`, and `scp` should only affect the targets that need them.

  Also ensure `--enable-doc --disable-xsltproc-check` still adds the `doc`
  directory to the build instead of silently skipping documentation.
