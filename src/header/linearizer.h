#ifndef LINEARIZER_H
#define LINEARIZER_H

#include "buffer.h"
#include <stdio.h>

buffer linearize(FILE*);
void free_linearization(buffer*);

#endif