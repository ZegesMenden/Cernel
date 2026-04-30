#include "kinternal.h"
#include "kthreads.h"
#include "kmemory.h"
#include "klogging.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include "pico/stdio.h"

extern void kernel_memory_init_internal();
extern void kernel_threading_init_internal();

static const char* kernel_exception_name(uint32_t cause_code) {
    switch (cause_code) {
        case 0: return "instruction address misaligned";
        case 1: return "instruction access fault";
        case 2: return "illegal instruction";
        case 3: return "breakpoint";
        case 4: return "load address misaligned";
        case 5: return "load access fault";
        case 6: return "store/AMO address misaligned";
        case 7: return "store/AMO access fault";
        case 8: return "environment call from U-mode";
        case 9: return "environment call from S-mode";
        case 11: return "environment call from M-mode";
        case 12: return "instruction page fault";
        case 13: return "load page fault";
        case 15: return "store/AMO page fault";
        default: return "unknown exception";
    }
}

static void kernel_report_fault(const char* tag, const char* heading, const char* detail) {
    printf("\n*** %s ***\n", heading);
    if (detail != NULL && detail[0] != '\0') {
        printf("%s: %s\n", tag, detail);
    }
    fflush(stdout);
    stdio_flush();
}

static uint32_t kernel_read_mcause(void) {
    uint32_t value;
    __asm__ volatile ("csrr %0, mcause" : "=r" (value));
    return value;
}

static uint32_t kernel_read_mepc(void) {
    uint32_t value;
    __asm__ volatile ("csrr %0, mepc" : "=r" (value));
    return value;
}

static uint32_t kernel_read_mtval(void) {
    uint32_t value;
    __asm__ volatile ("csrr %0, mtval" : "=r" (value));
    return value;
}

static uint32_t kernel_read_mstatus(void) {
    uint32_t value;
    __asm__ volatile ("csrr %0, mstatus" : "=r" (value));
    return value;
}

void __attribute__((noreturn)) kernel_panic_report(const char* fmt, ...) {
    printf("\n*** KERNEL PANIC ***\n");

    if (fmt != NULL && fmt[0] != '\0') {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        printf("\n");
    }

    fflush(stdout);
    stdio_flush();
    while (1) { ; }
}

void __attribute__((used, noreturn, section(".time_critical.isr_riscv_machine_exception"))) isr_riscv_machine_exception(void) {
    uint32_t mcause = kernel_read_mcause();
    uint32_t mepc = kernel_read_mepc();
    uint32_t mtval = kernel_read_mtval();
    uint32_t mstatus = kernel_read_mstatus();
    uint32_t cause_code = mcause & 0x7fffffffU;
    const char* cause_name = kernel_exception_name(cause_code);

    printf("\n*** MACHINE EXCEPTION ***\n");
    printf("cause=%s (mcause=0x%08lx)\n", cause_name, (unsigned long)mcause);
    printf("mepc=0x%08lx mtval=0x%08lx mstatus=0x%08lx\n",
           (unsigned long)mepc,
           (unsigned long)mtval,
           (unsigned long)mstatus);
    fflush(stdout);
    stdio_flush();

    while (1) { ; }
}

void kernel_init_internal() {
    kernel_memory_init_internal();
    kernel_threading_init_internal();    
}
