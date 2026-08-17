#ifndef ANALYZER_H 
#define ANALYZER_H

#include "linearizer.h"
#include "buffer.h"

enum error_type { 
    NAME_ERROR, 
    TYPE_ERROR,
};

struct error {
    char* lexeme;
    enum error_type type;
};

struct statistics {
    unsigned long variable_analyzed;
    buffer variable_unused_list;
    buffer error_list;
    unsigned long type_error_count;
    unsigned long name_error_count;
};

struct statistics analyze(struct output);
void free_stat(struct statistics*);

#endif 