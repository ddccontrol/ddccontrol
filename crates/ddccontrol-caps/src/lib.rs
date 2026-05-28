use std::collections::BTreeMap;
use std::ffi::CStr;
use std::fmt;
use std::os::raw::{c_char, c_int, c_ushort, c_void};
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr;
use std::slice;

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub enum MonitorType {
    #[default]
    Unknown,
    Lcd,
    Crt,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct VcpEntry {
    values: Option<Vec<u16>>,
}

impl VcpEntry {
    pub fn all_values() -> Self {
        Self { values: None }
    }

    pub fn with_values(values: Vec<u16>) -> Self {
        Self {
            values: Some(values),
        }
    }

    pub fn values(&self) -> Option<&[u16]> {
        self.values.as_deref()
    }
}

#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct Caps {
    monitor_type: MonitorType,
    vcp: BTreeMap<u8, VcpEntry>,
}

impl Caps {
    pub fn parse(input: &str) -> Result<Self, ParseError> {
        let mut caps = Self::default();
        caps.apply(input, Edit::Add)?;
        Ok(caps)
    }

    pub fn apply_add(&mut self, input: &str) -> Result<usize, ParseError> {
        self.apply(input, Edit::Add)
    }

    pub fn apply_remove(&mut self, input: &str) -> Result<usize, ParseError> {
        self.apply(input, Edit::Remove)
    }

    pub fn monitor_type(&self) -> MonitorType {
        self.monitor_type
    }

    pub fn is_supported(&self, code: u8) -> bool {
        self.vcp.contains_key(&code)
    }

    pub fn vcp(&self, code: u8) -> Option<&VcpEntry> {
        self.vcp.get(&code)
    }

    pub fn vcp_codes(&self) -> impl Iterator<Item = u8> + '_ {
        self.vcp.keys().copied()
    }

    fn apply(&mut self, input: &str, edit: Edit) -> Result<usize, ParseError> {
        let parsed = Parser::new(input).parse()?;

        if edit == Edit::Add {
            if let Some(monitor_type) = parsed.monitor_type {
                self.monitor_type = monitor_type;
            }
        }

        for item in &parsed.vcp {
            match edit {
                Edit::Add => {
                    self.vcp.insert(
                        item.code,
                        match &item.values {
                            Some(values) => VcpEntry::with_values(values.clone()),
                            None => VcpEntry::all_values(),
                        },
                    );
                }
                Edit::Remove => match &item.values {
                    Some(remove_values) => {
                        if let Some(entry) = self.vcp.get_mut(&item.code) {
                            if let Some(values) = &mut entry.values {
                                values.retain(|value| !remove_values.contains(value));
                                if values.is_empty() {
                                    self.vcp.remove(&item.code);
                                }
                            }
                        }
                    }
                    None => {
                        self.vcp.remove(&item.code);
                    }
                },
            }
        }

        Ok(parsed.vcp.len())
    }
}

#[repr(C)]
pub struct CVcpEntry {
    values_len: c_int,
    values: *mut c_ushort,
}

#[repr(C)]
pub struct CCaps {
    vcp: [*mut CVcpEntry; 256],
    monitor_type: c_int,
    raw_caps: *mut c_char,
}

unsafe extern "C" {
    fn malloc(size: usize) -> *mut c_void;
    fn free(ptr: *mut c_void);
}

#[no_mangle]
pub unsafe extern "C" fn ddccontrol_caps_parse(
    caps_str: *const c_char,
    caps: *mut CCaps,
    add: c_int,
) -> c_int {
    match catch_unwind(AssertUnwindSafe(|| {
        ddccontrol_caps_parse_inner(caps_str, caps, add)
    })) {
        Ok(result) => result,
        Err(_) => -1,
    }
}

unsafe fn ddccontrol_caps_parse_inner(
    caps_str: *const c_char,
    caps: *mut CCaps,
    add: c_int,
) -> c_int {
    if caps_str.is_null() || caps.is_null() {
        return -1;
    }

    let input = CStr::from_ptr(caps_str).to_string_lossy();
    let mut rust_caps = caps_from_c(caps);
    let result = if add != 0 {
        rust_caps.apply_add(&input)
    } else {
        rust_caps.apply_remove(&input)
    };

    match result {
        Ok(count) => {
            if replace_c_caps(caps, &rust_caps) {
                count as c_int
            } else {
                free_c_vcp_entries(caps);
                -1
            }
        }
        Err(_) => {
            free_c_vcp_entries(caps);
            -1
        }
    }
}

unsafe fn caps_from_c(caps: *mut CCaps) -> Caps {
    let mut rust_caps = Caps::default();
    rust_caps.monitor_type = match (*caps).monitor_type {
        1 => MonitorType::Lcd,
        2 => MonitorType::Crt,
        _ => MonitorType::Unknown,
    };

    for code in 0..256 {
        let entry = (*caps).vcp[code];
        if entry.is_null() {
            continue;
        }

        let values_len = (*entry).values_len;
        let values = if values_len < 0 {
            None
        } else if values_len == 0 || (*entry).values.is_null() {
            Some(Vec::new())
        } else {
            Some(slice::from_raw_parts((*entry).values, values_len as usize).to_vec())
        };

        rust_caps.vcp.insert(code as u8, VcpEntry { values });
    }

    rust_caps
}

unsafe fn replace_c_caps(caps: *mut CCaps, rust_caps: &Caps) -> bool {
    free_c_vcp_entries(caps);
    (*caps).monitor_type = match rust_caps.monitor_type {
        MonitorType::Unknown => 0,
        MonitorType::Lcd => 1,
        MonitorType::Crt => 2,
    };

    for (&code, entry) in &rust_caps.vcp {
        let c_entry = malloc(std::mem::size_of::<CVcpEntry>()) as *mut CVcpEntry;
        if c_entry.is_null() {
            return false;
        }

        match &entry.values {
            Some(values) => {
                (*c_entry).values_len = values.len() as c_int;
                if values.is_empty() {
                    (*c_entry).values = ptr::null_mut();
                } else {
                    let values_size = values.len() * std::mem::size_of::<c_ushort>();
                    let c_values = malloc(values_size) as *mut c_ushort;
                    if c_values.is_null() {
                        free(c_entry as *mut c_void);
                        return false;
                    }
                    ptr::copy_nonoverlapping(values.as_ptr(), c_values, values.len());
                    (*c_entry).values = c_values;
                }
            }
            None => {
                (*c_entry).values_len = -1;
                (*c_entry).values = ptr::null_mut();
            }
        }

        (*caps).vcp[code as usize] = c_entry;
    }

    true
}

