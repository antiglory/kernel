# kernel
a basic kernel made to x86

![Kernel Main](assets/image.png)

# Boot
# Prekernel
- https://chatgpt.com/c/69273a2d-ff88-8332-8ac7-70296ca9ac70
# Kernel
## PML4
- 4 KiB pages (PTE) for each section (.text, .rodata, .data, .bss etc).
## ISRs
### keyboard (ps1)
- minimal structure
- fill keyboard buffer with scancodes
- spinlock or disable IRQ while writing to the buffer (avoiding race condition)
## VGA
- `0xb8000` as always
- due to VA == PA -> VGA is virtually mapped to `~0xffffffff000b8000`
## Devices
### TTY
- (console driver) vga_putc() e vga_popc() -> who actually handles with the screen
- (keyboard driver) consumes the scancodes from the keyboard buffer populated by the ISR
- ldisc_input applies terminal methods (echo, canonical/non-canonical, backspace, etc) and handles with ASCII
- holds the input buffer that will be consumed by stdin (when tty->line_ready == true)
### stdin
- fd 0
- bufferized -> ring buffer
- free for read(0)
- is connected to TTY -> sleeps and reads the input buffer when it is woken up
### stdout
- fd 1
- bufferized -> ring buffer
