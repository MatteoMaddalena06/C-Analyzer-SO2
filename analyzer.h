#ifndef ANALYZER_H 
#define ANALYZER_H

enum error_type{
    NAME_ERROR, 
    TYPE_ERROR,
};

struct error{
    char* lexeme;
    unsigned int line;
    enum error_type type;
};

struct statistics{
    unsigned int variable_analyzed;
    unsigned int error_found;
    unsigned int variable_unused;
    unsigned int variable_name_uncorrect;
    unsigned int variable_type_uncorrect;
    struct error* error_list;
    unsigned int error_list_size;
    char** variable_unused_list;
    unsigned int variable_unused_list_size;
};

struct statistics analyze(char**);

#endif 