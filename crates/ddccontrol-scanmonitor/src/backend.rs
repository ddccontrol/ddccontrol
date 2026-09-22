// Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

//! Read monitor information through ddccontrol's existing system D-Bus service.
//!
//! Only opaque GLib pointers and C ABI scalar types cross this boundary. GLib
//! owns every allocation it returns; the wrappers below release those objects
//! with the matching GLib function. In particular, no native-endian decoding of
//! serialized D-Bus messages or Rust layout assumptions are needed here.

use ddccontrol_edid::is_valid_pnp_id;
use std::ffi::{c_char, c_int, c_uchar, c_uint, c_ushort, c_void, CStr, CString};
use std::ptr::{self, NonNull};

const SERVICE: &[u8] = b"ddccontrol.DDCControl\0";
const OBJECT_PATH: &[u8] = b"/ddccontrol/DDCControl\0";
const SERVICE_HELP: &str =
    "Install ddccontrol and make sure its system D-Bus service (ddccontrol.service) is available.";

#[derive(Debug, Eq, PartialEq)]
pub struct Monitor {
    pub device: String,
    pub name: String,
}

#[derive(Debug, Eq, PartialEq)]
pub struct OpenedMonitor {
    pub pnp_id: String,
    pub capabilities: String,
    pub name: Option<String>,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Reading {
    pub current: u16,
    pub maximum: u16,
}

pub struct Backend {
    proxy: NonNull<c_void>,
}

impl Backend {
    pub fn connect() -> Result<Self, String> {
        let mut error = ptr::null_mut();
        // SAFETY: String arguments are static, NUL-terminated UTF-8. Optional
        // interface metadata and cancellation are null. GIO returns an owned
        // proxy, released by Backend::drop, or an owned GError.
        let proxy = unsafe {
            g_dbus_proxy_new_for_bus_sync(
                1, // G_BUS_TYPE_SYSTEM
                3, // DO_NOT_LOAD_PROPERTIES | DO_NOT_CONNECT_SIGNALS
                ptr::null_mut(),
                SERVICE.as_ptr().cast(),
                OBJECT_PATH.as_ptr().cast(),
                SERVICE.as_ptr().cast(),
                ptr::null_mut(),
                &mut error,
            )
        };
        let proxy = NonNull::new(proxy).ok_or_else(|| {
            format!(
                "Cannot connect to the system D-Bus: {}. {SERVICE_HELP}",
                take_error(error)
            )
        })?;
        Ok(Self { proxy })
    }

    /// Discover monitors that the daemon identified as supporting DDC/CI.
    pub fn list(&self) -> Result<Vec<Monitor>, String> {
        let reply = self
            .call("RescanMonitors", &[], 120_000)
            .map_err(|error| format!("Cannot discover monitors: {error}. {SERVICE_HELP}"))?;
        decode_monitors(&reply)
    }

    pub fn open(&self, device: &str) -> Result<OpenedMonitor, String> {
        let reply = self.call("OpenMonitor", &[Variant::string(device)?], 60_000)?;
        let (pnp_id, capabilities) = decode_open(&reply)?;

        // Older ddccontrol daemons do not expose GetEdid. Opening and scanning
        // remain useful in that case; the caller can use the discovered name.
        let name = self
            .call("GetEdid", &[Variant::string(device)?], 10_000)
            .ok()
            .and_then(|reply| decode_edid_name(&reply).ok().flatten())
            .or_else(|| capability_model(&capabilities));

        Ok(OpenedMonitor {
            pnp_id,
            capabilities,
            name,
        })
    }

    /// GetControl performs the existing daemon's hardware retries. A successful
    /// unsupported-control reply is distinct from a transport or monitor error.
    pub fn read(&self, device: &str, code: u8) -> Result<Option<Reading>, String> {
        let reply = self.call(
            "GetControl",
            &[Variant::string(device)?, Variant::uint32(u32::from(code))],
            10_000,
        )?;
        decode_reading(&reply).map_err(|error| format!("Control 0x{code:02x}: {error}"))
    }

