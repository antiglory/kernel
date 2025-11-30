#ifndef TTY_H
#define TTY_H

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

#define INPUT_BUFF_SIZE VGA_HEIGHT * VGA_WIDTH
#define KEYBOARD_BUFF_SIZE 32

unsigned char keyboard_buffer[KEYBOARD_BUFF_SIZE] = {0};

struct tty
{
             uint32_t  id;                 // tty0, tty1...
    volatile uint16_t* vga;

    bool echo;                   // default -> always true 
    bool canonical;              // COOKED mode (line) or RAW mode
    bool enabled;

    unsigned char input[INPUT_BUFF_SIZE]; // input buffer
    size_t input_len;                     // current char at input buffer
    // struct ringbuf input;              // chars received from keyboard
    // struct ringbuf output;             // chars waiting for output

    bool line_ready;

    // pointers to console drivers
    void (*vga_write)(const unsigned char c, const unsigned short ref);
    void (*vga_erase)();
    void (*flush_output)(void);

    uint16_t cursor_x;
    uint16_t cursor_y;
    // uint8_t  color_fg;
    // uint8_t  color_bg;

    void (*ldisc_input)(const unsigned char c);    // line discipline for input
    // void (*ldisc_process)(struct tty *t);       // if i implement pipeline

    // struct task *reader_waiting;
    // struct task *writer_waiting;

    // spinlock_t lock; // critical

    // void *driver_data;   
};
struct tty tty0;

void clear_input_buffer(void)
{
    memset(tty0.input, 0, INPUT_BUFF_SIZE);
}

// console drivers
void vga_scroll(int32_t lines)
{
    if (lines <= 0)
        return;

    for (int i = 0; i < (VGA_HEIGHT - lines) * VGA_WIDTH; i++)
    {
        tty0.vga[i] = tty0.vga[i + lines * VGA_WIDTH];
    }

    for (int i = (VGA_HEIGHT - lines) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++)
    {
        tty0.vga[i] = (short)0x0720;
    }
}

void vga_pushc(const unsigned char c, unsigned short ref)
{
    if (ref == 0)
        ref = 0x0F00;

    int32_t position = tty0.cursor_y * VGA_WIDTH + tty0.cursor_x;
    
    if (tty0.cursor_y >= VGA_HEIGHT)
        tty0.cursor_y = VGA_HEIGHT - 1;

    if (tty0.cursor_x >= VGA_WIDTH)
        tty0.cursor_x = VGA_WIDTH - 1;
  
    if (c == '\n')
    {
        tty0.cursor_x = 0;
        tty0.cursor_y++;
        goto _end;
    }

    tty0.vga[position] = (c & 0x00FF) | (ref & 0xFF00);
    tty0.cursor_x++;
  
    if (tty0.cursor_x >= VGA_WIDTH)
    {
        tty0.cursor_x = 0;
        tty0.cursor_y++;
    }

_end:
    return;
}

void vga_popc(void)
{
    tty0.cursor_x--;
    int32_t position = tty0.cursor_y * VGA_WIDTH + tty0.cursor_x;
    
    tty0.vga[position] = (' ' | 0x0F00);
}

void vga_clear(void)
{
    for (int i = 0; i < VGA_HEIGHT * VGA_WIDTH; i++)
    {
        tty0.vga[i] = (short)0x0720;
    }

    tty0.cursor_x = 0;
    tty0.cursor_y = 0;
}

// keyboard
void ldisc_input(const unsigned char c)
{
    size_t len = 0;

    while (len < INPUT_BUFF_SIZE && tty0.input[len] != '\0')
    {
        len++;
    }

    if (len + 1 < INPUT_BUFF_SIZE)
    {
        if (c == '\n')
        {
            tty0.input[len] = '\n';
            tty0.line_ready = true;
           
            memset(tty0.input, 0, INPUT_BUFF_SIZE);
            
            if (tty0.echo == true)
                vga_pushc(c, 0);
        }
        else if (c == 0x08) // backspace
        {
            if (len > 0 && len - 1 >= 0)
            {
                tty0.input[len - 1] = 0x0;
                tty0.input[len] = '\0';
               
                if (tty0.echo == true)
                    vga_popc();
            }
        }
        else
        {
            tty0.input[len] = c;
            tty0.input[len + 1] = '\0';

            if (tty0.echo == true)
                vga_pushc(c, 0);
        }
    }
}

void keyboard_driver(const uint8_t al)
{  
    if (al & 0x80) // ignore key release
        return;
    else if (al == 0x1C) // return (enter key)
    {
        ldisc_input('\n');
        goto _end;
    }
  
    static const char ascii[] =
    {
        0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
        '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
        'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
        'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' '
    };

    if (al < sizeof(ascii))
    {
        char c = ascii[al];

        if (c != 0 && c >= ' ' && c <= '~' || c == '\b' /* 0x08 */)
            ldisc_input(c);
    }

_end:
    return;
}

static void print_string(const char* s)
{
    for (int j = 0; s[j] != '\0'; j++)
    {
        vga_pushc(s[j], 0);
    }
}

// old ----

// kernel print formatted (almost)
// i will change vga drivers usage to FDs functions like write()
void kprintf(const unsigned char* str, ...)
{
    va_list args;
    va_start(args, str);

    for (int32_t i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == '\n')
        {
            vga_pushc('\n', 0);
        }
        else if (str[i] == '%')
        {
            i++;

            unsigned char format = str[i];
            
            if (format == 'p') // pointer
            {
                uintptr_t ptr_val = va_arg(args, uintptr_t);
                
                unsigned char hex_str[19];
                hex_str[0] = '0';
                hex_str[1] = 'x';
                
                for (int j = 0; j < 16; j++)
                {
                    int32_t nibble = (ptr_val >> (60 - j * 4)) & 0xF;

                    if (nibble < 10)
                        hex_str[j + 2] = '0' + nibble;
                    else
                        hex_str[j + 2] = 'a' + (nibble - 10);
                }

                hex_str[18] = '\0';

                print_string(hex_str);
            }
            else if (format == 'x') // hex
            {
                unsigned int int_val = va_arg(args, unsigned int);
                unsigned char hex_str[9];
                
                for (int j = 0; j < 8; j++)
                {
                    int32_t nibble = (int_val >> (28 - j * 4)) & 0xF;

                    if (nibble < 10)
                        hex_str[j] = '0' + nibble;
                    else
                        hex_str[j] = 'a' + (nibble - 10);
                }

                hex_str[8] = '\0';

                print_string(hex_str);
            }
            else if (format == 'c') // char
            {
                unsigned char c = (unsigned char)va_arg(args, int);
                vga_pushc(c, 0);
            }
            else if (format == 's') // string
            {
                const char* s = va_arg(args, const char*);
                print_string(s);
            }
        }
        else
        {
            vga_pushc(str[i], 0);
        }
    }
    
    va_end(args);
}

#endif
