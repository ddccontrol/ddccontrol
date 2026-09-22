//! Hardware-free measurements of the actual decoder and C ABI tree builder.
//! Run in release mode with a database directory as the sole argument.
include!("../src/lib.rs");

use std::alloc::{GlobalAlloc, Layout, System};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::time::Instant;

struct Measured;
static LIVE: AtomicUsize = AtomicUsize::new(0);
static PEAK: AtomicUsize = AtomicUsize::new(0);

fn added(size: usize) {
    let current = LIVE.fetch_add(size, Ordering::Relaxed) + size;
    PEAK.fetch_max(current, Ordering::Relaxed);
}

unsafe impl GlobalAlloc for Measured {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let pointer = System.alloc(layout);
        if !pointer.is_null() {
            added(layout.size());
        }
        pointer
    }
    unsafe fn dealloc(&self, pointer: *mut u8, layout: Layout) {
        LIVE.fetch_sub(layout.size(), Ordering::Relaxed);
        System.dealloc(pointer, layout);
    }
    unsafe fn realloc(&self, pointer: *mut u8, layout: Layout, size: usize) -> *mut u8 {
        let result = System.realloc(pointer, layout, size);
        if !result.is_null() {
            LIVE.fetch_sub(layout.size(), Ordering::Relaxed);
            added(size);
        }
        result
    }
}

#[global_allocator]
static ALLOCATOR: Measured = Measured;

fn rss() -> String {
    std::fs::read_to_string("/proc/self/status")
        .unwrap_or_default()
        .lines()
        .filter(|line| line.starts_with("VmRSS:") || line.starts_with("VmHWM:"))
        .collect::<Vec<_>>()
        .join(", ")
}

fn heap() -> usize {
    #[cfg(all(target_os = "linux", target_env = "gnu"))]
    {
        let info = unsafe { libc::mallinfo2() };
        info.uordblks + info.hblkhd
    }
    #[cfg(not(all(target_os = "linux", target_env = "gnu")))]
    {
        0
    }
}

fn main() {
    let directory =
        std::path::PathBuf::from(std::env::args_os().nth(1).expect("database directory"));
    let mut ids: Vec<_> = std::fs::read_dir(directory.join("monitor"))
        .unwrap()
        .map(Result::unwrap)
        .filter_map(|entry| {
            let path = entry.path();
            (path.extension()? == "xml")
                .then(|| path.file_stem().unwrap().to_str().unwrap().to_owned())
        })
        .collect();
    ids.sort();
    let baseline = LIVE.load(Ordering::Relaxed);
    let heap_baseline = heap();
    PEAK.store(baseline, Ordering::Relaxed);
    let path = directory.join("ddccontrol-db.cbor");
    if path.exists() {
        let start = Instant::now();
        let bytes = std::fs::read(&path).unwrap();
        let read_us = start.elapsed().as_micros();
        let start = Instant::now();
        let decoded = cbor::decode(&bytes).unwrap();
        let decode_us = start.elapsed().as_micros();
        println!("file_bytes={} read_us={read_us} decode_us={decode_us} decoded_profiles={} source_files={} snapshot_bytes={} rust_live={} rust_peak={}",
            bytes.len(), decoded.profiles.len(), decoded.manifest.len(), decoded.snapshot.len(),
            LIVE.load(Ordering::Relaxed) - baseline, PEAK.load(Ordering::Relaxed) - baseline);
        drop(decoded);
        drop(bytes);
    }
    let path = std::ffi::CString::new(directory.to_str().unwrap()).unwrap();
    let start = Instant::now();
    assert_eq!(
        unsafe { monitor_db::ddcci_init_db(path.as_ptr() as *mut c_char) },
        1
    );
    let init_us = start.elapsed().as_micros();
    let mut profiles = Vec::with_capacity(ids.len());
    let start = Instant::now();
    for id in &ids {
        let mut caps = CCaps {
            vcp: [ptr::null_mut(); 256],
            monitor_type: 1,
            raw_caps: ptr::null_mut(),
        };
        for entry in &mut caps.vcp {
            unsafe {
                *entry = malloc(std::mem::size_of::<CVcpEntry>()) as *mut CVcpEntry;
                assert!(!(*entry).is_null());
                ptr::write(
                    *entry,
                    CVcpEntry {
                        values_len: -1,
                        values: ptr::null_mut(),
                    },
                );
            }
        }
        let name = std::ffi::CString::new(id.as_str()).unwrap();
        let profile = unsafe { monitor_db::ddcci_create_db(name.as_ptr(), &mut caps, 1) };
        assert!(!profile.is_null(), "{id}");
        profiles.push(profile);
        unsafe {
            free_c_vcp_entries(&mut caps);
        }
    }
    let build_us = start.elapsed().as_micros();
    println!("profiles={} init_us={init_us} build_us={build_us} rust_live={} rust_peak={} heap_delta={} {}",
        profiles.len(), LIVE.load(Ordering::Relaxed) - baseline, PEAK.load(Ordering::Relaxed) - baseline, heap().saturating_sub(heap_baseline), rss());
    unsafe {
        monitor_db::ddcci_release_db();
    }
    // Touch names after source storage/session release through the unchanged C ABI.
    for profile in &profiles {
        let name = unsafe { *(*profile as *const *const c_char) };
        assert!(!unsafe { CStr::from_ptr(name) }.to_bytes().is_empty());
    }
    println!(
        "after_session_release rust_live={} heap_delta={} {}",
        LIVE.load(Ordering::Relaxed) - baseline,
        heap().saturating_sub(heap_baseline),
        rss()
    );
    for profile in profiles {
        unsafe {
            monitor_db::ddcci_free_db(profile);
        }
    }
    println!(
        "after_tree_release rust_live={} heap_delta={} {}",
        LIVE.load(Ordering::Relaxed) - baseline,
        heap().saturating_sub(heap_baseline),
        rss()
    );
    let start = Instant::now();
    assert_eq!(
        unsafe { monitor_db::ddcci_init_db(path.as_ptr() as *mut c_char) },
        1
    );
    unsafe {
        monitor_db::ddcci_release_db();
    }
    println!("rescan_us={}", start.elapsed().as_micros());
}
