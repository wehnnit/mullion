// an export called by its address with a count of pointer-sized arguments, each the same value, as an export test
// reaches a function whose prototype no header has. on 32-bit x86 a stdcall callee pops its own arguments, so the call
// also says how many bytes of them it left: a callee declared with fewer arguments than its callers push (or more)
// leaves the stack off by the difference, which a caller with a frame pointer hides until something reads it.
// the counts are the Windows import libraries' (_Name@bytes in mingw-w64's lib32 .def files).
#pragma once
#include <windows.h>
#include <cstdint>

struct ExportCall {
  uint64_t result; // eax, edx:eax for a 64-bit result on x86; rax on x64
  long unpopped;   // x86: bytes of the pushed arguments the callee did not pop (negative: popped too many)
};

// a buffer of zeros for the arguments: a pointer every stub may read or write through
inline unsigned char export_zeros[1024];

inline ExportCall
call_export(FARPROC proc, unsigned arg_bytes, uintptr_t arg = reinterpret_cast<uintptr_t>(export_zeros)) {
#if defined(__i386__)
  uint32_t lo, hi, count = arg_bytes / 4;
  long unpopped;
  // count is read before the first push, while a stack-relative operand still means what it did
  asm volatile("mov %%esp, %%esi\n\t"
               "mov %[count], %%ecx\n\t"
               "1:\n\t"
               "test %%ecx, %%ecx\n\t"
               "jz 2f\n\t"
               "push %[arg]\n\t"
               "dec %%ecx\n\t"
               "jmp 1b\n\t"
               "2:\n\t"
               "call *%[fn]\n\t"
               "mov %%esi, %%ecx\n\t"
               "sub %%esp, %%ecx\n\t"
               "mov %%esi, %%esp\n\t"
               : "=&a"(lo), "=&d"(hi), "=&c"(unpopped)
               : [fn] "b"(proc), [arg] "D"(arg), [count] "m"(count)
               : "esi", "memory", "cc");
  return {(uint64_t(hi) << 32) | lo, unpopped};
#else
  // the caller cleans up on x64, so arguments beyond the callee's are harmless: twelve, the most any export takes
  (void)arg_bytes;
  using Fn = uint64_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                          uintptr_t, uintptr_t, uintptr_t, uintptr_t);
  return {reinterpret_cast<Fn>(reinterpret_cast<void *>(proc))(arg, arg, arg, arg, arg, arg, arg, arg, arg, arg, arg,
                                                               arg),
          0};
#endif
}
