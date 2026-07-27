#ifndef BUFFER_H 
#define BUFFER_H 

#include <stdlib.h>
#include <stdbool.h>

typedef struct {
    void* data;
    size_t head;
    size_t size;
    size_t data_size;

} buffer;

buffer create_buffer(size_t);
bool push_data(buffer*, void*);
void free_buffer(buffer*);

#endif