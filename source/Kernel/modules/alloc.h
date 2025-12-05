#ifndef ALLOC_H
#define ALLOC_H

/*
 * slab allocator + kbrk bump allocator
 *
 * notas:
 *  - slabs ocupam 1 página (PAGE_SIZE) cada; cada slab tem header no início da página
 *  - para objetos maiores que MAX_SMALL_OBJECT (2K), alocações grandes usam
 *    multiple-pages com um header simples para poder liberar
 *  - não há coalescing de grandes chunks
 *
 */

#define CHUNK_ALIGNMENT_BYTES 8

// classes de cache (potências de 2)
static const size_t cache_sizes[] =
{
    8, 16, 32, 64, 128, 256, 512, 1024, 2048
};
#define CACHE_CLASSES (sizeof(cache_sizes)/sizeof(cache_sizes[0]))

#define MAX_SMALL_OBJECT 2048

static uint8_t* kbrk_ptr = NULL;

static inline size_t align_up(size_t x)
{
    return (x + (CHUNK_ALIGNMENT_BYTES - 1)) & ~(CHUNK_ALIGNMENT_BYTES - 1);
}

void kbrk_init(void)
{
    uintptr_t base = (uintptr_t)_kernel_heap_start;
    base = (base + (CHUNK_ALIGNMENT_BYTES - 1)) & ~(CHUNK_ALIGNMENT_BYTES - 1);
    kbrk_ptr = (uint8_t*)base;
}

/* kbrk:
 *  - kbrk(0) => retorna current break
 *  - kbrk(n) => avança n (alinhado) e retorna old break
 *  - retorna NULL em sem espaço
 *
 * increment pode ser 0; retorna NULL se kbrk_ptr não inicializado
 */
void *kbrk(ssize_t increment)
{
    if (!kbrk_ptr)
        return NULL;

    uint8_t* old = kbrk_ptr;
    if (increment == 0)
        return old;

    size_t inc = (size_t)increment;
    inc = align_up(inc);

    uint8_t* new_ptr = kbrk_ptr + inc;
    if (new_ptr > _kernel_heap_end) // overflow
        return NULL;

    kbrk_ptr = new_ptr;
    return old;
}

typedef struct
{
    volatile int lock;
} spinlock_t;

static inline void spinlock_init(spinlock_t* lk)
{
    lk->lock = 0;
}

static inline void spin_lock(spinlock_t* lk)
{
    while (__atomic_test_and_set(&lk->lock, __ATOMIC_ACQUIRE));
        /* busy wait */
}

static inline void spin_unlock(spinlock_t* lk)
{
    __atomic_clear(&lk->lock, __ATOMIC_RELEASE);
}

typedef struct
{
    size_t pages;
} large_alloc_hdr_t;

// cada página usada como slab começa com slab_t header
typedef struct slab_t
{
    struct slab_t* next;
    void* free_list;
    uint16_t free_count;
    uint16_t total_objects;
    uint16_t object_size;
    uint16_t pad;
    // objects follow right after header (header ocupa o início da página)
} slab_t;

// cada cache mantém 3 listas: partial (tem objetos livres), full (0 livres), empty (todos livres)
// para simplicidade usamos single list 'partial' e criamos/descartamos slabs conforme necessário
typedef struct
{
    size_t object_size;
    slab_t* partial;   // slabs with some free space
    slab_t* full;      // filled slabs (optional)
    spinlock_t lock;
} kmem_cache_t;

static kmem_cache_t caches[CACHE_CLASSES];
static bool slab_inited = false;

// calcula index do cache para um dado size (first fit potências da tabela)
static int cache_index_for_size(size_t size)
{
    for (int i = 0; i < (int)CACHE_CLASSES; ++i)
        if (size <= cache_sizes[i])
            return i;

    return -1; // size > MAX_SMALL_OBJECT
}

// get slab base (page-aligned) a partir de um objeto
static inline slab_t* slab_from_obj(void* obj)
{
    uintptr_t a = (uintptr_t)obj;
    uintptr_t base = a & ~(PAGE_SIZE - 1);
    
    return (slab_t*)base;
}

// initialize caches (lazy)
static void slab_init(void)
{
    if (slab_inited)
        return;
    
    for (int i = 0; i < (int)CACHE_CLASSES; ++i)
    {
        caches[i].object_size = cache_sizes[i];
        caches[i].partial = NULL;
        caches[i].full = NULL;
    
        spinlock_init(&caches[i].lock);
    }
   
    slab_inited = true;
}

// create a new slab page for a given cache
static slab_t* create_slab_for_cache(kmem_cache_t* cache)
{
    // allocate one page via kbrk
    void* page = kbrk(PAGE_SIZE);
    if (!page)
        return NULL;

    memset(page, 0, PAGE_SIZE);

    slab_t* s = (slab_t*)page;
    s->next = NULL;
    s->free_list = NULL;
    s->object_size = (uint16_t)cache->object_size;
    // s->hsize = 0; /* not used; placeholder */

    // compute how many objects fit in this page after header
    size_t header_sz = sizeof(slab_t);
    size_t avail = PAGE_SIZE - header_sz;
    uint16_t nobj = (uint16_t)(avail / s->object_size);
    
    if (nobj == 0)
        // object size too big for single-page slab
        return NULL;

    s->total_objects = nobj;
    s->free_count = nobj;

    // build free list: each object is at offset header + i*object_size
    uint8_t* objs_base = (uint8_t*)page + header_sz;
    void* prev = NULL;
    
    for (uint16_t i = 0; i < nobj; ++i)
    {
        void *obj = objs_base + (i * s->object_size);
        // store pointer to next in the first pointer-sized bytes of object
        *((void**)obj) = prev;
        prev = obj;
    }
    
    // `prev` points to last object; i want free_list to be head of list
    s->free_list = prev;

    // insert slab into cache->partial
    s->next = cache->partial;
    cache->partial = s;

    return s;
}