unsafe fn free_c_vcp_entries(caps: *mut CCaps) {
    for entry in &mut (*caps).vcp {
        if !entry.is_null() {
            free((**entry).values as *mut c_void);
            free(*entry as *mut c_void);
            *entry = ptr::null_mut();
        }
    }
}

mod monitor_db {
    use super::{ddccontrol_caps_parse, free, malloc, CCaps};
    use roxmltree::{Document, Node};
    use std::ffi::{CStr, CString};
    use std::fs;
    use std::os::raw::{c_char, c_int, c_uchar, c_ushort, c_void};
    use std::path::{Path, PathBuf};
    use std::ptr;
    use std::sync::Mutex;

    const DBVERSION: i64 = 3;
    const DBPACKAGE: &[u8] = b"ddccontrol-db\0";
    const DEFAULT_DATADIR: &str = match option_env!("DDCONTROL_DATADIR") {
        Some(value) => value,
        None => "/usr/local/share/ddccontrol-db",
    };

    const CONTROL_TYPE_VALUE: c_int = 0;
    const CONTROL_TYPE_COMMAND: c_int = 1;
    const CONTROL_TYPE_LIST: c_int = 2;
    const REFRESH_TYPE_NONE: c_int = 0;
    const REFRESH_TYPE_ALL: c_int = 1;
    const INIT_TYPE_UNKNOWN: c_int = 0;
    const INIT_TYPE_STANDARD: c_int = 1;
    const INIT_TYPE_SAMSUNG: c_int = 2;

    #[repr(C)]
    pub struct CValueDb {
        id: *mut c_uchar,
        name: *mut c_uchar,
        value: c_uchar,
        next: *mut CValueDb,
    }

    #[repr(C)]
    struct CValueDbPrivate {
        public_value: CValueDb,
        value16: c_ushort,
    }

    #[repr(C)]
    pub struct CControlDb {
        id: *mut c_uchar,
        name: *mut c_uchar,
        address: c_uchar,
        delay: c_int,
        control_type: c_int,
        refresh: c_int,
        next: *mut CControlDb,
        value_list: *mut CValueDb,
    }

    #[repr(C)]
    pub struct CSubgroupDb {
        name: *mut c_uchar,
        pattern: *mut c_uchar,
        next: *mut CSubgroupDb,
        control_list: *mut CControlDb,
    }

    #[repr(C)]
    pub struct CGroupDb {
        name: *mut c_uchar,
        next: *mut CGroupDb,
        subgroup_list: *mut CSubgroupDb,
    }

    #[repr(C)]
    pub struct CMonitorDb {
        name: *mut c_uchar,
        init: c_int,
        group_list: *mut CGroupDb,
    }

    unsafe extern "C" {
        fn dgettext(domainname: *const c_char, msgid: *const c_char) -> *mut c_char;
    }

    static DB_CONTEXT: Mutex<Option<DbContext>> = Mutex::new(None);

    #[derive(Clone)]
    struct DbContext {
        datadir: PathBuf,
        options: OptionsDb,
    }

    #[derive(Clone, Default)]
    struct OptionsDb {
        groups: Vec<OptionGroup>,
    }

    #[derive(Clone)]
    struct OptionGroup {
        name: String,
        subgroups: Vec<OptionSubgroup>,
    }

    #[derive(Clone)]
    struct OptionSubgroup {
        name: String,
        pattern: Option<String>,
        controls: Vec<OptionControl>,
    }

    #[derive(Clone)]
    struct OptionControl {
        id: String,
        name: String,
        control_type: c_int,
        refresh: c_int,
        values: Vec<OptionValue>,
    }

    #[derive(Clone)]
    struct OptionValue {
        id: String,
        name: Option<String>,
    }

    #[derive(Default)]
    struct MonitorBuild {
        name: Option<String>,
        init: c_int,
        groups: Vec<DbGroup>,
    }

    struct DbGroup {
        name: String,
        subgroups: Vec<DbSubgroup>,
    }

    struct DbSubgroup {
        name: String,
        pattern: Option<String>,
        controls: Vec<DbControl>,
    }

    struct DbControl {
        id: String,
        name: String,
        address: u8,
        delay: c_int,
        control_type: c_int,
        refresh: c_int,
        values: Vec<DbValue>,
    }

    struct DbValue {
        id: String,
        name: String,
        value16: u16,
    }

    struct MonitorControl {
        id: String,
        address: u8,
        delay: c_int,
        values: Vec<MonitorValue>,
        line: u32,
    }

    struct MonitorValue {
        id: String,
        value16: u16,
        line: u32,
    }

