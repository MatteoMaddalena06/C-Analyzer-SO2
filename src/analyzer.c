#include "header/analyzer.h"
#include <stdlib.h>

void free_stat(struct statistics* stat)
{
    for(int i = 0; i < stat->error_list_size; i++)
        free(stat->error_list[i].lexeme);

    free(stat->error_list);
    stat->error_list = NULL;
    stat->error_list_size = 0;

    for(int i = 0; i < stat->variable_unused_list_size; i++)
        free(stat->variable_unused_list[i]);

    free(stat->variable_unused_list);
    stat->variable_unused_list = NULL;
    stat->variable_unused_list_size = 0;
}