/* allocate an object from a slab (assumes cache locked) */
static void* alloc_from_slab(kmem_cache_t* cache, slab_t* s)
{
    if (!s || s->free_count == 0)
        return NULL;

    // pop head from free_list
    void* obj = s->free_list;
    void* next = *((void**)obj);

    s->free_list = next;
    s->free_count--;

    // if slab became full, move to full list (optional)
    if (s->free_count == 0)
    {
        // remove slab from partial list
        slab_t** pp = &cache->partial;
        while (*pp && *pp != s)
            pp = &(*pp)->next;
        
        if (*pp)
            *pp = s->next;
        
        // push to full list
        s->next = cache->full;
        cache->full = s;
    }

    // zero object for safety
    memset(obj, 0, cache->object_size);
    return obj;
}

// free object to its slab (assumes cache locked)
static void free_to_slab(kmem_cache_t* cache, slab_t* s, void* obj)
{
    // push object into slab free_list
    *((void**)obj) = s->free_list;
    s->free_list = obj;
    s->free_count++;

    // if slab was full, move from full to partial
    if (s->free_count == 1)
    {
        // remove from full
        slab_t** pp = &cache->full;
        while (*pp && *pp != s)
            pp = &(*pp)->next;
        
        if (*pp)
        {
            *pp = s->next;
            // push to partial 
            s->next = cache->partial;
            cache->partial = s;
        }
    }

    // optional: if slab entirely free, we could free page back (not implemented here)
}

/* kmalloc: if size <= MAX_SMALL_OBJECT -> slab allocate
 *          else -> large allocation (multiple pages + header)
 */
void* kmalloc(size_t size)
{
    if (!slab_inited) slab_init();
    if (!kbrk_ptr) kbrk_init();

    if (size == 0) return NULL;

    size = align_up(size);

    if (size <= MAX_SMALL_OBJECT)
    {
        int idx = cache_index_for_size(size);
        if (idx < 0) idx = (int)CACHE_CLASSES - 1;
        
        kmem_cache_t* cache = &caches[idx];
        spin_lock(&cache->lock);

        // try partial slabs first
        slab_t* s = cache->partial;
        void* res = NULL;
        if (!s)
        {
            // create one slab
            s = create_slab_for_cache(cache);
            if (!s)
            {
                spin_unlock(&cache->lock);
                return NULL;
            }
        }

        // allocate from chosen slab (partial)
        res = alloc_from_slab(cache, s);
        spin_unlock(&cache->lock);
        
        return res;
    }
    else
    {
        // large allocation: allocate whole pages and store header
        size_t total = size + sizeof(large_alloc_hdr_t);
        size_t pages = (total + PAGE_SIZE - 1) / PAGE_SIZE;
        size_t bytes = pages * PAGE_SIZE;

        void *p = kbrk(bytes);
        if (!p) return NULL;

        large_alloc_hdr_t* hdr = (large_alloc_hdr_t*)p;
        hdr->pages = pages;

        void* payload = (void*)((uint8_t*)p + sizeof(large_alloc_hdr_t));
        // zero payload
        memset(payload, 0, bytes - sizeof(large_alloc_hdr_t));
        
        return payload;
    }
}

/* kfree: free slab object or large allocation */
void kfree(void* ptr)
{
    if (!ptr) return;
    if (!slab_inited) slab_init();

    /* determine whether pointer belongs to a slab page by testing page header patterns:
       we try to read slab header at page base and see if object_size matches a cache size
       alternatively i can check if the page contains a valid slab; i assume slabs are page-aligned
    */

    uintptr_t a = (uintptr_t)ptr;
    uintptr_t page_base = a & ~(PAGE_SIZE - 1);
    slab_t* s = (slab_t*)page_base;

    /* crude validation: object_size should be one of cache_sizes and not zero
       if it doesntt match, assume it is a large alloc (i placed large header at page start)
    */

    bool looks_like_slab = false;
    if (s->object_size != 0)
    {
        for (int i = 0; i < (int)CACHE_CLASSES; ++i)
        {
            if (s->object_size == (uint16_t)cache_sizes[i])
            {
                looks_like_slab = true;
                break;
            }
        }
    }

    if (looks_like_slab)
    {
        int idx = cache_index_for_size(s->object_size);
        if (idx < 0)
            return; // corruption?

        kmem_cache_t* cache = &caches[idx];
        spin_lock(&cache->lock);
        free_to_slab(cache, s, ptr);
        spin_unlock(&cache->lock);
        
        return;
    } 
    else
    {
        // assume large allocation header at page_base
        
        large_alloc_hdr_t* hdr = (large_alloc_hdr_t*)page_base;
        size_t pages = hdr->pages;
        
        if (pages == 0)
            return; // invalid free
        
        // for simplicity, do not return pages to kbrk (no free of bump); i could manage a free list of large regions
        // zero header to avoid double free detection lightly
        
        hdr->pages = 0;
        (void)pages;
        
        return;
    }
}

#endif
