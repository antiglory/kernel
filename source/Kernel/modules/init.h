#ifndef INIT_H
#define INIT_H

// #include "init/map.h"
#include "init/idt.h"
#include "init/pic.h"
#include "init/pit.h"

static inline uint8_t test_access(const void* addr)
{
    uint8_t val;
    asm volatile(
        "movb (%1), %0"
        : "=r"(val)
        : "r"(addr)
        : "memory"
    );
    return val;
}

void init_stub()
{
    sti();
    kprintf("kthread: main OK\n");
    main();
}

void init(void)
{ 
    init_pic();
    init_pit();
    init_idt();  

    asm volatile("sti\n\t");

    tty0.id = 0;
    tty0.vga = (uint16_t*)((uint64_t)(_kernel_vo + 0xB8000)); // ~0xffffffff000b8000
    tty0.echo = true;
    tty0.canonical = true;
    tty0.enabled = true;
    tty0.input_len = 0;
    tty0.line_ready = false;
    tty0.vga_write = &vga_pushc;
    tty0.vga_erase = &vga_popc;
    tty0.vga_flush = &vga_clear;
    tty0.ldisc_input = &ldisc_input;
    tty0.cursor_x = 0;
    tty0.cursor_y = 0;
    tty0.vga_flush();
    kprintf("system: tty0 OK\nsystem: idt OK\nsystem: pic OK\nsystem: pit OK\n");

    kprintf("memory test...\n");

    test_access(_kernel_text_start);
    kprintf("  .text: %p OK\n", _kernel_text_start);

    test_access(_kernel_rodata_start);
    kprintf("  .rodata: %p OK\n", _kernel_rodata_start);  

    test_access(_kernel_data_start);
    kprintf("  .data: %p OK\n", _kernel_data_start);

    test_access(_kernel_bss_start);
    kprintf("  .bss: %p OK\n", _kernel_bss_start);

    test_access(_kernel_stack_start);
    kprintf("  .stack: %p OK\n", _kernel_stack_start);

    test_access(_kernel_heap_start);
    kprintf("  .heap: %p OK\n", _kernel_heap_start);

    kprintf("stack test...\n");
    
    void* rsp;
    void* rbp;  
    asm volatile ("movq %%rsp, %0" : "=r" (rsp) : :);
    asm volatile ("movq %%rbp, %0" : "=r" (rbp) : :);
  
    if ((bool)((uint64_t)rsp & 0xF) == 0)
       kprintf("  stack top %p OK\n", rsp);
    else
    {
       kprintf("  stack top %p NOT OK\n", rsp);
       panic();
    }
    if ((bool)((uint64_t)rsp & 0xF) == 0)
        kprintf("  stack bottom %p OK\n", rbp);
    else
    {
        kprintf("  stack bottom %p NOT OK\n", rbp);
        panic();
    }

    kbrk_init();
    slab_init();
    kprintf("kmalloc: kbrk OK\nkmalloc: slab OK\n");

    init_fs();
    kprintf("system: ramfs OK\n");

    kthread_subsystem_init();
    kprintf("kthread: subsystem OK\n");

    if (kthread_create(kb_driver, NULL, "kb_driver") == -1)
    {
        kprintf("kthread: kb_driver NOT OK\n");
        panic();
    }

    kprintf("kthread: kb_driver OK\n");

    if (kthread_create(init_stub, NULL, "main") == -1)
    {
        kprintf("kthread: main NOT OK\n");
        panic();
    }

    waitq_init(&kb_thread);
    kb_queue.head = 0;
    kb_queue.tail = 0;
    kb_queue.count = 0;

    kthread_start_scheduler(); // idle() -> ... -> init_stub() -> main()
}

#endif

