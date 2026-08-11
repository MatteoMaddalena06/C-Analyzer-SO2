#include "header/linearizer.h"
#include "header/buffer.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

/* 
    variabili globali per gestire i typedef spezzati.
    Se pending_typedef = true, allora pending_name da considerare
    solo se != NULL; altrimenti ignorare
*/
static char* pending_name = NULL;
static bool pending_typedef = false;

/* 
    usata per gestire i for:
    conra il numero di statement fra le parentesi 
    del for ancora da elaborare 
*/
static int for_body_count = 0;

//enum per i tipi di blocchi da saltare
enum skip_type {
    NEWLINE_END,
    COMMENT_END,
    STRING_END,
    CHAR_END
};


//struct per le informazioni sul blocco da saltare
struct skip {
    bool skip_selected;
    enum skip_type type;
    unsigned int backslash_number;
};

/*
    Comunica se il carattere corrente è da saltare.
    Input:
        curr_char = carattere corrente
        succ_char = carattere successivo 
        prev_char = crattere precedente
        skip = puntatore alla struttura skip da modificare
    Output:
        se il carattere corrente deve essere saltato pone skip.skip_selected = true,
        altrimenti skip.skip_selected = false
*/
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

/*  
    Comunica se il carattere corrente fa parte di un delimitatore di 
    una sequenza di caratteri da saltare.
    Input:
        curr_char = carattere corrente
        prev_char = crattere precedente
    Output:
        true se è vero, false altrimenti
*/
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

/*
    Usata per memorizzare nella linearizzazione il separatore ("," o ";").
    Input:
        linearization = il puntatore alla linearizzazione 
        semicolon = il tipo di separatore (true per ";", false per ",")
 */
static  void store_separator(buffer* linearization, bool semicolon)
{
    char* tmp_ptr = (semicolon) ? strdup(";") : strdup(",");
    push_data(linearization, &tmp_ptr);
}

/*  
    Comunica se una stringa è parte della tabella dei tipi.
    Input:
        type_list = la tabella dei tipi
        id = la stringa
        len = la lunghezza della stringa
    Output:
        true se id è in type_list, false altrimenti
*/
static bool is_type_impl(buffer type_list, char* id, size_t len)
{
    char** types = (char**)type_list.data;
    int i;

    for(i = len - 1; id[i] == '*' || id[i] == ' '; i--);

    i += 1;

    for(int j = 0; j < type_list.head; j++)
    {
        if(strlen(types[j]) == i && !strncmp(types[j], id, i))
            return true;
    }

    return false;
}

//wrapper publico per la funzione is_type_impl
bool is_type(buffer type_list, char* id)
{ return is_type_impl(type_list, id, strlen(id)); }

//wrapper privato per la funzione is_type_impl
static bool is_type_range(struct output output, buffer statement, unsigned long start, unsigned long end)
{ return is_type_impl(output.type_list, (char*)statement.data + start, end - start); }

/*
    Memorizza una stringa nella tabella del tipi.
    Input:
        output = il puntatore all'output del linearizzatore
        statement = lo statement
        start = l'inizio della sottostringa nello statement (inclusivo)
        end = la fine della sottostringa nello statement (esclusiva)
*/
static void store_type(struct output* output, buffer statement, unsigned long start, unsigned long end) 
{
    char* tmp_ptr = strndup((char*)statement.data + start, end - start);
    push_data(&output->type_list, &tmp_ptr);
}

//inizializza l'output del linearizzatore
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

/*
    Estrae da un'espressione le variabili in essa contenute.
    Input:
        statement = lo statement
        start = l'inizioe dell'espressione nello statement (inclusivo)
        end = la fine dell'espressione nello statement (esclusiva)
        output = il puntatore all'output del linearizzatore

    Ipotesi: l'espressione deve essere C valida
*/
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

