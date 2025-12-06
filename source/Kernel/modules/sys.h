#ifndef SYS_H
#define SYS_H

ssize_t read(int fd, void* dest, size_t size)
{
    if (fd < 0)
        return -1;

    /* stdin -> tty0 */
    if (fd == 0)
    {
        struct tty* t = &tty0;

        while (!t->line_ready)
            thread_sleep(&t->read_wq);

        ssize_t n = strlen(t->input);

        memcpy(dest, t->input, n);

        if (n > 0 && ((char*)dest)[n - 1] == '\n')
            ((char*)dest)[n - 1] = '\0';

        t->line_ready = false;
        memset(t->input, 0, n);
       
        return (ssize_t)n;
    }

    // não-TTY: despacha para file_ops
    /*
    struct file* f = fd_lookup(fd);
    if (!f || !f->fops || !f->fops->read)
        return -1;

    return f->fops->read(f, dest, size);
    */
}

int32_t write(int fd, void* src, size_t size)
{
    if (fd < 0)
        return -1;

    /* stdout/stderr -> VGA */
    if (fd == 1 || fd == 2)
    {
        uint8_t* p = (uint8_t*)src;

        for (size_t i = 0; i < size; i++)
            vga_pushc(p[i], 0);

        return (int32_t)size;
    }

    /* stdin não recebe write */
    if (fd == 0)
        return -1;

    /* não-TTY: file_ops */
    /*
    struct file* f = fd_lookup(fd);
    if (!f || !f->fops || !f->fops->write)
        return -1;

    return f->fops->write(f, src, size);
    */
}

#endif
