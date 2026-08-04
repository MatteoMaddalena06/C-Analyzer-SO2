#ifndef LINEARIZER_H
#define LINEARIZER_H

#include "buffer.h"
#include <stdio.h>
#include <stdbool.h>

struct output {
    buffer linearization;
    buffer type_list;
    bool error_occured;
};

bool is_type(buffer, char*);
struct output linearize(FILE*);
void free_output(struct output*);

#endif