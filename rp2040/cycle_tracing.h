#ifndef __riscv
#define PROF_TP(par) \
  __asm__ __volatile__ ( \
    ".syntax unified\n\t" \
    "b 1f\n\t" \
    ".word 0xffffabcd\n\t" \
    ".asciz "#par"\n\t" \
    ".balign 4\n\t" \
    "1:\n\t" \
  );
#else
#define PROF_TP(par) \
  __asm__ __volatile__ ( \
    "j 1f\n\t" \
    ".word 0xffffabcd\n\t" \
    ".asciz "#par"\n\t" \
    ".balign 4\n\t" \
    "1:\n\t" \
  );
#endif

#define cycle_info(a) PROF_TP(a)
#ifdef CYCLE_TRACE
#define cycle_trace(a) PROF_TP(a)
#else
#define cycle_trace(a)
#endif
