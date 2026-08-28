/*
 * Shared helpers for linuwux PE test stubs.
 *
 * These are deliberately dumb Windows programs. They exist only to:
 *   1. Sit next to a marker file so liblinuwux.so classifies the process
 *      as a game (argv[1] Windows path → marker scan).
 *   2. Issue the CPUID leaves the real reflex / DenuvOwO loaders use so
 *      the library's protocol state machine, KUSER profiles, and static
 *      identity tables can be exercised under Wine.
 *
 * Build with a mingw-w64 cross compiler, e.g.:
 *   x86_64-w64-mingw32-gcc -O2 -o modern_arm.exe modern_arm.c
 *
 * CPUID is done with plain inline asm so we do not depend on MSVC
 * intrinsics (__cpuidex) that mingw does not always expose.
 */

#ifndef LINUWUX_TEST_COMMON_H
#define LINUWUX_TEST_COMMON_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* Leaves that drive the library protocol state machine. */
#define LEAF_ARM                       0x336933u
#define LEAF_FAKETIME                  0x336967u
#define LEAF_LEGACY_INIT               0x69696969u
#define LEAF_LEGACY_KUSER              0x1337u
#define LEAF_LEGACY_QUERY_SYSTEM_ID    0x336943u
#define LEAF_LEGACY_QUERY_FULL_HANDLER 0x336934u
#define LEAF_LEGACY_QUERY_FULL_ID      0x336944u

/* Dummy handler addresses the real DLL would pass in RCX on the arm leaf.
 * Values are arbitrary; the library only stores them for SIGSYS routing. */
#define FAKE_HANDLER      0x0000000140001000ULL
#define FAKE_FULL_HANDLER 0x0000000140002000ULL
#define FAKE_SYSTEM_ID    0x00000042u
#define FAKE_FULL_ID      0x00000043u

struct cpuid_regs {
    uint32_t eax, ebx, ecx, edx;
};

/*
 * CPUID with an explicit 64-bit RCX value.
 * The library reads the full RCX on the ARM leaf (TargetSysHandler);
 * for the 32-bit ID leaves only the low half matters.
 */
static inline void do_cpuid_rcx(uint32_t leaf, uint64_t rcx_value, struct cpuid_regs *out)
{
    uint32_t eax, ebx, ecx, edx;

    __asm__ volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(leaf), "c"(rcx_value)
        : "memory"
    );

    out->eax = eax;
    out->ebx = ebx;
    out->ecx = ecx;
    out->edx = edx;
}

static inline void do_cpuid(uint32_t leaf, uint32_t subleaf, struct cpuid_regs *out)
{
    do_cpuid_rcx(leaf, (uint64_t)subleaf, out);
}

/* Action leaf: library zeros EAX–EDX on success. RCX carries handler / id. */
static inline void issue_action(uint32_t leaf, uint64_t rcx_value, const char *label)
{
    struct cpuid_regs r;
    do_cpuid_rcx(leaf, rcx_value, &r);
    printf("  %-28s leaf=0x%08x  ->  eax=%08x ebx=%08x ecx=%08x edx=%08x\n",
           label, leaf, r.eax, r.ebx, r.ecx, r.edx);
}

static inline void issue_static(uint32_t leaf, const char *label)
{
    struct cpuid_regs r;
    do_cpuid(leaf, 0, &r);
    printf("  %-28s leaf=0x%08x  ->  eax=%08x ebx=%08x ecx=%08x edx=%08x\n",
           label, leaf, r.eax, r.ebx, r.ecx, r.edx);
}

static inline void banner(const char *name)
{
    printf("\n=== linuwux test: %s ===\n", name);
    fflush(stdout);
}

#endif /* LINUWUX_TEST_COMMON_H */
