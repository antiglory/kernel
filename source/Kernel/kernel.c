/* subsystems:
 *    - threading (round-robin scheduler)
 *    - tty (console)
 *    - ramfs (WIP)
 *    - interrupts, IRQ
 *    - timer
 */

// 0xffffffff80000000 - 0xffffffff80200000
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>

#include "modules/macro.h"
#include "modules/prototype.h"

#ifndef PAGE_SIZE
    #define PAGE_SIZE 4096ULL
#endif

// self
volatile uint64_t cpu_ticks = 0;

extern uint8_t _kernel_text_start[];
extern uint8_t _kernel_text_end[];
extern uint8_t _kernel_rodata_start[];
extern uint8_t _kernel_rodata_end[];
extern uint8_t _kernel_data_start[];
extern uint8_t _kernel_data_end[];
extern uint8_t _kernel_bss_start[];
extern uint8_t _kernel_bss_end[];
extern uint8_t _kernel_stack_start[];
extern uint8_t _kernel_stack_end[];
extern uint8_t _kernel_heap_start[];
extern uint8_t _kernel_heap_end[];
extern uint8_t _kernel_vma[];
extern uint8_t _kernel_lma[];
extern uint8_t _kernel_vo[];

// kernel entrypoint
void kstart(void)
{
    asm volatile("nop; nop; nop; nop");

    asm volatile(
        "mov %0, %%rax\n\t"
        "and $-16, %%rax\n\t"
        "sub $8, %%rax\n\t"
        "mov %%rax, %%rsp\n\t"
        "mov %%rsp, %%rbp\n\t"
        :
        : "r"(_kernel_stack_end)
        : "rax", "memory"
    );

    asm volatile(
        "movabs %[target], %%rax\n\t"
        "jmpq *%%rax\n\t"
        :
        : [target] "i"(init)
        : "rax"
    );

    __builtin_unreachable();
}

void kprintf(const unsigned char* str, ...);

#include "modules/wrapper.h"
#include "modules/io.h"
#include "modules/string.h"
#include "modules/alloc.h"

__attribute__((noreturn)) void panic(void)
{
    cli();
    kprintf("kernel panic!\n");
    halt();
}

#include "modules/threads.h"
#include "modules/tty.h"
#include "modules/sys.h"
#include "modules/init.h"

void sleep(uint64_t ms)
{
    uint64_t target = cpu_ticks + ms / 10;
    
    sti();
    while (cpu_ticks < target)
    {
        asm volatile("hlt");
    }
}

// .text -> 0xffffffff80100100
void main(void)
{
    asm volatile("int $0xF0"); // beep
    kprintf("\nv0 is alive!\n");

    // dump_runqueue();

    /*
    for (;;)
    {
        // dump_runqueue();
        kthread_yield();
    }
    */

    char str[32];
    while (1)
    {
	      kprintf("> ");
        read(0, str, sizeof(str));

        if (strcmp(str, "ping") == 0)
            kprintf("pong\n");
        else if (strcmp(str, "halt") == 0)
            halt();

        // kthread_yield();
    }
}
