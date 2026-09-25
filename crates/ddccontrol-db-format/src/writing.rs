//! Lossless tooling validates the contract without negotiating runtime support.
use super::*;

pub fn encode(value: &Value) -> Result<Vec<u8>, String> {
    fn canonical(value: &Value, depth: usize, items: &mut usize) -> Result<Value, String> {
        if depth > MAX_DEPTH || *items >= MAX_ITEMS {
            return Err("CBOR nesting/item limit exceeded".into());
        }
        *items += 1;
        Ok(match value {
            Value::Map(entries) => {
                if entries.len() > MAX_CONTAINER {
                    return Err("CBOR container limit exceeded".into());
                }
                let mut result = Vec::with_capacity(entries.len());
                for (key, value) in entries {
                    if !matches!(key, Value::Text(_)) && uint(key).is_err() {
                        return Err("CBOR map keys must be unsigned integers or text".into());
                    }
                    let key = canonical(key, depth + 1, items)?;
                    let value = canonical(value, depth + 1, items)?;
                    let mut wire = Vec::new();
                    ciborium::into_writer(&key, &mut wire).map_err(|e| e.to_string())?;
                    result.push((wire, key, value));
                }
                result.sort_by(|a, b| a.0.cmp(&b.0));
                if result.windows(2).any(|w| w[0].0 == w[1].0) {
                    return Err("CBOR duplicate map key".into());
                }
                Value::Map(result.into_iter().map(|(_, k, v)| (k, v)).collect())
            }
            Value::Array(values) => {
                if values.len() > MAX_CONTAINER {
                    return Err("CBOR container limit exceeded".into());
                }
                Value::Array(
                    values
                        .iter()
                        .map(|v| canonical(v, depth + 1, items))
                        .collect::<Result<_, _>>()?,
                )
            }
            Value::Bytes(bytes) if bytes.len() <= MAX_BYTES => value.clone(),
            Value::Text(text) if text.len() <= MAX_TEXT => value.clone(),
            Value::Integer(_) | Value::Bool(_) | Value::Null => value.clone(),
            _ => return Err("CBOR forbidden value or string limit exceeded".into()),
        })
    }
    let value = canonical(value, 0, &mut 0)?;
    // Enforce the aggregate file limit while encoding, before extending memory.
    struct Bounded(Vec<u8>);
    impl std::io::Write for Bounded {
        fn write(&mut self, bytes: &[u8]) -> std::io::Result<usize> {
            if bytes.len() > MAX_FILE - self.0.len() {
                return Err(std::io::Error::other("CBOR file limit exceeded"));
            }
            self.0.extend_from_slice(bytes);
            Ok(bytes.len())
        }
        fn flush(&mut self) -> std::io::Result<()> {
            Ok(())
        }
    }
    let mut out = Bounded(Vec::new());
    ciborium::into_writer(&value, &mut out).map_err(|e| e.to_string())?;
    Ok(out.0)
}

/// Validate and retain every field, including unknown required extensions.
/// Consumers use `decode`, which separately negotiates execution support.
pub fn validate(bytes: &[u8]) -> Result<Value, String> {
    let root = value(bytes)?;
    let db = decode_value(&root, false)?;
    if db.profiles.is_empty() {
        return Err("CBOR database has no profiles".into());
    }
    let invalid = invalid_reference_profiles(&db.profiles, true);
    if !invalid.is_empty() {
        return Err(format!(
            "CBOR missing/cyclic/excessive include: {}",
            invalid.join(", ")
        ));
    }
    fn extensions_at(value: &Value, key: u64) -> Result<(), String> {
        if let Some(extensions) = optional(value, key) {
            descriptors::extensions(extensions)?;
        }
        Ok(())
    }
    fn node(value: &Value, parent: &str, options: bool, ancestor: bool) -> Result<(), String> {
        let kind = uint(field(value, 0)?)?;
        let tag = kind_name(kind).unwrap_or("unknown");
        let active = executable_path(tag, parent, options, ancestor);
        if !active
            && fields(field(value, 1)?)?
                .iter()
                .any(|(k, _)| uint(k).is_ok_and(|id| id < 16))
        {
            return Err("CBOR executable attribute in inactive branch".into());
        }
        extensions_at(value, 3)?;
        for child in array(field(value, 2)?)? {
            node(child, tag, options, active)?;
        }
        Ok(())
    }
    extensions_at(&root, 7)?;
    node(field(&root, 5)?, "", true, true)?;
    for (_, profile) in map(field(&root, 6)?)? {
        node(profile, "", false, true)?;
    }
    Ok(root)
}

/// Unknown necessary syntax must not be bypassed by removing the CBOR file.
pub fn fallback_manifest(root: &Value) -> Result<Value, String> {
    fn unknown_node(node: &Value) -> Result<bool, String> {
        let kind = uint(field(node, 0)?)?;
        if kind_name(kind).is_none() && kind != 255 {
            return Ok(true);
        }
        for child in array(field(node, 2)?)? {
            if unknown_node(child)? {
                return Ok(true);
            }
        }
        Ok(false)
    }
    fn necessary(value: &Value) -> bool {
        match value {
            Value::Map(entries) => {
                (optional(value, 0)
                    .and_then(Value::as_text)
                    .is_some_and(valid_identity)
                    && optional(value, 1).and_then(Value::as_bool) == Some(true)
                    && optional(value, 2).is_some())
                    || entries.iter().any(|(_, v)| necessary(v))
            }
            Value::Array(values) => values.iter().any(necessary),
            _ => false,
        }
    }
    if necessary(root) || unknown_node(field(root, 5)?)? {
        return Ok(Value::Bool(false));
    }
    for (_, profile) in map(field(root, 6)?)? {
        if unknown_node(profile)? {
            return Ok(Value::Bool(false));
        }
    }
    Ok(field(root, 8)?.clone())
}
