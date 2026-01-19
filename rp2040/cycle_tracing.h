#ifndef __riscv
#define PROF_TP(tag,tp,par) \
  __asm__ __volatile__ ( \
    ".syntax unified\n\t" \
    "b 1f\n\t" \
    ".word 0xffffabcd\n\t" \
    ".asciz "#tag"\n\t" \
    "1:\n\t" \
  );
#else
#define PROF_TP(tag,tp,par) \
  __asm__ __volatile__ ( \
    "j 1f\n\t" \
    ".word 0xffffabcd\n\t" \
    ".asciz "#tag"\n\t" \
    "1:\n\t" \
  );
#endif

#define cycle_info(a) PROF_TP(a, "0x54505052", "0x00000000")
#ifdef CYCLE_TRACE
#define cycle_trace(a) PROF_TP(a, "0x54505052", "0x00000000")
#else
#define cycle_trace(a)
#endif
