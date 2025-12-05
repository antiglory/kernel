#ifndef IDT_H
#define IDT_H

__attribute__((noreturn)) static void gp_isr(void)
{
    kprintf("#GP got caught\n");

    panic();
}

__attribute__((noreturn)) static void pf_isr(void)
{
    kprintf("#PF got caught\n");

    panic();
}

__attribute__((noreturn)) static void df_isr(void)
{
    kprintf("#DF got caught\n");

    panic();
}

#define eoi_out()                   \
    asm volatile                    \
    (                               \
        "movb $0x20, %%al\n\t"      \
        "outb %%al, $0x20\n\t"      \
        : : : "al"                  \
    )                               \

static struct irq_frame
{
    uint64_t rip;
    uint64_t cs;
    uint64_t flags;
    uint64_t rsp;
    uint64_t ss;
};

__attribute__((interrupt)) static void irq0_isr(struct irq_frame* instance)
{
    cpu_ticks++;
    eoi_out();
}

// pressionar a tecla -> sinal elétrico pro controlador -> aciona PIC escravo -> aciona PIC mestre e trigga IRQ1 -> executa a ISR -> le o scancode na porta 0x60 -> transformar em caractere pela ascii -> interpreta e guarda o resultado no input buffer -> read() lê do buffer -> se tty.echo == true -> aparece na tela
__attribute__((interrupt)) static void irq1_isr(struct irq_frame* instance)
{
    uint8_t al;

    asm volatile("inb $0x60, %0" : "=a"(al));

    if (kb_queue.count < 256)
    {
        kb_queue.buffer[kb_queue.head] = al;
        kb_queue.head = (kb_queue.head + 1) % 256;
        kb_queue.count++;
        thread_wake_one(&kb_thread);
    }

    eoi_out();
}

__attribute__((interrupt)) static void beep_isr(struct irq_frame* instance)
{
    beep(); // speaker routine

    // se não for IRQ físico, não manda EOI
    // se for IRQ mapeado do PIC, manda EOI:
    eoi_out();
}

static struct idt_entry
{
    uint16_t base_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  flags;
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_ptr
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

static void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags)
{
    idt[num].base_low  = base & 0xFFFF;
    idt[num].base_mid  = (base >> 16) & 0xFFFF;
    idt[num].base_high = (base >> 32) & 0xFFFFFFFF;
    idt[num].selector  = sel;
    idt[num].ist       = 0x0;
    idt[num].flags     = flags;
    idt[num].reserved  = 0x0;
}

void init_idt(void)
{
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base  = (uint64_t)&idt;

    // clean
    for (int32_t i = 0; i < 256; i++)
        idt_set_gate(i, 0x0, 0x0, 0x0);

    // 0x08 = GDT64 code selector
    // 0x8E - present | ring 0 | interrupt gate

    // faults
    idt_set_gate(8,  (uintptr_t)df_isr, GDT64_CODE_PTR, 0x8E);
    idt_set_gate(13, (uintptr_t)gp_isr, GDT64_CODE_PTR, 0x8E);
    idt_set_gate(14, (uintptr_t)pf_isr, GDT64_CODE_PTR, 0x8E);

    // IRQs
    idt_set_gate(32, (uintptr_t)irq0_isr, GDT64_CODE_PTR, 0x8E);
    idt_set_gate(33, (uintptr_t)irq1_isr, GDT64_CODE_PTR, 0x8E);

    // software interrupts
    idt_set_gate(0xF0 /* 240 */, (uintptr_t)beep_isr, GDT64_CODE_PTR, 0x8E);

    asm volatile
    (
        "lidt %0\n\t"
        :
        : "m"(idtp)
    );

    return;
}

#endif
