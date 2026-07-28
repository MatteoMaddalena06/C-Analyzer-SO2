#include "header/analyzer.h"
#include <stdlib.h>

void free_stat(struct statistics* stat)
{
    unsigned int error_list_size = \
        stat->variable_name_uncorrect + stat->variable_type_uncorrect;

    for(int i = 0; i < error_list_size; i++)
        free(stat->error_list[i].lexeme);

    free(stat->error_list);
    stat->error_list = NULL;
    stat->variable_name_uncorrect = 0;
    stat->variable_type_uncorrect = 0;

    for(int i = 0; i < stat->variable_unused; i++)
        free(stat->variable_unused_list[i]);

    free(stat->variable_unused_list);
    stat->variable_unused_list = NULL;
    stat->variable_unused = 0;

    stat->variable_analyzed = 0;
}