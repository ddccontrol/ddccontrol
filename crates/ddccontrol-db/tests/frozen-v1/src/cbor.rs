//! The v1 wire decoder. The byte preflight deliberately runs before ciborium:
//! a generic deserializer is not a deterministic-CBOR or resource validator.
use ciborium::value::Value;
use sha2::{Digest, Sha256};
use std::collections::{BTreeMap, BTreeSet};

pub(crate) const MAX_FILE: usize = 256 * 1024 * 1024;
const MAX_TEXT: usize = 1024 * 1024;
const MAX_BYTES: usize = 16 * 1024 * 1024;
const MAX_ITEMS: usize = 4_000_000;
const MAX_CONTAINER: usize = 1_000_000;
const MAX_DEPTH: usize = 64;
const MAX_PROFILES: usize = 65_536;

#[derive(Clone, Debug)]
pub(crate) struct Node {
    pub tag: String,
    pub attrs: BTreeMap<String, String>,
    pub children: Vec<Node>,
    pub line: u32,
    /// Unknown required semantics: the caller must reject the proper unit.
    pub required: bool,
}

impl Node {
    pub fn attr(&self, name: &str) -> Option<&str> {
        self.attrs.get(name).map(String::as_str)
    }
}

#[derive(Debug)]
pub(crate) struct Database {
    pub options: Node,
    pub profiles: BTreeMap<String, Node>,
    // Retained for validation reports and the standalone measurement harness.
    #[allow(dead_code)]
    pub manifest: BTreeMap<String, Vec<u8>>,
    #[allow(dead_code)]
    pub snapshot: Vec<u8>,
}

struct Preflight<'a> {
    bytes: &'a [u8],
    position: usize,
    items: usize,
}

impl<'a> Preflight<'a> {
    fn take(&mut self, length: usize) -> Result<&'a [u8], String> {
        let end = self
            .position
            .checked_add(length)
            .ok_or("CBOR length overflow")?;
        let result = self.bytes.get(self.position..end).ok_or("truncated CBOR")?;
        self.position = end;
        Ok(result)
    }

    fn item(&mut self, depth: usize) -> Result<(), String> {
        if depth > MAX_DEPTH || self.items >= MAX_ITEMS {
            return Err("CBOR nesting/item limit exceeded".into());
        }
        self.items += 1;
        let head = self.take(1)?[0];
        let major = head >> 5;
        let info = head & 31;
        if major == 7 {
            return match info {
                20..=22 => Ok(()),
                _ => Err("CBOR floating point/simple value is forbidden".into()),
            };
        }
        if major == 6 {
            return Err("CBOR tags are forbidden".into());
        }
        let (argument, minimum) = match info {
            0..=23 => (u64::from(info), 0),
            24 => (u64::from(self.take(1)?[0]), 24),
            25 => (
                u64::from(u16::from_be_bytes(self.take(2)?.try_into().unwrap())),
                256,
            ),
            26 => (
                u64::from(u32::from_be_bytes(self.take(4)?.try_into().unwrap())),
                65536,
            ),
            27 => (
                u64::from_be_bytes(self.take(8)?.try_into().unwrap()),
                1u64 << 32,
            ),
            _ => return Err("CBOR indefinite/reserved length is forbidden".into()),
        };
        if argument < minimum {
            return Err("CBOR argument is not shortest form".into());
        }
        if major <= 1 {
            return Ok(());
        }
        let length = usize::try_from(argument).map_err(|_| "CBOR length overflow")?;
        match major {
            2 | 3 => {
                let maximum = if major == 2 { MAX_BYTES } else { MAX_TEXT };
                if length > maximum {
                    return Err("CBOR string limit exceeded".into());
                }
                let bytes = self.take(length)?;
                if major == 3 {
                    std::str::from_utf8(bytes).map_err(|_| "CBOR invalid UTF-8")?;
                }
            }
            4 | 5 => {
                let items = length
                    .checked_mul(if major == 5 { 2 } else { 1 })
                    .ok_or("CBOR length overflow")?;
                if length > MAX_CONTAINER
                    || items > MAX_ITEMS - self.items
                    || items > self.bytes.len() - self.position
                {
                    return Err("CBOR container/item limit or truncated container".into());
                }
                let mut previous: Option<&[u8]> = None;
                for _ in 0..length {
                    if major == 5 {
                        let start = self.position;
                        let key_major = self.bytes.get(start).ok_or("truncated CBOR map key")? >> 5;
                        if key_major != 0 && key_major != 3 {
                            return Err("CBOR map keys must be unsigned integers or text".into());
                        }
                        self.item(depth + 1)?;
                        let encoded = &self.bytes[start..self.position];
                        if previous.map(|key| key >= encoded).unwrap_or(false) {
                            return Err("CBOR duplicate or non-bytewise-ordered map key".into());
                        }
                        previous = Some(encoded);
                    }
                    self.item(depth + 1)?;
                }
            }
            _ => return Err("CBOR invalid major type".into()),
        }
        Ok(())
    }
}

