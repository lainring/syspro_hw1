/** @file alloc.c */
#include <stdio.h>
#include <unistd.h>
#include <string.h>

typedef struct s_block *t_block;
struct s_block {
    size_t size;
    t_block prev;
    t_block next;
    int free;
    int padding;
    void *ptr;
    char data[1];
};

#define BLOCK_SIZE 40
#define PAGE_SIZE 16384

void* first_block = NULL;
void* max_ptr = NULL;  // is the heap top location
void* now_ptr = NULL;  // which next is the start of new alloc

t_block get_block(void *p) {
    char *tmp;
    tmp = p;
    return (p = tmp -= BLOCK_SIZE);
}

int valid_addr(void *p) {
    if (first_block) {
        if (p > first_block && p < now_ptr) {
            return p == (get_block(p)->ptr);
        }
    }
    return 0;
}

void* more_bytes(size_t s) {
    if (now_ptr == NULL) {
        now_ptr = sbrk(0);
        max_ptr = now_ptr;
    }
    if (now_ptr + s > max_ptr) {
        size_t delta = s - (max_ptr - now_ptr);
        delta = (((delta % PAGE_SIZE)?1:0) + (delta / PAGE_SIZE)) * PAGE_SIZE;
        sbrk(delta);
        max_ptr += delta;
    } else {
    }
    now_ptr += s;
    return now_ptr - s + 1;
}

t_block fusion(t_block b) {
    if (b->next && b->next->free) {
        b->size += BLOCK_SIZE + b->next->size;
        b->next = b->next->next;
        if (b->next) {
             b->next->prev = b;
        }
    }
    return b;
}

t_block extend_heap(t_block last, size_t s) {
    t_block b = more_bytes(BLOCK_SIZE + s);
    b->size = s;
    b->next = NULL;
    b->prev = last;
    b->ptr = b->data;
    if (last) {
        last->next = b;
    }
    b->free = 0;
    return b;
}

void split_block(t_block b, size_t s) {
    t_block _new;
    _new = (t_block)(b->data + s);
    if (_new) {
        _new->size = 0;
    }
    //_new->size = b->size - s - BLOCK_SIZE;
    _new->next = b->next;
    _new->prev = b;
    _new->free = 1;
    _new->ptr = _new->data;
    b->size = s;
    b->next = _new;
    if (_new->next) {
         _new->next->prev = _new;
    }
}

size_t align8(size_t s) {
    if ((s & 0x7) == 0) {
        return s;
    }
    return ((s >> 3) + 1) << 3;
}

t_block find_block(t_block *last, size_t size) {
    t_block b = first_block;
    while (b && !(b->free && b->size >= size)) {
        *last = b;
        b = b->next;
    }
    return b;
}

void copy_block(t_block src, t_block dst) {
     size_t *sdata, *ddata;
     size_t i;
     sdata = src->ptr;
     ddata = dst->ptr;
     memcpy(ddata, sdata, src->size);
}

/**
 * Allocate space for array in memory
 *
 * Allocates a block of memory for an array of num elements, each of them size
 * bytes long, and initializes all its bits to zero. The effective result is
 * the allocation of an zero-initialized memory block of (num * size) bytes.
 *
 * @param num
 *    Number of elements to be allocated.
 * @param size
 *    Size of elements.
 *
 * @return
 *    A pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory, a
 *    NULL pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/calloc/
 */
void *calloc(size_t num, size_t size)
{
    /* Note: This function is complete. You do not need to modify it. */
    size_t *_new;
    size_t s8, i;
    _new = malloc(num * size);
    if (_new) {
         s8 = align8(num * size) << 3;
         memset(_new, 0, s8);
    }
    return _new;
}


/**
 * Allocate memory block
 *
 * Allocates a block of size bytes of memory, returning a pointer to the
 * beginning of the block.  The content of the newly allocated block of
 * memory is not initialized, remaining with indeterminate values.
 *
 * @param size
 *    Size of the memory block, in bytes.
 *
 * @return
 *    On success, a pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a null pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/malloc/
 */
