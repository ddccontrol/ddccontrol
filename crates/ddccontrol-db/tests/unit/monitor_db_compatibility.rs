use super::*;
use crate::{ddccontrol_caps_parse, free_c_vcp_entries, CCaps, CVcpEntry};
use libc::{c_char, c_int, c_uchar, malloc};
use std::ffi::{CStr, CString};
use std::mem::size_of;
use std::path::{Path, PathBuf};
use std::{env, fs, ptr};

static TEST_DB_LOCK: std::sync::Mutex<()> = std::sync::Mutex::new(());

thread_local! {
    static PANIC_IN_DB_CALL: std::cell::Cell<bool> = std::cell::Cell::default();
}

pub(super) fn panic_if_requested() {
    PANIC_IN_DB_CALL.with(|requested| {
        if requested.replace(false) {
            panic!("injected database operation panic");
        }
    });
}

#[test]
fn exported_database_functions_contain_panics() {
    let _context = DbTestContext::init(&fixture_datadir());
    let datadir = path_to_cstring(&fixture_datadir());
    let name = CString::new("compat-monitor").unwrap();
    let mut caps = OwnedCaps::with_all_vcp_codes();

    unsafe {
        PANIC_IN_DB_CALL.with(|requested| requested.set(true));
        assert_eq!(ddcci_init_db(datadir.as_ptr() as *mut c_char), 0);
        PANIC_IN_DB_CALL.with(|requested| requested.set(true));
        assert_eq!(ddcci_set_monitor_file(ptr::null(), ptr::null_mut()), 0);
        PANIC_IN_DB_CALL.with(|requested| requested.set(true));
        assert_eq!(ddcci_monitor_file_matches(name.as_ptr()), 0);
        PANIC_IN_DB_CALL.with(|requested| requested.set(true));
        assert!(ddcci_create_db(name.as_ptr(), &mut caps.0, 0).is_null());
        PANIC_IN_DB_CALL.with(|requested| requested.set(true));
        ddcci_release_db();
        PANIC_IN_DB_CALL.with(|requested| requested.set(true));
        ddcci_free_db(ptr::null_mut());
    }

    // The process survived all extern "C" calls and remains usable.
    assert!(OwnedMonitor::create("compat-monitor", &mut caps, false).is_some());
}

#[test]
fn database_recovers_after_mutex_poisoning() {
    let _context = DbTestContext::init(&fixture_datadir());
    let mut caps = OwnedCaps::with_all_vcp_codes();
    let monitor = OwnedMonitor::create("compat-monitor", &mut caps, false).unwrap();

    let result = std::panic::catch_unwind(|| {
        let _guard = db_context();
        panic!("injected panic while holding the database mutex");
    });
    assert!(result.is_err());
    assert!(DB_CONTEXT.is_poisoned());
    assert!(OwnedMonitor::create("compat-monitor", &mut caps, false).is_some());

    unsafe { ddcci_release_db() };
    assert!(OwnedMonitor::create("compat-monitor", &mut caps, false).is_none());
    // Releasing the global context must not invalidate existing C-owned trees.
    assert!(monitor.snapshot().contains("Compatibility Fixture Monitor"));
    drop(monitor);

    let datadir = path_to_cstring(&fixture_datadir());
    assert_eq!(unsafe { ddcci_init_db(datadir.as_ptr() as *mut c_char) }, 1);
    assert!(OwnedMonitor::create("compat-monitor", &mut caps, false).is_some());
}

#[test]
fn failed_database_reinitialization_clears_old_context() {
    let _context = DbTestContext::init(&fixture_datadir());
    let missing = path_to_cstring(&fixture_datadir().join("missing-database"));
    assert_eq!(unsafe { ddcci_init_db(missing.as_ptr() as *mut c_char) }, 0);
    let mut caps = OwnedCaps::with_all_vcp_codes();
    assert!(OwnedMonitor::create("compat-monitor", &mut caps, false).is_none());
}

#[test]
fn database_entry_points_accept_null_arguments() {
    let _context = DbTestContext::init(&fixture_datadir());
    let name = CString::new("compat-monitor").unwrap();
    let mut caps = OwnedCaps::with_all_vcp_codes();
    unsafe {
        assert!(ddcci_create_db(ptr::null(), &mut caps.0, 0).is_null());
        assert!(ddcci_create_db(name.as_ptr(), ptr::null_mut(), 0).is_null());
        ddcci_free_db(ptr::null_mut());
    }
}

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
    let load_all = load_all_profiles_requested();
    let requested_profiles = env::var("DDCCONTROL_DB_TEST_PROFILES").ok();
    if let Some(profiles) = select_requested_profiles(requested_profiles.as_deref(), load_all) {
        return profiles;
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
    if !load_all {
        profiles.truncate(25);
    }
    profiles
}