    #[no_mangle]
    pub unsafe extern "C" fn ddcci_init_db(usedatadir: *mut c_char) -> c_int {
        let datadir = if usedatadir.is_null() {
            PathBuf::from(DEFAULT_DATADIR)
        } else {
            PathBuf::from(CStr::from_ptr(usedatadir).to_string_lossy().into_owned())
        };

        match load_options(&datadir) {
            Ok(options) => {
                *DB_CONTEXT.lock().unwrap() = Some(DbContext { datadir, options });
                1
            }
            Err(err) => {
                eprintln!("{err}");
                0
            }
        }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ddcci_release_db() {
        *DB_CONTEXT.lock().unwrap() = None;
    }

    #[no_mangle]
    pub unsafe extern "C" fn ddcci_create_db(
        pnpname: *const c_char,
        caps: *mut CCaps,
        faulttolerance: c_int,
    ) -> *mut CMonitorDb {
        if pnpname.is_null() || caps.is_null() {
            return ptr::null_mut();
        }

        let pnpname = CStr::from_ptr(pnpname).to_string_lossy().into_owned();
        let context = match DB_CONTEXT.lock().unwrap().clone() {
            Some(context) => context,
            None => {
                eprintln!("Database must be inited before reading a monitor file.");
                return ptr::null_mut();
            }
        };

        let mut build = MonitorBuild {
            init: INIT_TYPE_UNKNOWN,
            groups: groups_from_options(&context.options),
            ..MonitorBuild::default()
        };
        let mut defined = [false; 256];

        let result = create_db_protected(
            &context,
            &mut build,
            &pnpname,
            caps,
            0,
            &mut defined,
            faulttolerance != 0,
        );

        if let Err(err) = result {
            if !err.missing_profile || faulttolerance == 0 {
                eprintln!("{}", err.message);
            }
            return ptr::null_mut();
        }

        if build.init == INIT_TYPE_UNKNOWN {
            if faulttolerance != 0 {
                eprintln!("Warning: init mode not set, using standard.");
                build.init = INIT_TYPE_STANDARD;
            } else {
                eprintln!("Error: init mode not set.");
                return ptr::null_mut();
            }
        }

        prune_empty_groups(&mut build.groups);
        match alloc_monitor(&build) {
            Some(monitor) => monitor,
            None => ptr::null_mut(),
        }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ddcci_free_db(monitor: *mut CMonitorDb) {
        if monitor.is_null() {
            return;
        }

        free((*monitor).name as *mut c_void);

        let mut group = (*monitor).group_list;
        while !group.is_null() {
            let next_group = (*group).next;
            free((*group).name as *mut c_void);

            let mut subgroup = (*group).subgroup_list;
            while !subgroup.is_null() {
                let next_subgroup = (*subgroup).next;
                free((*subgroup).name as *mut c_void);
                free((*subgroup).pattern as *mut c_void);

                let mut control = (*subgroup).control_list;
                while !control.is_null() {
                    let next_control = (*control).next;
                    free((*control).id as *mut c_void);
                    free((*control).name as *mut c_void);

                    let mut value = (*control).value_list;
                    while !value.is_null() {
                        let next_value = (*value).next;
                        free((*value).id as *mut c_void);
                        free((*value).name as *mut c_void);
                        free(value as *mut c_void);
                        value = next_value;
                    }

                    free(control as *mut c_void);
                    control = next_control;
                }

                free(subgroup as *mut c_void);
                subgroup = next_subgroup;
            }

            free(group as *mut c_void);
            group = next_group;
        }

        free(monitor as *mut c_void);
    }

    fn load_options(datadir: &Path) -> Result<OptionsDb, String> {
        let path = datadir.join("options.xml");
        let xml = fs::read_to_string(&path)
            .map_err(|err| format!("I/O error while reading options.xml: {err}."))?;
        let doc =
            Document::parse(&xml).map_err(|_| "Document not parsed successfully.".to_string())?;
        let root = doc.root_element();
        if root.tag_name().name() != "options" {
            return Err(format!(
                "options.xml of the wrong type, root node {} != options",
                root.tag_name().name()
            ));
        }

        let version = root.attribute("dbversion").ok_or_else(|| {
            "options.xml dbversion attribute missing, please update your database.".to_string()
        })?;
        let _date = root.attribute("date").ok_or_else(|| {
            "options.xml date attribute missing, please update your database.".to_string()
        })?;
        let version =
            parse_int(version).map_err(|_| "Can't convert version to int.".to_string())?;
        if version > DBVERSION {
            return Err(format!(
                "options.xml dbversion ({version}) is greater than the supported version ({DBVERSION}).\nPlease update ddccontrol program."
            ));
        }
        if version < DBVERSION {
            return Err(format!(
                "options.xml dbversion ({version}) is less than the supported version ({DBVERSION}).\nPlease update ddccontrol database."
            ));
        }

        let mut options = OptionsDb::default();
        for group in element_children(root).filter(|node| node.tag_name().name() == "group") {
            let name = required_attr(group, "name")?.to_string();
            let mut option_group = OptionGroup {
                name,
                subgroups: Vec::new(),
            };
            for subgroup in
                element_children(group).filter(|node| node.tag_name().name() == "subgroup")
            {
                let name = required_attr(subgroup, "name")?.to_string();
                let pattern = subgroup.attribute("pattern").map(ToString::to_string);
                let mut option_subgroup = OptionSubgroup {
                    name,
                    pattern,
                    controls: Vec::new(),
                };
                for control in
                    element_children(subgroup).filter(|node| node.tag_name().name() == "control")
                {
                    let refresh = match control.attribute("refresh") {
                        Some("none") | None => REFRESH_TYPE_NONE,
                        Some("all") => REFRESH_TYPE_ALL,
                        Some(_) => {
                            return Err(node_error(
                                control,
                                "Invalid refresh type (!= none, != all).",
                            ))
                        }
                    };
                    let control_type = match required_attr(control, "type")? {
                        "value" => CONTROL_TYPE_VALUE,
                        "command" => CONTROL_TYPE_COMMAND,
                        "list" => CONTROL_TYPE_LIST,
                        _ => return Err(node_error(control, "Invalid type.")),
                    };
                    let mut option_control = OptionControl {
                        id: required_attr(control, "id")?.to_string(),
                        name: required_attr(control, "name")?.to_string(),
                        control_type,
                        refresh,
                        values: Vec::new(),
                    };
                    for value in
                        element_children(control).filter(|node| node.tag_name().name() == "value")
                    {
                        option_control.values.push(OptionValue {
                            id: required_attr(value, "id")?.to_string(),
                            name: value.attribute("name").map(ToString::to_string),
                        });
                    }
                    option_subgroup.controls.push(option_control);
                }
                option_group.subgroups.push(option_subgroup);
            }
            options.groups.push(option_group);
        }

        Ok(options)
    }

    fn create_db_protected(
        context: &DbContext,
        build: &mut MonitorBuild,
        pnpname: &str,
        caps: *mut CCaps,
        recursionlevel: usize,
        defined: &mut [bool; 256],
        faulttolerance: bool,
    ) -> Result<(), DbError> {
        if !is_valid_monitor_profile_name(pnpname) {
            return Err(DbError::new(format!(
                "Invalid monitor profile name ({pnpname})."
            )));
        }
        if recursionlevel > 15 {
            return Err(DbError::new(format!(
                "Error, include recursion level > 15 (file: {pnpname})."
            )));
        }

        let path = context
            .datadir
            .join("monitor")
            .join(format!("{pnpname}.xml"));
        let xml = match fs::read_to_string(&path) {
            Ok(xml) => xml,
            Err(err) if err.kind() == std::io::ErrorKind::NotFound => {
                return Err(DbError::missing(format!(
                    "Cannot access {}: {err}",
                    path.display()
                )));
            }
            Err(err) => {
                return Err(DbError::new(format!(
                    "Cannot access {}: {err}",
                    path.display()
                )));
            }
        };
        let doc = Document::parse(&xml)
            .map_err(|_| DbError::new("Document not parsed successfully.".to_string()))?;
        let root = doc.root_element();
        if root.tag_name().name() != "monitor" {
            return Err(DbError::new(format!(
                "monitor/{pnpname}.xml of the wrong type, root node {} != monitor",
                root.tag_name().name()
            )));
        }

        if build.name.is_none() {
            build.name = Some(
                root.attribute("name")
                    .ok_or_else(|| DbError::new(node_error(root, "Can't find name property.")))?
                    .to_string(),
            );
        }

        if build.init == INIT_TYPE_UNKNOWN {
            if let Some(init) = root.attribute("init") {
                build.init = match init {
                    "standard" => INIT_TYPE_STANDARD,
                    "samsung" => INIT_TYPE_SAMSUNG,
                    _ => return Err(DbError::new(node_error(root, "Invalid type."))),
                };
            }
        }

        if root.attribute("caps").is_some() {
            if faulttolerance {
                eprintln!("Warning: caps property is deprecated.");
            } else {
                return Err(DbError::new(
                    "Error: caps property is deprecated.".to_string(),
                ));
            }
        }
        if root.attribute("include").is_some() {
            if faulttolerance {
                eprintln!("Warning: include property is deprecated.");
            } else {
                return Err(DbError::new(
                    "Error: include property is deprecated.".to_string(),
                ));
            }
        }

        let mut controls_or_include = false;
        let mut seen_controls = false;
        for child in element_children(root) {
            match child.tag_name().name() {
                "caps" => {
                    let remove = child.attribute("remove");
                    let add = child.attribute("add");
                    if remove.is_none() && add.is_none() {
                        return Err(DbError::new(node_error(
                            child,
                            "Can't find add or remove property in caps.",
                        )));
                    }
                    if let Some(remove) = remove {
                        apply_caps(remove, caps, 0)
                            .map_err(|_| DbError::new(node_error(child, "Invalid remove caps.")))?;
                    }
                    if let Some(add) = add {
                        apply_caps(add, caps, 1)
                            .map_err(|_| DbError::new(node_error(child, "Invalid add caps.")))?;
                    }
                }
                "include" => {
                    controls_or_include = true;
                    let file = required_attr(child, "file").map_err(DbError::new)?;
                    create_db_protected(
                        context,
                        build,
                        file,
                        caps,
                        recursionlevel + 1,
                        defined,
                        faulttolerance,
                    )?;
                }
                "controls" => {
                    if seen_controls {
                        return Err(DbError::new(node_error(
                            child,
                            "Two controls part in XML file.",
                        )));
                    }
                    seen_controls = true;
                    controls_or_include = true;
                    let monitor_controls = parse_monitor_controls(child).map_err(DbError::new)?;
                    add_controls_from_options(
                        build,
                        &context.options,
                        &monitor_controls,
                        caps,
                        defined,
                        faulttolerance,
                    )?;
                }
                _ => {}
            }
        }

        if !controls_or_include {
            return Err(DbError::new(
                "document of the wrong type, can't find controls or include.".to_string(),
            ));
        }

        Ok(())
    }

    fn add_controls_from_options(
        build: &mut MonitorBuild,
        options: &OptionsDb,
        monitor_controls: &[MonitorControl],
        caps: *mut CCaps,
        defined: &mut [bool; 256],
        faulttolerance: bool,
    ) -> Result<(), DbError> {
        let mut matched = vec![false; monitor_controls.len()];

        for (group_index, option_group) in options.groups.iter().enumerate() {
            for (subgroup_index, option_subgroup) in option_group.subgroups.iter().enumerate() {
                for option_control in &option_subgroup.controls {
                    let Some((monitor_index, monitor_control)) = monitor_controls
                        .iter()
                        .enumerate()
                        .find(|(_, control)| control.id == option_control.id)
                    else {
                        continue;
                    };

                    matched[monitor_index] = true;
                    let address = monitor_control.address as usize;
                    unsafe {
                        if (*caps).vcp[address].is_null() {
                            continue;
                        }
                    }
                    if defined[address] {
                        continue;
                    }

                    let mut values =
                        get_value_list(option_control, monitor_control, faulttolerance)?;
                    if option_control.control_type == CONTROL_TYPE_COMMAND && values.is_empty() {
                        values.push(DbValue {
                            id: "default".to_string(),
                            name: translate(&option_control.name),
                            value16: 0x01,
                        });
                    }

                    let control = DbControl {
                        id: option_control.id.clone(),
                        name: translate(&option_control.name),
                        address: monitor_control.address,
                        delay: monitor_control.delay,
                        control_type: option_control.control_type,
                        refresh: option_control.refresh,
                        values,
                    };
                    build.groups[group_index].subgroups[subgroup_index]
                        .controls
                        .push(control);
                    defined[address] = true;
                }
            }
        }

        for (index, control) in monitor_controls.iter().enumerate() {
            if !matched[index] {
                eprintln!(
                    "Element control (id={}) has not been found (line {}).",
                    control.id, control.line
                );
                if !faulttolerance {
                    return Err(DbError::new(
                        "Unmatched control in monitor XML.".to_string(),
                    ));
                }
            }
        }

        Ok(())
    }

    fn get_value_list(
        option_control: &OptionControl,
        monitor_control: &MonitorControl,
        faulttolerance: bool,
    ) -> Result<Vec<DbValue>, DbError> {
        let mut values = Vec::new();
        let mut matched = vec![false; monitor_control.values.len()];

        for option_value in &option_control.values {
            if let Some((monitor_index, monitor_value)) = monitor_control
                .values
                .iter()
                .enumerate()
                .find(|(_, value)| value.id == option_value.id)
            {
                matched[monitor_index] = true;
                let name = if option_control.control_type == CONTROL_TYPE_COMMAND {
                    option_value
                        .name
                        .as_ref()
                        .unwrap_or(&option_control.name)
                        .to_string()
                } else {
                    option_value
                        .name
                        .clone()
                        .ok_or_else(|| DbError::new("Can't find name property.".to_string()))?
                };
                values.push(DbValue {
                    id: option_value.id.clone(),
                    name: translate(&name),
                    value16: monitor_value.value16,
                });
            }
        }

        for (index, value) in monitor_control.values.iter().enumerate() {
            if !matched[index] {
                eprintln!(
                    "Element value (id={}) has not been found (line {}).",
                    value.id, value.line
                );
                if !faulttolerance {
                    return Err(DbError::new("Unmatched value in monitor XML.".to_string()));
                }
            }
        }

        Ok(values)
    }

    fn parse_monitor_controls(node: Node<'_, '_>) -> Result<Vec<MonitorControl>, String> {
        let mut controls = Vec::new();
        for control in element_children(node).filter(|node| node.tag_name().name() == "control") {
            let address = parse_int(required_attr(control, "address")?)
                .map_err(|_| node_error(control, "Can't convert address to int."))?;
            if !(0..=255).contains(&address) {
                return Err(node_error(
                    control,
                    "Address is outside the supported 0-255 range.",
                ));
            }
            let delay = match control.attribute("delay") {
                Some(delay) => parse_int_decimal(delay)
                    .map_err(|_| node_error(control, "Can't convert delay to int."))?
                    as c_int,
                None => -1,
            };
            let mut monitor_control = MonitorControl {
                id: required_attr(control, "id")?.to_string(),
                address: address as u8,
                delay,
                values: Vec::new(),
                line: line(control),
            };
            for value in element_children(control).filter(|node| node.tag_name().name() == "value")
            {
                let parsed_value = parse_int(required_attr(value, "value")?)
                    .map_err(|_| node_error(value, "Can't convert value to int."))?;
                if !(0..=u16::MAX as i64).contains(&parsed_value) {
                    return Err(node_error(
                        value,
                        "Value is outside the supported 0-65535 range.",
                    ));
                }
                monitor_control.values.push(MonitorValue {
                    id: required_attr(value, "id")?.to_string(),
                    value16: parsed_value as u16,
                    line: line(value),
                });
            }
            controls.push(monitor_control);
        }
        Ok(controls)
    }

    fn groups_from_options(options: &OptionsDb) -> Vec<DbGroup> {
        options
            .groups
            .iter()
            .map(|group| DbGroup {
                name: translate(&group.name),
                subgroups: group
                    .subgroups
                    .iter()
                    .map(|subgroup| DbSubgroup {
                        name: translate(&subgroup.name),
                        pattern: subgroup.pattern.clone(),
                        controls: Vec::new(),
                    })
                    .collect(),
            })
            .collect()
    }

    fn prune_empty_groups(groups: &mut Vec<DbGroup>) {
        for group in groups.iter_mut() {
            group
                .subgroups
                .retain(|subgroup| !subgroup.controls.is_empty());
        }
        groups.retain(|group| !group.subgroups.is_empty());
    }

    fn apply_caps(input: &str, caps: *mut CCaps, add: c_int) -> Result<(), ()> {
        let input = CString::new(input).map_err(|_| ())?;
        let parsed = unsafe { ddccontrol_caps_parse(input.as_ptr(), caps, add) };
        if parsed <= 0 {
            Err(())
        } else {
            Ok(())
        }
    }

    fn alloc_monitor(build: &MonitorBuild) -> Option<*mut CMonitorDb> {
        unsafe {
            let monitor = malloc(std::mem::size_of::<CMonitorDb>()) as *mut CMonitorDb;
            if monitor.is_null() {
                return None;
            }
            ptr::write(
                monitor,
                CMonitorDb {
                    name: c_string(build.name.as_deref().unwrap_or_default())?,
                    init: build.init,
                    group_list: alloc_groups(&build.groups)?,
                },
            );
            Some(monitor)
        }
    }

    unsafe fn alloc_groups(groups: &[DbGroup]) -> Option<*mut CGroupDb> {
        let mut head = ptr::null_mut();
        let mut tail = &mut head as *mut *mut CGroupDb;
        for group in groups {
            let node = malloc(std::mem::size_of::<CGroupDb>()) as *mut CGroupDb;
            if node.is_null() {
                return None;
            }
            ptr::write(
                node,
                CGroupDb {
                    name: c_string(&group.name)?,
                    next: ptr::null_mut(),
                    subgroup_list: alloc_subgroups(&group.subgroups)?,
                },
            );
            *tail = node;
            tail = &mut (*node).next;
        }
        Some(head)
    }

    unsafe fn alloc_subgroups(subgroups: &[DbSubgroup]) -> Option<*mut CSubgroupDb> {
        let mut head = ptr::null_mut();
        let mut tail = &mut head as *mut *mut CSubgroupDb;
        for subgroup in subgroups {
            let node = malloc(std::mem::size_of::<CSubgroupDb>()) as *mut CSubgroupDb;
            if node.is_null() {
                return None;
            }
            ptr::write(
                node,
                CSubgroupDb {
                    name: c_string(&subgroup.name)?,
                    pattern: match &subgroup.pattern {
                        Some(pattern) => c_string(pattern)?,
                        None => ptr::null_mut(),
                    },
                    next: ptr::null_mut(),
                    control_list: alloc_controls(&subgroup.controls)?,
                },
            );
            *tail = node;
            tail = &mut (*node).next;
        }
        Some(head)
    }

    unsafe fn alloc_controls(controls: &[DbControl]) -> Option<*mut CControlDb> {
        let mut head = ptr::null_mut();
        let mut tail = &mut head as *mut *mut CControlDb;
        for control in controls {
            let node = malloc(std::mem::size_of::<CControlDb>()) as *mut CControlDb;
            if node.is_null() {
                return None;
            }
            ptr::write(
                node,
                CControlDb {
                    id: c_string(&control.id)?,
                    name: c_string(&control.name)?,
                    address: control.address,
                    delay: control.delay,
                    control_type: control.control_type,
                    refresh: control.refresh,
                    next: ptr::null_mut(),
                    value_list: alloc_values(&control.values)?,
                },
            );
            *tail = node;
            tail = &mut (*node).next;
        }
        Some(head)
    }

    unsafe fn alloc_values(values: &[DbValue]) -> Option<*mut CValueDb> {
        let mut head = ptr::null_mut();
        let mut tail = &mut head as *mut *mut CValueDb;
        for value in values {
            let node = malloc(std::mem::size_of::<CValueDbPrivate>()) as *mut CValueDbPrivate;
            if node.is_null() {
                return None;
            }
            ptr::write(
                node,
                CValueDbPrivate {
                    public_value: CValueDb {
                        id: c_string(&value.id)?,
                        name: c_string(&value.name)?,
                        value: (value.value16 & 0xff) as c_uchar,
                        next: ptr::null_mut(),
                    },
                    value16: value.value16,
                },
            );
            *tail = &mut (*node).public_value;
            tail = &mut (*node).public_value.next;
        }
        Some(head)
    }

    unsafe fn c_string(input: &str) -> Option<*mut c_uchar> {
        let bytes = input.as_bytes();
        if bytes.contains(&0) {
            return None;
        }
        let ptr = malloc(bytes.len() + 1) as *mut c_uchar;
        if ptr.is_null() {
            return None;
        }
        ptr::copy_nonoverlapping(bytes.as_ptr(), ptr, bytes.len());
        *ptr.add(bytes.len()) = 0;
        Some(ptr)
    }

    fn translate(input: &str) -> String {
        let Ok(c_input) = CString::new(input) else {
            return input.to_string();
        };
        unsafe {
            let translated = dgettext(DBPACKAGE.as_ptr() as *const c_char, c_input.as_ptr());
            if translated.is_null() {
                input.to_string()
            } else {
                CStr::from_ptr(translated).to_string_lossy().into_owned()
            }
        }
    }

    fn required_attr<'a, 'd>(node: Node<'a, 'd>, name: &str) -> Result<&'a str, String> {
        node.attribute(name)
            .ok_or_else(|| node_error(node, &format!("Can't find {name} property.")))
    }

    fn element_children<'a, 'd>(node: Node<'a, 'd>) -> impl Iterator<Item = Node<'a, 'd>> {
        node.children().filter(|child| child.is_element())
    }

    fn node_error(node: Node<'_, '_>, message: &str) -> String {
        format!("Error: {message} @line {}", line(node))
    }

    fn line(node: Node<'_, '_>) -> u32 {
        node.document().text_pos_at(node.range().start).row
    }

    fn parse_int(input: &str) -> Result<i64, std::num::ParseIntError> {
        let (negative, rest) = input
            .strip_prefix('-')
            .map_or((false, input), |rest| (true, rest));
        let (radix, digits) =
            if let Some(rest) = rest.strip_prefix("0x").or_else(|| rest.strip_prefix("0X")) {
                (16, rest)
            } else if rest.len() > 1 && rest.starts_with('0') {
                (8, &rest[1..])
            } else {
                (10, rest)
            };
        let value = i64::from_str_radix(digits, radix)?;
        Ok(if negative { -value } else { value })
    }

    fn parse_int_decimal(input: &str) -> Result<i64, std::num::ParseIntError> {
        input.parse::<i64>()
    }

    fn is_valid_monitor_profile_name(name: &str) -> bool {
        !name.is_empty()
            && name
                .bytes()
                .all(|byte| byte.is_ascii_alphanumeric() || byte == b'_' || byte == b'-')
    }

    #[derive(Debug)]
    struct DbError {
        message: String,
        missing_profile: bool,
    }

    impl DbError {
        fn new(message: String) -> Self {
            Self {
                message,
                missing_profile: false,
            }
        }

        fn missing(message: String) -> Self {
            Self {
                message,
                missing_profile: true,
            }
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ParseError {
    position: usize,
    kind: ErrorKind,
}

impl ParseError {
    pub fn position(&self) -> usize {
        self.position
    }

    pub fn kind(&self) -> ErrorKind {
        self.kind
    }
}

impl fmt::Display for ParseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{} at byte {}", self.kind, self.position)
    }
}

impl std::error::Error for ParseError {}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ErrorKind {
    InvalidHex,
    UnexpectedCloseParen,
    UnexpectedEnd,
    UnbalancedParens,
    ValueWithoutVcpCode,
}

impl fmt::Display for ErrorKind {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::InvalidHex => f.write_str("invalid hexadecimal value"),
            Self::UnexpectedCloseParen => f.write_str("unexpected closing parenthesis"),
            Self::UnexpectedEnd => f.write_str("unexpected end of capability string"),
            Self::UnbalancedParens => f.write_str("unbalanced parentheses"),
            Self::ValueWithoutVcpCode => f.write_str("value list without VCP code"),
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum Edit {
    Add,
    Remove,
}

#[derive(Clone, Debug, Default, Eq, PartialEq)]
struct ParsedCaps {
    monitor_type: Option<MonitorType>,
    vcp: Vec<ParsedVcp>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct ParsedVcp {
    code: u8,
    values: Option<Vec<u16>>,
}

struct Parser<'a> {
    input: &'a str,
    bytes: &'a [u8],
    pos: usize,
}

impl<'a> Parser<'a> {
    fn new(input: &'a str) -> Self {
        Self {
            input,
            bytes: input.as_bytes(),
            pos: 0,
        }
    }

    fn parse(mut self) -> Result<ParsedCaps, ParseError> {
        let mut parsed = ParsedCaps::default();
        self.parse_sections(None, &mut parsed)?;
        Ok(parsed)
    }

    fn parse_sections(
        &mut self,
        stop_at: Option<u8>,
        parsed: &mut ParsedCaps,
    ) -> Result<(), ParseError> {
        loop {
            self.skip_spaces();

            match self.peek() {
                None if stop_at.is_some() => return Err(self.err(ErrorKind::UnbalancedParens)),
                None => return Ok(()),
                Some(byte) if Some(byte) == stop_at => {
                    self.pos += 1;
                    return Ok(());
                }
                Some(b')') => return Err(self.err(ErrorKind::UnexpectedCloseParen)),
                Some(b'(') => {
                    self.pos += 1;
                    self.parse_sections(Some(b')'), parsed)?;
                }
                Some(byte) if is_name_byte(byte) => {
                    let name = self.take_name();
                    self.skip_spaces();
                    if self.consume(b'(') {
                        match name {
                            "type" => self.parse_type(parsed)?,
                            "vcp" => self.parse_vcp(parsed)?,
                            _ => self.skip_balanced()?,
                        }
                    }
                }
                Some(_) => {
                    self.pos += 1;
                }
            }
        }
    }

    fn parse_type(&mut self, parsed: &mut ParsedCaps) -> Result<(), ParseError> {
        self.skip_spaces();
        let start = self.pos;

        while let Some(byte) = self.peek() {
            if byte == b')' || byte.is_ascii_whitespace() {
                break;
            }
            self.pos += 1;
        }

        let value = self.input[start..self.pos].to_ascii_lowercase();
        match value.as_str() {
            "lcd" => parsed.monitor_type = Some(MonitorType::Lcd),
            "crt" => parsed.monitor_type = Some(MonitorType::Crt),
            _ => {}
        }

        self.skip_balanced_from_current()
    }

    fn parse_vcp(&mut self, parsed: &mut ParsedCaps) -> Result<(), ParseError> {
        loop {
            self.skip_spaces();

            match self.peek() {
                None => return Err(self.err(ErrorKind::UnbalancedParens)),
                Some(b')') => {
                    self.pos += 1;
                    return Ok(());
                }
                Some(b'(') => return Err(self.err(ErrorKind::ValueWithoutVcpCode)),
                Some(_) => {
                    let code_position = self.pos;
                    let code = self.take_vcp_code(code_position)?;
                    self.skip_spaces();

                    let values = if self.consume(b'(') {
                        Some(self.parse_vcp_values()?)
                    } else {
                        None
                    };

                    parsed.vcp.push(ParsedVcp { code, values });
                }
            }
        }
    }

    fn parse_vcp_values(&mut self) -> Result<Vec<u16>, ParseError> {
        let mut values = Vec::new();

        loop {
            self.skip_spaces();

            match self.peek() {
                None => return Err(self.err(ErrorKind::UnbalancedParens)),
                Some(b')') => {
                    self.pos += 1;
                    return Ok(values);
                }
                Some(b'(') => {
                    self.pos += 1;
                    self.skip_balanced()?;
                }
                Some(_) => {
                    let value_position = self.pos;
                    let token = self.take_token()?;
                    let value = parse_hex_u16(token)
                        .ok_or_else(|| self.err_at(ErrorKind::InvalidHex, value_position))?;
                    values.push(value);
                }
            }
        }
    }

    fn skip_balanced(&mut self) -> Result<(), ParseError> {
        let mut depth = 1usize;

        while let Some(byte) = self.peek() {
            self.pos += 1;
            match byte {
                b'(' => depth += 1,
                b')' => {
                    depth -= 1;
                    if depth == 0 {
                        return Ok(());
                    }
                }
                _ => {}
            }
        }

        Err(self.err(ErrorKind::UnbalancedParens))
    }

    fn skip_balanced_from_current(&mut self) -> Result<(), ParseError> {
        loop {
            match self.peek() {
                None => return Err(self.err(ErrorKind::UnbalancedParens)),
                Some(b')') => {
                    self.pos += 1;
                    return Ok(());
                }
                Some(b'(') => {
                    self.pos += 1;
                    self.skip_balanced()?;
                }
                Some(_) => {
                    self.pos += 1;
                }
            }
        }
    }

    fn take_vcp_code(&mut self, position: usize) -> Result<u8, ParseError> {
        if self.bytes.len().saturating_sub(self.pos) < 2 {
            return Err(self.err_at(ErrorKind::InvalidHex, position));
        }

        let end = self.pos + 2;
        let token = &self.input[self.pos..end];
        if !token.bytes().all(|byte| byte.is_ascii_hexdigit()) {
            return Err(self.err_at(ErrorKind::InvalidHex, position));
        }

        self.pos = end;
        Ok(u8::from_str_radix(token, 16).expect("validated hex byte"))
    }

    fn take_name(&mut self) -> &'a str {
        let start = self.pos;
        while matches!(self.peek(), Some(byte) if is_name_byte(byte)) {
            self.pos += 1;
        }
        &self.input[start..self.pos]
    }

    fn take_token(&mut self) -> Result<&'a str, ParseError> {
        let start = self.pos;
        while let Some(byte) = self.peek() {
            if byte == b'(' || byte == b')' || byte.is_ascii_whitespace() {
                break;
            }
            self.pos += 1;
        }

        if start == self.pos {
            Err(self.err(ErrorKind::UnexpectedEnd))
        } else {
            Ok(&self.input[start..self.pos])
        }
    }

    fn skip_spaces(&mut self) {
        while matches!(self.peek(), Some(byte) if byte.is_ascii_whitespace()) {
            self.pos += 1;
        }
    }

    fn consume(&mut self, expected: u8) -> bool {
        if self.peek() == Some(expected) {
            self.pos += 1;
            true
        } else {
            false
        }
    }

    fn peek(&self) -> Option<u8> {
        self.bytes.get(self.pos).copied()
    }

    fn err(&self, kind: ErrorKind) -> ParseError {
        self.err_at(kind, self.pos)
    }

    fn err_at(&self, kind: ErrorKind, position: usize) -> ParseError {
        ParseError { position, kind }
    }
}

fn is_name_byte(byte: u8) -> bool {
    byte.is_ascii_alphanumeric() || byte == b'_'
}

fn parse_hex_u16(token: &str) -> Option<u16> {
    if token.is_empty() || token.len() > 4 || !token.bytes().all(|byte| byte.is_ascii_hexdigit()) {
        return None;
    }
    u16::from_str_radix(token, 16).ok()
}

#[cfg(test)]
mod tests {
    use super::*;

