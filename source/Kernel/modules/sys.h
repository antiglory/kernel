#ifndef SYS_H
#define SYS_H

#define MAX_FDS 512

static struct file* fd_table[MAX_FDS];

static ssize_t tty_read(struct file* f, void* buf, size_t size)
{
    struct tty* t = f->private_data;

    while (!t->line_ready)
    {
        thread_sleep(&t->read_wq);
    }

    ssize_t n = strlen(t->input);

    memcpy(buf, t->input, n);

    if (n > 0 && ((char*)buf)[n - 1] == '\n')
        ((char*)buf)[n - 1] = '\0';

    t->line_ready = false;
    memset(t->input, 0, n);

    return n;
}

static ssize_t tty_write(struct file* f, const void* buf, size_t size)
{
    const uint8_t* p = buf;

    for (size_t i = 0; i < size; i++)
    {
        vga_pushc(p[i], 0);
    }

    return size;
}

void init_fs(void)
{
    memset(fd_table, 0, sizeof(fd_table));

    // stdin, stdout -> (points to) tty (console)
    struct file* stdin_file = kmalloc(sizeof(struct file));
    struct fops_t* stdin_fops = kmalloc(sizeof(struct fops_t));

    stdin_file->fd = 0;
    stdin_file->offset = 0;
    stdin_file->ref_count = 1;
    stdin_file->private_data = &tty0;
    stdin_file->fops = stdin_fops;

    stdin_fops->read = tty_read;
    stdin_fops->write = NULL;

    fd_table[0] = stdin_file;

    struct file* stdout_file = kmalloc(sizeof(struct file));
    struct fops_t* stdout_fops = kmalloc(sizeof(struct fops_t));

    stdout_file->fd = 1;
    stdout_file->offset = 0;
    stdout_file->ref_count = 1;
    stdout_file->private_data = &tty0;
    stdout_file->fops = stdout_fops;

    stdout_fops->read = NULL;
    stdout_fops->write = tty_write;

    fd_table[1] = stdout_file;
}

struct file* fd_lookup(int fd)
{
    if (fd < 0 || fd >= MAX_FDS)
        return NULL;

    return fd_table[fd];
}

ssize_t read(int fd, void* dest, size_t size)
{
    struct file* f = fd_lookup(fd);
    if (!f || !f->fops || !f->fops->read)
        return -1;

    return f->fops->read(f, dest, size);
}

ssize_t write(int fd, const void* src, size_t size)
{
    struct file* f = fd_lookup(fd);
    if (!f || !f->fops || !f->fops->write)
        return -1;

    return f->fops->write(f, src, size);
}

#endif
