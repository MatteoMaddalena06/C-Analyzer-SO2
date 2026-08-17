#include "header/analyzer.h"
#include "header/buffer.h"
#include "header/linearizer.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/*
    Cerca una stringa nell'array arr.
    Input:
        arr = l'array di stringhe 
        size = il numero di elementi nell'array
        string = la stringa 
        len = la lunghezza della stringa
    Output:
        l'indice dove si trova la stringa cercata (se non esiste size)
*/
static unsigned long search_in_array_impl(char** arr, size_t size, char* string, size_t len)
{
    for(unsigned long i = 0; i < size; i++)
    {
        if(strlen(arr[i]) == len && !strncmp(string, arr[i], len))
            return i;
    }

    return size;
}


// wrapper per search_in_array_impl
static unsigned long search_in_array_range(char** arr, size_t size, char* string, unsigned long start, unsigned long end)
{ return search_in_array_impl(arr, size, string + start, end - start); }

// wrapper per search_in_array_impl
static unsigned long search_var(buffer table, char* string) 
{ return search_in_array_impl((char**)table.data, table.head, string, strlen(string)); }

/*
    Memorizza nella la tabella delle variabili una stringa solo se non è presente.
    Input:
        var_table = la tabella delle variabili
        string = la stringa
    Output:
        true se la stringa è stata inserita, false altrimenti
*/
static bool store_if_not_present(buffer* var_table, char* string)
{
    if(search_var(*var_table, string) < var_table->head)
        return false;

    char* tmp_ptr = strdup(string);
    push_data(var_table, &tmp_ptr);

    return true;
}

/*
    Elimina nella tabella delle variabili non usate una stringa solo se non è presente.
    Input:
        unused_var_list = la tabella delle variabili non usate
        string = la stringa 
    Output:
        true se la stringa è stata rimossa, false altrimenti
*/
static bool remove_if_present(buffer* unused_var_list, char* string)
{
    unsigned long index = search_var(*unused_var_list, string);

    if(index >= unused_var_list->head)
        return false;

    free(((char**)unused_var_list->data)[index]);
    remove_data(unused_var_list, index);
    return true;
}

/*
    Controlla se una stringa è un valido identificatore C.
    Input:
        string = la stringa 
        var = la modalità di controllo (true se la stringa rappresenta il nome di una variabile)
    Output:
        true se la stringa è valida, false altrimenti
*/
static bool is_correct(char* string, bool var)
{
    char* c_keywords[] = {
        "auto", "break", "case", "char", "const", "continue", "default",
        "do", "else", "enum", "extern", "for", "goto", "if", "register", 
        "return", "sizeof", "static", "struct", "switch", "typedef", "union",
        "volatile", "while"
    };

    char* std_type[] = {
        "void", "char", "signed char", "unsigned char",          
        "short", "short int", "signed short", "signed short int",       
        "unsigned short", "unsigned short int", "int", "signed", 
        "signed int", "unsigned", "unsigned int", "long", 
        "long int", "signed long", "signed long int", "unsigned long", 
        "unsigned long int", "long long", "long long int", "signed long long",  
        "signed long long int", "unsigned long long", "unsigned long long int", "float",
        "double", "long double", "_Bool",   
    };

    if(string == NULL || isdigit(string[0]))
        return false;

    unsigned long name_start = 0, name_end = strlen(string);

    if(!var)
    {
        unsigned long i;

        for(i = name_end; string[i - 1] == '*' && i > 0; i--);

        if(i > 0)
            name_end = i;

        if(search_in_array_range(std_type, 31, string, 0, name_end) < 31)
            return true;

        unsigned long first_token_end;

        for(first_token_end = 0; string[first_token_end] != ' ' && first_token_end < name_end; first_token_end++);

        if(first_token_end < name_end && 
           first_token_end == 6 && !strncmp("struct", string, first_token_end) || \
           first_token_end == 5 && !strncmp("union", string, first_token_end) || \
           first_token_end == 4 && !strncmp("enum", string, first_token_end))
            name_start = first_token_end + 1;
    }

    if(search_in_array_range(c_keywords, 24, string, name_start, name_end) < 24 || \
       search_in_array_range(std_type, 31, string, name_start, name_end) < 3)
        return false;

    for(unsigned long i = name_start; i < name_end; i++)
    {
        if(!isalnum(string[i]) && string[i] != '_')
            return false;
    }

    return true;
}

/*  
    Crea un errore.
    Input:
        lexeme = la stringa che causa l'errore
        type = il tipo di errore 
    Output:
        il puntatore all'errore allocato dinamicamente
*/
static struct error* create_error(char* lexeme, enum error_type type)
{
    struct error* tmp_ptr = (struct error*)malloc(sizeof(struct error));

    tmp_ptr->lexeme = strdup(lexeme);
    tmp_ptr->type = type;

    return tmp_ptr;
}

