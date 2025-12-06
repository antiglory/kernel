#ifndef THREADS_H
#define THREADS_H

/*
 * thread subsystem for homemade kernel
 * - thread creation
 * - cooperative yield scheduler (round-robin)
 * - waitqueue and semaphore primitive
 *
 * this is minimal and i think this is not fully ABI-compliant (stack alignment caveats)
 */

#define MAX_THREADS 64
#define KTHREAD_STACK_SIZE 8192 // TODO portar pra 16384

typedef enum
{
    THREAD_UNUSED = 0,
    THREAD_RUNNABLE,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_ZOMBIE,
} thread_state_t;

struct kthread;

typedef struct waitq
{
    struct kthread *head;
} waitq_t;

typedef struct kthread
{
    struct kthread*        next;
           int             id;
           thread_state_t  state;
           uint8_t*        stack;     // base pointer allocated
           uint64_t*       sp;
           void            (*fn)(void*);
           void*           arg;
           int             exit_code;
           char            name[32];
} kthread_t;

static kthread_t thread_table[MAX_THREADS];
static kthread_t *runqueue_head = NULL; // circular singly-linked list
static kthread_t *current = NULL;
static int next_tid = 1;

/* ABI system V AMD64:
 *   old_sp in RDI
 *   new_sp in RSI
 */
extern void context_switch(uint64_t** old_sp, uint64_t* new_sp);
asm(
    ".globl context_switch\n"
    "context_switch:\n\t"
    "pushq %rbp\n\t"
    "pushq %rbx\n\t"
    "pushq %r12\n\t"
    "pushq %r13\n\t"
    "pushq %r14\n\t"
    "pushq %r15\n\t"

    "movq %rsp, (%rdi)\n\t"  // *old_sp = current sp
    "movq %rsi, %rsp\n\t"    // RSP = new_sp

    "popq %r15\n\t"
    "popq %r14\n\t"
    "popq %r13\n\t"
    "popq %r12\n\t"
    "popq %rbx\n\t"
    "popq %rbp\n\t"

    "retq\n\t"               // return to kthread_entry
);

// TODO -> mais de um argumento
asm(
    ".globl kthread_entry\n"
    "kthread_entry:\n\t"
    "popq %rdi\n\t"        // arg -> RDI (first arg x86_64)
    "popq %rax\n\t"        // fn -> RAX
    "callq *%rax\n\t"      // do fn(arg)
    // placeholder -> thread doesnt return
    "hlt\n\t"              // TODO panic logic
);

extern void kthread_exit(int code);

/* trampoline: when we RET into here, RSP points to 'arg'
 * we pop arg into RDI, pop fn into RSI, then call *RSI (fn) with arg in RDI -> fn(arg);
 * after fn returns we call kthread_exit(0)
 */
void thread_trampoline(void);
asm (
    ".text\n"
    ".global thread_trampoline\n"
    "thread_trampoline:\n"
    "    pop %rdi\n"             // arg is in sp -> RDI
    "    pop %rsi\n"             // fn is the next qword -> RSI
    "    call *%rsi\n"           // do fn(arg)
    "    mov $0, %rdi\n"         // exit code 0 (exit code fits into RDI)
    "    call kthread_exit\n"    // do kthread_exit(0)
    "    hlt\n"                  // TODO panic logic
);

/*
static void dump_thread_sp(kthread_t* t)
{
    if (!t)
    {
        kprintf("dump: t==NULL\n");
        return;
    }
    
    kprintf("context switch counter = %d\n", cs_count);
    kprintf("thread %d name=%s stack=%p sp=%p\n", t->id, t->name, t->stack, (void*)t->sp);
    
    if (t->stack)
    {
        kprintf(" stack range: %p - %p\n", t->stack, (void*)(t->stack + KTHREAD_STACK_SIZE));
        
        if ((uint8_t*)t->sp < t->stack || (uint8_t*)t->sp > t->stack + KTHREAD_STACK_SIZE)
            kprintf(" WARNING: sp outside allocated stack!\n");

        // peek all 8 qwords
        uint64_t* p = t->sp;
        for (int i = 0; i < 8; ++i)
        {
            kprintf("  [sp+%d] = %p\n", i, (void*)p[i]);
        }
    }
}
*/

