#include "header/buffer.h"
#include <string.h>

buffer create_buffer(size_t data_size)
{
    buffer buffer;

    buffer.data = malloc(data_size);
    buffer.head = 0;
    buffer.size = 1; 
    buffer.data_size = data_size;

    return buffer;
}

bool push_data(buffer* buffer, void* data)
{
    memcpy((char*)buffer->data + buffer->head * buffer->data_size, data, buffer->data_size);
    buffer->head++;

    if(buffer->head >= buffer->size)
    {
        void* tmp_ptr = realloc(buffer->data, buffer->size * 2 * buffer->data_size);

        if(tmp_ptr == NULL)
            return false;

        buffer->data = tmp_ptr;
        buffer->size *= 2;
    }

    return true;
}

void reset_head(buffer* buffer)
{ buffer->head = 0; }

bool remove_data(buffer* buffer, unsigned long index)
{
    if(index >= buffer->head)
        return false;

    buffer->head--;

    for(unsigned long i = index; i < buffer->head; i++)
        memcpy(
            (char*)buffer->data + i * buffer->data_size, 
            (char*)buffer->data + (i + 1) * buffer->data_size,
            buffer->data_size
        );

    return true;
}

void free_buffer(buffer* buffer)
{
    free(buffer->data);
    buffer->data = NULL;
    buffer->head = -1;
    buffer->size = 0;
    buffer->data_size = 0;
}