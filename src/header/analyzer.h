#ifndef ANALYZER_H 
#define ANALYZER_H

#include "buffer.h"

enum error_type{
    NAME_ERROR, 
    TYPE_ERROR,
};

struct error{
    char* lexeme;
    enum error_type type;
};

struct statistics{
    unsigned int variable_analyzed;
    char** variable_unused_list;
    unsigned int variable_unused;
    struct error* error_list;
    unsigned int variable_name_uncorrect;
    unsigned int variable_type_uncorrect;
};

struct statistics analyze(buffer);
void free_stat(struct statistics*);

#endif 