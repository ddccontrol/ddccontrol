use crate::{ddccontrol_caps_parse, free_c_vcp_entries, CCaps, CVcpEntry};

use super::*;
use libc::{c_char, c_int, c_uchar, malloc};
use std::ffi::{CStr, CString};
use std::mem::size_of;
use std::path::{Path, PathBuf};
use std::{env, fs, ptr};

static TEST_DB_LOCK: std::sync::Mutex<()> = std::sync::Mutex::new(());

#[test]
fn compatibility_fixture_matches_golden_database_tree() {
    let _context = DbTestContext::init(&fixture_datadir());

    let mut caps = OwnedCaps::from_str("(prot(monitor)type(LCD)vcp(04 10 60))");
    let monitor = OwnedMonitor::create("compat-monitor", &mut caps, false).unwrap();
    let snapshot = monitor.snapshot();

    assert_eq!(
        snapshot,
        "\
monitor name=Compatibility Fixture Monitor init=1
group name=Image
  subgroup name=Picture pattern=image
    control id=brightness name=Brightness address=0x10 delay=120 type=0 refresh=1
    control id=input name=Input Source address=0x60 delay=-1 type=2 refresh=0
      value id=hdmi1 name=HDMI 1 value=0x11 value16=0x0011
      value id=displayport name=DisplayPort value=0x0F value16=0x000F
group name=Color
  subgroup name=Presets pattern=color
    control id=color_preset name=Color Preset address=0xC8 delay=-1 type=2 refresh=0
      value id=srgb name=sRGB value=0x34 value16=0x1234
      value id=native name=Native value=0x02 value16=0x0002
"
    );
}

#[test]
fn real_database_checkout_profiles_load_when_configured() {
    let Some(datadir) = env::var_os("DDCCONTROL_DB_TEST_DATADIR") else {
        return;
    };
    let datadir = resolve_real_database_datadir(PathBuf::from(datadir));
    let profiles = real_database_profiles(&datadir);
    assert!(
        !profiles.is_empty(),
        "no monitor profiles found in {}",
        datadir.join("monitor").display()
    );

    let _context = DbTestContext::init(&datadir);

    for profile in profiles {
        let mut caps = OwnedCaps::with_all_vcp_codes();
        let monitor = OwnedMonitor::create(&profile, &mut caps, true);
        assert!(
            monitor.is_some(),
            "failed to load monitor profile {profile}"
        );
    }
}

fn fixture_datadir() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("fixtures/compat-db")
}

fn resolve_real_database_datadir(path: PathBuf) -> PathBuf {
    if is_database_datadir(&path) {
        return path;
    }

    let db_path = path.join("db");
    if is_database_datadir(&db_path) {
        return db_path;
    }

    panic!(
        "DDCCONTROL_DB_TEST_DATADIR must point to a directory containing options.xml \
         and monitor/, or to a ddccontrol-db checkout with a db/ directory: {}",
        path.display()
    );
}

fn is_database_datadir(path: &Path) -> bool {
    path.join("options.xml").is_file() && path.join("monitor").is_dir()
}

fn real_database_profiles(datadir: &Path) -> Vec<String> {
    if let Ok(profiles) = env::var("DDCCONTROL_DB_TEST_PROFILES") {
        return profiles
            .split(',')
            .map(str::trim)
            .filter(|profile| !profile.is_empty())
            .map(ToString::to_string)
            .collect();
    }

    let mut profiles: Vec<_> = fs::read_dir(datadir.join("monitor"))
        .unwrap()
        .filter_map(Result::ok)
        .filter_map(|entry| {
            let path = entry.path();
            (path.extension().and_then(|ext| ext.to_str()) == Some("xml"))
                .then(|| path.file_stem()?.to_str().map(ToString::to_string))?
        })
        .collect();
    profiles.sort();
    profiles.truncate(25);
    profiles
}

fn path_to_cstring(path: &Path) -> CString {
    #[cfg(unix)]
    {
        use std::os::unix::ffi::OsStrExt;

        CString::new(path.as_os_str().as_bytes()).unwrap()
    }
    #[cfg(not(unix))]
    {
        CString::new(path.to_string_lossy().as_bytes()).unwrap()
    }
}

struct DbTestContext {
    _guard: std::sync::MutexGuard<'static, ()>,
}

impl DbTestContext {
    fn init(datadir: &Path) -> Self {
        let guard = TEST_DB_LOCK.lock().unwrap();
        let datadir = path_to_cstring(datadir);
        assert_eq!(unsafe { ddcci_init_db(datadir.as_ptr() as *mut c_char) }, 1);
        Self { _guard: guard }
    }
}

