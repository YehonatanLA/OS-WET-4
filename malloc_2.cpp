#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#define TOO_BIG (100000000)

class MallocMetadata {

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
        is_free = true;
        next = nullptr;
        prev = nullptr;

    }
};



class FreeList{
    MallocMetadata* head;
public:
    FreeList();
    ~FreeList();
    void addBlock(MallocMetadata* new_block);
    void removeBlock(MallocMetadata* old_block);
};





/**stats methods (for debu)
 * what is debu?*/
size_t _num_free_blocks()
{

}

size_t _num_free_bytes()
{

}

size_t _num_allocated_blocks()
{

}

size_t _num_allocated_bytes()
{

}

size_t _num_meta_data_bytes()
{

}

size_t _size_meta_data()
{

}



/*The Malloc functions starts here*/

void* smalloc(size_t size)
{

}

void* scalloc(size_t num, size_t size)
{

}

void sfree(void* p)
{

}

void* srealloc(void* oldp, size_t size)
{

}