/*
    Corregge l'indice per gestire gli spazi
    Input:
        statement = lo statement
        index = l'indice 
        next = true se bisogna spostare l'indice in avanti, false altrimenti
    Output:
        l'indice corretto
*/
static unsigned long adjust_index(buffer statement, unsigned long index, bool next)
{
    char* chars = (char*)statement.data;

    if(next)
        return index + ((index + 1 < statement.head && chars[index + 1] == ' ') ? 2 : 1);

    else 
        return index + ((index - 1 >= 0 && chars[index - 1] == ' ') ? -1 : 0);
}

/*
    Restituisce l'inizio dello statement (ignora la spazzatura all'inizio).
    Input:
        statement = lo statement
    Output:
        l'indice in cui inizia lo statement (inclusivo)
*/
static unsigned long get_statement_start(buffer statement)
{
    char* chars = (char*)statement.data;
    unsigned long i;

    for(i = 0; chars[i] != ':' && i < statement.head; i++);

    if(i >= statement.head)
        i = 0;

    else 
        i++;

    for(; !isalnum(chars[i]) && chars[i] != '_' && chars[i] != '(' && i < statement.head; i++);

    return i;
}

/*
    Cerca un carattere nello statement.
    Input:
        statement = lo statement
        start = l'indice (inclusivo) da cui iniziare la ricerca
    Output:
        se il carattere cercato è presente, restituisce il suo indice, altrimenti statement.head
*/
static unsigned long find_char(buffer statement, char c, unsigned long start)
{
    char* chars = (char*)statement.data;
    unsigned long i;

    for(i = start; chars[i] != c && i < statement.head; i++);

    return i;
}

/*
    Estrae un token partendo dal suo inizio.
    Input:
        statement = lo statement
        start = l'indice (inclusivo) da cui iniziare l'estrazione
    Output:
        l'indice (esclusivo) di fine token
*/
static unsigned long extract_token_from_start(buffer statement, unsigned long start)
{
    char* chars = (char*)statement.data;
    unsigned long i;

    for(i = start; (isalnum(chars[i]) || chars[i] == '_') && i < statement.head; i++);

    return i;
}

/*
    Estrae un token partendo dalla sua fine.
    Input:
        statement = lo statement
        end = l'indice (esclusivo) da cui iniziare l'estrazione
    Output:
        l'indice (inclusivo) di inizio token
*/
static unsigned long extract_token_from_end(buffer statement, unsigned long end)
{
    char* chars = (char*)statement.data;
    unsigned long i;

    for(i = end; i > 0 && (isalnum(chars[i - 1]) || chars[i - 1] == '_'); i--);

    return i;
}

/*
    Confronta una sottostringa dello statement con una stringa data.
    Input:
        statement = lo statement
        string = la stringa data
        start = l'indice (inclusivo) di inizio delle sottostringa
        end = l'indice (esclusivo) di fine della sottostringa
    Output:
        true se statement[start:end] == string, false altrimenti
*/
static bool compare(buffer statement, char* string, unsigned long start, unsigned long end)
{
    char* chars = (char*)statement.data + start;
    size_t string_size;

    for(string_size = 0; string[string_size] != '\0'; string_size++)
    {
        if(string_size >= end - start)
            return false;

        if(string[string_size] != chars[string_size])
            return false;
    }

    return string_size == (end - start);
}

/*
    Controlla se il token è un costrutto condizionale.
    Input:
        statement = lo statement
        start = l'indice (inclusivo) di inizio token
        end = l'indice (esclusivo) di fine token
    Output:
        true se è un costrutto condizionale, false altrimenti
*/
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

/*
    Cerca un tipo nello statement[start:end].
    Input:
        statement = lo statement
        start = l'indice (inclusivo) della sottostringa in cui cercare 
        end = l'indice (esclusivo) della sottostringa in cui cercare 
        output = l'output del linearizzatore
    Output:
        l'indice (esclusivo) di fine tipo
*/
static unsigned long find_type(buffer statement, unsigned long start, unsigned long end, struct output output)
{
    char* chars = (char*)statement.data;
    unsigned long type_end = end;

    while(!is_type_range(output, statement, start, type_end))
    {
        type_end = extract_token_from_end(statement, type_end);

        if(type_end <= start)
            return start;

        type_end--;
    }

    return type_end;
}

