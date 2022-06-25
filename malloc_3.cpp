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

void _splitBlocks(MallocMetadata *pMetadata, size_t size, bool split_free);
void _mergeAdjacentBlocks(MallocMetadata *pMetadata, bool is_next_free, bool is_prev_free, bool combine_free);
void _removeFromBlockList(MallocMetadata *pMetadata);
Stats::Stats() : num_free_blocks(0), num_free_bytes(0), num_allocated_blocks(0), num_allocated_bytes(0) {}


// Our global variables
MallocMetadata *free_list_head = nullptr;
MallocMetadata *free_list_head_mmap = nullptr;
MallocMetadata *list_head = (MallocMetadata *) sbrk(0);
size_t meta_size = sizeof(MallocMetadata);
Stats memory_stats = Stats();
MallocMetadata *wilderness_block = nullptr;


void _addToBlockList(MallocMetadata *new_meta_data) {
    MallocMetadata *temp;

    if (new_meta_data > wilderness_block) {
        wilderness_block = new_meta_data;
    }
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

void _splitBlocks(MallocMetadata *pMetadata, size_t size, bool split_free=true) {

    if (pMetadata->size < SPLIT_AMOUNT + meta_size + size)
        return;

    size_t extra_size = pMetadata->size - size - meta_size;
    MallocMetadata *extra_meta_data = (MallocMetadata *) ((char*) (pMetadata) + size + meta_size);
    *extra_meta_data = MallocMetadata(extra_size);
    extra_meta_data->prev_size = size;
    extra_meta_data->is_free = true;

    if (pMetadata != wilderness_block)
    {
        MallocMetadata *next_meta = (MallocMetadata* )((char*) pMetadata + meta_size + pMetadata->size);
        next_meta->prev_size = extra_meta_data->size;
    }
    if (pMetadata == wilderness_block)
        wilderness_block = extra_meta_data;

    pMetadata->size = size;

    memory_stats.num_allocated_blocks++;
    memory_stats.num_allocated_bytes -= meta_size;
    memory_stats.num_free_blocks++;
    if (split_free)
        memory_stats.num_free_bytes -= meta_size;
    else
        memory_stats.num_free_bytes += extra_size;
    _removeFromBlockList(pMetadata);
    _addToBlockList(pMetadata);
    _addToBlockList(extra_meta_data);
}

void _mergeAdjacentBlocks(MallocMetadata *pMetadata, bool is_next_free, bool is_prev_free, bool combine_free = true) {
    MallocMetadata *prev_meta = (MallocMetadata* )((char*)pMetadata - meta_size - pMetadata->prev_size);
    MallocMetadata *next_meta = (MallocMetadata* )((char*)pMetadata + meta_size + pMetadata->size);
    if (is_next_free && is_prev_free) {
        size_t prev_size = prev_meta->size;
        size_t next_size = next_meta->size;
        prev_meta->size = prev_size + pMetadata->size + next_size + 2 * meta_size;
        if (next_meta != wilderness_block)
        {
            MallocMetadata * new_next_meta = (MallocMetadata* )((char*)prev_meta + meta_size + prev_meta->size);
            new_next_meta->prev_size = prev_meta->size;
        } else
        {
            wilderness_block = prev_meta;
        }
        _removeFromBlockList(pMetadata);
        _removeFromBlockList(next_meta);
        _removeFromBlockList(prev_meta);
        _addToBlockList(prev_meta);

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
            MallocMetadata * new_next_meta = (MallocMetadata* )((char*)prev_meta + meta_size + prev_meta->size);
            new_next_meta->prev_size = prev_meta->size;
        } else
        {
            wilderness_block = prev_meta;
        }
        _removeFromBlockList(pMetadata);
        _removeFromBlockList(prev_meta);
        _addToBlockList(prev_meta);

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
            MallocMetadata *new_next_meta = (MallocMetadata *) ((char*) pMetadata + meta_size + pMetadata->size);
            new_next_meta->prev_size = pMetadata->size;
        } else
        {
            wilderness_block = pMetadata;
        }
        _removeFromBlockList(next_meta);
        _removeFromBlockList(pMetadata);
        _addToBlockList(pMetadata);


        memory_stats.num_free_blocks --;
        memory_stats.num_allocated_bytes += meta_size;
        memory_stats.num_allocated_blocks --;
    }
}

