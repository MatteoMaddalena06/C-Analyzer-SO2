#ifndef ANALYZER_H 
#define ANALYZER_H

#include "linearizer.h"
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
    buffer variable_unused_list;
    buffer error_list;
  
};

struct statistics analyze(struct output);
void free_stat(struct statistics*);

#endif 