static inline uint64_t *prepare_stack(
    void (*fn)(void*), void* arg, 
    void* stack_base, size_t stack_size
)
{
    uint64_t* sp = (uint64_t*)((char*)stack_base + stack_size);
   
    // align to 16
    sp = (uint64_t*)((uintptr_t)sp & ~0xFUL);
    
    extern void kthread_entry(void); 
    
    --sp; *sp = (uint64_t)fn;
    --sp; *sp = (uint64_t)arg;
    --sp; *sp = (uint64_t)kthread_entry;

    // callee-saved
    --sp; *sp = 0; // rbp
    --sp; *sp = 0; // rbx
    --sp; *sp = 0; // r12
    --sp; *sp = 0; // r13
    --sp; *sp = 0; // r14
    --sp; *sp = 0; // r15
    
    return sp; // tá retornando o addr que aponta pra essa stack/esse layout definido acima
}

// runqueue helpers (circular singly-linked list)
static void enqueue_runnable(kthread_t* t)
{
    if (!runqueue_head)
        runqueue_head = t->next = t;
    else
    {
        t->next = runqueue_head->next;
        runqueue_head->next = t;
    }
}

static void remove_from_runqueue(kthread_t* t)
{
    if (!runqueue_head) return;
    kthread_t* prev = runqueue_head;
    kthread_t* it = runqueue_head->next;

    if (it == t)
    {
        if (it == runqueue_head)
        {
            runqueue_head = NULL;
            return;
        }
        else
        {
            prev->next = it->next;
            runqueue_head = prev;
            return;
        }
    }

    while (it != runqueue_head)
    {
        if (it == t)
        {
            prev->next = it->next;
            return;
        }
        prev = it; it = it->next;
    }

    if (runqueue_head == t)
    {
        if (runqueue_head->next == runqueue_head)
            runqueue_head = NULL;
        else
        {
            kthread_t* tail = runqueue_head;
            while (tail->next != runqueue_head) tail = tail->next;
            tail->next = runqueue_head->next;
            runqueue_head = runqueue_head->next;
        }
    }
}

// round-robin scheduler
void schedule(void)
{
    cli();

    kthread_t *prev = current;
    
    if (!runqueue_head)
    {
        sti();
        return; 
    }

    kthread_t *next = NULL;

    // thread bloqueou ou morreu
    if (prev->state == THREAD_BLOCKED || prev->state == THREAD_ZOMBIE)
    {
        next = runqueue_head;
    }
    
    else // thread atual só cedeu a CPU pra próxima thread 
    {
        next = prev->next;

        runqueue_head = next;
    }

    if (next == prev && prev->state == THREAD_RUNNING)
    {
        sti();
        return;
    }

    current = next;

    if (prev->state == THREAD_RUNNING)
        prev->state = THREAD_RUNNABLE;
        
    next->state = THREAD_RUNNING;

    // dump_runqueue();

    sti();
    context_switch(&prev->sp, next->sp);

    // thread returns to here ?
}

// gives the CPU to the next thread runnable in queue
void kthread_yield(void)
{
    schedule();
}

int kthread_create(void (*fn)(void*), void* arg, const char* name)
{
    for (int i = 0; i < MAX_THREADS; ++i)
    {
        if (thread_table[i].state == THREAD_UNUSED)
        {
            kthread_t* t = &thread_table[i];
            memset(t, 0, sizeof(*t));
            t->id = next_tid++;
            t->state = THREAD_RUNNABLE;
            
            if (name)
              strncpy(t->name, name, sizeof(t->name)-1);

            t->stack = kmalloc(KTHREAD_STACK_SIZE);
            if (!t->stack)
                return -1;

            t->fn  = fn;
            t->arg = arg;
            t->sp  = prepare_stack(fn, arg, t->stack, KTHREAD_STACK_SIZE);

            cli();
            enqueue_runnable(t);
            sti();
            
            return t->id;
        }
    }

    return -1;
}

void kthread_exit(int code)
{
    cli();
    if (!current)
        // should not happen
        panic();

    current->exit_code = code;
    current->state = THREAD_ZOMBIE;

    remove_from_runqueue(current);

    if (current->stack)
    {
        kfree(current->stack);
        current->stack = NULL;
    }

    if (!runqueue_head)
    {
        sti();
        panic();
    }

    kthread_t* next = runqueue_head->next;
    next->state = THREAD_RUNNING;
    
    kthread_t* old = current;
    current = next;

    uint64_t** old_sp_storage = &old->sp;
    context_switch(old_sp_storage, next->sp);
    
    for(;;)
        asm volatile("hlt");
}

