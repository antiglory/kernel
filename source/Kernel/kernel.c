// 0xffffffff80000000 - 0xffffffff80200000 [pt_ffffff8000000]
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>

#include "modules/macro.h"
#include "modules/prototype.h"

#define PAGE_SIZE 4096ULL

// self
volatile uint64_t cpu_ticks = 0;

extern char _kernel_text_start[];
extern char _kernel_text_end[];
extern char _kernel_rodata_start[];
extern char _kernel_rodata_end[];
extern char _kernel_data_start[];
extern char _kernel_data_end[];
extern char _kernel_bss_start[];
extern char _kernel_bss_end[];
extern char _kernel_stack_start[];
extern char _kernel_stack_end[];
extern char _kernel_heap_start[];
extern char _kernel_heap_end[];
extern char _kernel_vma[];
extern char _kernel_lma[];
extern char _kernel_vo[];

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

#include "modules/wrapper.h"
#include "modules/io.h"
#include "modules/string.h"
#include "modules/tty.h"

__attribute__((noreturn)) void panic(void)
{
    asm volatile("cli\n\t");

    kprintf("kernel panic!\n");

    halt();
}

#include "modules/init.h"
// #include "modules/alloc.h"
// #include "modules/loader.h"

void sleep(uint64_t ms)
{
    uint64_t target = cpu_ticks + ms / 10;

    while (cpu_ticks < target)
    {
        asm volatile("hlt");
    }
}

// .text -> 0xffffffff80101000
void main(void)
{
    asm volatile("int $0xF0"); // beep

    kprintf("hello kernel!\n");
  
    while (1);

    /*
    char str[32];
    while (1)
    {
	      kprintf("> ");
	      gets(str);
    
        if (strcmp(str, "ping") == 0) kprintf("pong\n");
        else if (strcmp(str, "halt") == 0) halt();
    }
    */
}
