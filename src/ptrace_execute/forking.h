#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdio.h>

// Debug prints
// #define DEBUG

#ifdef DEBUG
#define DEBUG_printf(...) do{ fprintf( stderr, __VA_ARGS__ ); } while( 0 )
#else
#define DEBUG_printf(...) do{ } while ( 0 )
#endif


  // Needed for compatibility
#if defined(__i386)
#define REGISTER_IP EIP

#elif defined(__x86_64)
#define REGISTER_IP RIP

#else
#error Unsupported architecture
#endif

  /**
      The main idea of this api is to:
      1) Create the sections and write all the needed content
      2) Execute a child pointing to one of the sections
      3) Read the sections as needed
      4) Dispose the child
      5) Dispose the sections if these are not used any more and/or goto 1)

      All these function *don't* fail so there is no error reporting
      return but in `execute_with_steps` that it might trigger a
      hardware fault.

      XXX_ not sure if using size_t is correct here as this will
      interface with another architectures.
  */

  // Create a new section
  void *create_section_rwx(size_t);

  // Create a new section bounded by non accessible memory
  void *create_isolated_section_rwx(size_t len);

  // Write content to a created section
  void write_section(unsigned char *dst, const unsigned char *src, size_t len);

  // Read content from a section (after execution)
  // *ONLY* 4-byte aligned
  void read_child_section(unsigned char *buffer, const unsigned char *start, size_t len);

  // Free a section
  void dispose_section(void *section, size_t len);

  // 0 if success or -1 if failure
  int execute_with_steps(// Where to start executing
                         void *start,
                         // The steps before the instruction,
                         size_t steps_before,
                         // The steps after the instruction.
                         size_t steps_after,
                         // The instruction pointer (OUT). Will be
                         // read just after executing the instruction.
                         size_t *ip);
  void dispose_execution();


#ifdef __cplusplus
}
#endif
