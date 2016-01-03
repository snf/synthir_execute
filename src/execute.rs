#[derive(Clone, Debug, Copy)]
pub enum ExecuteError {
    Before,
    After,
    Instruction
}

pub trait Execute {
    fn create_section_rwx(len: u32) -> u64;
    fn create_isolated_section_rwx(len: u32) -> u64;
    fn write_memory(dst: u64, src: &[u8]);
    fn read_child_memory(src: u64, buf: &mut [u8], len: u32);
    fn dispose_section(dst: u64, len: u32);
    fn execute_with_steps(addr: u64, steps_before: u32, steps_after: u32)
                          -> Result<u64, ExecuteError>;
    fn dispose_execution();
}
