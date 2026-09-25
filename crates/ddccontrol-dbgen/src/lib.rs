//! Offline database production. No monitor access, gettext, clock or host data.
use ddccontrol_db_format::{self as format, encode, field, map, optional, Value};
use roxmltree::Node;
use sha2::{Digest, Sha256};
use std::{
    collections::BTreeMap,
    fs::{self, File},
    io::Read,
    path::{Path, PathBuf},
};

pub use format::SOURCE_METADATA as METADATA;

fn object(entries: Vec<(u64, Value)>) -> Value {
    Value::Map(entries.into_iter().map(|(k, v)| (k.into(), v)).collect())
}

/// Attribute bounds are distinct from future descriptor datatypes.
pub fn source_integer(raw: &str, attribute: &str) -> Result<Value, String> {
    let (lo, hi) = format::attribute_id(attribute)
        .and_then(format::attribute_integer_bounds)
        .ok_or("not a numeric source attribute")?;
    let input = raw.trim_start_matches(|c: char| c.is_ascii_whitespace());
    let number = if attribute == "delay" {
        // Reader delays are decimal even when the source has leading zeroes.
        input
            .parse::<i64>()
            .map_err(|_| "invalid integer or outside normative range")
    } else {
        ddccontrol_xml::parse_integer(input)
    };
    let number = number
        .map_err(|e| format!("{attribute} outside normative range or invalid integer: {e}"))?;
    if !(lo..=hi).contains(&number) {
        return Err(format!("{attribute} outside normative range"));
    }
    Ok(number.into())
}

pub fn parse_xml(bytes: &[u8], expected: &str) -> Result<Value, String> {
    let xml = format::xml::decode_source(bytes)?;
    let document = roxmltree::Document::parse(&xml).map_err(|e| format!("XML: {e}"))?;
    if document.root_element().tag_name().name() != expected {
        return Err(format!("expected XML root {expected}"));
    }
    fn node(
        element: Node<'_, '_>,
        parent: &str,
        options: bool,
        ancestor: bool,
        depth: usize,
    ) -> Result<Value, String> {
        if depth > format::MAX_DEPTH {
            return Err("XML element nesting exceeds conversion resource bound".into());
        }
        let tag = element.tag_name().name();
        let active = format::executable_path(tag, parent, options, ancestor);
        let allowed = if active {
            format::executable_attributes(tag, parent, options)
        } else {
            &[]
        };
        let mut attrs = Vec::new();
        let mut inert = Vec::new();
        for attribute in element.attributes() {
            let id = format::attribute_id(attribute.name());
            if attribute.namespace().is_none() && id.is_some_and(|id| allowed.contains(&id)) {
                let id = id.unwrap();
                let value = if matches!(id, 6 | 7 | 8 | 12) {
                    source_integer(attribute.value(), attribute.name())?
                } else {
                    attribute.value().into()
                };
                attrs.push((id.into(), value));
            } else {
                let name = attribute
                    .namespace()
                    .map(|ns| format!("{{{ns}}}{}", attribute.name()))
                    .unwrap_or_else(|| attribute.name().into());
                inert.push((name.into(), attribute.value().into()));
            }
        }
        let kind = format::kind_id(tag).unwrap_or(255);
        let children = element
            .children()
            .filter(Node::is_element)
            .map(|child| node(child, tag, options, active, depth + 1))
            .collect::<Result<Vec<_>, _>>()?;
        let mut entries = vec![
            (0, kind.into()),
            (1, Value::Map(attrs)),
            (2, Value::Array(children)),
        ];
        let mut metadata = Vec::new();
        if !inert.is_empty() {
            metadata.push((0, Value::Map(inert)));
        }
        if kind == 255 {
            metadata.push((1, tag.into()));
        }
        // Comments/PI do not activate controls or interrupt source text.
        let head: String = element
            .children()
            .take_while(|n| !n.is_element())
            .filter_map(|n| if n.is_text() { n.text() } else { None })
            .collect();
        let tail: String = element
            .next_siblings()
            .skip(1)
            .take_while(|n| !n.is_element())
            .filter_map(|n| if n.is_text() { n.text() } else { None })
            .collect();
        for (key, value) in [(2, head), (3, tail)] {
            if !value.trim().is_empty() {
                metadata.push((key, value.into()));
            }
        }
        if !metadata.is_empty() {
            if !metadata.iter().any(|(k, _)| *k == 0) {
                metadata.push((0, Value::Map(vec![])));
            }
            entries.push((
                3,
                Value::Array(vec![object(vec![
                    (0, METADATA.into()),
                    (1, false.into()),
                    (2, object(metadata)),
                ])]),
            ));
        }
        Ok(object(entries))
    }
    node(document.root_element(), "", expected == "options", true, 0)
}