pub(crate) fn preflight(bytes: &[u8]) -> Result<(), String> {
    if bytes.len() > MAX_FILE {
        return Err("CBOR file limit exceeded".into());
    }
    let mut check = Preflight {
        bytes,
        position: 0,
        items: 0,
    };
    check.item(0)?;
    if check.position != bytes.len() {
        return Err("CBOR extra bytes after root value".into());
    }
    Ok(())
}

fn value(bytes: &[u8]) -> Result<Value, String> {
    preflight(bytes)?;
    ciborium::from_reader(bytes).map_err(|error| format!("CBOR decode: {error}"))
}

fn map(value: &Value) -> Result<&[(Value, Value)], String> {
    value
        .as_map()
        .map(Vec::as_slice)
        .ok_or_else(|| "CBOR expected map".into())
}

fn fields(value: &Value) -> Result<&[(Value, Value)], String> {
    let entries = map(value)?;
    for (key, _) in entries {
        uint(key)?;
    }
    Ok(entries)
}

fn field(value: &Value, key: u64) -> Result<&Value, String> {
    optional(value, key).ok_or_else(|| format!("CBOR missing field {key}"))
}

fn optional(value: &Value, key: u64) -> Option<&Value> {
    value.as_map()?.iter().find_map(|(candidate, value)| {
        (candidate.as_integer().and_then(|n| u64::try_from(n).ok()) == Some(key)).then_some(value)
    })
}

fn uint(value: &Value) -> Result<u64, String> {
    value
        .as_integer()
        .and_then(|n| u64::try_from(n).ok())
        .ok_or_else(|| "CBOR expected unsigned integer".into())
}

fn integer(value: &Value) -> Result<i128, String> {
    value
        .as_integer()
        .map(i128::from)
        .ok_or_else(|| "CBOR expected integer".into())
}

fn text(value: &Value) -> Result<&str, String> {
    value.as_text().ok_or_else(|| "CBOR expected text".into())
}

fn array(value: &Value) -> Result<&[Value], String> {
    value
        .as_array()
        .map(Vec::as_slice)
        .ok_or_else(|| "CBOR expected array".into())
}

fn bytes32(value: &Value) -> Result<Vec<u8>, String> {
    match value.as_bytes() {
        Some(bytes) if bytes.len() == 32 => Ok(bytes.clone()),
        _ => Err("CBOR expected 32-byte SHA-256 digest".into()),
    }
}

fn required_extensions(value: &Value, key: u64) -> Result<bool, String> {
    let Some(extensions) = optional(value, key) else {
        return Ok(false);
    };
    let mut required = false;
    let mut identities = BTreeSet::new();
    for extension in array(extensions)? {
        fields(extension)?;
        let id = text(field(extension, 0)?)?;
        let scheme = id.split_once(':').map(|(scheme, _)| scheme).unwrap_or("");
        if !scheme.starts_with(|character: char| character.is_ascii_alphabetic())
            || !scheme
                .bytes()
                .all(|byte| byte.is_ascii_alphanumeric() || b"+.-".contains(&byte))
            || id
                .chars()
                .any(|character| character.is_whitespace() || character.is_control())
            || !identities.insert(id)
        {
            return Err("CBOR invalid or duplicate extension identity".into());
        }
        let needs = field(extension, 1)?
            .as_bool()
            .ok_or("CBOR extension requires boolean")?;
        let payload = field(extension, 2)?;
        if id == "https://ddccontrol.sourceforge.net/cbor/ext/source-metadata/1" {
            if needs {
                return Err("CBOR source metadata must be optional".into());
            }
            fields(payload)?;
            for (key, value) in map(field(payload, 0)?)? {
                text(key)?;
                text(value)?;
            }
            for key in 1..=3 {
                if let Some(value) = optional(payload, key) {
                    text(value)?;
                }
            }
        }
        // The baseline interpreter implements no additional executable feature.
        // Even a recognized metadata identity is never an execution capability.
        required |= needs;
    }
    Ok(required)
}

