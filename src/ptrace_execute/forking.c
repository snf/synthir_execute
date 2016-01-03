#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/user.h>
#include <sys/reg.h>
#include <sys/mman.h>
#include <signal.h>
#include <string.h>
#include <assert.h>

#include "forking.h"

// Function pointer used for jumping to assembly code
typedef void (*voidFun)(void);

// Child is global because we can only handle one at a time.
pid_t child;

void *create_section_rwx(size_t len) {
  void *addr = mmap(0, len, PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  memset(addr, 0, len);
  return addr;
}

void *create_isolated_section_rwx(size_t len) {
  // Check that len is page aligned
  assert((len & ~(PAGE_SIZE - 1)) == len);
  size_t len_ = len + PAGE_SIZE * 2;
  void *map = mmap(0, len_, PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  mprotect(map, PAGE_SIZE, PROT_NONE);
  mprotect(map + len + PAGE_SIZE, PAGE_SIZE, PROT_NONE);
  void *ret = map + PAGE_SIZE;
  memset(ret, 0, len);
  return ret;
}

void write_section(unsigned char *dst, const unsigned char *src, size_t len) {
  memcpy(dst, src, len);
}

void read_child_section(unsigned char *buffer, const unsigned char *start, size_t len) {
  size_t start_addr  = (size_t) start;
  unsigned int *buffer_uint = (unsigned int*) buffer;

  assert(len % 4 == 0);

  for(size_t i = 0; i < len/4; i++) {
    buffer_uint[i] = ptrace(PTRACE_PEEKDATA, child,
                              start_addr + i * 4, NULL);
  }
}

void dispose_section(void *section, size_t len) {
  munmap(section, len);
}

void forked(void *start) {
  // Tell parent to attach us and jump to code
  ptrace(PTRACE_TRACEME, 0, NULL, NULL);
  // Raise SIGSTOP so parent attaches here before continuing
  DEBUG_printf("child: raising SIGSTOP\n");
  raise(SIGSTOP);
  // Jump to code
  DEBUG_printf("reading mem at start\n");
  DEBUG_printf("mem: %p\n", *(unsigned long **)start);
  DEBUG_printf("child: jumping to start: %p\n", start);
  ((voidFun)start)();
}

int ignore_x_steps(size_t steps) {
  int status;

  for(size_t i = 0; i < steps; i++) {
    int ret = ptrace(PTRACE_SINGLESTEP, child, NULL, NULL);
    DEBUG_printf("singlestep ret: %d\n", ret);
    while(1) {
      waitpid(child, &status, 0);
      DEBUG_printf("status: %x\n", status);
      if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
#ifdef DEBUG
        size_t ip = ptrace(PTRACE_PEEKUSER, child, sizeof(size_t)*REGISTER_IP);
        DEBUG_printf("IP: 0x%x\n", ip);
        DEBUG_printf("mem: 0x%x\n", ptrace(PTRACE_PEEKDATA, child, ip));
#endif
        break;
      } else {
        DEBUG_printf("other signal: %x\n", signal);
        return -1;
      }
    }
  }

  return 0;
}

/**
    Return:
    * -1 in case of child exception on instruction execution
    * -2 in case of child exception before reaching execution
    * -3 in case of child exception after the execution
    * 0 if there was no exception

    As a note, `ip` can not be trusted in case of any exception
*/
int execute_with_steps(void *start, size_t steps_before,
                       size_t steps_after, size_t *ip) {

  int status;

  DEBUG_printf("forking\n");

  child = fork();
  if(child == 0) {
    forked(start);
  }
  else {
    // Continue until the first TRAP instruction
    DEBUG_printf("trying to catch TRAP\n");
    while(1) {
      waitpid(child, &status, 0);
      DEBUG_printf("status: %x\n", status);
      if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
        //if (WIFSTOPPED(status)) {
        break;
      }
      ptrace(PTRACE_CONT, child, NULL, NULL);
    }
    DEBUG_printf("after SIGTRAP\n");

    // Single step to reach instruction
    // We start at -1 because trap instruction was already executed
    if (ignore_x_steps(steps_before - 1) != 0) {
      return -2;
    }

    *ip = ptrace(PTRACE_PEEKUSER, child, sizeof(size_t)*REGISTER_IP);
    DEBUG_printf("IP: 0x%x\n", *ip);
    DEBUG_printf("mem: 0x%x\n", ptrace(PTRACE_PEEKDATA, child, *ip));

    // Execute instruction
    DEBUG_printf("instruction!\n");
    if (ignore_x_steps(1) != 0) {
      return -1;
    }
    DEBUG_printf("instruction executed\n");

    // Get IP
    *ip = ptrace(PTRACE_PEEKUSER, child, sizeof(size_t)*REGISTER_IP);
    DEBUG_printf("IP: 0x%x\n", *ip);
    DEBUG_printf("mem: 0x%x\n", ptrace(PTRACE_PEEKDATA, child, *ip));

    // Single step the rest of the code
    if (ignore_x_steps(steps_after) != 0) {
      return -3;
    }

    // Everything is ok
    return 0;
  }
  return 1;
}

void dispose_execution() {
  int status, ret;
  DEBUG_printf("dettaching and killing pid: %d\n", child);
  status = -1;
  ret = -1;
  // Kill child
  kill(child, SIGKILL);
  // Detach from child
  ptrace(PTRACE_DETACH, child, NULL, NULL);
  // Wait so it's not left as zombie
  while(!WIFSIGNALED(status)) {
    ret = waitpid(child, &status, 0);
    DEBUG_printf("status: %d, ret: %d\n", status, ret);
  }
  // Null child pid
  child = 0;
}

// TEST ME :)

// int main(int argc, char *argv[]) {
//   size_t ip;
//   char *mem = (char *) create_section_rwx(0x1000);
//   memset(mem, 0x90, 0x100);
//   mem[0] = 0xcc;

//   // push rbp
//   // mem[16] = 0x55;

//   // xor rax, rax
//   mem[4] = 0x48;
//   mem[5] = 0x31;
//   mem[6] = 0xc0;

//   // mov rax, [rax]
//   mem[7] = 0x48;
//   mem[8] = 0x8b;
//   mem[9] = 0x00;

//   int ret = execute_with_steps(mem, 5, 0, &ip);

//   DEBUG_printf("finished with: %d\n", ret);
//   DEBUG_printf("killing and disposing resources\n");

//   dispose_execution();
//   dispose_section(mem, 0x1000);

// }
