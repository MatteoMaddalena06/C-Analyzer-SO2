#include "header/linearizer.h"
#include "header/buffer.h"
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

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

static bool is_type_impl(buffer type_list, char* id, size_t len)
{
    char** types = (char**)type_list.data;
    unsigned long i;

    for(i = 0; i < len && id[i] != '*'; i++);

    if(i != len && i >= 1 && id[i - 1] == ' ')
        i--;
    
    for(int j = 0; j < type_list.head; j++)
    {
        if(strlen(types[j]) == i && !strncmp(types[j], id, i))
            return true;
    }

    return false;
}

bool is_type(buffer type_list, char* id)
{ return is_type_impl(type_list, id, strlen(id)); }

static bool is_type_range(struct output output, buffer statement, unsigned long start, unsigned long end)
{ return is_type_impl(output.type_list, (char*)statement.data + start, end - start); }

static void store_type(struct output* output, buffer statement, unsigned long start, unsigned long end) 
{
    char* tmp_ptr = strndup((char*)statement.data + start, end - start);
    push_data(&output->type_list, &tmp_ptr);
}

static struct output init_output()
{
    char* std_type[] = {
        "void",                  
        "char",                   
        "signed char",            
        "unsigned char",          
        "short",                  
        "short int",              
        "signed short",          
        "signed short int",       
        "unsigned short",         
        "unsigned short int",     
        "int",                    
        "signed",                  
        "signed int",             
        "unsigned",               
        "unsigned int",           
        "long",                   
        "long int",               
        "signed long",            
        "signed long int",        
        "unsigned long",          
        "unsigned long int",     
        "long long",              
        "long long int",          
        "signed long long",      
        "signed long long int",   
        "unsigned long long",     
        "unsigned long long int", 
        "float",                 
        "double",                 
        "long double",           
        "_Bool",                  
    };

    buffer type_buffer = create_buffer(sizeof(char*));

    for(int i = 0; i < 31; i++)
    {
        char* tmp_ptr = strdup(std_type[i]);
        push_data(&type_buffer, &tmp_ptr);
    }

    struct output out = {
        create_buffer(sizeof(char*)),
        type_buffer,
        false
    };

    return out;
}

static void analyze_expr(buffer statement, unsigned long start, unsigned long end, struct output* output)
{
    char* chars = (char*)statement.data;
    unsigned long variable_char_count = 0, space_count = 0, start_bracket, brackets_count = 0;
    bool not_var = false;

    for(int i = start; i < end; i++)
    {
        if(chars[i] == ')' && brackets_count)
        {
            brackets_count--;

            if(!brackets_count && !is_type_range(*output, statement, start_bracket + 1, i))
                analyze_expr(statement, start_bracket + 1, i + 1, output);
        }
        else if(chars[i] == '(')
        {
            if(brackets_count == 0) 
                start_bracket = i;

            brackets_count++;
            variable_char_count = space_count = 0;
            not_var = false;
        }

        if(brackets_count != 0)
            continue;

        if(i >= start + 1 && chars[i - 1] == '.')
            not_var = true;

        if(i >= start + 2 && chars[i - 1] == '>' && chars[i - 2] == '-')
            not_var = true;

        if(chars[i] == ' ')
            space_count++;

        else if(!isalnum(chars[i]) && chars[i] != '_')
        {
            if(!not_var && variable_char_count != 0)
            {
                char* variable_name = strndup(chars + i - space_count - variable_char_count, variable_char_count);
                push_data(&output->linearization, &variable_name);
            }

            variable_char_count = space_count = 0;
            not_var = false;
        }
        else 
        {
            if(isdigit(chars[i]) && variable_char_count == 0)
                not_var = true;

            space_count = 0;
            variable_char_count++;
        }
    }
}

static unsigned long get_statement_start(buffer statement)
{
    char* chars = (char*)statement.data;
    unsigned long i;

    for(i = 0; !isalnum(chars[i]); i++);

    return i;
}

static unsigned long find_char(buffer statement, char c, unsigned long start)
{
    char* chars = (char*)statement.data;
    unsigned long i;

    for(i = start; chars[i] != c && i < statement.head; i++);

    return i;
}

static unsigned long extract_token_from_start(buffer statement, unsigned long start)
{
    char* chars = (char*)statement.data;
    unsigned long i;

    for(i = start; isalnum(chars[i]) && i < statement.head; i++);

    return i;
}

static unsigned long extract_token_from_end(buffer statement, unsigned long end)
{
    char* chars = (char*)statement.data;
    unsigned long i;

    for(i = end; i > 0 && isalnum(chars[i - 1]); i--);

    return i;
}

static bool compare(buffer statement, char* string, unsigned long start, unsigned long end)
{
    char* chars = (char*)statement.data + start;
    size_t string_size;

    for(string_size = 0; string[string_size] != '\0'; string_size++)
    {
        if(string[string_size] != chars[string_size])
            return false;
    }

    return string_size == (end - start);
}

