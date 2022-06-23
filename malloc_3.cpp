#include <unistd.h>
#include <cstring>
#include <cstdio>

#define TOO_BIG (100000000)


#define SPLIT_AMOUNT (128)

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

MallocMetadata *_splitBlocks(MallocMetadata *pMetadata, size_t size);

void _mergeAdjacentBlocks(MallocMetadata *pMetadata, bool is_next_free, bool is_prev_free);

void _removeFromBlockList(MallocMetadata *pMetadata);

Stats::Stats() : num_free_blocks(0), num_free_bytes(0), num_allocated_blocks(0), num_allocated_bytes(0) {}


// Our global variables
MallocMetadata *free_list_head = nullptr;
intptr_t meta_size = (intptr_t) (sizeof(MallocMetadata));
Stats memory_stats = Stats();
MallocMetadata *wilderness_block = nullptr;


void _addToBlockList(MallocMetadata *new_meta_data) {
    if (!free_list_head) {
        wilderness_block = new_meta_data;
        free_list_head = new_meta_data;
        return;
    }
    MallocMetadata *temp = free_list_head;
    //TODO: check pointer arithmetics
    if ((size_t) (wilderness_block) < (size_t) (new_meta_data)) {
        wilderness_block = new_meta_data;
    }

    while (temp->getNext() != nullptr) {
        if (new_meta_data->getSize() <= temp->getSize()) {
            break;
        }
        temp = temp->getNext();
    }

    while (temp->getNext() != nullptr && new_meta_data->getSize() == temp->getSize()) {
        //TODO: check pointer arithmetics
        if (new_meta_data < temp) {
            break;
        }
        temp = temp->getNext();
    }

    if (!temp->getNext()) {
        // *checking if new_meta_data is last or one before last
        // *if not last node, just add it like any other node
        if ((new_meta_data->getSize() > temp->getSize()) ||
            (new_meta_data->getSize() == temp->getSize() && new_meta_data > temp)) {
            // *is last node
            temp->setNext(new_meta_data);
            new_meta_data->setPrev(temp);
            return;
        }
    }
    else if (!temp->getPrev()) {
        // *adding as first node
        free_list_head = new_meta_data;
        new_meta_data->setNext(temp);
        temp->setPrev(new_meta_data);
        return;
    }
    // *any other case
    new_meta_data->setNext(temp);
    new_meta_data->setPrev(temp->getPrev());
    temp->getPrev()->setNext(new_meta_data);
    temp->setPrev(new_meta_data);

}

