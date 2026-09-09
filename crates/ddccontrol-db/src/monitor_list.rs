// Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

use ddccontrol_monitorlist::Monitor;
use libc::{c_char, c_int, c_uchar, c_void, free, malloc};
use std::ffi::CStr;
use std::fs;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::path::PathBuf;
use std::ptr;

#[repr(C)]
pub struct CMonitorList {
    filename: *mut c_char,
    supported: c_uchar,
    name: *mut c_char,
    digital: c_uchar,
    next: *mut CMonitorList,
}

#[no_mangle]
/// Load a cache into C-owned storage. Return 0 on success, -1 on failure.
/// `output` is written only on success, including NULL for an empty cache.
///
/// # Safety
///
/// `filename` and `version` must be readable NUL-terminated strings and `output`
/// must point to writable storage for one pointer. The returned nodes and
/// strings use C malloc and must be released with ddcci_free_list.
pub unsafe extern "C" fn ddccontrol_monitorlist_load(
    filename: *const c_char,
    version: *const c_char,
    output: *mut *mut CMonitorList,
) -> c_int {
    catch_unwind(AssertUnwindSafe(|| {
        if filename.is_null() || version.is_null() || output.is_null() {
            return -1;
        }
        let Some(list) = load_inner(CStr::from_ptr(filename), CStr::from_ptr(version)) else {
            return -1;
        };
        ptr::write(output, list.0);
        std::mem::forget(list);
        0
    }))
    .unwrap_or(-1)
}

unsafe fn load_inner(filename: &CStr, version: &CStr) -> Option<OwnedList> {
    let bytes = fs::read(path_from_c(filename)).ok()?;
    let monitors = ddccontrol_monitorlist::parse_bytes(&bytes, version.to_str().ok()?).ok()?;
    list_to_c(&monitors, &mut |size| malloc(size))
}

#[no_mangle]
/// Save a borrowed list to a local cache. Return 0 on success, -1 on failure.
/// Validation and serialization finish before opening the output file.
///
/// # Safety
///
/// `filename` and `version` must be readable NUL-terminated strings. `list` may
/// be NULL (empty) or point to a valid acyclic list with non-NULL NUL-terminated
/// UTF-8 strings. All inputs remain caller-owned and valid throughout the call.
pub unsafe extern "C" fn ddccontrol_monitorlist_save(
    filename: *const c_char,
    version: *const c_char,
    list: *const CMonitorList,
) -> c_int {
    catch_unwind(AssertUnwindSafe(|| {
        if filename.is_null() || version.is_null() {
            return -1;
        }
        save_inner(CStr::from_ptr(filename), CStr::from_ptr(version), list).map_or(-1, |()| 0)
    }))
    .unwrap_or(-1)
}

unsafe fn save_inner(filename: &CStr, version: &CStr, mut list: *const CMonitorList) -> Option<()> {
    let mut monitors = Vec::new();
    while let Some(node) = list.as_ref() {
        if node.filename.is_null() || node.name.is_null() {
            return None;
        }
        monitors.push(Monitor {
            filename: CStr::from_ptr(node.filename).to_str().ok()?.to_string(),
            supported: node.supported,
            name: CStr::from_ptr(node.name).to_str().ok()?.to_string(),
            digital: node.digital,
        });
        list = node.next;
    }
    let xml = ddccontrol_monitorlist::serialize(&monitors, version.to_str().ok()?).ok()?;
    fs::write(path_from_c(filename), xml).ok()
}

// Own partial allocations until the whole list can be handed to C. Dropping
// this guard also cleans up if allocation fails or Rust unwinds during load.
struct OwnedList(*mut CMonitorList);

impl Drop for OwnedList {
    fn drop(&mut self) {
        unsafe {
            let mut node = self.0;
            while !node.is_null() {
                let next = (*node).next;
                free((*node).filename.cast());
                free((*node).name.cast());
                free(node.cast());
                node = next;
            }
        }
    }
}

// The allocator must return malloc-compatible storage; accepting it here lets
// tests fail every allocation in turn without changing the exported C ABI.
unsafe fn list_to_c(
    monitors: &[Monitor],
    allocate: &mut impl FnMut(usize) -> *mut c_void,
) -> Option<OwnedList> {
    let mut list = OwnedList(ptr::null_mut());
    let mut tail = &mut list.0 as *mut *mut CMonitorList;
    for monitor in monitors {
        let node = allocate(std::mem::size_of::<CMonitorList>()).cast::<CMonitorList>();
        if node.is_null() {
            return None;
        }
        ptr::write(
            node,
            CMonitorList {
                filename: ptr::null_mut(),
                supported: monitor.supported,
                name: ptr::null_mut(),
                digital: monitor.digital,
                next: ptr::null_mut(),
            },
        );
        *tail = node;
        (*node).filename = c_string(&monitor.filename, allocate)?;
        (*node).name = c_string(&monitor.name, allocate)?;
        tail = &mut (*node).next;
    }
    Some(list)
}

unsafe fn c_string(
    input: &str,
    allocate: &mut impl FnMut(usize) -> *mut c_void,
) -> Option<*mut c_char> {
    if input.as_bytes().contains(&0) {
        return None;
    }
    let output = allocate(input.len().checked_add(1)?).cast::<c_char>();
    if output.is_null() {
        return None;
    }
    ptr::copy_nonoverlapping(input.as_ptr(), output.cast::<u8>(), input.len());
    *output.add(input.len()) = 0;
    Some(output)
}

fn path_from_c(path: &CStr) -> PathBuf {
    #[cfg(unix)]
    {
        use std::os::unix::ffi::OsStrExt;
        PathBuf::from(std::ffi::OsStr::from_bytes(path.to_bytes()))
    }
    #[cfg(not(unix))]
    {
        PathBuf::from(path.to_string_lossy().into_owned())
    }
}

#[cfg(test)]
mod tests {
    include!("../tests/unit/monitor_list.rs");
}
