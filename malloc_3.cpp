#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <sys/mman.h>

#define TOO_BIG (100000000)
#define SPLIT_AMOUNT (128)
#define LARGE_MMAP (128*1024)

class MallocMetadata {
// Not a fan of the name
public:
    size_t size;
    size_t prev_size;
    bool is_free;
    bool is_mmap;
    MallocMetadata *next;
    MallocMetadata *prev;

    MallocMetadata(size_t new_size) {
        size = new_size;
        prev_size = 0;
        is_free = false;
        next = nullptr;
        prev = nullptr;
        is_mmap = false;

    }
};

class Stats {
public:
    size_t num_free_blocks;
    size_t num_free_bytes;
    size_t num_allocated_blocks;
    size_t num_allocated_bytes;
    Stats();
};

void _splitBlocks(MallocMetadata *pMetadata, size_t size);
void _mergeAdjacentBlocks(MallocMetadata *pMetadata, bool is_next_free, bool is_prev_free, bool combine_free);
void _removeFromBlockList(MallocMetadata *pMetadata);
Stats::Stats() : num_free_blocks(0), num_free_bytes(0), num_allocated_blocks(0), num_allocated_bytes(0) {}


// Our global variables
MallocMetadata *free_list_head = nullptr;
MallocMetadata *free_list_head_mmap = nullptr;
MallocMetadata *list_head = (MallocMetadata *) sbrk(0);
intptr_t meta_size = (intptr_t) (sizeof(MallocMetadata));
Stats memory_stats = Stats();
MallocMetadata *wilderness_block = nullptr;


void _addToBlockList(MallocMetadata *new_meta_data) {
    MallocMetadata *temp;
    if (!new_meta_data->is_mmap) {
        if (!free_list_head) {
            free_list_head = new_meta_data;
            return;
        }
        temp = free_list_head;
    } else {
        if (!free_list_head_mmap && new_meta_data->is_mmap) {
            free_list_head_mmap = new_meta_data;
            return;
        }
        temp = free_list_head_mmap;
    }


    if (temp->size > new_meta_data->size || (temp->size == new_meta_data->size && temp > new_meta_data)) {
        // *adding as first node
        free_list_head = new_meta_data;
        new_meta_data->next = temp;
        temp->prev = new_meta_data;
        return;
    }

    while (temp->next != nullptr &&
            (new_meta_data->size > temp->next->size ||
            new_meta_data->size == temp->next->size && new_meta_data > temp->next)) {
        temp = temp->next;
    }

    if (!temp->next) {
        // *checking if new_meta_data is last or one before last
        // *if not last node, just add it like any other node
        temp->next = new_meta_data;
        new_meta_data->prev = temp;
    } else {
        // *any other case
        new_meta_data->next = temp->next;
        temp->next->prev = new_meta_data;
        new_meta_data->prev = temp;
        temp->next = new_meta_data;
    }
}

MallocMetadata *_findBestFreeBlock(size_t size) {
    //no head
    MallocMetadata *temp;
    if (size < LARGE_MMAP)
    {
        if (!free_list_head) return nullptr;
        temp = free_list_head;

    } else {
        if (!free_list_head_mmap) return nullptr;
        temp = free_list_head_mmap;
    }

    //finding the right block
    while (temp) {
        if (temp->size >= size && temp->is_free) {
            return temp;
        }
        temp = temp->next;
    }
    return nullptr;
}

void _splitBlocks(MallocMetadata *pMetadata, size_t size) {

    //TODO: check the pointer arithmetics
    if (pMetadata->size < SPLIT_AMOUNT + meta_size + size)
    {
        return;
    }
    MallocMetadata *extra_meta_data = (MallocMetadata *) ((size_t) (pMetadata) + size + meta_size);
    *extra_meta_data = MallocMetadata(pMetadata->size - size - meta_size);
    extra_meta_data->prev_size = size;
    extra_meta_data->is_free = true;
    if (pMetadata != wilderness_block)
    {
        MallocMetadata *next_meta = (MallocMetadata* )((size_t) pMetadata + meta_size + pMetadata->size);
        next_meta->prev_size = extra_meta_data->size;
    }
    if (pMetadata == wilderness_block)
        wilderness_block = extra_meta_data;

    pMetadata->size = size;

    memory_stats.num_allocated_blocks++;
    memory_stats.num_allocated_bytes -= meta_size;
    memory_stats.num_free_blocks++;
    memory_stats.num_free_bytes -= meta_size;
    _addToBlockList(extra_meta_data);
}

