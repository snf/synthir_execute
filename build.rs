extern crate gcc;

fn main() {
    gcc::Config::new()
        .file("src/ptrace_execute/forking.c")
        .flag("-std=gnu99")
        .compile("libforking.a");
}
