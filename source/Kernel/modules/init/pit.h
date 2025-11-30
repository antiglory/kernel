#ifndef PIT_H
#define PIT_H

#define PIT_HZ             100 // 100 Hz = 10 ms per tick
#define PIT_BASE_FREQUENCY 1193182

void init_pit(void)
{
    uint16_t divisor = PIT_BASE_FREQUENCY / PIT_HZ;

    outb(0x43, 0x36);               // mode 3, lobyte/hibyte, channel 0
    outb(0x40, divisor & 0xFF);     // low byte
    outb(0x40, divisor >> 8);       // high byte

    // kprintf("[ BOOT ] initialized PIT\n");
}

#endif