pub fn read_bounded(path: &Path) -> Result<Vec<u8>, String> {
    let mut bytes = Vec::new();
    File::open(path)
        .map_err(|e| format!("{}: {e}", path.display()))?
        .take(format::MAX_FILE as u64 + 1)
        .read_to_end(&mut bytes)
        .map_err(|e| format!("{}: {e}", path.display()))?;
    if bytes.len() > format::MAX_FILE {
        return Err(format!("{}: file exceeds 256 MiB", path.display()));
    }
    Ok(bytes)
}

/// Enumerate source paths without following file contents. Errors remain entries
/// so callers protecting output paths can still identify the other source files;
/// conversion must reject every discovery error before reading any snapshot.
pub fn source_paths(directory: &Path) -> Vec<Result<PathBuf, String>> {
    let mut paths = vec![Ok(directory.join("options.xml"))];
    match fs::read_dir(directory.join("monitor")) {
        Ok(entries) => {
            for entry in entries {
                match entry {
                    Ok(entry) if entry.path().extension().is_some_and(|ext| ext == "xml") => {
                        paths.push(Ok(entry.path()));
                    }
                    Ok(_) => {}
                    Err(error) => paths.push(Err(error.to_string())),
                }
            }
        }
        Err(error) => paths.push(Err(error.to_string())),
    }
    paths
}

fn sources(directory: &Path) -> Result<BTreeMap<String, Vec<u8>>, String> {
    let paths = source_paths(directory)
        .into_iter()
        .collect::<Result<Vec<_>, _>>()?;
    if paths.len() < 2 || paths.len() > format::MAX_PROFILES + 1 {
        return Err("profile count outside bounds".into());
    }
    let mut total = 0;
    let mut result = BTreeMap::new();
    for path in paths {
        let relative = path.strip_prefix(directory).map_err(|e| e.to_string())?;
        let relative = relative.to_str().ok_or("source filename is not UTF-8")?;
        if relative != "options.xml" {
            let id = path
                .file_stem()
                .and_then(|id| id.to_str())
                .ok_or("source filename is not UTF-8")?;
            if !format::profile_id(id) {
                return Err(format!("unsafe profile identifier: {id}"));
            }
        }
        let bytes = read_bounded(&path)?;
        total += bytes.len();
        if total > format::MAX_FILE {
            return Err("XML source snapshot exceeds 256 MiB conversion resource limit".into());
        }
        // Manifest paths always use '/', including on non-Unix build hosts.
        let manifest_path = if relative == "options.xml" {
            relative.to_string()
        } else {
            format!("monitor/{}", path.file_name().unwrap().to_str().unwrap())
        };
        result.insert(manifest_path, bytes);
    }
    Ok(result)
}

