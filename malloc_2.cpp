#include <unistd.h>
#include <cstring>
#include <cstdio>

#define TOO_BIG (100000000)


class MallocMetadata {
// Not a fan of the name
private:
    size_t size;
    bool is_free;
    MallocMetadata *next;
    MallocMetadata *prev;


public:
    size_t getSize() const {
        return size;
    }

    void setSize(size_t new_size) {
        MallocMetadata::size = new_size;
    }

    bool isFree() const {
        return is_free;
    }

    void setIsFree(bool isFree) {
        is_free = isFree;
    }

    MallocMetadata *getNext() const {
        return next;
    }

    void setNext(MallocMetadata *new_next) {
        MallocMetadata::next = new_next;
    }

    MallocMetadata *getPrev() const {
        return prev;
    }

    void setPrev(MallocMetadata *new_prev) {
        MallocMetadata::prev = new_prev;
    }

    MallocMetadata(size_t new_size) {
        size = new_size;
        is_free = false;
        next = nullptr;
        prev = nullptr;

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

Stats::Stats() : num_free_blocks(0), num_free_bytes(0), num_allocated_blocks(0), num_allocated_bytes(0) {}


// Our global variables
MallocMetadata *free_list_head = nullptr;
intptr_t meta_size = (intptr_t) (sizeof(MallocMetadata));
Stats memory_stats = Stats();


void _addToBlockList(MallocMetadata *new_meta_data) {
    if (!free_list_head) {
        free_list_head = new_meta_data;
        return;
    }
    MallocMetadata *temp = free_list_head;

    while (temp->getNext() != nullptr) {
        temp = temp->getNext();
    }

    temp->setNext(new_meta_data);
    new_meta_data->setPrev(temp);

}

MallocMetadata *_findFirstFreeBlock(size_t size) {
    //no head
    if (!free_list_head) {
        return nullptr;
    }
    MallocMetadata *temp = free_list_head;
    //finding the right block
    while (temp) {

        if (temp->getSize() >= size && temp->isFree()) {
            return temp;
        }
        temp = temp->getNext();
    }
    return nullptr;
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

/*The Malloc functions starts here*/

void *smalloc(size_t size) {
    if (size <= 0 || size > TOO_BIG) {
        return NULL;
    }

    MallocMetadata *first_free_block = _findFirstFreeBlock(size);
    intptr_t *user_start_block;

    if (!first_free_block) {
        void *prev_program_break = sbrk((intptr_t) size + meta_size);
        if (prev_program_break == (void *) (-1)) {
            return NULL;
        }
        MallocMetadata *new_meta_data = (MallocMetadata *) prev_program_break;
        *new_meta_data = MallocMetadata(size);
        _addToBlockList(new_meta_data);
        memory_stats.num_allocated_blocks++;
        memory_stats.num_allocated_bytes += size;

        user_start_block = (intptr_t *) (prev_program_break);
    }
    else {
        //meaning that there is a freed block big enough for allocation

        first_free_block->setIsFree(false);
        user_start_block = (intptr_t*) first_free_block;
        memory_stats.num_free_blocks--;
        memory_stats.num_free_bytes -= first_free_block->getSize();
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
    MallocMetadata *meta = ((MallocMetadata *)p) - meta_size;
    if (meta->isFree()) {
        return;
    }
    memory_stats.num_free_blocks++;
    meta->setIsFree(true);
    memory_stats.num_free_bytes += meta->getSize();
}

void *srealloc(void *oldp, size_t size) {
    if (size <= 0 || size > TOO_BIG) {
        return NULL;
    }
    if(!oldp){
        return smalloc(size);
    }
    intptr_t *p_size = (intptr_t *) (oldp);
    //assuming that p_size > meta_size
    MallocMetadata *meta = (MallocMetadata *) (p_size - meta_size);

    if (size <= meta->getSize()) {
        return oldp;
    }
    else {
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
}