    const BASIC_LCD: &str = include_str!("../fixtures/basic_lcd.caps");
    const COMPACT_VCP: &str = include_str!("../fixtures/compact_vcp.caps");
    const FALLBACK_CRT: &str = include_str!("../fixtures/fallback_crt.caps");
    const NESTED_VALUES: &str = include_str!("../fixtures/nested_values.caps");
    const DB_PATCH: &str = include_str!("../fixtures/db_patch_add_remove.caps");

    #[test]
    fn parses_basic_lcd_fixture() {
        let caps = Caps::parse(BASIC_LCD).unwrap();

        assert_eq!(caps.monitor_type(), MonitorType::Lcd);
        assert!(caps.is_supported(0x10));
        assert!(caps.is_supported(0x12));
        assert_eq!(
            caps.vcp(0x14).unwrap().values(),
            Some([0x05, 0x08, 0x0b].as_slice())
        );
        assert_eq!(
            caps.vcp(0x60).unwrap().values(),
            Some([0x01, 0x03].as_slice())
        );
    }

    #[test]
    fn parses_fallback_crt_fixture() {
        let caps = Caps::parse(FALLBACK_CRT).unwrap();

        assert_eq!(caps.monitor_type(), MonitorType::Crt);
        assert_eq!(
            caps.vcp_codes().collect::<Vec<_>>(),
            vec![0x10, 0x12, 0x16, 0x18, 0x1a, 0x50, 0x92]
        );
    }

