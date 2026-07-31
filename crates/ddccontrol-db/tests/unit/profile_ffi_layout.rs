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
fn profile_ffi_layout_matches_c_abi_contract() {
    assert_eq!(size_of::<c_int>(), 4);
    assert_eq!(size_of::<c_uchar>(), 1);
    assert_eq!(size_of::<c_ushort>(), 2);
    assert_eq!(field_offset!(CProfile, filename), 0);
    assert_eq!(field_offset!(CProfile, name), size_of::<*mut c_char>());
    assert_eq!(field_offset!(CProfile, pnpid), size_of::<*mut c_char>() * 2);
    assert_eq!(field_offset!(CProfile, size), size_of::<*mut c_char>() * 3);
    assert_eq!(
        field_offset!(CProfile, address),
        size_of::<*mut c_char>() * 3 + size_of::<c_int>()
    );
    assert_eq!(
        field_offset!(CProfile, value),
        align_up(
            field_offset!(CProfile, address) + MAX_CONTROLS,
            align_of::<c_ushort>()
        )
    );
    assert_eq!(
        field_offset!(CProfile, next),
        align_up(
            field_offset!(CProfile, value) + MAX_CONTROLS * size_of::<c_ushort>(),
            align_of::<*mut CProfile>()
        )
    );
}
