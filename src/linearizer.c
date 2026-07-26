#include "header/linearizer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

enum skip_type {
    NEWLINE_END,
    COMMENT_END,
    STRING_END,
    CHAR_END
};

struct skip {
    bool skip_selected;
    enum skip_type type;
    unsigned int backslash_number;
};

static void set_skip(char curr_char, char succ_char, char prev_char, struct skip* skip)
{
    if(!skip->skip_selected)
    {
        if(curr_char == '#' || (curr_char == '/' && succ_char == '/'))
        {
            skip->skip_selected = true;
            skip->type = NEWLINE_END;
        }
        else if(curr_char == '/' && succ_char == '*')
        {
            skip->skip_selected = true;
            skip->type = COMMENT_END;
        }
        else if(curr_char == '"')
        {
            skip->skip_selected = true;
            skip->backslash_number = 0;
            skip->type = STRING_END;
        }
        else if(curr_char == '\'')
        {
            skip->skip_selected = true;
            skip->backslash_number = 0;
            skip->type = CHAR_END;
        }
    }
    else 
    {
        if((skip->type == NEWLINE_END && curr_char == '\n') || (skip->type == COMMENT_END && curr_char == '/' && prev_char == '*') || \
           (skip->type == STRING_END && curr_char == '"' && skip->backslash_number % 2 == 0) || \
           (skip->type == CHAR_END && curr_char == '\'' && skip->backslash_number % 2 == 0))
            skip->skip_selected = false;

        if(skip->type == STRING_END || skip->type == CHAR_END)
            skip->backslash_number = (curr_char == '\\') ? skip->backslash_number + 1 : 0;
    }
}

static bool skip_end_delimiter(char curr_char, char prev_char)
{
    switch(curr_char)
    {
        case '"':  return true;
        case '\'': return true; 
        case '/':  return prev_char == '*';
    }

    return false;
} 

char** linearize(FILE* in_stream)
{
    char curr_char, succ_char, prev_char = 0;

    if((curr_char = fgetc(in_stream)) == EOF)
        return NULL;

    if((succ_char = fgetc(in_stream)) == EOF && ferror(in_stream))
        return NULL;

    int statement_head = 0, statement_size = 1;
    char* statement = (char*)malloc(sizeof(char));

    struct skip skip = {false};
    bool skip_space = false;

    do
    {
        set_skip(curr_char, succ_char, prev_char, &skip);

        if(skip.skip_selected || skip_end_delimiter(curr_char, prev_char) || \
            (isspace(curr_char) && curr_char != ' ') || (curr_char == ' ' && skip_space))
        {
            prev_char = curr_char;
            curr_char = succ_char;
            continue; 
        }  

        skip_space = (curr_char == ' ');

        if(curr_char != ';' && curr_char != '{')
        {            
            statement[statement_head++] = curr_char;

            if(statement_head >= statement_size)
            {
                statement_size *= 2;
                statement = (char*)realloc(statement, statement_size);
            }
        }
        else
        {
            //qua lo statement è costruiro
        }  
        
        prev_char = curr_char;
        curr_char = succ_char;
    }
    while((succ_char = fgetc(in_stream)) != EOF);

    free(statement);

    if(ferror(in_stream))
        return NULL;
}
    
void free_linearization(char** linearization)
{
    for(int i = 0; linearization[i] != NULL; i++)
        free(linearization[i]);

    free(linearization);
}