const ATTRIBUTES: &[&str] = &[
    "name",
    "id",
    "type",
    "refresh",
    "pattern",
    "init",
    "address",
    "delay",
    "value",
    "file",
    "add",
    "remove",
    "dbversion",
    "date",
    "caps",
    "include",
];
const KINDS: &[&str] = &[
    "options", "group", "subgroup", "control", "value", "monitor", "caps", "include", "controls",
];

fn node(value: &Value, parent: &str, in_options: bool) -> Result<Node, String> {
    fields(value)?;
    let kind = uint(field(value, 0)?)?;
    let mut tag = KINDS
        .get(usize::try_from(kind).unwrap_or(usize::MAX))
        .copied()
        .unwrap_or("unknown");
    if kind == 255 {
        let extensions = array(field(value, 3)?)?;
        let metadata = extensions
            .iter()
            .find(|extension| {
                optional(extension, 0).and_then(Value::as_text)
                    == Some("https://ddccontrol.sourceforge.net/cbor/ext/source-metadata/1")
            })
            .ok_or("CBOR inert source node requires source metadata")?;
        tag = text(field(field(metadata, 2)?, 1)?)?;
        if KINDS.contains(&tag) {
            return Err("CBOR inert node impersonates executable kind".into());
        }
    }
    let mut required = required_extensions(value, 3)?;
    if kind >= KINDS.len() as u64 && kind != 255 {
        required = true;
    }
    let allowed: &[u64] = match (tag, parent, in_options) {
        ("options", "", true) => &[12, 13],
        ("group", "options", true) => &[0],
        ("subgroup", "group", true) => &[0, 4],
        ("control", "subgroup", true) => &[0, 1, 2, 3],
        ("value", "control", true) => &[0, 1],
        ("monitor", "", false) => &[0, 5, 14, 15],
        ("control", "controls", false) => &[1, 6, 7],
        ("value", "control", false) => &[1, 8],
        ("caps", "monitor", false) => &[10, 11],
        ("include", "monitor", false) => &[9],
        _ => &[],
    };
    let mut attrs = BTreeMap::new();
    for (key, value) in fields(field(value, 1)?)? {
        let id = uint(key)?;
        // New fields are optional information only. A behavior-changing addition
        // also needs a required extension on its smallest independent unit.
        if id >= ATTRIBUTES.len() as u64 {
            if value.as_integer().is_none() && value.as_text().is_none() {
                return Err("CBOR unknown attribute must be integer or text".into());
            }
            continue;
        }
        if !allowed.contains(&id) {
            return Err(format!(
                "CBOR attribute {id} is not executable on {parent}/{tag}"
            ));
        }
        let attr = match id {
            6 | 8 => {
                let number = uint(value)?;
                if number > if id == 6 { 255 } else { 65535 } {
                    return Err(format!("CBOR attribute {id} out of range"));
                }
                number.to_string()
            }
            7 => {
                let number = integer(value)?;
                i32::try_from(number)
                    .map_err(|_| "CBOR delay out of range")?
                    .to_string()
            }
            12 => {
                if integer(value)? != 3 {
                    return Err("CBOR unsupported XML semantics".into());
                }
                "3".into()
            }
            _ => {
                let string = text(value)?;
                if string.contains('\0') {
                    return Err("CBOR NUL in executable text".into());
                }
                string.to_string()
            }
        };
        attrs.insert(ATTRIBUTES[id as usize].to_string(), attr);
    }
    let children = array(field(value, 2)?)?
        .iter()
        .map(|child| node(child, tag, in_options))
        .collect::<Result<Vec<_>, _>>()?;
    Ok(Node {
        tag: tag.into(),
        attrs,
        children,
        line: 0,
        required,
    })
}

fn profile_id(id: &str) -> bool {
    !id.is_empty()
        && id.len() <= 255
        && id
            .bytes()
            .all(|byte| byte.is_ascii_alphanumeric() || byte == b'_' || byte == b'-')
}

fn manifest(value: &Value) -> Result<BTreeMap<String, Vec<u8>>, String> {
    let mut output = BTreeMap::new();
    for (key, value) in map(value)? {
        let path = text(key)?;
        let valid = path == "options.xml"
            || path
                .strip_prefix("monitor/")
                .and_then(|name| name.strip_suffix(".xml"))
                .map(profile_id)
                .unwrap_or(false);
        if !valid {
            return Err("CBOR invalid snapshot source path".into());
        }
        output.insert(path.to_string(), bytes32(value)?);
    }
    if !output.contains_key("options.xml") || output.len() > MAX_PROFILES + 1 {
        return Err("CBOR invalid snapshot source manifest".into());
    }
    Ok(output)
}