MallocMetadata *_findBestFreeBlock(size_t size) {
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

MallocMetadata *_splitBlocks(MallocMetadata *pMetadata, size_t size) {
    //TODO: check the pointer arithmetics
    MallocMetadata *extra_meta_data = (MallocMetadata *) ((size_t) (pMetadata) + size + meta_size);
    size_t size_diff = (size_t) (extra_meta_data - pMetadata);
    *extra_meta_data = MallocMetadata(pMetadata->getSize() - size_diff - meta_size);
    extra_meta_data->setIsFree(true);
    pMetadata->setSize(size);
    //TODO: remove comments
    //memory_stats.num_allocated_blocks++;
    //memory_stats.num_allocated_bytes -= meta_size;
    //memory_stats.num_free_blocks++;
    //memory_stats.num_free_bytes -= meta_size;
    return extra_meta_data;
}

void _mergeAdjacentBlocks(MallocMetadata *pMetadata, bool is_next_free, bool is_prev_free) {
    if (is_next_free && is_prev_free) {
        MallocMetadata *prev_meta = pMetadata->getPrev();
        size_t next_meta_size = pMetadata->getNext()->getSize();
        prev_meta->setSize(prev_meta->getSize() + pMetadata->getSize() + next_meta_size + 2 * meta_size);
        _removeFromBlockList(pMetadata->getNext());
        _removeFromBlockList(pMetadata);
        //TODO: remove comments
        //memory_stats.num_free_blocks -= 2;
        // memory_stats.num_free_bytes += meta_size * 2;
    }
    else if (is_prev_free) {
        // only is_prev_free == true
        MallocMetadata *prev_meta = pMetadata->getPrev();
        prev_meta->setSize(prev_meta->getSize() + pMetadata->getSize() + meta_size);
        _removeFromBlockList(pMetadata);
        //TODO: remove comments
        //memory_stats.num_free_blocks --;
        // memory_stats.num_free_bytes += meta_size;
    }
    else {
        //only is_next_free == true
        size_t next_meta_size = pMetadata->getNext()->getSize();
        pMetadata->setSize(pMetadata->getSize() + next_meta_size + meta_size);
        _removeFromBlockList(pMetadata->getNext());
        //TODO: remove comments
        //memory_stats.num_free_blocks --;
        // memory_stats.num_free_bytes += meta_size;
    }
}

void _removeFromBlockList(MallocMetadata *pMetadata) {
    //function assumes has the list has at least two items, since the only call will be from mergeAdjacent
    //also assumes that the first block won't be removed, since we only remove blocks whom where
    // merged to their prev (or the next merged to current)
    if (!pMetadata->getNext()) {
        //last node
        wilderness_block = pMetadata->getPrev();
        pMetadata->getPrev()->setNext(nullptr);
        pMetadata->getPrev()->setNext(nullptr);
    }
    else {
        pMetadata->getPrev()->setNext(pMetadata->getNext());
        pMetadata->setPrev(nullptr);
        pMetadata->getNext()->setPrev(pMetadata->getPrev());
        pMetadata->setNext(nullptr);
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

/*The Malloc functions starts here*/

void *smalloc(size_t size) {
    if (size <= 0 || size > TOO_BIG) {
        return NULL;
    }

    MallocMetadata *best_free_block = _findBestFreeBlock(size);
    intptr_t *user_start_block;

    if (!best_free_block) {
        intptr_t size_needed;

        if (wilderness_block->isFree()) {
            size_needed = (intptr_t) (size) - (intptr_t) (wilderness_block->getSize());
            void *prev_program_break = sbrk(size_needed);
            if (prev_program_break == (void *) (-1)) {
                return NULL;
            }
            //TODO: remove comments
            //memory_stats.num_allocated_bytes += size_needed;
            wilderness_block->setIsFree(false);
            return (void *) wilderness_block;
        }

        else {
            void *prev_program_break = sbrk((intptr_t) (size) + meta_size);
            if (prev_program_break == (void *) (-1)) {
                return NULL;
            }
            MallocMetadata *new_meta_data = (MallocMetadata *) prev_program_break;
            *new_meta_data = MallocMetadata(size);
            _addToBlockList(new_meta_data);
            user_start_block = (intptr_t *) (prev_program_break);
            memory_stats.num_allocated_blocks++;
            memory_stats.num_allocated_bytes += size;
        }
    }
    else {
        //meaning that there is a freed block big enough for allocation
        if (best_free_block->getSize() >= SPLIT_AMOUNT + meta_size + size) {
            MallocMetadata *new_meta = _splitBlocks(best_free_block, size);
            _addToBlockList(new_meta);
        }
        best_free_block->setIsFree(false);
        user_start_block = (intptr_t *) best_free_block;
        memory_stats.num_free_blocks--;
        memory_stats.num_free_bytes -= best_free_block->getSize();
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
    if (meta->isFree()) {
        return;
    }
    bool is_next_free = meta->getNext() && meta->getNext()->isFree();
    bool is_prev_free = meta->getPrev() && meta->getPrev()->isFree();

    if (is_next_free || is_prev_free) {
        _mergeAdjacentBlocks(meta, is_next_free, is_prev_free);
    }
    memory_stats.num_free_blocks++;
    meta->setIsFree(true);
    memory_stats.num_free_bytes += meta->getSize();
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

