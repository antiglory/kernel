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
ssize_t write(int fd, const void* src, size_t size);
ssize_t read(int fd, void* dest, size_t size);

#include "modules/wrapper.h"
// #include "modules/spinlock.h"
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

struct file;
struct fops_t
{
    ssize_t (*read)(struct file* f, void* buf, size_t size);
    ssize_t (*write)(struct file* f, const void* buf, size_t size);
};

struct file
{
    int fd;
    int flags;
    off_t offset;
    int ref_count;
    void* private_data;
    struct fops_t* fops;
};

#include "modules/tty.h"
#include "modules/sys.h"
#include "modules/klib.h"
#include "modules/init.h"

void sleep(uint64_t ms)
{
    uint64_t target = cpu_ticks + ms / 10;
    
    while (cpu_ticks < target)
    {
        safe_halt();
    }
}

#define MAX_TOKENS 8

// mini parser for user input
int32_t tokenize(char* input, char* tokens[], int max_tokens)
{
    int32_t count = 0;

    while (*input == ' ' || *input == '\t')
        input++;

    while (*input && count < max_tokens)
    {
        tokens[count++] = input;

        // anda até próximo espaço ou final
        while (*input && *input != ' ' && *input != '\t')
            input++;

        if (*input)
        {
            *input = '\0';
            input++;
            
            while (*input == ' ' || *input == '\t')
                input++;
        }
    }

    return count;
}

// .text -> 0xffffffff80100100
void main(void)
{
    asm volatile("int $0xF0"); // beep
    kprintf("\nv0 is alive!\n");

    char* str = kmalloc(32 * sizeof(char));

    char** argv = kmalloc(MAX_TOKENS * sizeof(char));
    int* argc = kmalloc(sizeof(int));

    for (;;)
    {
        kprintf("> ");
        read(0, str, sizeof(str));

        *argc = tokenize(str, argv, MAX_TOKENS);
        if (*argc == 0) continue;

        if (strcmp(argv[0], "ping") == 0)
            kprintf("pong\n");
        else if (strcmp(argv[0], "halt") == 0)
            halt();
        else if (strcmp(argv[0], "debug") == 0)
        {
            if (*argc <= 1);
            else
            {
                if (strcmp(argv[1], "runqueue") == 0)
                    dump_runqueue();
                else if (strcmp(argv[1], "testmem") == 0)
                    test_all_access();
            }
        }
    }

    kfree(str);
    kfree(argv);
    kfree(argc);

    // kthread_yield();
}