/// Hash canonical manifest bytes, avoiding any hash of its own digest field.
fn manifest_digest(value: &Value) -> Result<Vec<u8>, String> {
    // Value retains input map order, already checked as bytewise deterministic.
    let mut encoded = Vec::new();
    ciborium::into_writer(value, &mut encoded).map_err(|error| error.to_string())?;
    Ok(Sha256::digest(&encoded).to_vec())
}

pub(crate) fn verify_manifest(
    bytes: &[u8],
    files: &BTreeMap<String, Vec<u8>>,
) -> Result<(), String> {
    let decoded = value(bytes)?;
    if decoded.as_bool() == Some(false) {
        return Err("XML fallback prohibited: this snapshot requires CBOR semantics".into());
    }
    let expected = manifest(&decoded)?;
    if expected.len() != files.len() {
        return Err("XML snapshot file set mismatch".into());
    }
    for (path, hash) in expected {
        let source = files.get(&path).ok_or("XML snapshot missing file")?;
        if Sha256::digest(source).as_slice() != hash {
            return Err(format!("XML snapshot mismatch: {path}"));
        }
    }
    Ok(())
}

pub(crate) fn decode(bytes: &[u8]) -> Result<Database, String> {
    let root = value(bytes)?;
    fields(&root)?;
    if field(&root, 0)?.as_bytes().map(Vec::as_slice) != Some(b"DDCDB") {
        return Err("CBOR database magic mismatch".into());
    }
    if uint(field(&root, 1)?)? != 1 {
        return Err("CBOR unsupported format version".into());
    }
    if text(field(&root, 2)?)?.is_empty() {
        return Err("CBOR missing database revision".into());
    }
    if uint(field(&root, 3)?)? != 3 {
        return Err("CBOR unsupported XML semantics".into());
    }
    if text(field(&root, 4)?)? != "ddccontrol-db" {
        return Err("CBOR unsupported gettext domain".into());
    }
    if required_extensions(&root, 7)? {
        return Err("CBOR unknown required database feature".into());
    }
    let source_manifest = field(&root, 8)?;
    let manifest = manifest(source_manifest)?;
    let snapshot = bytes32(field(&root, 9)?)?;
    if snapshot != manifest_digest(source_manifest)? {
        return Err("CBOR snapshot digest mismatch".into());
    }
    let options = node(field(&root, 5)?, "", true)?;
    if options.tag != "options" {
        return Err("CBOR options root type mismatch".into());
    }
    let entries = map(field(&root, 6)?)?;
    if entries.len() > MAX_PROFILES {
        return Err("CBOR profile limit exceeded".into());
    }
    let mut profiles = BTreeMap::new();
    for (key, value) in entries {
        let id = text(key)?;
        if !profile_id(id) {
            return Err("CBOR invalid profile identity".into());
        }
        let profile = node(value, "", false)?;
        if profile.tag != "monitor" {
            return Err("CBOR monitor root type mismatch".into());
        }
        if !manifest.contains_key(&format!("monitor/{id}.xml")) {
            return Err("CBOR profile absent from source manifest".into());
        }
        profiles.insert(id.to_string(), profile);
    }
    if manifest.len() != profiles.len() + 1 {
        return Err("CBOR manifest/profile set mismatch".into());
    }
    // Reference validity is checked before any caller may build a tree. A
    // damaged reference graph blocks only the profiles that depend on it.
    let mut unavailable = Vec::new();
    let mut cache = BTreeMap::new();
    for id in profiles.keys() {
        if reference_height(id, &profiles, &mut BTreeSet::new(), &mut cache).is_none() {
            unavailable.push(id.clone());
        }
    }
    for id in unavailable {
        eprintln!("CBOR profile {id} unavailable: missing/cyclic/excessive include");
        profiles.get_mut(&id).unwrap().required = true;
    }
    Ok(Database {
        options,
        profiles,
        manifest,
        snapshot,
    })
}

