#ifndef LINEARIZER_H
#define LINEARIZER_H

#include <stdio.h>

char** linearize(FILE*);
void free_linearization(char**);

#endif