void *malloc(size_t size)
{
    t_block b, last;
    size_t s;
    s = align8(size);
    if (first_block) {
        last = first_block;
        b = find_block(&last, s);
        if (b) {
            if (b->size - s >= BLOCK_SIZE + 8) {
                split_block(b, s);
            }
            b->free = 0;
        } else {
             b = extend_heap(last, s);
             if (!b) {
                 return NULL;
             }
        }
    } else {
         b = extend_heap(NULL, s);
         if (!b) {
             return NULL;
         }
         first_block = b;
    }
    return b->data;
}


/**
 * Deallocate space in memory
 *
 * A block of memory previously allocated using a call to malloc(),
 * calloc() or realloc() is deallocated, making it available again for
 * further allocations.
 *
 * Notice that this function leaves the value of ptr unchanged, hence
 * it still points to the same (now invalid) location, and not to the
 * null pointer.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(),
 *    calloc() or realloc() to be deallocated.  If a null pointer is
 *    passed as argument, no action occurs.
 */
void free(void *ptr)
{
    // "If a null pointer is passed as argument, no action occurs."
    t_block b;
    if (valid_addr(ptr)) {
        b = get_block(ptr);
        b->free = 1;
        if (b->prev && b->prev->free) {
            b = fusion(b->prev);
        }
        if (b->next) {
             fusion(b);
        } else {
            if (b->prev) {
                b->prev->next = NULL;
            } else {
                 first_block = NULL;
            }
            now_ptr -= (BLOCK_SIZE + b->size);
        }
    } else {
         //printf("invalid ptr %ld!!!\n", (size_t)ptr);
    }
}


/**
 * Reallocate memory block
 *
 * The size of the memory block pointed to by the ptr parameter is changed
 * to the size bytes, expanding or reducing the amount of memory available
 * in the block.
 *
 * The function may move the memory block to a new location, in which case
 * the new location is returned. The content of the memory block is preserved
 * up to the lesser of the new and old sizes, even if the block is moved. If
 * the new size is larger, the value of the newly allocated portion is
 * indeterminate.
 *
 * In case that ptr is NULL, the function behaves exactly as malloc, assigning
 * a new block of size bytes and returning a pointer to the beginning of it.
 *
 * In case that the size is 0, the memory previously allocated in ptr is
 * deallocated as if a call to free was made, and a NULL pointer is returned.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(), calloc()
 *    or realloc() to be reallocated.
 *
 *    If this is NULL, a new block is allocated and a pointer to it is
 *    returned by the function.
 *
 * @param size
 *    New size for the memory block, in bytes.
 *
 *    If it is 0 and ptr points to an existing block of memory, the memory
 *    block pointed by ptr is deallocated and a NULL pointer is returned.
 *
 * @return
 *    A pointer to the reallocated memory block, which may be either the
 *    same as the ptr argument or a new location.
 *
 *    The type of this pointer is void*, which can be cast to the desired
 *    type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a NULL pointer is returned, and the memory block pointed to by
 *    argument ptr is left unchanged.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/realloc/
 */
void *realloc(void *ptr, size_t size)
{
    // "In case that ptr is NULL, the function behaves exactly as malloc()"
    size_t s;
    t_block b, _new;
    void *newp;
    if (!ptr) {
        return malloc(size);
    }
    if (valid_addr(ptr)) {
         s = align8(size);
         b = get_block(ptr);
         if (b->size >= s) {
             if (b->size - s >= (BLOCK_SIZE + 8)) {
                  split_block(b, s);
             }
         } else {
             if (b->next && b->next->free
                     && (b->size + BLOCK_SIZE + b->next->size) >= s) {
                  fusion(b);
                  if (b->size - s >= (BLOCK_SIZE + 8)) {
                      split_block(b, s);
                  }
             } else {
                  newp = malloc(s);
                  if (!newp) {
                      return NULL;
                  }
                  _new = get_block(newp);
                  copy_block(b, _new);
                  free(ptr);
                  return newp;
             }
         }
         return ptr;
    }
    return NULL;
}
