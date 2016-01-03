use execute::{Execute, ExecuteError};

pub struct PtraceExecute;

#[allow(non_camel_case_types)]
mod native {
    pub type ptrdiff_t = ::libc::c_long;
    pub type size_t = ::libc::c_ulong;
    pub type wchar_t = ::libc::c_int;
    extern "C" {
        pub fn create_section_rwx(arg1: size_t) -> *mut ::libc::c_void;
        pub fn create_isolated_section_rwx(arg1: size_t) -> *mut ::libc::c_void;
        pub fn write_section(dst: *mut ::libc::c_uchar,
                             src: *const ::libc::c_uchar,
                             len: size_t) -> ();
        pub fn read_child_section(buffer: *mut ::libc::c_uchar,
                                  start: *const ::libc::c_uchar,
                                  len: size_t) -> ();
        pub fn dispose_section(section: *const ::libc::c_void, len: size_t) -> ();
        pub fn execute_with_steps(start: *const ::libc::c_void,
                                  steps_before: size_t, steps_after: size_t,
                                  ip: *mut size_t) -> ::libc::c_int;
        pub fn dispose_execution() -> ();
    }
}

impl Execute for PtraceExecute {
    fn create_section_rwx(len: u32) -> u64 {
        unsafe {
            native::create_section_rwx(len as native::size_t) as u64
        }
    }

    fn create_isolated_section_rwx(len: u32) -> u64 {
        unsafe {
            native::create_isolated_section_rwx(len as native::size_t) as u64
        }
    }

    fn write_memory(dst: u64, src: &[u8]) {
        unsafe {
            native::write_section(dst as *mut ::libc::c_uchar,
                                  src.as_ptr(), src.len() as native::size_t);
        }
    }

    fn read_child_memory(src: u64, buf: &mut [u8], len: u32) {
        unsafe {
            native::read_child_section(buf.as_mut_ptr(),
                                       src as *const ::libc::c_uchar,
                                       len as native::size_t);
        }
    }

    fn dispose_section(dst: u64, len: u32) {
        unsafe {
            native::dispose_section(dst as *const ::libc::c_void,
                                    len as native::size_t);
        }
    }

    fn execute_with_steps(addr: u64, steps_before: u32, steps_after: u32)
                          -> Result<u64, ExecuteError> {
        unsafe {
            let mut ip: u64 = 0;
            let res = native::execute_with_steps(
                addr as *mut ::libc::c_void,
                steps_before as native::size_t,
                steps_after as native::size_t,
                &mut ip as *mut native::size_t);
            match res {
                -1 => Err(ExecuteError::Instruction),
                -2 => Err(ExecuteError::Before),
                -3 => Err(ExecuteError::After),
                 0 => Ok(ip),
                _  => panic!("should not return any other value")
            }
        }
    }

    fn dispose_execution() {
        unsafe {
            native::dispose_execution();
        }
    }
}