fn reference_height(
    id: &str,
    profiles: &BTreeMap<String, Node>,
    active: &mut BTreeSet<String>,
    cache: &mut BTreeMap<String, Option<usize>>,
) -> Option<usize> {
    if let Some(height) = cache.get(id) {
        return *height;
    }
    if active.len() >= 256 || !active.insert(id.to_string()) {
        return None;
    }
    let height = profiles.get(id).and_then(|profile| {
        let mut maximum = 0;
        for child in profile
            .children
            .iter()
            .filter(|child| child.tag == "include")
        {
            let target = child.attr("file")?;
            maximum = maximum.max(reference_height(target, profiles, active, cache)?);
        }
        (maximum < 256).then_some(maximum + 1)
    });
    active.remove(id);
    cache.insert(id.to_string(), height);
    height
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn strict_deterministic_subset() {
        for good in [
            &[0x00][..],
            &[0x18, 0xff],
            &[0x19, 1, 0],
            &[0x43, 0, 1, 2],
            &[0xa2, 0, 0, 0x61, b'a', 0xf6],
        ] {
            preflight(good).unwrap();
        }
        for bad in [
            &[][..],
            &[0x18, 0x17],
            &[0x19, 0, 0xff],
            &[0x9f, 0xff],
            &[0xc0, 0],
            &[0xfa, 0, 0, 0, 0],
            &[0xa2, 0, 0, 0, 1],
            &[0xa2, 1, 0, 0, 0],
            &[0xa1, 0x20, 0],
            &[0x61, 0xff],
            &[0, 0],
            &[0x5a, 0xff, 0xff, 0xff, 0xff],
        ] {
            assert!(preflight(bad).is_err(), "accepted {bad:?}");
        }
        let mut deep = vec![0x81; 66];
        deep.push(0);
        assert!(preflight(&deep).is_err());
    }

    #[test]
    fn truncated_prefixes_do_not_panic() {
        let bytes = [0xa1, 0, 0x82, 0x63, b'a', b'b', b'c', 0x19, 1, 0];
        preflight(&bytes).unwrap();
        for end in 0..bytes.len() {
            assert!(preflight(&bytes[..end]).is_err());
        }
    }

    const REFERENCE: &[u8] = include_bytes!("../fixtures/cbor/reference-v1.cbor");

    #[test]
    fn frozen_independent_reference_and_full_truncation() {
        let database = decode(REFERENCE).unwrap();
        assert_eq!(database.profiles.len(), 2);
        assert_eq!(database.manifest.len(), 3);
        assert_eq!(database.snapshot.len(), 32);
        let monitor = &database.profiles["TST0001"];
        assert_eq!(monitor.attr("name"), Some("Écran fixture"));
        assert_eq!(
            monitor.children[1].children[0].children[0].attr("value"),
            Some("65443")
        );
        for end in 0..REFERENCE.len() {
            assert!(decode(&REFERENCE[..end]).is_err(), "truncation {end}");
        }
    }

    fn canonical(value: &mut Value) {
        match value {
            Value::Map(entries) => {
                for (key, value) in entries.iter_mut() {
                    canonical(key);
                    canonical(value);
                }
                entries.sort_by_cached_key(|(key, _)| {
                    let mut bytes = Vec::new();
                    ciborium::into_writer(key, &mut bytes).unwrap();
                    bytes
                });
            }
            Value::Array(values) => values.iter_mut().for_each(canonical),
            _ => {}
        }
    }

    fn encode(mut value: Value) -> Vec<u8> {
        canonical(&mut value);
        let mut bytes = Vec::new();
        ciborium::into_writer(&value, &mut bytes).unwrap();
        bytes
    }

    #[test]
    fn future_optional_fields_are_ignored_but_required_database_features_fail() {
        let mut root = value(REFERENCE).unwrap();
        root.as_map_mut()
            .unwrap()
            .push((Value::Integer(1000.into()), Value::Bytes(vec![0, 255])));
        let bytes = encode(root.clone());
        assert_eq!(decode(&bytes).unwrap().profiles.len(), 2);
        let extension = Value::Map(vec![
            (
                Value::Integer(0.into()),
                Value::Text("urn:example:future:1".into()),
            ),
            (Value::Integer(1.into()), Value::Bool(true)),
            (Value::Integer(2.into()), Value::Null),
        ]);
        let extensions = root
            .as_map_mut()
            .unwrap()
            .iter_mut()
            .find(|(key, _)| uint(key) == Ok(7));
        if let Some((_, value)) = extensions {
            *value = Value::Array(vec![extension]);
        } else {
            root.as_map_mut()
                .unwrap()
                .push((Value::Integer(7.into()), Value::Array(vec![extension])));
        }
        assert!(decode(&encode(root))
            .unwrap_err()
            .contains("required database feature"));
    }

    #[test]
    fn deterministic_encoding_matches_independent_producer_bytes() {
        assert_eq!(encode(value(REFERENCE).unwrap()), REFERENCE);
        // Core deterministic order differs from length-first "canonical" CBOR.
        let data = Value::Map(vec![
            (Value::Text("a".into()), Value::Null),
            (Value::Integer(u64::MAX.into()), Value::Null),
        ]);
        let bytes = encode(data);
        assert_eq!(bytes[1], 0x1b);
        preflight(&bytes).unwrap();
    }
}