static bool is_condition_token(buffer statement, unsigned long start, unsigned long end)
{
    char* condition_tokens[] = {
        "if",
        "while",
        "for",
        "switch"
    };

    for(int i = 0; i < 4; i++)
    {
        if(compare(statement, condition_tokens[i], start, end))
            return true;
    }

    return false;
}

static void analyze_declaration(buffer statement, unsigned long start, unsigned long end)
{
    //TODO
}

static void analyze_for_statement(buffer statement) 
{
    //TODO
}

static void analyze_composite_type_statement(buffer statement, unsigned long statement_start, struct output* output)
{
    char* chars = (char*)statement.data;
    unsigned long last_token_end = statement.head - ((chars[statement.head - 2] == ' ') ? 2 : 1);
    unsigned long last_token_start = extract_token_from_end(statement, last_token_end);

    char* tmp_ptr = strndup(chars + statement_start, last_token_start - statement_start - 1);
    push_data(&output->linearization, &tmp_ptr);

    tmp_ptr = strndup(chars + last_token_start, last_token_end - last_token_start);
    push_data(&output->linearization, &tmp_ptr);

    tmp_ptr = strdup(";");
    push_data(&output->linearization, &tmp_ptr);

    store_type(output, statement, last_token_start, last_token_end);
}

static void analyze_typedef_statement(buffer statement, unsigned long statement_start, unsigned first_token_end, struct output* output)
{
    char* chars = (char*)statement.data;
    unsigned long last_token_end = statement.head - ((chars[statement.head - 2] == ' ') ? 2 : 1);
    unsigned long last_token_start = extract_token_from_end(statement, last_token_end);

    char* tmp_ptr = strdup("typedef");
    push_data(&output->linearization, &tmp_ptr);

    tmp_ptr = strndup(chars + first_token_end + 1, last_token_start - first_token_end - 2);
    push_data(&output->linearization, &tmp_ptr);

    tmp_ptr = strndup(chars + last_token_start, last_token_end - last_token_start);
    push_data(&output->linearization, &tmp_ptr);

    tmp_ptr = strdup(";");
    push_data(&output->linearization, &tmp_ptr);

    store_type(output, statement, last_token_start, last_token_end);
}

static void analyze_semicolon_statement(buffer statement, struct output* output)
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

static void analyze_bracket_statement(buffer statement, struct output* output)
{
    char* chars = (char*)statement.data;
    unsigned long statement_start = get_statement_start(statement);
    unsigned long first_token_end = extract_token_from_start(statement, statement_start);

    if(compare(statement, "typedef", statement_start, first_token_end))
    {
        analyze_typedef_statement(statement, statement_start, first_token_end, output);
        return;
    }
    
    if(compare(statement, "struct", statement_start, first_token_end) || \
        compare(statement, "union", statement_start, first_token_end) || \
        compare(statement, "enum", statement_start, first_token_end))
    {
        analyze_composite_type_statement(statement, statement_start, output);
        return;
    }

    unsigned long bracket_pos = find_char(statement, '(', statement_start);

    if(bracket_pos >= statement.head)
        return;

    unsigned long prebracket_token_start = extract_token_from_end(
        statement, bracket_pos - ((chars[bracket_pos - 1] == ' ') ? 1 : 0));

    if(!is_condition_token(statement, prebracket_token_start, bracket_pos))
    {
        unsigned long declaration_start = bracket_pos + 1;
        unsigned long declaration_end = find_char(statement, ',', declaration_start);

        while(declaration_end < statement.head)
        {
            analyze_declaration(statement, declaration_start, declaration_end);
            declaration_start = declaration_end + 1;
            declaration_end = find_char(statement, ',', declaration_start);
        }
    }
    else if(compare(statement, "for", prebracket_token_start, bracket_pos)) 
        analyze_for_statement(statement);

    else
        analyze_expr(statement, bracket_pos, statement.head, output);
}

struct output linearize(FILE* in_stream)
{
    struct output output = init_output();
    char curr_char, succ_char, prev_char = 0;

    if((curr_char = fgetc(in_stream)) == EOF || \
       (succ_char = fgetc(in_stream)) == EOF && ferror(in_stream))
    {
        output.error_occured = true;
        return output;
    }

    buffer statement = create_buffer(sizeof(char));

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
            analyze_semicolon_statement(statement, &output);
            reset_head(&statement);
        }
        else if(curr_char == '{')
        {
            analyze_bracket_statement(statement, &output);
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
        output.error_occured = true;

    return output;
}
    
void free_output(struct output* output)
{
    for(int i = 0; i < output->linearization.head; i++)
        free(((char**)output->linearization.data)[i]);

    free_buffer(&output->linearization);

    for(int i = 0; i < output->type_list.head; i++)
        free(((char**)output->type_list.data)[i]);

    free_buffer(&output->type_list);
}

//TEST
int main(void)
{
    FILE* fp = fopen("test.c", "r");

    struct output out = linearize(fp);

    for(int i = 0; i < out.linearization.head; i++)
        printf("%s\n", ((char**)out.linearization.data)[i]);

    printf("\n");

    for(int i = 0; i < out.type_list.head; i++)
        printf("%s\n", ((char**)out.type_list.data)[i]);  

    free_output(&out);
}