fn select_requested_profiles(profiles: Option<&str>, load_all: bool) -> Option<Vec<String>> {
    assert!(
        !(load_all && profiles.is_some()),
        "DDCCONTROL_DB_TEST_ALL=1 and DDCCONTROL_DB_TEST_PROFILES cannot be used together"
    );

    profiles.map(|profiles| {
        profiles
            .split(',')
            .map(str::trim)
            .filter(|profile| !profile.is_empty())
            .map(ToString::to_string)
            .collect()
    })
}

fn load_all_profiles_requested() -> bool {
    matches!(env::var("DDCCONTROL_DB_TEST_ALL").as_deref(), Ok("1"))
}

#[test]
#[should_panic(
    expected = "DDCCONTROL_DB_TEST_ALL=1 and DDCCONTROL_DB_TEST_PROFILES cannot be used together"
)]
fn rejects_combined_all_and_selected_profile_modes() {
    select_requested_profiles(Some("DELA114"), true);
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

struct TemporaryDatabase(PathBuf);

impl TemporaryDatabase {
    fn new() -> Self {
        static NEXT: std::sync::atomic::AtomicUsize = std::sync::atomic::AtomicUsize::new(0);
        let index = NEXT.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
        let directory = env::temp_dir().join(format!(
            "ddccontrol-monitor-file-{}-{index}",
            std::process::id()
        ));
        fs::create_dir_all(directory.join("monitor")).unwrap();
        // distcheck makes source fixtures read-only. Copy their contents into
        // newly created writable files because these tests edit the temporary DB.
        for filename in ["options.xml", "monitor/compat-common.xml"] {
            fs::write(
                directory.join(filename),
                fs::read(fixture_datadir().join(filename)).unwrap(),
            )
            .unwrap();
        }
        Self(directory)
    }

    fn write(&self, filename: &str, xml: &str) -> PathBuf {
        let path = self.0.join(filename);
        fs::write(&path, xml).unwrap();
        path
    }
}

impl Drop for TemporaryDatabase {
    fn drop(&mut self) {
        fs::remove_dir_all(&self.0).unwrap();
    }
}

const LOCAL_MONITOR: &str = r#"<monitor name="Local monitor" init="standard">
    <controls><control id="brightness" address="0x10" delay="80"/></controls>
    </monitor>"#;

#[test]
fn scanner_hints_do_not_override_or_invalidate_installed_monitor_mappings() {
    let database = TemporaryDatabase::new();
    database.write("monitor/DEL1234.xml", LOCAL_MONITOR);
    let snapshot = || {
        let _context = DbTestContext::init(&database.0);
        let mut caps = OwnedCaps::with_all_vcp_codes();
        OwnedMonitor::create("DEL1234", &mut caps, false).unwrap().snapshot()
    };
    let expected = snapshot();
    let options = fs::read_to_string(database.0.join("options.xml")).unwrap()
        .replace("type=", "address=\"invalid scanner hint\" type=")
        .replace("<value id=", "<value value=\"invalid scanner hint\" id=");
    database.write("options.xml", &options);
    assert_eq!(snapshot(), expected);
}

fn set_monitor_file(path: &Path) -> c_int {
    let path = path_to_cstring(path);
    unsafe { ddcci_set_monitor_file(path.as_ptr(), ptr::null_mut()) }
}

fn monitor_file_matches(pnpid: &str) -> bool {
    let pnpid = CString::new(pnpid).unwrap();
    unsafe { ddcci_monitor_file_matches(pnpid.as_ptr()) == 1 }
}

#[test]
fn local_monitor_file_overrides_only_matching_root_and_snapshots_xml() {
    let database = TemporaryDatabase::new();
    let installed = LOCAL_MONITOR.replace("Local monitor", "Installed monitor");
    database.write("monitor/DEL1234.xml", &installed);
    database.write("monitor/ACR1234.xml", &installed);
    database.write("monitor/VESA.xml", &installed);
    let path = database.write("DEL1234.xml", LOCAL_MONITOR);
    let _context = DbTestContext::init(&database.0);
    let old_context = db_context().clone().unwrap();
    let c_path = path_to_cstring(&path);
    let mut pnpid: [c_char; 8] = [42; 8];
    assert_eq!(
        unsafe { ddcci_set_monitor_file(c_path.as_ptr(), pnpid.as_mut_ptr()) },
        1
    );
    assert_eq!(
        unsafe { CStr::from_ptr(pnpid.as_ptr()) }.to_bytes(),
        b"DEL1234"
    );
    assert!(old_context.monitor_file.is_none());
    assert!(monitor_file_matches("DEL1234"));
    assert!(!monitor_file_matches("ACR1234"));
    assert!(!monitor_file_matches("VESA"));
    assert_eq!(unsafe { ddcci_monitor_file_matches(ptr::null()) }, 0);
    fs::remove_file(path).unwrap();

    let mut caps = OwnedCaps::with_all_vcp_codes();
    let monitor = OwnedMonitor::create("DEL1234", &mut caps, true).unwrap();
    assert_eq!(
        monitor.snapshot(),
        "\
monitor name=Local monitor init=1
group name=Image
  subgroup name=Picture pattern=image
    control id=brightness name=Brightness address=0x10 delay=80 type=0 refresh=1
"
    );
    for id in ["ACR1234", "VESA"] {
        assert!(OwnedMonitor::create(id, &mut caps, true)
            .unwrap()
            .snapshot()
            .contains("Installed monitor"));
    }

    // Includes resolve in the regular database even when their ID is overridden.
    database.write(
        "monitor/ACR1234.xml",
        r#"<monitor name="Other monitor" init="samsung"><include file="DEL1234"/></monitor>"#,
    );
    // The current load session keeps the original database snapshot.
    assert!(OwnedMonitor::create("ACR1234", &mut caps, true)
        .unwrap()
        .snapshot()
        .contains("Installed monitor"));
    assert_eq!(reinitialize(&database.0), 1);
    assert_eq!(set_monitor_file(&database.write("DEL1234.xml", LOCAL_MONITOR)), 1);
    assert!(OwnedMonitor::create("ACR1234", &mut caps, true)
        .unwrap()
        .snapshot()
        .contains("Other monitor init=2"));
    assert_eq!(set_monitor_file(&database.write("DEL1234.xml", r#"<monitor name="Including itself" init="standard"><include file="DEL1234"/></monitor>"#)), 1);
    assert!(OwnedMonitor::create("DEL1234", &mut caps, true)
        .unwrap()
        .snapshot()
        .contains("brightness"));
}

#[test]
fn local_monitor_file_resolves_database_includes_and_applies_caps_patches() {
    let database = TemporaryDatabase::new();
    let path = database.write("DEL1234.xml", r#"<monitor name="Caps override" init="standard">
        <caps add="(vcp(60 C8))" remove="(vcp(10))"/>
        <controls><control id="color_preset" address="0xc8"><value id="srgb" value="0x1234"/></control></controls>
        <include file="compat-common"/>
        </monitor>"#);
    let _context = DbTestContext::init(&database.0);
    assert_eq!(set_monitor_file(&path), 1);
    let mut caps = OwnedCaps::from_str("(vcp(10))");
    let monitor = OwnedMonitor::create("DEL1234", &mut caps, true).unwrap();
    let snapshot = monitor.snapshot();
    assert!(!snapshot.contains("brightness"));
    assert!(snapshot.contains("control id=input"));
    assert!(snapshot.contains("value16=0x1234"));
    assert!(caps.0.vcp[0x10].is_null());
    assert!(!caps.0.vcp[0x60].is_null());
    assert!(!caps.0.vcp[0xc8].is_null());

    // Includes stay pinned until a new session; the new invalid definition
    // cannot be accepted when the local override is revalidated.
    database.write("monitor/compat-common.xml", r#"<monitor name="Broken"><controls><control id="missing" address="0x10"/></controls></monitor>"#);
    assert_eq!(OwnedMonitor::create("DEL1234", &mut caps, true).unwrap().snapshot(), snapshot);
    assert_eq!(reinitialize(&database.0), 1);
    assert_eq!(set_monitor_file(&path), 0);
    assert!(!monitor_file_matches("DEL1234"));
}

#[test]
fn invalid_local_monitor_files_fail_atomically_before_hardware_discovery() {
    let database = TemporaryDatabase::new();
    let path = database.write("DEL1234.xml", LOCAL_MONITOR);
    let _context = DbTestContext::init(&database.0);
    assert_eq!(set_monitor_file(&path), 1);
    let invalid = [
        "<monitor>",
        r#"<options/>"#,
        r#"<monitor init="standard"><controls/></monitor>"#,
        r#"<monitor name="Missing init"><controls/></monitor>"#,
        r#"<monitor name="Invalid init" init="invalid"><controls/></monitor>"#,
        r#"<monitor name="Missing controls" init="standard"/>"#,
        r#"<monitor name="Deprecated" init="standard" caps="(vcp(10))"><controls/></monitor>"#,
        r#"<monitor name="Deprecated" init="standard" include="compat-common"><controls/></monitor>"#,
        r#"<monitor name="Unknown" init="standard"><mystery/><controls/></monitor>"#,
        r#"<monitor name="Duplicate controls" init="standard"><controls/><controls/></monitor>"#,
        r#"<monitor name="Missing include" init="standard"><include file="missing"/></monitor>"#,
        r#"<monitor name="Invalid include" init="standard"><include file="../DEL1234"/></monitor>"#,
        r#"<monitor name="Malformed include" init="standard"><include/></monitor>"#,
        r#"<monitor name="Malformed caps" init="standard"><caps add="(vcp(zz))"/><controls/></monitor>"#,
        r#"<monitor name="Empty caps" init="standard"><caps/><controls/></monitor>"#,
        r#"<monitor name="Unknown control" init="standard"><controls><control id="missing" address="0x10"/></controls></monitor>"#,
        r#"<monitor name="Unknown value" init="standard"><controls><control id="input" address="0x60"><value id="missing" value="1"/></control></controls></monitor>"#,
        r#"<monitor name="Invalid value" init="standard"><controls><control id="input" address="0x60"><value id="hdmi1" value="65536"/></control></controls></monitor>"#,
        r#"<monitor name="Missing address" init="standard"><controls><control id="brightness"/></controls></monitor>"#,
        r#"<monitor name="Invalid address" init="standard"><controls><control id="brightness" address="256"/></controls></monitor>"#,
        r#"<monitor name="Invalid delay" init="standard"><controls><control id="brightness" address="0x10" delay="bad"/></controls></monitor>"#,
        r#"<monitor name="Removed caps" init="standard"><caps remove="(vcp(60))"/><controls><control id="input" address="0x60"><value id="missing" value="1"/></control></controls></monitor>"#,
        r#"<monitor name="Shadowed values" init="standard"><include file="compat-common"/><controls><control id="input" address="0x60"><value id="missing" value="1"/></control></controls></monitor>"#,
    ];
    let c_path = path_to_cstring(&path);
    for xml in invalid {
        fs::write(&path, xml).unwrap();
        let mut pnpid: [c_char; 8] = [42; 8];
        assert_eq!(
            unsafe { ddcci_set_monitor_file(c_path.as_ptr(), pnpid.as_mut_ptr()) },
            0,
            "accepted {xml}"
        );
        assert_eq!(pnpid, [42; 8]);
        assert!(monitor_file_matches("DEL1234"));
        let mut caps = OwnedCaps::with_all_vcp_codes();
        assert!(OwnedMonitor::create("DEL1234", &mut caps, true)
            .unwrap()
            .snapshot()
            .contains("Local monitor"));
    }
    for filename in [
        "DEL1234",
        "DEL123.xml",
        "DEL12345.xml",
        "del1234.xml",
        "DEL12G4.xml",
        "D1L1234.xml",
        "VESA.xml",
    ] {
        assert_eq!(
            set_monitor_file(&database.write(filename, LOCAL_MONITOR)),
            0
        );
    }
    assert_eq!(set_monitor_file(&database.0.join("ABC1234.xml")), 0);
    assert_eq!(
        unsafe { ddcci_set_monitor_file(ptr::null(), ptr::null_mut()) },
        0
    );
    assert!(monitor_file_matches("DEL1234"));
}

#[test]
fn local_monitor_file_validates_includes_even_if_overridden_and_resets_with_database() {
    let database = TemporaryDatabase::new();
    let path = database.write("DEL1234.xml", r#"<monitor name="Local monitor" init="standard"><include file="compat-common"/></monitor>"#);
    let _context = DbTestContext::init(&database.0);
    database.write(
        "monitor/compat-common.xml",
        r#"<monitor name="Bad included init" init="invalid"><controls/></monitor>"#,
    );
    assert_eq!(reinitialize(&database.0), 1);
    assert_eq!(set_monitor_file(&path), 0);
    database.write(
        "monitor/compat-common.xml",
        r#"<monitor name="Cycle"><include file="compat-common"/></monitor>"#,
    );
    assert_eq!(reinitialize(&database.0), 1);
    assert_eq!(set_monitor_file(&path), 0);
    database.write("DEL1234.xml", LOCAL_MONITOR);
    assert_eq!(set_monitor_file(&path), 1);
    let mut caps = OwnedCaps::with_all_vcp_codes();
    let monitor = OwnedMonitor::create("DEL1234", &mut caps, false).unwrap();
    unsafe { ddcci_release_db() };
    assert!(!monitor_file_matches("DEL1234"));
    assert_eq!(set_monitor_file(&path), 0);
    assert!(monitor.snapshot().contains("Local monitor"));
    let c_datadir = path_to_cstring(&database.0);
    assert_eq!(
        unsafe { ddcci_init_db(c_datadir.as_ptr() as *mut c_char) },
        1
    );
    assert!(!monitor_file_matches("DEL1234"));
    assert_eq!(set_monitor_file(&path), 1);
    assert_eq!(
        unsafe { ddcci_init_db(c_datadir.as_ptr() as *mut c_char) },
        1
    );
    assert!(!monitor_file_matches("DEL1234"));
    assert!(OwnedMonitor::create("DEL1234", &mut caps, true).is_none());
}

#[cfg(unix)]
#[test]
fn local_monitor_file_preserves_non_utf8_directory_names() {
    use std::ffi::OsStr;
    use std::os::unix::ffi::OsStrExt;
    let database = TemporaryDatabase::new();
    let directory = database.0.join(OsStr::from_bytes(b"non-utf8-\xff"));
    fs::create_dir(&directory).unwrap();
    let path = directory.join("DEL1234.xml");
    fs::write(&path, LOCAL_MONITOR).unwrap();
    let _context = DbTestContext::init(&database.0);
    assert_eq!(set_monitor_file(&path), 1);
    assert!(monitor_file_matches("DEL1234"));
    let mut caps = OwnedCaps::with_all_vcp_codes();
    assert!(OwnedMonitor::create("DEL1234", &mut caps, true)
        .unwrap()
        .snapshot()
        .contains("Local monitor"));
}

#[test]
fn required_profile_feature_blocks_generic_fallback_and_preserves_caps() {
    let _context = DbTestContext::init(&fixture_datadir());
    {
        let mut context = db_context();
        Arc::make_mut(context.as_mut().unwrap())
            .profiles
            .get_mut("compat-monitor")
            .unwrap()
            .as_mut()
            .unwrap()
            .required = true;
    }
    let mut caps = OwnedCaps::from_str("(type(LCD)vcp(10 60))");
    let original = unsafe { crate::caps_from_c(&mut caps.0) };
    assert!(OwnedMonitor::create("compat-monitor", &mut caps, true).is_none());
    assert_eq!(ddcci_db_requirements_failed(), 1);
    assert_eq!(unsafe { crate::caps_from_c(&mut caps.0) }, original);
    assert!(OwnedMonitor::create("compat-common", &mut caps, true).is_some());
    assert_eq!(ddcci_db_requirements_failed(), 0);
}

#[test]
fn required_included_profile_blocks_the_including_profile() {
    let _context = DbTestContext::init(&fixture_datadir());
    {
        let mut context = db_context();
        Arc::make_mut(context.as_mut().unwrap())
            .profiles
            .get_mut("compat-common")
            .unwrap()
            .as_mut()
            .unwrap()
            .required = true;
    }
    let mut caps = OwnedCaps::with_all_vcp_codes();
    let original = unsafe { crate::caps_from_c(&mut caps.0) };
    assert!(OwnedMonitor::create("compat-monitor", &mut caps, true).is_none());
    assert_eq!(ddcci_db_requirements_failed(), 1);
    assert_eq!(unsafe { crate::caps_from_c(&mut caps.0) }, original);
}

#[test]
fn required_control_feature_is_isolated_and_cannot_be_revived_by_include() {
    let _context = DbTestContext::init(&fixture_datadir());
    {
        let mut context = db_context();
        let root = Arc::make_mut(context.as_mut().unwrap())
            .profiles
            .get_mut("compat-monitor")
            .unwrap()
            .as_mut()
            .unwrap();
        let mut control = parse_xml(r#"<control id="brightness" address="0x10"/>"#).unwrap();
        control.required = true;
        let controls_index = root
            .children
            .iter()
            .position(|node| node.tag == "controls")
            .unwrap();
        let mut controls = root.children.remove(controls_index);
        controls.children.push(control);
        root.children.insert(0, controls);
    }
    let mut caps = OwnedCaps::with_all_vcp_codes();
    let monitor = OwnedMonitor::create("compat-monitor", &mut caps, true).unwrap();
    assert!(!monitor.snapshot().contains("control id=brightness"));
    assert!(monitor.snapshot().contains("control id=input"));
    assert_eq!(ddcci_db_requirements_failed(), 0);
}

#[test]
fn failed_profile_does_not_publish_partial_caps_or_monitor_tree() {
    let _context = DbTestContext::init(&fixture_datadir());
    {
        let mut context = db_context();
        Arc::make_mut(context.as_mut().unwrap()).profiles.insert("broken".to_string(),
            parse_xml(r#"<monitor name="Broken" init="standard"><caps add="(vcp(FF))"/><include file="missing"/></monitor>"#));
    }
    let mut caps = OwnedCaps::from_str("(type(LCD)vcp(10))");
    let original = unsafe { crate::caps_from_c(&mut caps.0) };
    assert!(OwnedMonitor::create("broken", &mut caps, true).is_none());
    assert_eq!(unsafe { crate::caps_from_c(&mut caps.0) }, original);
    assert_eq!(ddcci_db_requirements_failed(), 0);
}

#[test]
fn include_cycles_fail_without_publishing_caps() {
    let _context = DbTestContext::init(&fixture_datadir());
    {
        let mut context = db_context();
        Arc::make_mut(context.as_mut().unwrap()).profiles.insert("cycle".to_string(),
            parse_xml(r#"<monitor name="Cycle" init="standard"><caps add="(vcp(FF))"/><include file="cycle"/></monitor>"#));
    }
    let mut caps = OwnedCaps::from_str("(vcp(10))");
    let original = unsafe { crate::caps_from_c(&mut caps.0) };
    assert!(OwnedMonitor::create("cycle", &mut caps, true).is_none());
    assert_eq!(unsafe { crate::caps_from_c(&mut caps.0) }, original);
}

#[test]
fn simultaneous_monitor_trees_outlive_released_database_session() {
    let _context = DbTestContext::init(&fixture_datadir());
    let monitors: Vec<_> = (0..4)
        .map(|_| {
            std::thread::spawn(|| {
                let mut caps = OwnedCaps::with_all_vcp_codes();
                let monitor = OwnedMonitor::create("compat-monitor", &mut caps, true).unwrap();
                // Transfer the allocation address; each thread relinquishes ownership.
                let address = monitor.0 as usize;
                std::mem::forget(monitor);
                address
            })
        })
        .collect::<Vec<_>>()
        .into_iter()
        .map(|thread| OwnedMonitor(thread.join().unwrap() as *mut CMonitorDb))
        .collect();
    let retained_session = db_context().clone().unwrap();
    unsafe { ddcci_release_db() };
    assert_eq!(Arc::strong_count(&retained_session), 1);
    drop(retained_session);
    for monitor in &monitors {
        assert!(monitor.snapshot().contains("Compatibility Fixture Monitor"));
    }
    let datadir = path_to_cstring(&fixture_datadir());
    assert_eq!(unsafe { ddcci_init_db(datadir.as_ptr() as *mut c_char) }, 1);
    let mut caps = OwnedCaps::with_all_vcp_codes();
    assert!(OwnedMonitor::create("compat-monitor", &mut caps, true).is_some());
}

include!("cbor_integration.rs");

#[test]
fn samsung_init_missing_and_zero_delays_and_command_defaults_remain_distinct() {
    let _context = DbTestContext::init(&fixture_datadir());
    {
        let mut context = db_context();
        Arc::make_mut(context.as_mut().unwrap()).profiles.insert("samsung-synthetic".to_string(),
            parse_xml(r#"<monitor name="Synthetic only" init="samsung"><controls><control id="brightness" address="0x10" delay="0"/><control id="factory_reset" address="0x04"/></controls><include file="compat-common"/></monitor>"#));
    }
    let mut caps = OwnedCaps::with_all_vcp_codes();
    let monitor = OwnedMonitor::create("samsung-synthetic", &mut caps, false).unwrap();
    let snapshot = monitor.snapshot();
    assert!(snapshot.contains("monitor name=Synthetic only init=2"));
    assert!(snapshot.contains("address=0x10 delay=0"));
    assert!(snapshot.contains("address=0x04 delay=-1"));
    assert!(snapshot.contains("value id=default name=Factory Reset value=0x01 value16=0x0001"));
}
