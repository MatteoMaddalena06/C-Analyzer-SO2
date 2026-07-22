#ifndef LINEARIZER_H
#define LINEARIZER_H
#include <stdbool.h>

#include <stdio.h>

char** linearize(FILE*);
void free_linearization(char**);
bool TypePresent(char*, char**);

#endif