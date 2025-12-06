#ifndef STRING_H
#define STRING_H

void* memcpy(void* dest, const void* src, size_t n)
{
          unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    
    if (d > s && d < s + n)
    {
        d += n;
        s += n;

        while (n--)
        {
            *--d = *--s;
        }
    } 
    else
    {
        while (n--)
        {
            *d++ = *s++;
        }
    }
    
    return dest;
}

void* memset(void* s, int32_t c, size_t n)
{
    unsigned char* p = (unsigned char*)s;

    while (n--)
    {
        *p++ = (unsigned char)c;
    }
    
    return s;
}

int32_t strcmp(const unsigned char* str1, const unsigned char* str2)
{
    while (*str1 && (*str1 == *str2))
    {
        str1++;
        str2++;
    }

    return (unsigned char)(*str1) - (unsigned char)(*str2);
}

char* strncpy(char* dest, const char* src, size_t n)
{
    size_t i;
    char* dest_start = dest;

    for (i = 0; i < n && src[i] != '\0'; i++)
    {
        dest[i] = src[i];
    }

    for (; i < n; i++)
    {
        dest[i] = '\0';
    }

    return dest_start;
}

size_t strlen(const char* str)
{
    size_t len = 0;

    while (*str != '\0')
    {
        str++;
        len++;
    }

    return len;
}

#endif
