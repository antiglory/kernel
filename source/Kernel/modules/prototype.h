#ifndef PROTOTYPE_H
#define PROTOTYPE_H

// init.h
TEXT  ALIGNED void init(void);
TEXT  ALIGNED void init_idt(void);
TEXT  ALIGNED void init_pic(void);
TEXT  ALIGNED void init_pit(void);

// self
BTEXT ALIGNED void kstart() __attribute__((noreturn, naked));
TEXT  ALIGNED void main(void);

// ../../Linker/kernel.ld

#endif