/*
    Estrae una sottodichiarazione da una sequenza di dichiarazioni.
    Input:
        statement = lo statement
        start = l'indice (inclusivo) di inizio della dichiarazione
        function = true per le dichiarazioni di funzioni, false altrimenti
    Output:
        l'indice (esclusivo) di fine della sottodichiarazione
*/
static unsigned long extract_subdeclaration(buffer statement, unsigned long start, bool function)
{
    char* chars = (char*)statement.data;
    unsigned long i;

    for(i = start; chars[i] != ';' && chars[i] != ',' && chars[i] != '=' && (!function || chars[i] != ')') && i < statement.head; i++);

    return i;
}

/*  
    Cerca la fine di un'espressione in una dichiarazione.
    Input:
        statement = lo statement
        start = l'indice (inclusivo) di inizio dell'espressione
    Output:
        l'indice (esclusivo) di fine dell'espressione
*/
static unsigned long find_declaration_expr_end(buffer statement, unsigned long start)
{
    char* chars = (char*)statement.data;
    unsigned long count_brackets = 0, i;

    for(i = start; i < statement.head && (chars[i] != ',' || count_brackets != 0) && chars[i] != ';'; i++)
    {
        if(chars[i] == '(')
            count_brackets++;

        else if(chars[i] == ')')
            count_brackets--;
    }

    return i;
}

/*
    Estrae le variabili da una dichiarazione.
    Input:
        statement = lo statement
        start = l'indice (inclusivo) di inizio dichiarazione
        end = l'indice (esclusivo) di fine dichiarazione
        output = il puntatore all'output del linearizzatore
        function = true per le dichiarazioni di funzione, false altrimenti

    Ipotesi: 
        1. i nomi dei tipi e delle variabili non possono contenere '{' o ';'; non sono ammessi neanche spazi all'inzio e alla fine
           del nome (vengono considerati come separatori) e '*' all'inzio del nome di una variabile (vengono resi parte del nome della variabile)
        2. non sono ammesse dichiarazioni che contengono '{'. Ad esempio int arr[] = {1, 2, 3} non viene analizzato bene
*/
static void analyze_declaration(buffer statement, unsigned long start, unsigned long end, struct output* output, bool function)
{
    char* chars = (char*)statement.data;
    unsigned long subdecl_start = start; 
    unsigned long subdecl_end = extract_subdeclaration(statement, subdecl_start, function);
    char* tmp_ptr;
    bool type_present = true;

    if(function && subdecl_end - subdecl_start <= 1)
        return;

    while(subdecl_end < statement.head)
    {
        unsigned long name_start = subdecl_start;

        if(type_present)
        {
            unsigned long type_end = find_type(
                statement, subdecl_start, adjust_index(statement, subdecl_end, false), *output);

            tmp_ptr = strndup(chars + subdecl_start, type_end - subdecl_start);
            push_data(&output->linearization, &tmp_ptr);

            name_start = type_end + 1;
        }

        tmp_ptr = strndup(chars + name_start, adjust_index(statement, subdecl_end, false) - name_start);
        push_data(&output->linearization, &tmp_ptr);

        if(chars[subdecl_end] == '=')
        {
            unsigned long end_expr = find_declaration_expr_end(statement, subdecl_end + 1);
            analyze_expr(statement, subdecl_end + 1, end_expr + 1, output);
            subdecl_start = adjust_index(statement, end_expr, true);

            if(chars[end_expr] == ',')
            {
                type_present = false;
                store_separator(&output->linearization, false);
            }
        }
        else
        {
            subdecl_start = adjust_index(statement, subdecl_end, true);

            if(chars[subdecl_end] == ',')
            {
                type_present = function;
                store_separator(&output->linearization, function);
            }
        }

        subdecl_end = extract_subdeclaration(statement, subdecl_start, function);
    }

    store_separator(&output->linearization, true);
} 

