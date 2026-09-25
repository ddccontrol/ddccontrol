//! Typed descriptive data. Validation never makes an operation executable.
use super::*;
const DESCRIPTION: &str = "https://ddccontrol.sourceforge.net/cbor/ext/function-description/1";

type Check = fn(&Value) -> Result<(), String>;
fn object(
    v: &Value,
    checks: &[(u64, Check)],
    required: &[u64],
    extensible: bool,
) -> Result<(), String> {
    for &id in required {
        field(v, id)?;
    }
    for (key, value) in fields(v)? {
        let id = uint(key)?;
        if let Some((_, check)) = checks.iter().find(|(k, _)| *k == id) {
            check(value)?;
        } else if !extensible {
            return Err("CBOR unassigned field in fixed descriptor".into());
        }
    }
    Ok(())
}
fn any(_: &Value) -> Result<(), String> {
    Ok(())
}
fn txt(v: &Value) -> Result<(), String> {
    text(v).map(|_| ())
}
fn int(v: &Value) -> Result<(), String> {
    integer(v).map(|_| ())
}
fn unsigned(v: &Value) -> Result<(), String> {
    uint(v).map(|_| ())
}
fn range(v: &Value, lo: u64, hi: u64) -> Result<(), String> {
    let n = uint(v)?;
    if (lo..=hi).contains(&n) {
        Ok(())
    } else {
        Err("CBOR descriptor integer out of range".into())
    }
}
fn vcp(v: &Value) -> Result<(), String> {
    range(v, 0, 255)
}
fn identity(v: &Value) -> Result<(), String> {
    if valid_identity(text(v)?) {
        Ok(())
    } else {
        Err("CBOR invalid descriptor identity".into())
    }
}
fn binary(v: &Value) -> Result<(), String> {
    v.as_bytes()
        .map(|_| ())
        .ok_or_else(|| "CBOR descriptor requires bytes".into())
}
fn list(v: &Value, check: Check) -> Result<(), String> {
    array(v)?.iter().try_for_each(check)
}
fn datum(v: &Value) -> Result<(), String> {
    match v {
        Value::Integer(_) | Value::Text(_) | Value::Bytes(_) => Ok(()),
        _ => object(
            v,
            &[(0, int), (1, |v| range(v, 1, u64::MAX))],
            &[0, 1],
            false,
        ),
    }
}
fn quantity(v: &Value) -> Result<(), String> {
    object(v, &[(0, |v| range(v, 1, 2)), (1, datum)], &[0], false)?;
    if (uint(field(v, 0)?)? == 1) != optional(v, 1).is_some() {
        return Err("CBOR declared quantity needs datum; discovered quantity forbids datum".into());
    }
    Ok(())
}
fn choice(v: &Value) -> Result<(), String> {
    object(v, &[(0, datum), (1, txt)], &[0], false)
}
fn bitfield(v: &Value) -> Result<(), String> {
    object(
        v,
        &[
            (0, unsigned),
            (1, |v| range(v, 1, 64)),
            (2, |v| list(v, choice)),
            (3, txt),
        ],
        &[0, 1],
        false,
    )
}
fn version(v: &Value) -> Result<(), String> {
    object(v, &[(0, identity), (1, txt)], &[0, 1], false)
}
fn condition(v: &Value) -> Result<(), String> {
    object(v, &[(0, identity), (1, any)], &[0, 1], false)
}
fn opaque(v: &Value) -> Result<(), String> {
    object(
        v,
        &[(0, identity), (1, identity), (2, binary)],
        &[0, 1, 2],
        false,
    )
}
fn operation(v: &Value) -> Result<(), String> {
    object(
        v,
        &[
            (0, unsigned),
            (1, vcp),
            (2, any),
            (3, identity),
            (4, |v| list(v, condition)),
        ],
        &[0],
        true,
    )
}
fn description(v: &Value) -> Result<(), String> {
    object(
        v,
        &[
            (0, |v| range(v, 1, 6)),
            (1, |v| range(v, 0, 3)),
            (2, |v| list(v, operation)),
            (3, identity),
            (4, vcp),
            (5, identity),
            (6, quantity),
            (7, quantity),
            (8, quantity),
            (9, quantity),
            (10, |v| list(v, choice)),
            (11, |v| list(v, bitfield)),
            (12, |v| list(v, version)),
            (13, txt),
            (14, |v| list(v, vcp)),
            (15, |v| list(v, condition)),
            (16, |v| list(v, opaque)),
            (17, txt),
            (18, extensions),
            (19, |v| list(v, condition)),
        ],
        &[],
        true,
    )
}
pub(super) fn extensions(v: &Value) -> Result<(), String> {
    // Use exactly the consumer's envelope, identity and metadata rules.
    required_extensions(&Value::Map(vec![(0.into(), v.clone())]), 0)?;
    for ext in array(v)? {
        if text(field(ext, 0)?)? == DESCRIPTION {
            description(field(ext, 2)?)?;
        }
    }
    Ok(())
}