/*
    Memorizza una stringa nell'array degli errori se non è corretta (secondo i criteri implementati da is_correct)
    Input:
        stat = il puntatore alle statistiche 
        string = la stringa 
        type = il tipo di errore 
    Output:
        true se la stringa viene memorizzata, false altrimenti
*/
static bool store_if_not_correct(struct statistics* stat, char* string, enum error_type type)
{
    if(is_correct(string, type == NAME_ERROR))
        return false;

    struct error* tmp_ptr = create_error(string, type);
    push_data(&stat->error_list, &tmp_ptr);
    
    if(type == TYPE_ERROR)
        stat->type_error_count++;

    else 
        stat->name_error_count++;

    return true;
}

/*
    Memorizza un errore se non è presente nella lista degli errori.
    Input:
        stat = il puntatore alle statistiche
        string = la stringa 
        type = il tipo di errore
    Output:
        true se l'errore viene memorizzato, false altrimenti
*/
static bool store_error_if_not_present(struct statistics* stat, char* string, enum error_type type)
{
    for(unsigned long i = 0; i < stat->error_list.head; i++)
    {
        struct error tmp = *((struct error**)stat->error_list.data)[i];

        if(!strcmp(tmp.lexeme, string) && tmp.type == type)
            return false;
    }

    struct error* tmp_ptr = create_error(string, type);
    push_data(&stat->error_list, &tmp_ptr);

    return true;
}

//inizializza le statistiche
static struct statistics init_stat()
{
    struct statistics stat;

    stat.variable_analyzed = 0;
    stat.type_error_count = 0;
    stat.name_error_count = 0;
    stat.variable_unused_list = create_buffer(sizeof(char*));
    stat.error_list = create_buffer(sizeof(struct error*));
    
    return stat;
}

//i tipi di blocchi (lista di id compresi fra ';')
enum statement_case {
    DECLARATION_CASE,
    EXPRESSION_CASE,
    TYPEDEF_CASE
};

/*
    Analizza la linearizzazione.
    Input:
        out = l'output prodotto dal linearizzatore
    Output:
        le statistiche
*/
struct statistics analyze(struct output out)
{
    struct statistics stat = init_stat();

    char** linearization = (char**)out.linearization.data;
    char** type_list = (char**)out.type_list.data;

    buffer var_table = create_buffer(sizeof(char*));

    unsigned long counter = 0;
    enum statement_case stm_case;
    
    for(unsigned long i = 0; i < out.linearization.head; i++)
    {
        if(!strcmp(linearization[i], ";"))
        {
            counter = 0;
            continue;
        }

        if(!strcmp(linearization[i], ","))
        {
            counter = 1;
            continue;
        }

        if(!counter && !strcmp(linearization[i], "typedef"))
            stm_case = TYPEDEF_CASE;

        else if(!counter && is_type(out.type_list, linearization[i]))
        {
            if(!is_correct(linearization[i], true))
                store_error_if_not_present(&stat, linearization[i], TYPE_ERROR);

            stm_case = DECLARATION_CASE;
        } 
        else if(!counter || stm_case == EXPRESSION_CASE)
        {
            if(!store_if_not_present(&var_table, linearization[i]))
                remove_if_present(&stat.variable_unused_list,linearization[i]);
            
            else
                stat.variable_analyzed++;

            stm_case = EXPRESSION_CASE;  
        }
        else if(counter && stm_case == TYPEDEF_CASE)
            store_if_not_correct(&stat, linearization[i], TYPE_ERROR);

        else if(counter && stm_case == DECLARATION_CASE)
        {
            if(counter == 1)
            {
                if(!store_if_not_correct(&stat, linearization[i], NAME_ERROR))
                {
                    char* tmp_ptr = strdup(linearization[i]);
                    push_data(&var_table, &tmp_ptr);
 
                    tmp_ptr = strdup(linearization[i]);
                    push_data(&stat.variable_unused_list, &tmp_ptr);
                }

                stat.variable_analyzed++;
            }
            else if(!store_if_not_present(&var_table, linearization[i]))
                remove_if_present(&stat.variable_unused_list,linearization[i]);

            else 
                stat.variable_analyzed++;
        }

        counter++;
    }
    
    for(unsigned long i = 0; i < var_table.head; i++)
        free(((char**)var_table.data)[i]);

    free_buffer(&var_table);

    return stat;
}

/*
    Dealloca le statistiche.
    Input:
        stat = le statistiche
*/
void free_stat(struct statistics* stat)
{
    stat->variable_analyzed = 0;
    stat->name_error_count = 0;
    stat->variable_analyzed = 0;

    for(unsigned long i = 0; i < stat->error_list.head; i++)
    {
        struct error* err = ((struct error**)stat->error_list.data)[i];  
    
        free(err->lexeme);
        free(err);
    }

    free_buffer(&stat->error_list);

    for(unsigned long i = 0; i < stat->variable_unused_list.head; i++)
        free(((char**)stat->variable_unused_list.data)[i]);

    free_buffer(&stat->variable_unused_list);
}