void _removeFromBlockList(MallocMetadata *pMetadata) {
    if (pMetadata == free_list_head)
        free_list_head = pMetadata->next;

    if (pMetadata == free_list_head_mmap)
        free_list_head_mmap = pMetadata->next;

    if (pMetadata == wilderness_block)
        wilderness_block = (MallocMetadata *)((char*) pMetadata - pMetadata->prev_size - meta_size);

    if (pMetadata->prev) pMetadata->prev->next = pMetadata->next;
    if (pMetadata->next) pMetadata->next->prev = pMetadata->prev;
    pMetadata->prev = nullptr;
    pMetadata->next = nullptr;

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

void *change_wilderness_size(size_t alloc_size, bool combine_free=true) {
    size_t size_needed;
    MallocMetadata* wilderness = wilderness_block;
    size_needed = (size_t) (alloc_size) - (size_t) (wilderness->size);
    void *prev_program_break = sbrk(size_needed);
    if (prev_program_break == (void *) (-1)) {
        return NULL;
    }

    memory_stats.num_allocated_bytes += size_needed;
    if (combine_free)
    {
        memory_stats.num_free_blocks--;
        memory_stats.num_free_bytes -= wilderness->size;
    }

    wilderness->is_free = false;
    wilderness->size = alloc_size;
    _removeFromBlockList(wilderness);
    _addToBlockList(wilderness);
    return wilderness_block;
}


/*The Malloc functions starts here*/
void *smalloc(size_t size) {
//    size_t alloc_size = size + (8 - (size + meta_size) % 8) % 8;
    size_t alloc_size = size;
    if (alloc_size <= 0 || alloc_size > TOO_BIG) {
    return NULL;
    }

    MallocMetadata *best_free_block = _findBestFreeBlock(alloc_size);
    char *user_start_block;

    if (!best_free_block) {

        if (alloc_size < LARGE_MMAP && wilderness_block && wilderness_block->is_free) {
            user_start_block = (char *) change_wilderness_size(alloc_size);
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
            user_start_block = (char *) (prev_program_break);
            memory_stats.num_allocated_blocks++;
            memory_stats.num_allocated_bytes += alloc_size;
        }
    }
    else {
        //meaning that there is a freed block big enough for allocation
        _splitBlocks(best_free_block, alloc_size);
        best_free_block->is_free = false;
        user_start_block = (char *) best_free_block;
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
    char *p_size = (char *) (p);
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

    bool is_next_free = meta != wilderness_block && ((MallocMetadata *)((char*) meta + meta_size + meta->size))->is_free;
    bool is_prev_free = meta != list_head && ((MallocMetadata *)((char*) meta - meta->prev_size - meta_size))->is_free;

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
    char *p_size = (char *) (oldp);
    //assuming that p_size > meta_size
    MallocMetadata *meta = (MallocMetadata *) (p_size - meta_size);
    MallocMetadata *next_meta = (MallocMetadata* )((char*) meta + meta_size + meta->size);
    MallocMetadata *prev_meta = (MallocMetadata* )((char*) meta - meta_size - meta->prev_size);
    void *new_allocated;


    // priority a
    if (size <= meta->size)
    {
        memory_stats.num_free_bytes += meta->size - size;
        _splitBlocks(meta, size, false);
        return oldp;
    }

    // priority b
    else if (prev_meta->is_free && size <= (meta->size + prev_meta->size))
    {
        _mergeAdjacentBlocks(meta, false, true, false);
        prev_meta->is_free = false;
        _splitBlocks(prev_meta, size, false);
        new_allocated =  (void *) ((char *) prev_meta + meta_size);
    }

    else if (prev_meta->is_free && wilderness_block == meta)
    {
        _mergeAdjacentBlocks(meta, false, true, false);
        prev_meta->is_free = false;
        if (!change_wilderness_size(size, false))
            return NULL;
        _splitBlocks(prev_meta, size, false);
        new_allocated = (void *) ((char *) prev_meta + meta_size);
    }

    // priority c
    else if (wilderness_block == meta)
    {
        if (!change_wilderness_size(size, false))
            return NULL;
        _splitBlocks(meta, size, false);
        return (void *) ((char *) meta + meta_size);
    }

    // priority d
    else if (next_meta->is_free && size <= (meta->size + next_meta->size))
    {
        _mergeAdjacentBlocks(meta, true, false, false);
        _splitBlocks(meta, size, false);
        return (void *) ((char *) meta + meta_size);
    }

    // priority e
    else if (next_meta->is_free && prev_meta->is_free && size <= (meta->size + next_meta->size + prev_meta->size))
    {
        _mergeAdjacentBlocks(meta, true, true, false);
        prev_meta->is_free = false;
        _splitBlocks(prev_meta, size, false);
        new_allocated = (void *) ((char *) prev_meta + meta_size);
    }

    // priority f
    else if (next_meta->is_free && prev_meta->is_free && wilderness_block == next_meta)
    {
        _mergeAdjacentBlocks(meta, true, true, false);
        prev_meta->is_free = false;
        if (!change_wilderness_size(size, false))
            return NULL;
        new_allocated = (void *) ((char *) prev_meta + meta_size);
    }
    else if (next_meta->is_free && wilderness_block == next_meta)
    {
        _mergeAdjacentBlocks(meta, true, false, false);
        if (!change_wilderness_size(size, false))
            return NULL;
        return (void *) ((char *) meta + meta_size);

    } else {
        new_allocated = smalloc(size);
        sfree(oldp);
    }

    if (!new_allocated) {
        return NULL;
    }
    else {
        memmove(new_allocated, oldp, size);
        return new_allocated;
    }
}

