#include "header/analyzer.h"
#include "header/buffer.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static unsigned long search_in_array(char** arr, size_t size, char* string)
{
    for(unsigned long i = 0; i<size; i++)
    {
        if(!strcmp(string, arr[i]))
            return i;
    }

    return size;
}

static unsigned long search_var(buffer table, char* string) 
{ return search_in_array((char**)table.data, table.head, string); }

static struct error* create_error(char* lexeme, enum error_type type)
{
    struct error* tmp_ptr = (struct error*)malloc(sizeof(struct error));

    tmp_ptr->lexeme = strdup(lexeme);
    tmp_ptr->type = type;

    return tmp_ptr;
}

static bool is_correct(char* string)
{
    char* c_keywords[] = {
        "auto", "break", "case", "char", "const", "continue", "default",
        "do", "double", "else", "enum", "extern", "float", "for", "goto",
        "if", "int", "long", "register", "return", "short", "signed",
        "sizeof", "static", "struct", "switch", "typedef", "union",
        "unsigned", "void", "volatile", "while"
    };
    
    if (string == NULL || isdigit(string[0]) || search_in_array(c_keywords, 32, string))
        return false;
    
    for(int i = 0; string[i] != '\0'; i++)
    {
        if(!isalnum(string[i]) && string[i] != '_')
            return false;
    }

    return true;
}

static struct statistics init_stat()
{
    struct statistics stat;

    stat.variable_analyzed = 0;
    stat.variable_unused_list = create_buffer(sizeof(char*));
    stat.error_list = create_buffer(sizeof(struct error*));
    
    return stat;
}

enum statement_case {
    DECLARATION_CASE,
    EXPRESSION_CASE,
    TYPEDEF_CASE
};


struct statistics analyze(struct output out)
{
    struct statistics stat = init_stat();

    char** linearization = (char**)out.linearization.data;
    char** type_list = (char**)out.type_list.data;

    buffer var_table = create_buffer(sizeof(char*));
    buffer unused_var = create_buffer(sizeof(char*));

    unsigned long counter = 0;
    enum statement_case stm_case;
    
    for(unsigned long i = 0; i < out.linearization.head; i++)
    {
        if(!strcmp(linearization[i], ";"))
        {
            counter = 0;
            continue;
        }

        if(counter == 0 && !strcmp(linearization[i], "typedef"))
            stm_case = TYPEDEF_CASE;

        else if(counter == 0 && is_type(out.type_list, linearization[i]))
        {
            if(!is_correct(linearization[i]))
            {
                struct error* tmp_ptr = create_error(linearization[i], TYPE_ERROR);
                push_data(&stat.error_list, &tmp_ptr);
            }
            
            stm_case = DECLARATION_CASE;
        } 
        else if(counter == 0)
        {
            if(search_var(var_table, linearization[i]) >= var_table.head)
            {
                char* tmp_ptr = strdup(linearization[i]);
                push_data(&var_table, &tmp_ptr);
            }
            else
                remove_data(&unused_var, search_var(unused_var,linearization[i]));

            stm_case = EXPRESSION_CASE;  
        }

        counter++;
    }

    free_buffer(&var_table);
    free_buffer(&unused_var);

    return stat;
}

void free_stat(struct statistics* stat)
{
    stat->variable_analyzed = 0;

    for(unsigned long i = 0; i < stat->variable_unused_list.head; i++)
        free(((struct error**)stat->variable_unused_list.data)[i]);

    free_buffer(&stat->variable_unused_list);

    for(unsigned long i = 0; i < stat->error_list.head; i++)
        free(((char**)stat->error_list.data)[i]);

    free_buffer(&stat->error_list);
}