    fn call(
        &self,
        method: &str,
        arguments: &[Variant],
        timeout_ms: c_int,
    ) -> Result<Variant, String> {
        let method_name = CString::new(method).map_err(|_| "Invalid D-Bus method name")?;
        let parameters = Variant::tuple(arguments);
        let mut error = ptr::null_mut();
        // SAFETY: The proxy and parameters remain alive throughout this call.
        // Parameters hold a non-floating reference, so GIO does not consume our
        // reference. The reply (or GError on failure) transfers ownership to us.
        let reply = unsafe {
            g_dbus_proxy_call_sync(
                self.proxy.as_ptr(),
                method_name.as_ptr(),
                parameters.0.as_ptr(),
                0, // G_DBUS_CALL_FLAGS_NONE
                timeout_ms,
                ptr::null_mut(),
                &mut error,
            )
        };
        NonNull::new(reply)
            .map(Variant)
            .ok_or_else(|| format!("{method} failed: {}", take_error(error)))
    }
}

impl Drop for Backend {
    fn drop(&mut self) {
        // SAFETY: This is the owned reference returned when connecting.
        unsafe { g_object_unref(self.proxy.as_ptr()) };
    }
}

fn decode_monitors(reply: &Variant) -> Result<Vec<Monitor>, String> {
    reply.expect_type("(asa(y)asa(y))")?;
    let devices = reply.child(0);
    let supported = reply.child(1);
    let names = reply.child(2);
    let digital = reply.child(3);
    let count = devices.len();
    if supported.len() != count || names.len() != count || digital.len() != count {
        return Err("The D-Bus service returned inconsistent monitor lists".into());
    }
    let mut monitors = Vec::new();
    for index in 0..count {
        // The complete reply signature above guarantees the byte child type.
        if unsafe { g_variant_get_byte(supported.child(index).child(0).0.as_ptr()) } != 0 {
            monitors.push(Monitor {
                device: devices.child(index).string_value(),
                name: names.child(index).string_value(),
            });
        }
    }
    Ok(monitors)
}

fn decode_open(reply: &Variant) -> Result<(String, String), String> {
    reply.expect_type("(ss)")?;
    let mut pnp_id = reply.child(0).string_value();
    let mut capabilities = reply.child(1).string_value();
    // Match the existing C client's compatibility handling for older services
    // which returned these two strings in the opposite order.
    if pnp_id.starts_with('(') && is_valid_pnp_id(&capabilities) {
        std::mem::swap(&mut pnp_id, &mut capabilities);
    }
    if !is_valid_pnp_id(&pnp_id) {
        return Err(format!(
            "The D-Bus service returned an invalid monitor Plug and Play ID: {pnp_id:?}"
        ));
    }
    Ok((pnp_id, capabilities))
}

fn capability_model(capabilities: &str) -> Option<String> {
    for (index, _) in capabilities.match_indices("model(") {
        if index != 0
            && !matches!(capabilities.as_bytes()[index - 1], b'(' | b')')
            && !capabilities.as_bytes()[index - 1].is_ascii_whitespace()
        {
            continue;
        }
        let rest = &capabilities[index + "model(".len()..];
        let model = rest.split_once(')')?.0.trim();
        if !model.is_empty() && !model.contains('(') {
            return Some(model.to_owned());
        }
    }
    None
}

fn decode_edid_name(reply: &Variant) -> Result<Option<String>, String> {
    reply.expect_type("(ay)")?;
    let bytes = reply.child(0);
    // The checked reply type guarantees an array of bytes. Reading individual
    // values also avoids host-endian or alignment assumptions.
    let bytes: Vec<u8> = (0..bytes.len())
        .map(|index| unsafe { g_variant_get_byte(bytes.child(index).0.as_ptr()) })
        .collect();
    let edid = ddccontrol_edid::parse(&bytes).map_err(|error| error.to_string())?;
    let name = edid.info().monitor_name.trim();
    Ok((!name.is_empty()).then(|| name.to_owned()))
}

fn decode_reading(reply: &Variant) -> Result<Option<Reading>, String> {
    reply.expect_type("(iqq)")?;
    // SAFETY: The checked tuple signature guarantees all three scalar types.
    let result = unsafe { g_variant_get_int32(reply.child(0).0.as_ptr()) };
    if result < 0 {
        return Err(format!(
            "the monitor did not return a reading (error {result})"
        ));
    }
    if result == 0 {
        return Ok(None);
    }
    Ok(Some(Reading {
        current: unsafe { g_variant_get_uint16(reply.child(1).0.as_ptr()) },
        maximum: unsafe { g_variant_get_uint16(reply.child(2).0.as_ptr()) },
    }))
}

/// An owned, non-floating GVariant reference.
struct Variant(NonNull<c_void>);

impl Variant {
    fn string(value: &str) -> Result<Self, String> {
        let value = CString::new(value).map_err(|_| "A device name contains a NUL byte")?;
        // SAFETY: GLib copies the NUL-terminated UTF-8 string.
        Ok(unsafe { Self::sink(g_variant_new_string(value.as_ptr())) })
    }