void _mergeAdjacentBlocks(MallocMetadata *pMetadata, bool is_next_free, bool is_prev_free, bool combine_free = true) {
    MallocMetadata *prev_meta = (MallocMetadata* )((size_t)pMetadata - meta_size - pMetadata->prev_size);
    MallocMetadata *next_meta = (MallocMetadata* )((size_t)pMetadata + meta_size + pMetadata->size);
    if (is_next_free && is_prev_free) {
        size_t prev_size = prev_meta->size;
        size_t next_size = next_meta->size;
        prev_meta->size = prev_size + pMetadata->size + next_size + 2 * meta_size;
        if (next_meta != wilderness_block)
        {
            MallocMetadata * new_next_meta = (MallocMetadata* )((size_t)prev_meta + meta_size + prev_meta->size);
            new_next_meta->prev_size = prev_meta->size;
        } else
        {
            wilderness_block = prev_meta;
        }
        _removeFromBlockList(pMetadata);
        _removeFromBlockList(next_meta);

        memory_stats.num_allocated_blocks -= 2;
        memory_stats.num_allocated_bytes += meta_size * 2;
        memory_stats.num_free_blocks -= 2;
        if (combine_free)
            memory_stats.num_free_bytes += meta_size * 2 + pMetadata->size;
        else
            memory_stats.num_free_bytes -= (next_size + prev_size);

    }
    else if (is_prev_free) {
        // only is_prev_free == true
        size_t prev_size = prev_meta->size;
        prev_meta->size = prev_meta->size + pMetadata->size + meta_size;
        if (pMetadata != wilderness_block)
        {
            MallocMetadata * new_next_meta = (MallocMetadata* )((size_t)prev_meta + meta_size + prev_meta->size);
            new_next_meta->prev_size = prev_meta->size;
        } else
        {
            wilderness_block = prev_meta;
        }
        _removeFromBlockList(pMetadata);

        memory_stats.num_allocated_bytes += meta_size;
        memory_stats.num_allocated_blocks --;
        memory_stats.num_free_blocks --;
        if (combine_free)
            memory_stats.num_free_bytes += meta_size + pMetadata->size;
        else
            memory_stats.num_free_bytes -= prev_size;
    }
    else {
        //only is_next_free == true
        size_t next_size = next_meta->size;
        if (combine_free)
            memory_stats.num_free_bytes += meta_size + pMetadata->size;
        else
            memory_stats.num_free_bytes -= next_size;
        pMetadata->size = pMetadata->size + next_size + meta_size;
        if (next_meta != wilderness_block)
        {
            MallocMetadata *new_next_meta = (MallocMetadata *) ((size_t) pMetadata + meta_size + pMetadata->size);
            new_next_meta->prev_size = pMetadata->size;
        } else
        {
            wilderness_block = pMetadata;
        }
        _removeFromBlockList(next_meta);


        memory_stats.num_free_blocks --;
        memory_stats.num_allocated_bytes += meta_size;
        memory_stats.num_allocated_blocks --;
    }
}

void _removeFromBlockList(MallocMetadata *pMetadata) {
    //function assumes has the list has at least two items, since the only call will be from mergeAdjacent
    //also assumes that the first block won't be removed, since we only remove blocks whom where
    // merged to their prev (or the next merged to current)
    if (pMetadata == wilderness_block) {
        //last node
        wilderness_block = (MallocMetadata *)((size_t) pMetadata - pMetadata->prev_size - meta_size);
        if (pMetadata->prev) pMetadata->prev->next = nullptr;
    }
    else {
        if (pMetadata->prev) pMetadata->prev->next = pMetadata->next;
        pMetadata->prev = nullptr;
        if (pMetadata->next) pMetadata->next->prev = pMetadata->prev;
        pMetadata->next = nullptr;
    }

}