    #[test]
    fn parses_compact_vcp_codes() {
        let caps = Caps::parse(COMPACT_VCP).unwrap();

        assert!(caps.is_supported(0x02));
        assert!(caps.is_supported(0x03));
        assert!(caps.is_supported(0x04));
        assert!(caps.is_supported(0x05));
        assert_eq!(
            caps.vcp(0x14).unwrap().values(),
            Some([0x05, 0x08].as_slice())
        );
        assert_eq!(
            caps.vcp(0x60).unwrap().values(),
            Some([0x03, 0x04].as_slice())
        );
    }

    #[test]
    fn tolerates_nested_value_groups() {
        let caps = Caps::parse(NESTED_VALUES).unwrap();

        assert_eq!(caps.vcp(0xe0).unwrap().values(), Some([0x02].as_slice()));
        assert_eq!(
            caps.vcp(0xe1).unwrap().values(),
            Some([0x03, 0x04].as_slice())
        );
        assert!(caps.is_supported(0xdf));
    }

    #[test]
    fn supports_database_style_add_remove() {
        let mut caps = Caps::parse(DB_PATCH).unwrap();

        assert!(caps.is_supported(0x10));
        assert!(caps.is_supported(0x12));
        assert_eq!(
            caps.vcp(0x60).unwrap().values(),
            Some([0x01, 0x03].as_slice())
        );

        assert_eq!(caps.apply_remove("(vcp(12 60(01)))").unwrap(), 2);
        assert!(!caps.is_supported(0x12));
        assert_eq!(caps.vcp(0x60).unwrap().values(), Some([0x03].as_slice()));

        assert_eq!(caps.apply_add("(vcp(12 60(0f)))").unwrap(), 2);
        assert!(caps.is_supported(0x12));
        assert_eq!(caps.vcp(0x60).unwrap().values(), Some([0x0f].as_slice()));
    }

