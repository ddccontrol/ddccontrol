// Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

//! Database XML decoding and diagnostics shared by options and monitor definitions.

use roxmltree::Node;

pub(crate) use ddccontrol_db_format::xml::{
    decode_xml_bytes, normalize_xml_document, read_xml_file,
};

pub(crate) fn required_attr<'a, 'd>(node: Node<'a, 'd>, name: &str) -> Result<&'a str, String> {
    node.attribute(name)
        .ok_or_else(|| node_error(node, &format!("Can't find {name} property.")))
}

pub(crate) fn element_children<'a, 'd>(node: Node<'a, 'd>) -> impl Iterator<Item = Node<'a, 'd>> {
    node.children().filter(|child| child.is_element())
}

pub(crate) fn node_error(node: Node<'_, '_>, message: &str) -> String {
    format!("Error: {message} @line {}", line(node))
}

pub(crate) fn line(node: Node<'_, '_>) -> u32 {
    node.document().text_pos_at(node.range().start).row
}
