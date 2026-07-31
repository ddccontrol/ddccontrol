# ddccontrol-edid

`ddccontrol-edid` parses the EDID fields used by ddccontrol without performing
hardware I/O. It preserves the behavior of the historical
`ddcci_parse_edid_buf()` implementation, including support for a partial base
block containing the first 23 bytes.

The parser extracts:

- the seven-character PNP ID;
- the digital/analog input flag;
- the numeric serial number and manufacture date;
- the EDID version and physical dimensions;
- monitor-name and ASCII-serial descriptors from complete base blocks; and
- at most the first 128 bytes of the base block.

For compatibility, the parser validates the EDID header but not the base-block
checksum. I2C access and user-visible diagnostics remain in the C library.
