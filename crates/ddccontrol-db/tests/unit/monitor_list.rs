use super::*;
use std::mem::{align_of, offset_of, size_of};

#[test]
fn monitor_list_layout_matches_c_fields_and_padding() {
    let pointer_size = size_of::<*mut c_char>();
    let align = align_of::<*mut c_char>();
    let align_up = |size: usize| (size + align - 1) & !(align - 1);
    assert_eq!(size_of::<c_uchar>(), 1);
    assert_eq!(offset_of!(CMonitorList, filename), 0);
    assert_eq!(offset_of!(CMonitorList, supported), pointer_size);
    assert_eq!(offset_of!(CMonitorList, name), align_up(pointer_size + 1));
    assert_eq!(
        offset_of!(CMonitorList, digital),
        offset_of!(CMonitorList, name) + pointer_size
    );
    assert_eq!(
        offset_of!(CMonitorList, next),
        align_up(offset_of!(CMonitorList, digital) + 1)
    );
    assert_eq!(
        size_of::<CMonitorList>(),
        offset_of!(CMonitorList, next) + pointer_size
    );
}

#[test]
fn allocation_failure_and_unwinding_clean_up_partial_lists() {
    let monitors = vec![
        Monitor {
            filename: "dev:/dev/i2c-1".into(),
            supported: 1,
            name: "Test".into(),
            digital: 128,
        };
        2
    ];
    for fail_at in 0..6 {
        for panic in [false, true] {
            let mut calls = 0;
            let result = catch_unwind(AssertUnwindSafe(|| unsafe {
                list_to_c(&monitors, &mut |size| {
                    if calls == fail_at {
                        assert!(!panic, "simulated allocation panic");
                        return ptr::null_mut();
                    }
                    calls += 1;
                    malloc(size)
                })
            }));
            if panic {
                assert!(result.is_err());
            } else {
                assert!(result.unwrap().is_none());
            }
        }
    }
    unsafe {
        let list = list_to_c(&monitors, &mut |size| malloc(size)).unwrap();
        assert_eq!(
            CStr::from_ptr((*list.0).filename).to_bytes(),
            b"dev:/dev/i2c-1"
        );
        assert_eq!((*list.0).digital, 128);
        let second = (*list.0).next;
        assert!(!second.is_null());
        assert!((*second).next.is_null());
        assert!(
            list_to_c(&[], &mut |_| panic!("empty list must not allocate"))
                .unwrap()
                .0
                .is_null()
        );
    }
}

#[test]
fn null_ffi_inputs_fail_without_changing_output() {
    unsafe {
        let sentinel = ptr::NonNull::<CMonitorList>::dangling().as_ptr();
        let mut output = sentinel;
        assert_eq!(
            ddccontrol_monitorlist_load(ptr::null(), c"3.3.0".as_ptr(), &mut output),
            -1
        );
        assert_eq!(output, sentinel);
        assert_eq!(
            ddccontrol_monitorlist_load(c"missing".as_ptr(), ptr::null(), &mut output),
            -1
        );
        assert_eq!(output, sentinel);
        assert_eq!(
            ddccontrol_monitorlist_load(c"missing".as_ptr(), c"3.3.0".as_ptr(), ptr::null_mut()),
            -1
        );
        assert_eq!(
            ddccontrol_monitorlist_save(ptr::null(), c"3.3.0".as_ptr(), ptr::null()),
            -1
        );
        assert_eq!(
            ddccontrol_monitorlist_save(c"missing".as_ptr(), ptr::null(), ptr::null()),
            -1
        );
    }
}
