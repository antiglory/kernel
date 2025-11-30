#ifndef IO_H
#define IO_H

void speaker_on(uint32_t freq)
{
    // PIT base clock: 1.193182 MHz
    uint32_t divisor = 1193182 / freq;

    // PIT -> channel 2 -> square wave mode (0xB6)
    outb(0x43, 0xB6);

    // setup divisor (LSB -> MSB)
    outb(0x42, divisor & 0xFF);
    outb(0x42, (divisor >> 8) & 0xFF);

    // set bits 0 e 1 from 0x61 -> turn on
    unsigned char val = inb(0x61);
    if ((val & 0x03) != 0x03)
        outb(0x61, val | 0x03);
}

void speaker_off()
{
    unsigned char val = inb(0x61) & 0xFC;  // clean bits 0 e 1
    outb(0x61, val);
}

void delay()
{
    for (volatile unsigned long i = 0; i < 500000UL; i++);
}

void beep()
{
    speaker_on(1000);  // ~1 khz
    delay();
    speaker_off();
}

#endif
