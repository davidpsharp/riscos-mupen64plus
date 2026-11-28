/* Unaligned loads and crash reports on RISC OS.
 *
 * RISC OS 5 on a Pi runs with alignment faults on (SCTLR.A=1), RoMac's emulated CPU without, and
 * code written for Linux reads packed data with unaligned LDR/STR. A fault while ARMEABISupport's
 * abort handler is in the path kills the program with "Stack frame has gone out of bounds" and
 * leaves every later UnixLib program dying with "recursion in the abort handler" until a restart.
 * As riscos-half-life, riscos-pcem and ra2-recomp do (riscos-kb lesson
 * 2026-11-27-romac-does-not-trap-unaligned-access), OS_MMUControl 0 clears the bit while the
 * emulator runs and puts it back at exit and before a fatal signal ends the program. LDRD/STRD,
 * LDM/STM and VLDR/VSTR still need alignment. *Set Mupen64Plus$StrictAlign 1 leaves the bit alone.
 *
 * A fatal signal also prints the registers: UnixLib's own report has none when the abort comes
 * through ARMEABISupport. UnixLib passes a struct sigcontext, not a ucontext_t.
 *
 * RISC OS port of Mupen64Plus. Copyright (C) 2026 David Sharp. GPL-2.0. */
#ifdef __riscos__
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <swis.h>

#define OS_MMUControl 0x6B
#define SCTLR_A 2u

static int align_changed;

static void restore_alignment(void)
{
    if (align_changed)
    {
        align_changed = 0;
        _swix(OS_MMUControl, _INR(0, 2), 0, SCTLR_A, ~0u);   /* new = (old AND R2) EOR R1 */
    }
}

static void crash_signal(int sig, siginfo_t *info, void *context)
{
    const struct sigcontext *sc = context;

    restore_alignment();
    if (sc)
        fprintf(stderr, "Signal %d at pc %08lx (lr %08lx, sp %08lx, cpsr %08lx, fault %08lx, "
                "r0 %08lx r1 %08lx r2 %08lx r3 %08lx)\n", sig, sc->arm_pc, sc->arm_lr, sc->arm_sp,
                sc->arm_cpsr, sc->fault_address, sc->arm_r0, sc->arm_r1, sc->arm_r2, sc->arm_r3);
    else
        fprintf(stderr, "Signal %d, si_addr %p\n", sig, info ? info->si_addr : NULL);
    fflush(stderr);
    signal(sig, SIG_DFL);
    raise(sig);
}

__attribute__((constructor)) static void riscos_align_init(void)
{
    struct sigaction sa = { 0 };
    unsigned old = 0;
    const char *strict = getenv("Mupen64Plus$StrictAlign");

    if (!(strict && *strict == '1') &&
        !_swix(OS_MMUControl, _INR(0, 2) | _OUT(1), 0, 0, ~SCTLR_A, &old))
    {
        align_changed = (old & SCTLR_A) != 0;
        atexit(restore_alignment);
    }

    sa.sa_sigaction = crash_signal;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGEMT, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
}
#endif
