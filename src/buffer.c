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

void free_buffer(buffer* buffer)
{
    free(buffer->data);
    buffer->head = -1;
    buffer->size = 0;
}