pub fn convert(directory: &Path, revision: Option<&str>) -> Result<Value, String> {
    let files = sources(directory)?;
    let options =
        parse_xml(&files["options.xml"], "options").map_err(|e| format!("options.xml: {e}"))?;
    let revision = revision
        .or_else(|| optional(field(&options, 1).ok()?, 13).and_then(Value::as_text))
        .filter(|r| !r.is_empty())
        .ok_or("database revision is required")?;
    let mut profiles = Vec::new();
    for (path, bytes) in &files {
        if path == "options.xml" {
            continue;
        }
        let id = path
            .strip_prefix("monitor/")
            .unwrap()
            .strip_suffix(".xml")
            .unwrap();
        let node = parse_xml(bytes, "monitor").map_err(|e| format!("{path}: {e}"))?;
        profiles.push((id.into(), node));
    }
    let manifest = Value::Map(
        files
            .iter()
            .map(|(path, bytes)| {
                (
                    path.clone().into(),
                    Value::Bytes(Sha256::digest(bytes).to_vec()),
                )
            })
            .collect(),
    );
    let snapshot = format::manifest_digest(&manifest)?;
    let root = object(vec![
        (0, Value::Bytes(b"DDCDB".to_vec())),
        (1, 1.into()),
        (2, revision.into()),
        (3, 3.into()),
        (4, "ddccontrol-db".into()),
        (5, options),
        (6, Value::Map(profiles)),
        (8, manifest),
        (9, Value::Bytes(snapshot)),
    ]);
    let wire = encode(&root)?;
    let root = format::validate(&wire)?;
    if files != sources(directory)? {
        return Err("XML source snapshot changed during generation".into());
    }
    Ok(root)
}

/// JSON uses tagged maps and bytes, preserving integer keys and full CBOR integers.
/// Exact indentation and key order match the independently committed fixtures.
pub fn diagnostic(value: &Value) -> Result<String, String> {
    fn string(s: &str, out: &mut String) {
        out.push('"');
        for c in s.chars() {
            match c {
                '"' => out.push_str("\\\""),
                '\\' => out.push_str("\\\\"),
                '\n' => out.push_str("\\n"),
                '\r' => out.push_str("\\r"),
                '\t' => out.push_str("\\t"),
                '\u{8}' => out.push_str("\\b"),
                '\u{c}' => out.push_str("\\f"),
                c if c < ' ' => out.push_str(&format!("\\u{:04x}", c as u32)),
                _ => out.push(c),
            }
        }
        out.push('"');
    }
    fn line(out: &mut String, depth: usize) {
        out.push('\n');
        out.push_str(&"  ".repeat(depth));
    }
    fn list(values: &[Value], out: &mut String, depth: usize) -> Result<(), String> {
        out.push('[');
        for (i, v) in values.iter().enumerate() {
            line(out, depth + 1);
            write(v, out, depth + 1)?;
            if i + 1 < values.len() {
                out.push(',');
            }
        }
        if !values.is_empty() {
            line(out, depth);
        }
        out.push(']');
        Ok(())
    }
    fn write(value: &Value, out: &mut String, depth: usize) -> Result<(), String> {
        match value {
            Value::Integer(n) => out.push_str(&i128::from(*n).to_string()),
            Value::Text(s) => string(s, out),
            Value::Bool(b) => out.push_str(if *b { "true" } else { "false" }),
            Value::Null => out.push_str("null"),
            Value::Array(values) => list(values, out, depth)?,
            Value::Bytes(bytes) => {
                out.push('{');
                line(out, depth + 1);
                out.push_str("\"$bytes\": \"");
                for b in bytes {
                    out.push_str(&format!("{b:02x}"));
                }
                out.push('"');
                line(out, depth);
                out.push('}');
            }
            Value::Map(entries) => {
                out.push('{');
                line(out, depth + 1);
                out.push_str("\"$map\": ");
                let pairs: Vec<_> = entries
                    .iter()
                    .map(|(k, v)| Value::Array(vec![k.clone(), v.clone()]))
                    .collect();
                list(&pairs, out, depth + 1)?;
                line(out, depth);
                out.push('}');
            }
            _ => return Err("unsupported diagnostic value".into()),
        }
        Ok(())
    }
    let mut out = String::new();
    write(value, &mut out, 0)?;
    out.push('\n');
    Ok(out)
}

pub fn summary(root: &Value, bytes: usize) -> Result<String, String> {
    let snapshot = field(root, 9)?
        .as_bytes()
        .ok_or("snapshot requires bytes")?;
    Ok(format!(
        "{} profiles, {bytes} bytes, snapshot {}",
        map(field(root, 6)?)?.len(),
        snapshot
            .iter()
            .map(|b| format!("{b:02x}"))
            .collect::<String>()
    ))
}