impl Drop for DbTestContext {
    fn drop(&mut self) {
        unsafe {
            ddcci_release_db();
        }
    }
}

struct OwnedCaps(CCaps);

impl OwnedCaps {
    fn empty() -> Self {
        Self(CCaps {
            vcp: [ptr::null_mut(); 256],
            monitor_type: 0,
            raw_caps: ptr::null_mut(),
        })
    }

    fn from_str(input: &str) -> Self {
        let mut caps = Self::empty();
        let input = CString::new(input).unwrap();
        assert!(unsafe { ddccontrol_caps_parse(input.as_ptr(), &mut caps.0, 1) } > 0);
        caps
    }

    fn with_all_vcp_codes() -> Self {
        let mut caps = Self::empty();
        caps.0.monitor_type = 1;
        for entry in &mut caps.0.vcp {
            let c_entry = unsafe { malloc(size_of::<CVcpEntry>()) as *mut CVcpEntry };
            assert!(!c_entry.is_null());
            unsafe {
                ptr::write(
                    c_entry,
                    CVcpEntry {
                        values_len: -1,
                        values: ptr::null_mut(),
                    },
                );
            }
            *entry = c_entry;
        }
        caps
    }

    fn as_mut_ptr(&mut self) -> *mut CCaps {
        &mut self.0
    }
}

impl Drop for OwnedCaps {
    fn drop(&mut self) {
        unsafe {
            free_c_vcp_entries(&mut self.0);
        }
    }
}

struct OwnedMonitor(*mut CMonitorDb);

impl OwnedMonitor {
    fn create(profile: &str, caps: &mut OwnedCaps, faulttolerance: bool) -> Option<Self> {
        let profile = CString::new(profile).unwrap();
        let monitor = unsafe {
            ddcci_create_db(
                profile.as_ptr(),
                caps.as_mut_ptr(),
                c_int::from(faulttolerance),
            )
        };
        (!monitor.is_null()).then_some(Self(monitor))
    }

    fn snapshot(&self) -> String {
        snapshot_monitor(self.0)
    }
}

impl Drop for OwnedMonitor {
    fn drop(&mut self) {
        unsafe {
            ddcci_free_db(self.0);
        }
    }
}

fn snapshot_monitor(monitor: *mut CMonitorDb) -> String {
    let mut snapshot = String::new();
    let monitor_ref = unsafe { &*monitor };
    snapshot.push_str(&format!(
        "monitor name={} init={}\n",
        c_xml_string(monitor_ref.name),
        monitor_ref.init
    ));

    let mut group = monitor_ref.group_list;
    while !group.is_null() {
        let group_ref = unsafe { &*group };
        snapshot.push_str(&format!("group name={}\n", c_xml_string(group_ref.name)));

        let mut subgroup = group_ref.subgroup_list;
        while !subgroup.is_null() {
            let subgroup_ref = unsafe { &*subgroup };
            let pattern = if subgroup_ref.pattern.is_null() {
                String::new()
            } else {
                c_xml_string(subgroup_ref.pattern)
            };
            snapshot.push_str(&format!(
                "  subgroup name={} pattern={}\n",
                c_xml_string(subgroup_ref.name),
                pattern
            ));

            let mut control = subgroup_ref.control_list;
            while !control.is_null() {
                let control_ref = unsafe { &*control };
                snapshot.push_str(&format!(
                    "    control id={} name={} address=0x{:02X} delay={} type={} refresh={}\n",
                    c_xml_string(control_ref.id),
                    c_xml_string(control_ref.name),
                    control_ref.address,
                    control_ref.delay,
                    control_ref.control_type,
                    control_ref.refresh
                ));

                let mut value = control_ref.value_list;
                while !value.is_null() {
                    let value_ref = unsafe { &*value };
                    let private = value as *mut CValueDbPrivate;
                    let private_ref = unsafe { &*private };
                    snapshot.push_str(&format!(
                        "      value id={} name={} value=0x{:02X} value16=0x{:04X}\n",
                        c_xml_string(value_ref.id),
                        c_xml_string(value_ref.name),
                        value_ref.value,
                        private_ref.value16
                    ));
                    value = value_ref.next;
                }

                control = control_ref.next;
            }

            subgroup = subgroup_ref.next;
        }

        group = group_ref.next;
    }

    snapshot
}

fn c_xml_string(ptr: *mut c_uchar) -> String {
    unsafe {
        CStr::from_ptr(ptr as *const c_char)
            .to_string_lossy()
            .into_owned()
    }
}