    #[test]
    fn accepts_uppercase_type_names() {
        assert_eq!(
            Caps::parse("(type(LCD))").unwrap().monitor_type(),
            MonitorType::Lcd
        );
        assert_eq!(
            Caps::parse("(type(CRT))").unwrap().monitor_type(),
            MonitorType::Crt
        );
    }

    #[test]
    fn unknown_type_does_not_override_existing_type() {
        let mut caps = Caps::parse("(type(lcd))").unwrap();

        assert_eq!(caps.apply_add("(type(OLED))").unwrap(), 0);
        assert_eq!(caps.monitor_type(), MonitorType::Lcd);
    }

    #[test]
    fn supports_mixed_controls_and_value_lists() {
        let caps = Caps::parse("(vcp(10(01 02) 12 14(0A)))").unwrap();

        assert_eq!(caps.vcp_codes().collect::<Vec<_>>(), vec![0x10, 0x12, 0x14]);
        assert_eq!(
            caps.vcp(0x10).unwrap().values(),
            Some([0x01, 0x02].as_slice())
        );
        assert_eq!(caps.vcp(0x12).unwrap().values(), None);
        assert_eq!(caps.vcp(0x14).unwrap().values(), Some([0x0a].as_slice()));
    }

    #[test]
    fn rejects_invalid_vcp_identifier() {
        assert_eq!(
            Caps::parse("(vcp(GG))").unwrap_err().kind(),
            ErrorKind::InvalidHex
        );
        assert_eq!(
            Caps::parse("(vcp(-1))").unwrap_err().kind(),
            ErrorKind::InvalidHex
        );
    }

