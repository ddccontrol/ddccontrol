// Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

//! Shared options.xml vocabulary for the database loader and monitor scanner.

use crate::xml::{element_children, node_error, read_xml_file, required_attr};
use ddccontrol_xml::parse_integer as parse_int;
use roxmltree::Document;
use std::path::Path;

const DBVERSION: i64 = 3;

// These discriminants match the existing C ABI; the bridge converts them to c_int.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ControlType {
    Value = 0,
    Command = 1,
    List = 2,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Refresh {
    None = 0,
    All = 1,
}

#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct OptionsDb {
    pub groups: Vec<OptionGroup>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct OptionGroup {
    pub name: String,
    pub subgroups: Vec<OptionSubgroup>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct OptionSubgroup {
    pub name: String,
    pub pattern: Option<String>,
    pub controls: Vec<OptionControl>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct OptionControl {
    /// Required CBOR semantics prevent this control from being used.
    pub(crate) unavailable: bool,
    pub id: String,
    pub name: String,
    pub control_type: ControlType,
    pub refresh: Refresh,
    /// Optional scanner hint; installed monitor files supply their own addresses.
    pub raw_address: Option<String>,
    pub values: Vec<OptionValue>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct OptionValue {
    pub id: String,
    pub name: Option<String>,
    /// Optional scanner hint; monitor files supply the actual enum mapping.
    pub raw_value: Option<String>,
}

impl OptionsDb {
    pub fn controls(&self) -> impl Iterator<Item = &OptionControl> {
        self.groups
            .iter()
            .flat_map(|group| &group.subgroups)
            .flat_map(|subgroup| &subgroup.controls)
    }
}

impl OptionControl {
    /// Interpret an optional scanner address, rejecting invalid or oversized hints.
    pub fn address(&self) -> Result<Option<u8>, String> {
        self.raw_address
            .as_deref()
            .map(|text| {
                parse_int(text)
                    .ok()
                    .and_then(|value| u8::try_from(value).ok())
                    .ok_or_else(|| {
                        format!("Invalid address in options.xml for {}: {text}", self.id)
                    })
            })
            .transpose()
    }
}

impl OptionValue {
    /// Unparseable hints have no confirmed mapping and remain manual candidates.
    pub fn value(&self) -> Option<u16> {
        parse_int(self.raw_value.as_deref()?)
            .ok()
            .and_then(|value| u16::try_from(value).ok())
    }
}

/// Load options.xml using the same encoding and compatibility rules as monitor definitions.
pub fn load(datadir: &Path) -> Result<OptionsDb, String> {
    let xml = read_xml_file(&datadir.join("options.xml"))
        .map_err(|err| format!("I/O error while reading options.xml: {err}."))?;
    parse(&xml)
}

/// Parse the shared options vocabulary, retaining optional address and value hints.
/// Hints are interpreted only by the scanner; monitor loading does not depend on them.
pub fn parse(xml: &str) -> Result<OptionsDb, String> {
    let doc = Document::parse(xml).map_err(|_| "Document not parsed successfully.".to_string())?;
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
    let version = parse_int(version).map_err(|_| "Can't convert version to int.".to_string())?;
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
        for subgroup in element_children(group).filter(|node| node.tag_name().name() == "subgroup")
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
                    Some("none") | None => Refresh::None,
                    Some("all") => Refresh::All,
                    Some(_) => {
                        return Err(node_error(
                            control,
                            "Invalid refresh type (!= none, != all).",
                        ))
                    }
                };
                let control_type = match required_attr(control, "type")? {
                    "value" => ControlType::Value,
                    "command" => ControlType::Command,
                    "list" => ControlType::List,
                    _ => return Err(node_error(control, "Invalid type.")),
                };
                let mut option_control = OptionControl {
                    unavailable: false,
                    id: required_attr(control, "id")?.to_string(),
                    name: required_attr(control, "name")?.to_string(),
                    control_type,
                    refresh,
                    raw_address: control.attribute("address").map(ToString::to_string),
                    values: Vec::new(),
                };
                for value in
                    element_children(control).filter(|node| node.tag_name().name() == "value")
                {
                    option_control.values.push(OptionValue {
                        id: required_attr(value, "id")?.to_string(),
                        name: value.attribute("name").map(ToString::to_string),
                        raw_value: value.attribute("value").map(ToString::to_string),
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn retains_scanner_hints_without_making_them_mandatory_for_monitor_loading() {
        let xml = r#"<options dbversion="0x3" date="2026-09-22">
            <group name="Image"><subgroup name="Inputs" pattern="color">
                <control id="input" name="Input" type="list" refresh="all" address="0140">
                    <value id="hdmi" name="HDMI" value="+0x11"/>
                    <value id="vendor" value="65535"/>
                    <value id="unknown" value="65536"/>
                    <value id="negative" value="-1"/>
                    <value id="missing"/>
                </control>
                <control id="brightness" name="Brightness" type="value"/>
                <control id="invalid_hint" name="Hint" type="command" address="0x100"/>
            </subgroup></group>
        </options>"#;
        let options = parse(xml).unwrap();
        assert_eq!(
            options.groups[0].subgroups[0].pattern.as_deref(),
            Some("color")
        );
        let controls: Vec<_> = options.controls().collect();
        assert_eq!(controls[0].address().unwrap(), Some(0x60));
        assert_eq!(controls[0].control_type, ControlType::List);
        assert_eq!(controls[0].refresh, Refresh::All);
        assert_eq!(
            controls[0]
                .values
                .iter()
                .map(OptionValue::value)
                .collect::<Vec<_>>(),
            [Some(0x11), Some(65535), None, None, None]
        );
        assert_eq!(controls[1].address().unwrap(), None);
        assert!(controls[2].address().is_err());
    }
}
