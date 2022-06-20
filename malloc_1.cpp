#include <stdio.h>
#include <unistd.h>
#define TOO_BIG (100000000)

void* smalloc(size_t size)
{
    if (size <= 0 || size > TOO_BIG)
    {
        return NULL;
    }
    void* prev_program_break = sbrk((intptr_t)size);

    if(prev_program_break == (void*)(-1))
    {
        return NULL;
    }
    return prev_program_break;


}