/*
    Gestisce i tipi composti (struct/enum/union).
    Input:
        statement = lo statement
        statement_start = l'inizio (inclusivo) reale dello statement
        output = il puntatore all'output del linearizzatore
        pending = true per i typedef spezzati, false altrimenti
*/
static void analyze_composite_type(buffer statement, unsigned long statement_start, struct output* output, bool pending)
{
    char* chars = (char*)statement.data;
    unsigned long last_token_end = adjust_index(statement, statement.head - 1, false);

    if(compare(statement, "struct", statement_start, last_token_end) || \
       compare(statement, "union", statement_start, last_token_end) || \
       compare(statement, "enum", statement_start, last_token_end))
    {
        if(pending)
        {
            pending_typedef = true;
            pending_name = NULL;
        }

        return;
    }
    
    store_type(output, statement, statement_start, last_token_end);

    char* tmp_ptr = strndup(chars + statement_start, last_token_end - statement_start);

    if(pending)
    {
        pending_typedef = true;
        pending_name = tmp_ptr;
        return;
    }

    push_data(&output->linearization, &tmp_ptr);

    store_separator(&output->linearization, true);
}

/*
    Analizza i typedef che terminano per ';'.
    Input:
        statement = lo statement
        statement_start = l'inizio (inclusivo) reale dello statement
        first_token_end = la fine (esclusiva) del primo token dello statement
        output = il puntatore all'output del linearizzatore
*/
static void analyze_typedef_statement(buffer statement, unsigned long statement_start, unsigned first_token_end, struct output* output)
{
    char* chars = (char*)statement.data;
    unsigned long name_end = adjust_index(statement, statement.head - 1, false);
    unsigned long type_end = find_type(statement, first_token_end + 1, name_end, *output);

    char* tmp_ptr = strdup("typedef");
    push_data(&output->linearization, &tmp_ptr);

    tmp_ptr = strndup(chars + first_token_end + 1, type_end - first_token_end - 1);
    push_data(&output->linearization, &tmp_ptr);

    tmp_ptr = strndup(chars + type_end + 1, name_end - type_end - 1);
    push_data(&output->linearization, &tmp_ptr);

    store_separator(&output->linearization, true);

    store_type(output, statement, type_end + 1, name_end);
}

/*
    Gestisce tutti gli statement che terminano per ';'.
    Input:
        statement = lo statement
        output = il puntatore all'output del linearizzatore
*/
static void analyze_semicolon_statement(buffer statement, struct output* output)
{
    char* chars = (char*)statement.data;
    unsigned long statement_start = get_statement_start(statement);
    unsigned long first_token_end = extract_token_from_start(statement, statement_start);
    char* tmp_ptr;

    if(statement_start >= statement.head)
        return;

    if(for_body_count > 0)
    {
        analyze_expr(statement, statement_start, statement.head, output);
        store_separator(&output->linearization, true);
        for_body_count--;

        return;
    }

    if((chars[0] == '}' || (statement.head >= 2 && chars[1] == '}')) && pending_typedef) 
    {
        tmp_ptr = strdup("typedef");
        push_data(&output->linearization, &tmp_ptr);

        if(pending_name != NULL)
            push_data(&output->linearization, &pending_name);

        unsigned long name_end = adjust_index(statement, statement.head - 1, false);
        tmp_ptr = strndup(chars + statement_start, name_end - statement_start);
        push_data(&output->linearization, &tmp_ptr);

        store_separator(&output->linearization, true);

        store_type(output, statement, statement_start, name_end);
        pending_typedef = false;

        return;
    }
    
    if(compare(statement, "while", statement_start, first_token_end) || \
        compare(statement, "return", statement_start, first_token_end))
    {
        analyze_expr(statement, first_token_end, statement.head, output);
        store_separator(&output->linearization, true);

        return;
    }
    
    if(compare(statement, "typedef", statement_start, first_token_end))
    {
        analyze_typedef_statement(statement, statement_start, first_token_end, output);
        return; 
    }

    if(compare(statement, "continue", statement_start, first_token_end) || \
       compare(statement, "break", statement_start, first_token_end) || \
       compare(statement, "goto", statement_start, first_token_end))
        return;

    if(compare(statement, "for", statement_start, first_token_end))
    {
        unsigned long bracket_pos = find_char(statement, '(', first_token_end);
        statement_start = adjust_index(statement, bracket_pos, true);

        for_body_count = 2;
    }

    if(find_type(statement, statement_start, statement.head, *output) <= statement_start)
    {
        analyze_expr(statement, statement_start, statement.head, output);
        store_separator(&output->linearization, true);
    }
    else 
        analyze_declaration(statement, statement_start, statement.head, output, false);
}

