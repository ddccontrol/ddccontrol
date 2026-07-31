# ddccontrol-profile

`ddccontrol-profile` parses and serializes the local user profiles stored below
`~/.ddccontrol/profiles`. It preserves profile format version 1 while keeping
filesystem access and the C ABI in `ddccontrol-db`.

The crate deliberately contains no monitor I/O. Reading current VCP values,
applying a profile, retry handling, and monitor profile-list management remain
in the C library.