    fn uint32(value: u32) -> Self {
        // SAFETY: This GLib scalar constructor has no preconditions.
        unsafe { Self::sink(g_variant_new_uint32(value)) }
    }

    fn tuple(children: &[Self]) -> Self {
        let pointers: Vec<_> = children.iter().map(|child| child.0.as_ptr()).collect();
        // SAFETY: All child variants remain alive throughout construction. GLib
        // retains its own references because these children are non-floating.
        unsafe { Self::sink(g_variant_new_tuple(pointers.as_ptr(), pointers.len())) }
    }

    unsafe fn sink(pointer: *mut c_void) -> Self {
        // GLib allocation routines abort on OOM, so constructors cannot return
        // null for the valid arguments supplied by this module.
        Self(NonNull::new(g_variant_ref_sink(pointer)).expect("GLib returned a null variant"))
    }

    fn expect_type(&self, expected: &str) -> Result<(), String> {
        // SAFETY: The variant is alive; GLib returns its owned type string.
        let actual = unsafe { CStr::from_ptr(g_variant_get_type_string(self.0.as_ptr())) };
        if actual.to_bytes() == expected.as_bytes() {
            Ok(())
        } else {
            Err(format!(
                "Unexpected D-Bus reply type: expected {expected}, got {}",
                actual.to_string_lossy()
            ))
        }
    }

    fn len(&self) -> usize {
        // SAFETY: Call sites only use len on containers after checking type.
        unsafe { g_variant_n_children(self.0.as_ptr()) }
    }

    fn child(&self, index: usize) -> Self {
        assert!(index < self.len());
        // SAFETY: The index is valid, and GLib returns a new owned reference.
        Self(
            NonNull::new(unsafe { g_variant_get_child_value(self.0.as_ptr(), index) })
                .expect("GLib returned a null variant child"),
        )
    }

