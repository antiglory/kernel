#ifndef SPINLOCK_H
#define SPINLOCK_H

typedef struct
{
    int locked;
} spinlock_t;

static inline void spin_lock(spinlock_t* l)
{
    while (__sync_lock_test_and_set(&l->locked, 1))
    {
        asm volatile("pause");
    }
}

static inline void spin_unlock(spinlock_t* l)
{
    __sync_lock_release(&l->locked);
}

#endif
