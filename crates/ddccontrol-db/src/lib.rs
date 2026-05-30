use ddccontrol_caps::{Caps, MonitorType, VcpEntry};
use std::ffi::CStr;
use std::os::raw::{c_char, c_int, c_ushort, c_void};
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr;
use std::slice;

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

extern "C" {
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
    rust_caps.set_monitor_type(match (*caps).monitor_type {
        1 => MonitorType::Lcd,
        2 => MonitorType::Crt,
        _ => MonitorType::Unknown,
    });

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

        rust_caps.insert_vcp(
            code as u8,
            match values {
                Some(values) => VcpEntry::with_values(values),
                None => VcpEntry::all_values(),
            },
        );
    }

    rust_caps
}

unsafe fn replace_c_caps(caps: *mut CCaps, rust_caps: &Caps) -> bool {
    free_c_vcp_entries(caps);
    (*caps).monitor_type = match rust_caps.monitor_type() {
        MonitorType::Unknown => 0,
        MonitorType::Lcd => 1,
        MonitorType::Crt => 2,
    };

    for (code, entry) in rust_caps.vcp_entries() {
        let c_entry = malloc(std::mem::size_of::<CVcpEntry>()) as *mut CVcpEntry;
        if c_entry.is_null() {
            return false;
        }

        match entry.values() {
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
    #[cfg(all(not(test), feature = "gettext"))]
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

    #[cfg(all(not(test), feature = "gettext"))]
    extern "C" {
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

    struct ParsedMonitorControls {
        controls: Vec<MonitorControl>,
        elements: Vec<MonitorElement>,
    }

    struct MonitorElement {
        name: String,
        id: Option<String>,
        line: u32,
    }

    struct MonitorControl {
        id: String,
        raw_address: Option<String>,
        raw_delay: Option<String>,
        values: Vec<MonitorValue>,
        child_index: usize,
    }

    struct MonitorValue {
        element_name: String,
        id: Option<String>,
        raw_value: Option<String>,
        line: u32,
    }

    #[no_mangle]
    pub unsafe extern "C" fn ddcci_init_db(usedatadir: *mut c_char) -> c_int {
        let datadir = if usedatadir.is_null() {
            PathBuf::from(DEFAULT_DATADIR)
        } else {
            PathBuf::from(CStr::from_ptr(usedatadir).to_string_lossy().into_owned())
        };

        *DB_CONTEXT.lock().unwrap() = None;
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
        monitor_controls: &ParsedMonitorControls,
        caps: *mut CCaps,
        defined: &mut [bool; 256],
        faulttolerance: bool,
    ) -> Result<(), DbError> {
        let mut matched = vec![false; monitor_controls.elements.len()];

        for (group_index, option_group) in options.groups.iter().enumerate() {
            for (subgroup_index, option_subgroup) in option_group.subgroups.iter().enumerate() {
                for option_control in &option_subgroup.controls {
                    let Some(monitor_control) = monitor_controls
                        .controls
                        .iter()
                        .find(|control| control.id == option_control.id)
                    else {
                        continue;
                    };

                    matched[monitor_control.child_index] = true;
                    let address = monitor_control_address(monitor_control)? as usize;
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
                        address: address as u8,
                        delay: monitor_control_delay(monitor_control)?,
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

        for (index, control) in monitor_controls.elements.iter().enumerate() {
            if !matched[index] {
                eprintln!(
                    "Element {} (id={}) has not been found (line {}).",
                    control.name,
                    control.id.as_deref().unwrap_or("(null)"),
                    control.line
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

    fn monitor_control_address(monitor_control: &MonitorControl) -> Result<u8, DbError> {
        let raw_address = monitor_control
            .raw_address
            .as_deref()
            .ok_or_else(|| DbError::new("Can't find address property.".to_string()))?;
        let address = parse_int(raw_address)
            .map_err(|_| DbError::new("Can't convert address to int.".to_string()))?;
        if !(0..=255).contains(&address) {
            return Err(DbError::new(
                "Address is outside the supported 0-255 range.".to_string(),
            ));
        }
        Ok(address as u8)
    }

    fn monitor_control_delay(monitor_control: &MonitorControl) -> Result<c_int, DbError> {
        match monitor_control.raw_delay.as_deref() {
            Some(delay) => parse_int_decimal(delay)
                .map(|delay| delay as c_int)
                .map_err(|_| DbError::new("Can't convert delay to int.".to_string())),
            None => Ok(-1),
        }
    }

    fn get_value_list(
        option_control: &OptionControl,
        monitor_control: &MonitorControl,
        faulttolerance: bool,
    ) -> Result<Vec<DbValue>, DbError> {
        let mut values = Vec::new();
        let mut matched = vec![false; monitor_control.values.len()];

        for monitor_value in monitor_control
            .values
            .iter()
            .filter(|value| value.element_name == "value")
        {
            if monitor_value.id.is_none() {
                return Err(DbError::new("Can't find id property.".to_string()));
            }
        }

        for option_value in &option_control.values {
            if let Some((monitor_index, monitor_value)) = monitor_control
                .values
                .iter()
                .enumerate()
                .find(|(_, value)| {
                    value.element_name == "value"
                        && value.id.as_deref() == Some(option_value.id.as_str())
                })
            {
                matched[monitor_index] = true;
                let raw_value = monitor_value
                    .raw_value
                    .as_deref()
                    .ok_or_else(|| DbError::new("Can't find value property.".to_string()))?;
                let parsed_value = parse_int(raw_value)
                    .map_err(|_| DbError::new("Can't convert value to int.".to_string()))?;
                if !(0..=u16::MAX as i64).contains(&parsed_value) {
                    return Err(DbError::new(
                        "Value is outside the supported 0-65535 range.".to_string(),
                    ));
                }
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
                    value16: parsed_value as u16,
                });
            }
        }

        for (index, value) in monitor_control.values.iter().enumerate() {
            if !matched[index] {
                eprintln!(
                    "Element {} (id={}) has not been found (line {}).",
                    value.element_name,
                    value.id.as_deref().unwrap_or("(null)"),
                    value.line
                );
                if !faulttolerance {
                    return Err(DbError::new("Unmatched value in monitor XML.".to_string()));
                }
            }
        }

        Ok(values)
    }

    fn parse_monitor_controls(node: Node<'_, '_>) -> Result<ParsedMonitorControls, String> {
        let mut controls = Vec::new();
        let mut elements = Vec::new();
        for (child_index, control) in element_children(node).enumerate() {
            elements.push(MonitorElement {
                name: control.tag_name().name().to_string(),
                id: control.attribute("id").map(ToString::to_string),
                line: line(control),
            });
            if control.tag_name().name() != "control" {
                continue;
            }
            let Some(id) = control.attribute("id") else {
                continue;
            };
            let mut monitor_control = MonitorControl {
                id: id.to_string(),
                raw_address: control.attribute("address").map(ToString::to_string),
                raw_delay: control.attribute("delay").map(ToString::to_string),
                values: Vec::new(),
                child_index,
            };
            for value in element_children(control) {
                monitor_control.values.push(MonitorValue {
                    element_name: value.tag_name().name().to_string(),
                    id: value.attribute("id").map(ToString::to_string),
                    raw_value: value.attribute("value").map(ToString::to_string),
                    line: line(value),
                });
            }
            controls.push(monitor_control);
        }
        Ok(ParsedMonitorControls { controls, elements })
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
        #[cfg(test)]
        {
            return input.to_string();
        }

        #[cfg(all(not(test), not(feature = "gettext")))]
        {
            input.to_string()
        }

        #[cfg(all(not(test), feature = "gettext"))]
        {
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
        let input = trim_ascii_start(input);
        let (negative, rest) = if let Some(rest) = input.strip_prefix('-') {
            (true, rest)
        } else if let Some(rest) = input.strip_prefix('+') {
            (false, rest)
        } else {
            (false, input)
        };
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
        trim_ascii_start(input).parse::<i64>()
    }

    fn trim_ascii_start(input: &str) -> &str {
        input.trim_start_matches(|byte: char| byte.is_ascii_whitespace())
    }

    fn is_valid_monitor_profile_name(name: &str) -> bool {
        !name.is_empty()
            && name
                .bytes()
                .all(|byte| byte.is_ascii_alphanumeric() || byte == b'_' || byte == b'-')
    }

    #[cfg(test)]
    mod tests {
        use super::*;

        #[test]
        fn parse_int_matches_strtol_style_database_values() {
            assert_eq!(parse_int(" 1").unwrap(), 1);
            assert_eq!(parse_int("+1").unwrap(), 1);
            assert_eq!(parse_int("-1").unwrap(), -1);
            assert_eq!(parse_int("0x10").unwrap(), 16);
            assert_eq!(parse_int("010").unwrap(), 8);
            assert!(parse_int("09").is_err());
            assert!(parse_int("1 ").is_err());
        }

        #[test]
        fn unmatched_monitor_values_are_not_parsed() {
            let option_control = OptionControl {
                id: "input".to_string(),
                name: "Input".to_string(),
                control_type: CONTROL_TYPE_LIST,
                refresh: REFRESH_TYPE_NONE,
                values: vec![OptionValue {
                    id: "hdmi".to_string(),
                    name: Some("HDMI".to_string()),
                }],
            };
            let monitor_control = MonitorControl {
                id: "input".to_string(),
                raw_address: Some("0x60".to_string()),
                raw_delay: None,
                values: vec![
                    MonitorValue {
                        element_name: "value".to_string(),
                        id: Some("hdmi".to_string()),
                        raw_value: Some(" 1".to_string()),
                        line: 1,
                    },
                    MonitorValue {
                        element_name: "value".to_string(),
                        id: Some("unused".to_string()),
                        raw_value: Some("not-an-int".to_string()),
                        line: 2,
                    },
                ],
                child_index: 0,
            };

            let values = get_value_list(&option_control, &monitor_control, true).unwrap();

            assert_eq!(values.len(), 1);
            assert_eq!(values[0].id, "hdmi");
            assert_eq!(values[0].value16, 1);
        }

        #[test]
        fn parse_monitor_controls_keeps_unknown_control_children_for_validation() {
            let doc = Document::parse(
                r#"<controls>
                    <unknown id="bad"/>
                    <control id="input" address="0x60"/>
                </controls>"#,
            )
            .unwrap();

            let parsed = parse_monitor_controls(doc.root_element()).unwrap();

            assert_eq!(parsed.elements.len(), 2);
            assert_eq!(parsed.elements[0].name, "unknown");
            assert_eq!(parsed.controls.len(), 1);
            assert_eq!(parsed.controls[0].child_index, 1);
        }

        #[test]
        fn missing_value_id_is_deferred_until_control_is_matched() {
            let doc = Document::parse(
                r#"<controls>
                    <control id="input" address="0x60">
                        <value value="1"/>
                    </control>
                </controls>"#,
            )
            .unwrap();

            let parsed = parse_monitor_controls(doc.root_element()).unwrap();

            assert_eq!(parsed.controls.len(), 1);
            assert!(parsed.controls[0].values[0].id.is_none());
        }

        #[test]
        fn matched_monitor_values_require_id() {
            let option_control = OptionControl {
                id: "input".to_string(),
                name: "Input".to_string(),
                control_type: CONTROL_TYPE_LIST,
                refresh: REFRESH_TYPE_NONE,
                values: vec![OptionValue {
                    id: "hdmi".to_string(),
                    name: Some("HDMI".to_string()),
                }],
            };
            let monitor_control = MonitorControl {
                id: "input".to_string(),
                raw_address: Some("0x60".to_string()),
                raw_delay: None,
                values: vec![MonitorValue {
                    element_name: "value".to_string(),
                    id: None,
                    raw_value: Some("1".to_string()),
                    line: 1,
                }],
                child_index: 0,
            };

            assert!(get_value_list(&option_control, &monitor_control, true).is_err());
        }

        #[test]
        fn parse_monitor_controls_keeps_control_without_id_unmatched() {
            let doc = Document::parse(
                r#"<controls>
                    <control address="0x60"/>
                </controls>"#,
            )
            .unwrap();

            let parsed = parse_monitor_controls(doc.root_element()).unwrap();

            assert_eq!(parsed.elements.len(), 1);
            assert_eq!(parsed.elements[0].name, "control");
            assert!(parsed.elements[0].id.is_none());
            assert!(parsed.controls.is_empty());
        }

        #[test]
        fn parse_monitor_controls_defers_address_and_delay_validation() {
            let doc = Document::parse(
                r#"<controls>
                    <control id="unknown" address="not-hex" delay="bad"/>
                </controls>"#,
            )
            .unwrap();

            let parsed = parse_monitor_controls(doc.root_element()).unwrap();

            assert_eq!(parsed.controls.len(), 1);
            assert!(monitor_control_address(&parsed.controls[0]).is_err());
            assert!(monitor_control_delay(&parsed.controls[0]).is_err());
        }

        #[test]
        fn unknown_monitor_value_children_use_unmatched_validation() {
            let option_control = OptionControl {
                id: "input".to_string(),
                name: "Input".to_string(),
                control_type: CONTROL_TYPE_LIST,
                refresh: REFRESH_TYPE_NONE,
                values: vec![OptionValue {
                    id: "hdmi".to_string(),
                    name: Some("HDMI".to_string()),
                }],
            };
            let monitor_control = MonitorControl {
                id: "input".to_string(),
                raw_address: Some("0x60".to_string()),
                raw_delay: None,
                values: vec![
                    MonitorValue {
                        element_name: "value".to_string(),
                        id: Some("hdmi".to_string()),
                        raw_value: Some("1".to_string()),
                        line: 1,
                    },
                    MonitorValue {
                        element_name: "unknown".to_string(),
                        id: Some("extra".to_string()),
                        raw_value: None,
                        line: 2,
                    },
                ],
                child_index: 0,
            };

            assert!(get_value_list(&option_control, &monitor_control, false).is_err());
            assert!(get_value_list(&option_control, &monitor_control, true).is_ok());
        }
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