void waitq_init(waitq_t* wq)
{
    wq->head = NULL;
}

void thread_sleep(waitq_t* wq)
{
    cli();

    current->state = THREAD_BLOCKED;

    remove_from_runqueue(current);

    current->next = wq->head;
    wq->head = current;

    // dump_runqueue();

    schedule();
    
    sti();
}

/*
void thread_sleep(waitq_t* wq)
{
    cli();

    current->state = THREAD_BLOCKED;
    current->next = wq->head;
    wq->head = current;
    remove_from_runqueue(current);

    dump_runqueue();
    halt();
    schedule();
    
    sti();
}
*/

void thread_wake_one(waitq_t* wq)
{
    cli();
 
    if (!wq->head)
    {
        sti();
        return;
    }
    
    kthread_t* t = wq->head;
    wq->head = t->next;
    t->next = NULL;
    t->state = THREAD_RUNNABLE;
    enqueue_runnable(t);
  
    sti();
}

void thread_wake_all(waitq_t* wq)
{
    cli();
    
    kthread_t* it = wq->head;
    while (it)
    {
        kthread_t* n = it->next;
        it->next = NULL;
        it->state = THREAD_RUNNABLE;
        enqueue_runnable(it);
        it = n;
    }
    wq->head = NULL;

    sti();
}

// semaphore 
typedef struct
{
    int count;
    waitq_t wq;
} sem_t;

void sem_init(sem_t* s, int initial)
{
    s->count = initial;
    waitq_init(&s->wq);
}

void sem_wait(sem_t* s)
{
    cli();

    s->count--;
    if (s->count < 0)
        thread_sleep(&s->wq);
  
    sti();
}

void sem_post(sem_t* s)
{
    cli();
    s->count++;

    if (s->count <= 0)
        thread_wake_one(&s->wq);

    sti();
}

// idle thread
static void idle_thread_fn(void* arg)
{
    (void)arg; // isnt needed at this point

    for(;;)
    {
        kthread_yield();
    }
}

void kthread_subsystem_init(void)
{
    memset(thread_table, 0, sizeof(thread_table));
    
    for (int i = 0; i < MAX_THREADS; ++i) 
        thread_table[i].state = THREAD_UNUSED;
    
    kthread_create(idle_thread_fn, NULL, "idle");
}

static const char* state_str(thread_state_t st)
{
    switch (st)
    {
        case THREAD_UNUSED:   return "UNUSED";
        case THREAD_RUNNABLE: return "RUNNABLE";
        case THREAD_RUNNING:  return "RUNNING";
        case THREAD_BLOCKED:  return "BLOCKED";
        case THREAD_ZOMBIE:   return "ZOMBIE";
        default:              return "?";
    }
}

void dump_runqueue(void)
{
    cli();

    if (!runqueue_head)
    {
        kprintf("[runqueue] <empty>\n");

        sti();
        return;
    }

    kprintf("[runqueue] head=%p (thread %d)\n", runqueue_head, runqueue_head->id);

    kthread_t* it = runqueue_head;
    int i = 0;

    do
    {
        kprintf(
            "  #%d  t=%p  id=%d  state=%s  sp=%p  stack=%p..%p  name=\"%s\" next=\"%s\" (%p)\n",
            i,
            it,
            it->id,
            state_str(it->state),
            (void*)it->sp,
            it->stack,
            it->stack ? it->stack + KTHREAD_STACK_SIZE : NULL,
            it->name,
            it->next->name,
            it->next
        );
        it = it->next;
        i++;
    }
    while (it && it != runqueue_head);

    sti();
}

__attribute__((noreturn))
void kthread_start_scheduler(void)
{
    if (!runqueue_head)
        panic();

    static uint64_t* saved_sp = NULL;

    // chooses the next thread to be run in queue
    kthread_t* next = runqueue_head;

    current = next;
    next->state = THREAD_RUNNING;

    // dump_thread_sp(next);
    
    context_switch(&saved_sp, next->sp);

    __builtin_unreachable();
}

#endif
