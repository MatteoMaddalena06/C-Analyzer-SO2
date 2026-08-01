#include "header/linearizer.h"
#include "header/buffer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

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

static void analyze_expr(buffer statement, unsigned long start, unsigned long end, buffer* linearization)
{
    char* chars = (char*)statement.data;
    unsigned long variable_char_count = 0, space_count = 0;
    bool is_number = false, skip_field = false;

    for(int i = start; i < end; i++)
    {
        if(i - 1 >= start && chars[i - 1] == '.')
            skip_field = true;

        if(i - 2 >= start && chars[i - 1] == '>' && chars[i - 2] == '-')
            skip_field = true;

        if(chars[i] == ' ')
            space_count++;

        else if(!isalnum(chars[i]) && chars[i] != '_')
        {
            if(i - space_count - 1 >= start && (chars[i] == '(' || chars[i] == '&') && chars[i - space_count - 1] == ')')
                move_head(linearization, -1);  

            if(!is_number && variable_char_count != 0 && chars[i] != '(' && !skip_field)
            {
                char* variable_name = strndup(chars + i - space_count - variable_char_count, variable_char_count);
                push_data(linearization, &variable_name);
            }

            variable_char_count = space_count = 0;
            is_number = skip_field = false;
        }
        else 
        {
            if(i - space_count - 1 >= start && chars[i - space_count - 1] == ')')
                move_head(linearization, -1);

            if(isdigit(chars[i]) && variable_char_count == 0)
                is_number = true;

            space_count = 0;
            variable_char_count++;
        }
    }
}

static void analyze_semicolon_statement(buffer statement, buffer* linearization)
{
    /*
        casi ';':
            while
            for 
            return 
            dichiarazioni
            espressioni
            break
            continue
            typedef 
    */ 
}

static void analyze_bracket_statement(buffer statement, buffer* linearization)
{
    /*
        casi '{':
            if 
            else if 
            while 
            do 
            for
            typedef struct/union/enum nome {
            struct/union/enum nome {
            roba };
    */
}

buffer linearize(FILE* in_stream)
{
    buffer linearization = {NULL};
    char curr_char, succ_char, prev_char = 0;

    if((curr_char = fgetc(in_stream)) == EOF)
        return linearization;

    if((succ_char = fgetc(in_stream)) == EOF && ferror(in_stream))
        return linearization;

    buffer statement = create_buffer(sizeof(char));
    linearization = create_buffer(sizeof(char*));

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

        push_data(&statement, &curr_char);

        if(curr_char == ';')
        {
            analyze_semicolon_statement(statement, &linearization);
            reset_head(&statement);
        }
        else if(curr_char == '{')
        {
            analyze_bracket_statement(statement, &linearization);
            reset_head(&statement);
        }
        
        prev_char = curr_char;
        curr_char = succ_char;
    }
    while((succ_char = fgetc(in_stream)) != EOF);

    int tmp = errno;
    free_buffer(&statement);
    errno = tmp;

    if(ferror(in_stream))
        linearization.data = NULL;

    return linearization;
}
    
void free_linearization(buffer* linearization)
{
    for(int i = 0; i < linearization->head - 1; i++)
        free(((char**)linearization->data)[i]);

    free_buffer(linearization);
}

int main(void)
{
    /*FILE* fp = fopen("test.c", "r");

    buffer linearization = linearize(fp);

    for(int i = 0; i < linearization.head; i++)
        printf("%s\n", ((char**)linearization.data)[i]);

    free_linearization(&linearization);
    fclose(fp);*/

    buffer linearization = create_buffer(sizeof(char*));

    buffer statement = {
        "                                   ((int)(*a) + 5 + (int)b + (c + d / 32) + foo(a, b, std_45) + a->r + sizeof(int + a)) {"
    };

    analyze_expr(statement, 0, 122, &linearization);

    for(int i = 0; i < linearization.head; i++)
        printf("%s\n", ((char**)linearization.data)[i]);

    free_linearization(&linearization);
}