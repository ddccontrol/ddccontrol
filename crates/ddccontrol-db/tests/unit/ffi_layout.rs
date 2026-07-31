use super::*;
use std::mem::{align_of, size_of};

macro_rules! field_offset {
    ($ty:ty, $field:tt) => {{
        let value = std::mem::MaybeUninit::<$ty>::uninit();
        let base = value.as_ptr();
        unsafe { std::ptr::addr_of!((*base).$field) as usize - base as usize }
    }};
}

fn align_up(value: usize, align: usize) -> usize {
    (value + align - 1) & !(align - 1)
}

#[test]
fn caps_ffi_layout_matches_c_abi_contract() {
    assert_eq!(size_of::<c_int>(), 4);
    assert_eq!(size_of::<c_ushort>(), 2);

    assert_eq!(field_offset!(CVcpEntry, values_len), 0);
    assert_eq!(
        field_offset!(CVcpEntry, values),
        align_up(size_of::<c_int>(), align_of::<*mut c_ushort>())
    );

    let expected_vcp_bytes = size_of::<[*mut CVcpEntry; 256]>();
    assert_eq!(field_offset!(CCaps, vcp), 0);
    assert_eq!(field_offset!(CCaps, monitor_type), expected_vcp_bytes);
    assert_eq!(
        field_offset!(CCaps, raw_caps),
        align_up(
            expected_vcp_bytes + size_of::<c_int>(),
            align_of::<*mut c_char>()
        )
    );
}

#[test]
fn edid_ffi_layout_matches_c_abi_contract() {
    assert_eq!(size_of::<c_char>(), 1);
    assert_eq!(size_of::<c_uchar>(), 1);
    assert_eq!(size_of::<c_int>(), 4);
    assert_eq!(size_of::<c_uint>(), 4);

    assert_eq!(field_offset!(CEdidInfo, serial_number), 0);
    assert_eq!(field_offset!(CEdidInfo, manufacture_week), 4);
    assert_eq!(field_offset!(CEdidInfo, manufacture_year), 8);
    assert_eq!(field_offset!(CEdidInfo, version), 12);
    assert_eq!(field_offset!(CEdidInfo, revision), 16);
    assert_eq!(field_offset!(CEdidInfo, max_width_cm), 20);
    assert_eq!(field_offset!(CEdidInfo, max_height_cm), 24);
    assert_eq!(field_offset!(CEdidInfo, monitor_name), 28);
    assert_eq!(field_offset!(CEdidInfo, serial_ascii), 42);

    assert_eq!(field_offset!(CEdidResult, pnpid), 0);
    assert_eq!(field_offset!(CEdidResult, digital), 8);
    assert_eq!(field_offset!(CEdidResult, edid), 9);
    assert_eq!(field_offset!(CEdidResult, edid_len), 140);
    assert_eq!(field_offset!(CEdidResult, info), 144);
}
