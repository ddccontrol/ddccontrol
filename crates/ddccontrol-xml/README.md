# ddccontrol-xml

Internal, dependency-free helpers for XML attribute escaping, XML 1.0 character
validation, and the database's decimal/hexadecimal/octal integer syntax. These
helpers are shared by the database, user profiles, monitor cache, and scanner.
Document parsing still uses `roxmltree` in the format-specific crates.

Callers keep their error policy: profiles and cached lists reject invalid XML
characters, while the scanner replaces them in monitor-provided text before
escaping it. XML decoding and schema validation remain in their owning crates.
