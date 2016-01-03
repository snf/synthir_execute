extern crate libc;

pub mod execute;
pub mod ptrace_execute;

pub use execute::Execute;
pub use ptrace_execute::PtraceExecute;

#[test]
fn it_works() {
}
