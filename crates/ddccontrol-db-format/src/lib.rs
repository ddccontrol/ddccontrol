//! The v1 wire decoder. The byte preflight deliberately runs before ciborium:
//! a generic deserializer is not a deterministic-CBOR or resource validator.
pub use ciborium::value::Value;
use sha2::{Digest, Sha256};
use std::collections::{BTreeMap, BTreeSet};

pub const SOURCE_METADATA: &str = "https://ddccontrol.sourceforge.net/cbor/ext/source-metadata/1";

pub const MAX_FILE: usize = 256 * 1024 * 1024;
pub const MAX_TEXT: usize = 1024 * 1024;
pub const MAX_BYTES: usize = 16 * 1024 * 1024;
pub const MAX_ITEMS: usize = 4_000_000;
pub const MAX_CONTAINER: usize = 1_000_000;
pub const MAX_DEPTH: usize = 64;
pub const MAX_PROFILES: usize = 65_536;
pub const MAX_INCLUDE_DEPTH: usize = 256;

#[derive(Clone, Debug)]
pub struct Node {
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
pub struct Database {
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

pub fn preflight(bytes: &[u8]) -> Result<(), String> {
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

pub fn value(bytes: &[u8]) -> Result<Value, String> {
    preflight(bytes)?;
    ciborium::from_reader(bytes).map_err(|error| format!("CBOR decode: {error}"))
}

pub fn map(value: &Value) -> Result<&[(Value, Value)], String> {
    value
        .as_map()
        .map(Vec::as_slice)
        .ok_or_else(|| "CBOR expected map".into())
}

pub fn fields(value: &Value) -> Result<&[(Value, Value)], String> {
    let entries = map(value)?;
    for (key, _) in entries {
        uint(key)?;
    }
    Ok(entries)
}

pub fn field(value: &Value, key: u64) -> Result<&Value, String> {
    optional(value, key).ok_or_else(|| format!("CBOR missing field {key}"))
}

pub fn optional(value: &Value, key: u64) -> Option<&Value> {
    value.as_map()?.iter().find_map(|(candidate, value)| {
        (candidate.as_integer().and_then(|n| u64::try_from(n).ok()) == Some(key)).then_some(value)
    })
}

pub fn uint(value: &Value) -> Result<u64, String> {
    value
        .as_integer()
        .and_then(|n| u64::try_from(n).ok())
        .ok_or_else(|| "CBOR expected unsigned integer".into())
}

pub fn integer(value: &Value) -> Result<i128, String> {
    value
        .as_integer()
        .map(i128::from)
        .ok_or_else(|| "CBOR expected integer".into())
}

pub fn text(value: &Value) -> Result<&str, String> {
    value.as_text().ok_or_else(|| "CBOR expected text".into())
}

pub fn array(value: &Value) -> Result<&[Value], String> {
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
        if !valid_identity(id) || !identities.insert(id) {
            return Err("CBOR invalid or duplicate extension identity".into());
        }
        let needs = field(extension, 1)?
            .as_bool()
            .ok_or("CBOR extension requires boolean")?;
        let payload = field(extension, 2)?;
        if id == SOURCE_METADATA {
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

/// Permanent field IDs: never derive these numbers from source order.
pub const ATTRIBUTES: &[(u64, &str)] = &[
    (0, "name"),
    (1, "id"),
    (2, "type"),
    (3, "refresh"),
    (4, "pattern"),
    (5, "init"),
    (6, "address"),
    (7, "delay"),
    (8, "value"),
    (9, "file"),
    (10, "add"),
    (11, "remove"),
    (12, "dbversion"),
    (13, "date"),
    (14, "caps"),
    (15, "include"),
];
pub const KINDS: &[(u64, &str)] = &[
    (0, "options"),
    (1, "group"),
    (2, "subgroup"),
    (3, "control"),
    (4, "value"),
    (5, "monitor"),
    (6, "caps"),
    (7, "include"),
    (8, "controls"),
];
pub fn kind_name(id: u64) -> Option<&'static str> {
    KINDS
        .iter()
        .find_map(|(n, name)| (*n == id).then_some(*name))
}
pub fn kind_id(name: &str) -> Option<u64> {
    KINDS
        .iter()
        .find_map(|(n, tag)| (*tag == name).then_some(*n))
}
pub fn attribute_id(name: &str) -> Option<u64> {
    ATTRIBUTES
        .iter()
        .find_map(|(n, attr)| (*attr == name).then_some(*n))
}

/// Numeric source fields retain these units and ranges permanently. Future
/// descriptor data use their own types instead of widening legacy VCP fields.
pub fn attribute_integer_bounds(id: u64) -> Option<(i64, i64)> {
    match id {
        6 => Some((0, 255)),
        7 => Some((i32::MIN.into(), i32::MAX.into())),
        8 => Some((0, 65535)),
        12 => Some((3, 3)),
        _ => None,
    }
}

fn node(value: &Value, parent: &str, in_options: bool) -> Result<Node, String> {
    fields(value)?;
    let kind = uint(field(value, 0)?)?;
    let mut tag = kind_name(kind).unwrap_or("unknown");
    if kind == 255 {
        let extensions = array(field(value, 3)?)?;
        let metadata = extensions
            .iter()
            .find(|extension| {
                optional(extension, 0).and_then(Value::as_text) == Some(SOURCE_METADATA)
            })
            .ok_or("CBOR inert source node requires source metadata")?;
        tag = text(field(field(metadata, 2)?, 1)?)?;
        if kind_id(tag).is_some() {
            return Err("CBOR inert node impersonates executable kind".into());
        }
    }
    let mut required = required_extensions(value, 3)?;
    if kind_name(kind).is_none() && kind != 255 {
        required = true;
    }
    let allowed = executable_attributes(tag, parent, in_options);
    let mut attrs = BTreeMap::new();
    for (key, value) in fields(field(value, 1)?)? {
        let id = uint(key)?;
        // New fields are optional information only. A behavior-changing addition
        // also needs a required extension on its smallest independent unit.
        let Some((_, name)) = ATTRIBUTES.iter().find(|(key, _)| *key == id) else {
            if value.as_integer().is_none() && value.as_text().is_none() {
                return Err("CBOR unknown attribute must be integer or text".into());
            }
            continue;
        };
        if !allowed.contains(&id) {
            return Err(format!(
                "CBOR attribute {id} is not executable on {parent}/{tag}"
            ));
        }
        let attr = match id {
            6 | 7 | 8 | 12 => {
                let number = integer(value)?;
                let (low, high) = attribute_integer_bounds(id).unwrap();
                if !(i128::from(low)..=i128::from(high)).contains(&number) {
                    return Err(format!("CBOR attribute {id} out of range"));
                }
                number.to_string()
            }
            _ => {
                let string = text(value)?;
                if string.contains('\0') {
                    return Err("CBOR NUL in executable text".into());
                }
                string.to_string()
            }
        };
        attrs.insert((*name).to_string(), attr);
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

pub fn profile_id(id: &str) -> bool {
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
pub fn manifest_digest(value: &Value) -> Result<Vec<u8>, String> {
    Ok(Sha256::digest(encode(value)?).to_vec())
}

pub fn verify_manifest(bytes: &[u8], files: &BTreeMap<String, Vec<u8>>) -> Result<(), String> {
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

pub fn decode(bytes: &[u8]) -> Result<Database, String> {
    decode_value(&value(bytes)?, true)
}

fn decode_value(root: &Value, consumer: bool) -> Result<Database, String> {
    fields(root)?;
    if field(root, 0)?.as_bytes().map(Vec::as_slice) != Some(b"DDCDB") {
        return Err("CBOR database magic mismatch".into());
    }
    if uint(field(root, 1)?)? != 1 {
        return Err("CBOR unsupported format version".into());
    }
    if text(field(root, 2)?)?.is_empty() {
        return Err("CBOR missing database revision".into());
    }
    if uint(field(root, 3)?)? != 3 {
        return Err("CBOR unsupported XML semantics".into());
    }
    if text(field(root, 4)?)? != "ddccontrol-db" {
        return Err("CBOR unsupported gettext domain".into());
    }
    if required_extensions(root, 7)? && consumer {
        return Err("CBOR unknown required database feature".into());
    }
    let source_manifest = field(root, 8)?;
    let manifest = manifest(source_manifest)?;
    let snapshot = bytes32(field(root, 9)?)?;
    if snapshot != manifest_digest(source_manifest)? {
        return Err("CBOR snapshot digest mismatch".into());
    }
    let options = node(field(root, 5)?, "", true)?;
    if options.tag != "options" {
        return Err("CBOR options root type mismatch".into());
    }
    let entries = map(field(root, 6)?)?;
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
    for id in invalid_reference_profiles(&profiles, false) {
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

fn invalid_reference_profiles(
    profiles: &BTreeMap<String, Node>,
    bound_visits: bool,
) -> Vec<String> {
    let indices: BTreeMap<_, _> = profiles
        .keys()
        .enumerate()
        .map(|(index, id)| (id.as_str(), index))
        .collect();
    let mut pending = vec![0usize; profiles.len()];
    let mut parents = vec![Vec::new(); profiles.len()];
    for (index, profile) in profiles.values().enumerate() {
        for include in profile
            .children
            .iter()
            .filter(|child| child.tag == "include")
        {
            pending[index] += 1;
            if let Some(target) = include.attr("file").and_then(|id| indices.get(id)) {
                parents[*target].push(index);
            }
            // A missing target (or file attribute) stays pending forever.
        }
    }

    // Resolve leaves first. A profile's height is independent of the path
    // used to reach it, so an excessive ancestor cannot invalidate its suffix.
    // This also avoids recursive calls on graphs up to MAX_PROFILES records.
    let mut ready: Vec<_> = pending
        .iter()
        .enumerate()
        .filter_map(|(index, &count)| (count == 0).then_some(index))
        .collect();
    let mut heights = vec![1usize; profiles.len()];
    let mut visits = vec![1usize; profiles.len()];
    let mut valid = vec![false; profiles.len()];
    while let Some(index) = ready.pop() {
        if heights[index] > MAX_INCLUDE_DEPTH || (bound_visits && visits[index] > MAX_CONTAINER) {
            continue;
        }
        valid[index] = true;
        for &parent in &parents[index] {
            heights[parent] = heights[parent].max(heights[index] + 1);
            visits[parent] = visits[parent].saturating_add(visits[index]);
            pending[parent] -= 1;
            if pending[parent] == 0 {
                ready.push(parent);
            }
        }
    }
    // Cycles, unresolved targets and excessive paths cannot finish; neither
    // can their dependants. Duplicate includes have one pending edge each.
    profiles
        .keys()
        .enumerate()
        .filter(|(index, _)| !valid[*index])
        .map(|(_, id)| id.clone())
        .collect()
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

    const REFERENCE: &[u8] = include_bytes!("../../ddccontrol-db/fixtures/cbor/reference-v1.cbor");

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

    fn graph_database(edges: &BTreeMap<String, Vec<String>>) -> Database {
        let mut root = value(REFERENCE).unwrap();
        let options_hash = field(&root, 8)
            .unwrap()
            .as_map()
            .unwrap()
            .iter()
            .find(|(key, _)| key.as_text() == Some("options.xml"))
            .unwrap()
            .1
            .clone();
        let mut manifest = vec![(Value::Text("options.xml".into()), options_hash)];
        let mut profiles = Vec::new();
        for (id, targets) in edges {
            let children = if targets.is_empty() {
                vec![Value::Map(vec![
                    (0.into(), 8.into()),
                    (1.into(), Value::Map(vec![])),
                    (2.into(), Value::Array(vec![])),
                ])]
            } else {
                targets
                    .iter()
                    .map(|target| {
                        Value::Map(vec![
                            (0.into(), 7.into()),
                            (
                                1.into(),
                                Value::Map(vec![(9.into(), target.clone().into())]),
                            ),
                            (2.into(), Value::Array(vec![])),
                        ])
                    })
                    .collect()
            };
            profiles.push((
                id.clone().into(),
                Value::Map(vec![
                    (0.into(), 5.into()),
                    (
                        1.into(),
                        Value::Map(vec![
                            (0.into(), id.clone().into()),
                            (5.into(), "standard".into()),
                        ]),
                    ),
                    (2.into(), Value::Array(children)),
                ]),
            ));
            let body = if targets.is_empty() {
                "<controls/>".to_string()
            } else {
                targets
                    .iter()
                    .map(|target| format!("<include file=\"{target}\"/>"))
                    .collect()
            };
            let xml = format!("<monitor name=\"{id}\" init=\"standard\">{body}</monitor>");
            manifest.push((
                format!("monitor/{id}.xml").into(),
                Value::Bytes(Sha256::digest(xml.as_bytes()).to_vec()),
            ));
        }
        let manifest = Value::Map(manifest);
        let snapshot = Sha256::digest(encode(manifest.clone())).to_vec();
        for (id, replacement) in [
            (6, Value::Map(profiles)),
            (8, manifest),
            (9, Value::Bytes(snapshot)),
        ] {
            root.as_map_mut()
                .unwrap()
                .iter_mut()
                .find(|(key, _)| uint(key) == Ok(id))
                .unwrap()
                .1 = replacement;
        }
        decode(&encode(root)).unwrap()
    }

    #[test]
    fn include_depth_rejection_preserves_valid_suffixes_in_both_id_orders() {
        for count in [256, 257] {
            for reversed in [false, true] {
                let ids: Vec<_> = (0..count)
                    .map(|index| {
                        format!("P{:03}", if reversed { count - 1 - index } else { index })
                    })
                    .collect();
                let edges = ids
                    .iter()
                    .enumerate()
                    .map(|(index, id)| {
                        (
                            id.clone(),
                            ids.get(index + 1).cloned().into_iter().collect(),
                        )
                    })
                    .collect();
                let database = graph_database(&edges);
                for (index, id) in ids.iter().enumerate() {
                    assert_eq!(
                        database.profiles[id].required,
                        count - index > 256,
                        "{id}, count={count}, reversed={reversed}"
                    );
                }
            }
        }
    }

    #[test]
    fn invalid_includes_reject_only_their_dependants() {
        let edges = [
            ("cycle_a", vec!["cycle_b"]),
            ("cycle_b", vec!["cycle_a"]),
            ("cycle_parent", vec!["cycle_a", "leaf"]),
            ("missing", vec!["absent"]),
            ("missing_parent", vec!["missing"]),
            ("self_cycle", vec!["self_cycle"]),
            ("good", vec!["leaf"]),
            ("duplicate", vec!["leaf", "leaf"]),
            ("leaf", vec![]),
        ]
        .into_iter()
        .map(|(id, targets)| {
            (
                id.to_string(),
                targets.into_iter().map(str::to_string).collect(),
            )
        })
        .collect();
        let database = graph_database(&edges);
        let rejected: Vec<_> = database
            .profiles
            .iter()
            .filter(|(_, node)| node.required)
            .map(|(id, _)| id.as_str())
            .collect();
        assert_eq!(
            rejected,
            [
                "cycle_a",
                "cycle_b",
                "cycle_parent",
                "missing",
                "missing_parent",
                "self_cycle"
            ]
        );
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

/// Attribute roles are shared with the source converter; namespaced attributes
/// are never passed here as unqualified executable names.
pub fn executable_attributes(tag: &str, parent: &str, in_options: bool) -> &'static [u64] {
    match (tag, parent, in_options) {
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
    }
}

pub fn executable_path(tag: &str, parent: &str, in_options: bool, ancestor_active: bool) -> bool {
    ancestor_active
        && (!executable_attributes(tag, parent, in_options).is_empty()
            || (tag == "controls" && parent == "monitor" && !in_options))
}

pub fn valid_identity(id: &str) -> bool {
    let scheme = id.split_once(':').map(|(scheme, _)| scheme).unwrap_or("");
    scheme.starts_with(|c: char| c.is_ascii_alphabetic())
        && scheme
            .bytes()
            .all(|b| b.is_ascii_alphanumeric() || b"+.-".contains(&b))
        && !id.chars().any(|c| c.is_whitespace() || c.is_control())
}

mod writing;
pub use writing::{encode, fallback_manifest, validate};
mod descriptors;

pub mod xml;
