#ifndef PIC_H
#define PIC_H

void init_pic(void)
{
    outb(0x20, 0x11); // ICW1
    outb(0xA0, 0x11);

    outb(0x21, 0x20); // ICW2
    outb(0xA1, 0x28);

    outb(0x21, 0x04); // ICW3
    outb(0xA1, 0x02);

    outb(0x21, 0x01); // ICW4
    outb(0xA1, 0x01);

    // kprintf("[ BOOT ] initialized PIC\n");
}

#endif
