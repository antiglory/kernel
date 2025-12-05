#ifndef WRAPPER_H
#define WRAPPER_H

TEXT  ALIGNED void halt(void) __attribute__((noreturn));

#define jmp(addr)                   \
    asm volatile                    \
    (                               \
        "jmp *%0\n\t"               \
        :                           \
        : "r" ((void*)addr)         \
        :                           \
    )                               \

inline void cli(void) { asm volatile("cli" ::: "memory"); }
inline void sti(void) { asm volatile("sti" ::: "memory"); }

inline void halt(void)
{
    asm volatile
    (
        "cli\n\t"
        "hlt\n\t"
    );

    __builtin_unreachable();
}

inline void outb(uint16_t port, uint8_t value)
{
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#endif