/**stats methods (for debug)*/
size_t _num_free_blocks() {
    return memory_stats.num_free_blocks;
}

size_t _num_free_bytes() {
    return memory_stats.num_free_bytes;
}

size_t _num_allocated_blocks() {
    return memory_stats.num_allocated_blocks;
}

size_t _num_allocated_bytes() {
    return memory_stats.num_allocated_bytes;
}

size_t _num_meta_data_bytes() {
    return meta_size * memory_stats.num_allocated_blocks;
}

size_t _size_meta_data() {
    return meta_size;
}

void *change_wilderness_size(size_t alloc_size) {
    intptr_t size_needed;
    size_needed = (intptr_t) (alloc_size) - (intptr_t) (wilderness_block->size);
    void *prev_program_break = sbrk(size_needed);
    if (prev_program_break == (void *) (-1)) {
        return NULL;
    }

    memory_stats.num_allocated_bytes += size_needed;
    memory_stats.num_free_blocks--;
    memory_stats.num_free_bytes -= wilderness_block->size;

    wilderness_block->is_free = false;
    wilderness_block->size = alloc_size;
    return wilderness_block;
}


/*The Malloc functions starts here*/
void *smalloc(size_t size) {
    size_t alloc_size = size + (8 - (size + meta_size) % 8) % 8;
    if (alloc_size <= 0 || alloc_size > TOO_BIG) {
    return NULL;
    }

    MallocMetadata *best_free_block = _findBestFreeBlock(alloc_size);
    intptr_t *user_start_block;

    if (!best_free_block) {

        if (alloc_size < LARGE_MMAP && wilderness_block && wilderness_block->is_free) {
            user_start_block = (intptr_t *) change_wilderness_size(alloc_size);
        }
        else {
            void *prev_program_break = nullptr;
            if(alloc_size >= LARGE_MMAP)
            {
                prev_program_break = mmap(NULL, alloc_size + meta_size, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
            } else {
                prev_program_break = sbrk((intptr_t) (alloc_size) + meta_size);
            }
            if (prev_program_break == (void *) (-1)) {
                return NULL;
            }
            MallocMetadata *new_meta_data = (MallocMetadata *) prev_program_break;
            *new_meta_data = MallocMetadata(alloc_size);
            if(alloc_size >= LARGE_MMAP)
            {
                new_meta_data->is_mmap = true;
            }
            if (wilderness_block) {
                new_meta_data->prev_size = wilderness_block->size;
            }
            wilderness_block = new_meta_data;
            _addToBlockList(new_meta_data);
            user_start_block = (intptr_t *) (prev_program_break);
            memory_stats.num_allocated_blocks++;
            memory_stats.num_allocated_bytes += alloc_size;
        }
    }
    else {
        //meaning that there is a freed block big enough for allocation
        _splitBlocks(best_free_block, alloc_size);
        best_free_block->is_free = false;
        user_start_block = (intptr_t *) best_free_block;
        memory_stats.num_free_blocks--;
        memory_stats.num_free_bytes -= best_free_block->size;
    }
    user_start_block += meta_size;
    return (void *) user_start_block;
}


void *scalloc(size_t num, size_t size) {
    void *new_room = smalloc(size * num);
    if (!new_room) {
        return NULL;
    }
    new_room = memset(new_room, 0, size * num);
    return new_room;
}

void sfree(void *p) {
    if (!p) {
        return;
    }
    intptr_t *p_size = (intptr_t *) (p);
    //assuming that p_size > meta_size
    MallocMetadata *meta = (MallocMetadata *) (p_size - meta_size);

    if (meta->is_free) {
        return;
    }
    if (meta->is_mmap) {
        munmap(meta, meta->size + meta_size);
        return;
    }

//    if (meta != wilderness_block)
//        printf("\nSize: %zu, prev block size is %zu, next block is %zu\n", meta->size, meta->prev_size, ((MallocMetadata *)((size_t) meta + meta_size + meta->size))->size);
//    else
//        printf("\nSize: %zu, prev block size is %zu\n", meta->size, meta->prev_size);

    bool is_next_free = meta != wilderness_block && ((MallocMetadata *)((size_t) meta + meta_size + meta->size))->is_free;
    bool is_prev_free = meta != list_head && ((MallocMetadata *)((size_t) meta - meta->prev_size - meta_size))->is_free;

    if (is_next_free || is_prev_free) {
        _mergeAdjacentBlocks(meta, is_next_free, is_prev_free);
    } else
    {
        memory_stats.num_free_bytes += meta->size;
    }

    memory_stats.num_free_blocks++;
    meta->is_free = true;



}


void *srealloc(void *oldp, size_t size) {
    if (size <= 0 || size > TOO_BIG) {
        return NULL;
    }
    if (!oldp) {
        return smalloc(size);
    }
    intptr_t *p_size = (intptr_t *) (oldp);
    //assuming that p_size > meta_size
    MallocMetadata *meta = (MallocMetadata *) (p_size - meta_size);
    MallocMetadata *next_meta = (MallocMetadata* )((size_t) meta + meta_size + meta->size);
    MallocMetadata *prev_meta = (MallocMetadata* )((size_t) meta - meta_size - meta->prev_size);

    // priority a
    if (size <= meta->size)
    {
        _splitBlocks(prev_meta, size);
        return oldp;
    }

    // priority b
    if (prev_meta->is_free && size <= (meta->size + prev_meta->size))
    {
        _mergeAdjacentBlocks(meta, false, true);
        prev_meta->is_free = false;
        _splitBlocks(prev_meta, size);
        return (void *) ((size_t) prev_meta + meta_size);
    }

    if (prev_meta->is_free && wilderness_block == meta)
    {
        _mergeAdjacentBlocks(meta, false, true);
        prev_meta->is_free = false;
        if (!change_wilderness_size(size))
            return NULL;
        _splitBlocks(prev_meta, size);
        return (void *) ((size_t) prev_meta + meta_size);
    }

    // priority c
    if (wilderness_block == meta)
    {
        if (!change_wilderness_size(size))
            return NULL;
        _splitBlocks(meta, size);
        return (void *) ((size_t) meta + meta_size);
    }

    // priority d
    if (next_meta->is_free && size <= (meta->size + next_meta->size))
    {
        _mergeAdjacentBlocks(meta, true, false);
        _splitBlocks(meta, size);
        return (void *) ((size_t) meta + meta_size);
    }

    // priority e
    if (next_meta->is_free && prev_meta->is_free && size <= (meta->size + next_meta->size + prev_meta->size))
    {
        _mergeAdjacentBlocks(meta, true, true);
        prev_meta->is_free = false;
        _splitBlocks(prev_meta, size);
        return (void *) ((size_t) prev_meta + meta_size);
    }

    // priority f
    if (next_meta->is_free && prev_meta->is_free && wilderness_block == next_meta)
    {
        _mergeAdjacentBlocks(meta, true, true);
        prev_meta->is_free = false;
        if (!change_wilderness_size(size))
            return NULL;
        return (void *) ((size_t) prev_meta + meta_size);
    }

    if (next_meta->is_free && wilderness_block == next_meta)
    {
        _mergeAdjacentBlocks(meta, true, true);
        if (!change_wilderness_size(size))
            return NULL;
        return (void *) ((size_t) meta + meta_size);
    }

    void *new_allocated = smalloc(size);
    if (!new_allocated) {
        return NULL;
    }
    else {
        memmove(new_allocated, oldp, size);
        sfree(oldp);
        return new_allocated;
    }
}