/*
    Gestisce tutti gli statement che terminano per '{'.
    Input:
        statement = lo statement
        output = il puntatore all'output del linearizzatore
*/
static void analyze_bracket_statement(buffer statement, struct output* output)
{
    char* chars = (char*)statement.data;
    unsigned long statement_start = get_statement_start(statement);
    unsigned long first_token_end = extract_token_from_start(statement, statement_start);

    if(statement_start >= statement.head)
        return;

    if(for_body_count > 0)
    {
        analyze_expr(statement, statement_start, statement.head, output);
        store_separator(&output->linearization, true);

        for_body_count--;

        return;
    }

    if(compare(statement, "typedef", statement_start, first_token_end))
    {
        analyze_composite_type(statement, first_token_end + 1, output, true);
        return;
    }

    if(compare(statement, "struct", statement_start, first_token_end) || \
        compare(statement, "union", statement_start, first_token_end) || \
        compare(statement, "enum", statement_start, first_token_end))
    {
        analyze_composite_type(statement, statement_start, output, false);
        return;
    }

    unsigned long bracket_pos = find_char(statement, '(', statement_start);

    if(bracket_pos >= statement.head)
        return;
    
    unsigned long prebracket_token_start = extract_token_from_end(
        statement, adjust_index(statement, bracket_pos, false));

    if(is_condition_token(statement, prebracket_token_start, bracket_pos))
    {
        analyze_expr(statement, bracket_pos, statement.head, output);
        store_separator(&output->linearization, true);
    }
    else
        analyze_declaration(statement, adjust_index(statement, bracket_pos, true), statement.head, output, true);
}

/*
    Elabora l'intero file dividendolo in statement (sequenze di caratteri terminanti per ';' o '{').
    Input:
        in_stream = lo stream di input del file
    Output:
        la struct output popolata
*/
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

/*
    Dealloca l'output (linearizzazione + tabella dei tipi).
    Input:
        output = il puntatore all'output del linearizzatore
*/
void free_output(struct output* output)
{
    for(int i = 0; i < output->linearization.head; i++)
        free(((char**)output->linearization.data)[i]);

    free_buffer(&output->linearization);

    for(int i = 0; i < output->type_list.head; i++)
        free(((char**)output->type_list.data)[i]);

    free_buffer(&output->type_list);
}

/*TEST
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
}*/

/*
    Lista delle ipotesi:
        1.  il codice è sintatticamente e semanticamente corretto tranne nel nome delle variabili e dei tipi
        2. gli identificatori di variabili e tipi non possono iniziare e finire con gli spazi (vegnono considerati spazi separatori) o finire con '*' 
            (vengono considerati puntatori al tipo).
        3.  le espressioni sono ben scritte (non possono quindi contenere tipo o variabili mal formattati)
        4.  le parentesi graffe nelle dichiarazioni non sono ammesse
        5.  i nomi delle variabili e dei tipi non possono essere composti da '{' o ';'
        6.  gli usi dei campi dei tipi composti sono ignorati
        7.  l'operatore ternario non viene gestito
        8.  sono ignorati costrutti avanzati del linguaggio presenti nelle ultime versioni
        9.  vengono gestiti solo i tipi interni al file; tutti gli altri vengono trattati come variabili
        10. le MACRO non sono gestite e perciò vengono considerate variabili
        11. niente shadowing delle variabili
        12. non sono ammessi costrutti condizinoali senza parentesi graffe
*/