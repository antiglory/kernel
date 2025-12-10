#ifndef KLIB_H
#define KLIB_H

static void print_string(const char* s)
{
    for (int j = 0; s[j] != '\0'; j++)
    {
        kputc(s[j]);
    }
}

// kernel print formatted
void kprintf(const unsigned char* str, ...)
{
    va_list args;
    va_start(args, str);

    for (int32_t i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == '\n')
            kputc('\n');
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
                kputc(c);
            }
            else if (format == 's') // string
            {
                const char* s = va_arg(args, const char*);
                print_string(s);
            }
            else if (format == 'd') // signed decimal
            {
                int val = va_arg(args, int);

                char buf[16];
                int idx = 0;

                if (val == 0)
                    buf[idx++] = '0';
                else
                {
                    if (val < 0)
                    {
                        kputc('-');
                        val = -val;
                    }

                    char tmp[16];
                    int t = 0;

                    while (val > 0)
                    {
                        tmp[t++] = '0' + (val % 10);
                        val /= 10;
                    }

                    while (t--)
                        buf[idx++] = tmp[t];
                }

                buf[idx] = '\0';
                print_string(buf);
            }
        }
        else
            kputc(str[i]);
    }

    va_end(args);
}

#endif