    fn string_value(&self) -> String {
        // SAFETY: Call sites have checked the parent tuple's exact signature,
        // including this child's string type. The copied string outlives GLib.
        unsafe {
            CStr::from_ptr(g_variant_get_string(self.0.as_ptr(), ptr::null_mut()))
                .to_string_lossy()
                .into_owned()
        }
    }
}

impl Drop for Variant {
    fn drop(&mut self) {
        // SAFETY: This wrapper owns exactly one GLib variant reference.
        unsafe { g_variant_unref(self.0.as_ptr()) };
    }
}

#[repr(C)]
struct GError {
    domain: c_uint,
    code: c_int,
    message: *mut c_char,
}

fn take_error(error: *mut GError) -> String {
    if error.is_null() {
        return "unknown D-Bus error".into();
    }
    // SAFETY: GIO transfers this GError to the caller after a failed call.
    // Its message is NUL-terminated, copied before freeing with GLib.
    unsafe {
        let message = CStr::from_ptr((*error).message)
            .to_string_lossy()
            .into_owned();
        g_error_free(error);
        message
    }
}

#[link(name = "gio-2.0")]
extern "C" {
    fn g_dbus_proxy_new_for_bus_sync(
        bus_type: c_int,
        flags: c_int,
        info: *mut c_void,
        name: *const c_char,
        object_path: *const c_char,
        interface_name: *const c_char,
        cancellable: *mut c_void,
        error: *mut *mut GError,
    ) -> *mut c_void;
    fn g_dbus_proxy_call_sync(
        proxy: *mut c_void,
        method_name: *const c_char,
        parameters: *mut c_void,
        flags: c_int,
        timeout_msec: c_int,
        cancellable: *mut c_void,
        error: *mut *mut GError,
    ) -> *mut c_void;
}

#[link(name = "gobject-2.0")]
extern "C" {
    fn g_object_unref(object: *mut c_void);
}

#[link(name = "glib-2.0")]
extern "C" {
    fn g_error_free(error: *mut GError);
    fn g_variant_unref(value: *mut c_void);
    fn g_variant_ref_sink(value: *mut c_void) -> *mut c_void;
    fn g_variant_new_tuple(children: *const *mut c_void, n_children: usize) -> *mut c_void;
    fn g_variant_new_string(string: *const c_char) -> *mut c_void;
    fn g_variant_new_uint32(value: u32) -> *mut c_void;
    fn g_variant_get_type_string(value: *mut c_void) -> *const c_char;
    fn g_variant_n_children(value: *mut c_void) -> usize;
    fn g_variant_get_child_value(value: *mut c_void, index: usize) -> *mut c_void;
    fn g_variant_get_string(value: *mut c_void, length: *mut usize) -> *const c_char;
    fn g_variant_get_byte(value: *mut c_void) -> c_uchar;
    fn g_variant_get_int32(value: *mut c_void) -> i32;
    fn g_variant_get_uint16(value: *mut c_void) -> c_ushort;
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn open_reply_accepts_current_and_legacy_field_order() {
        for fields in [["DEL1234", "(vcp(10 12))"], ["(vcp(10 12))", "DEL1234"]] {
            let reply = Variant::tuple(&[
                Variant::string(fields[0]).unwrap(),
                Variant::string(fields[1]).unwrap(),
            ]);
            assert_eq!(
                decode_open(&reply).unwrap(),
                ("DEL1234".into(), "(vcp(10 12))".into())
            );
        }
    }

    #[test]
    fn invalid_pnp_id_cannot_become_an_output_filename() {
        let reply = Variant::tuple(&[
            Variant::string("../../x").unwrap(),
            Variant::string("").unwrap(),
        ]);
        assert!(decode_open(&reply).unwrap_err().contains("invalid monitor"));
    }

    #[test]
    fn wrong_reply_types_are_rejected_before_accessing_fields() {
        let reply = Variant::tuple(&[Variant::uint32(1)]);
        assert!(decode_monitors(&reply).is_err());
        assert!(decode_open(&reply).is_err());
        assert!(decode_reading(&reply).is_err());
        assert!(decode_edid_name(&reply).is_err());
    }

    #[test]
    fn device_names_cannot_contain_embedded_nuls() {
        assert!(Variant::string("dev:/dev/i2c-1\0unexpected").is_err());
    }

    #[test]
    fn model_fallback_requires_a_complete_model_field() {
        assert_eq!(
            capability_model("(prot(monitor)model(Display & More)vcp(10))").as_deref(),
            Some("Display & More")
        );
        for raw in [
            "(vcp(10))",
            "(model())",
            "(model(unclosed",
            "(othermodel(Wrong))",
        ] {
            assert_eq!(capability_model(raw), None, "{raw}");
        }
    }
}