    #[test]
    fn rejects_invalid_values() {
        assert_eq!(
            Caps::parse("(vcp(10(0G)))").unwrap_err().kind(),
            ErrorKind::InvalidHex
        );
        assert_eq!(
            Caps::parse("(vcp(10(10000)))").unwrap_err().kind(),
            ErrorKind::InvalidHex
        );
    }

    #[test]
    fn rejects_value_list_without_vcp_code() {
        assert_eq!(
            Caps::parse("(vcp((01)))").unwrap_err().kind(),
            ErrorKind::ValueWithoutVcpCode
        );
    }

    #[test]
    fn rejects_unbalanced_parentheses() {
        assert_eq!(
            Caps::parse("(vcp(10)").unwrap_err().kind(),
            ErrorKind::UnbalancedParens
        );
        assert_eq!(
            Caps::parse(")(vcp(10))").unwrap_err().kind(),
            ErrorKind::UnexpectedCloseParen
        );
    }

    #[test]
    fn accepts_nested_unknown_sections_before_vcp() {
        let caps = Caps::parse("(prot(monitor)foo(bar(baz))vcp(10))").unwrap();

        assert!(caps.is_supported(0x10));
    }

    #[test]
    fn supports_boundary_control_and_value_ranges() {
        let caps = Caps::parse("(vcp(00(0000) FF(FFFF)))").unwrap();

        assert_eq!(caps.vcp(0x00).unwrap().values(), Some([0x0000].as_slice()));
        assert_eq!(caps.vcp(0xff).unwrap().values(), Some([0xffff].as_slice()));
    }
}
