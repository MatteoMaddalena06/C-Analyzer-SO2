#include <stdio.h> 
#include <argp.h> 
#include <stdbool.h>
#include <stdlib.h>
#include "analyzer.h"
#include "linearizer.h"

struct user_input{
    bool input_file_selected;
    char* input_filename;
    bool output_file_selected;
    char* output_filename;
    bool verbose_selected;
};

static int parse_opt(int key, char* arg, struct argp_state* state)
{
    struct user_input* user_in = state->input;

    switch(key)
    {
        case 'i': 
            user_in->input_file_selected = true;
            user_in->input_filename = arg;
            break;

        case 'o': 
            user_in->output_file_selected = true;
            user_in->output_filename = arg;
            break;

        case 'v':
            user_in->verbose_selected = true;
            break;

        default: return ARGP_ERR_UNKNOWN;     
    }

    return 0;
}

static void write_statistics(FILE* out_stream, struct statistics stat)
{
    fprintf(out_stream, "TOTAL VARIABLE ANALYZED: %d\n", stat.variable_analyzed);
    fprintf(out_stream, "TOTAL ERROR FOUND: %d\n", stat.error_found);
    fprintf(out_stream, "TOTAL UNUSED VARIABLE: %d\n", stat.variable_unused);
    fprintf(out_stream, "TOTATL UNCORRECT VARIABLE NAME: %d\n", stat.variable_name_uncorrect);
    fprintf(out_stream, "TOTAL UNCORRECT TYPE NAME: %d\n", stat.variable_type_uncorrect);

    fprintf(out_stream, "ERROR LIST:\n");

    for(int i = 0; i < stat.error_list_size; i++)
    {
        fprintf(out_stream, (stat.error_list[i].type == TYPE_ERROR) ? "TYPE ERROR, " : "NAME_ERROR, ");
        fprintf(out_stream, "%s, ", stat.error_list[i].lexeme);
        fprintf(out_stream, (i == stat.error_list_size - 1) ? "%d\n" : "%d / ", stat.error_list[i].line); 
    }

    fprintf(out_stream, "UNUSED VARIABLE LIST:\n");

    for(int i = 0; i < stat.variable_unused_list_size; i++)
        fprintf(out_stream, (i == stat.variable_unused_list_size - 1) ? "%s\n" : "%s, ", stat.variable_unused_list[i]);
}

int main(int argc, char** argv)
{
    struct user_input user_in = {false, NULL, false, NULL, false};

    struct argp_option options[] = {
        {"in", 'i', "FILENAME", 0, "Select the input file (mandatory)"},
        {"out", 'o', "FILENAME", 0, "Select the output file"},
        {"verbose", 'v', NULL, 0, "Print on standard output the processing statistics"},
        {0}
    };

    struct argp argp = {options, &parse_opt};

    if(argp_parse(&argp, argc, argv, 0, NULL, &user_in))
    {
        fprintf(stderr, "Error parsing the command line\n");
        return EXIT_FAILURE;
    }
    
    if(!user_in.input_file_selected)
    {
        fprintf(stderr, "--input (-i) option is mandatory\n");
        return EXIT_FAILURE;
    }

    FILE* in_stream = fopen(user_in.input_filename, "r");

    if(in_stream == NULL)
    {
        perror("Error opening the input file");
        return EXIT_FAILURE;
    }

    char** linearization = linearize(in_stream);

    fclose(in_stream);

    if(linearization == NULL)
    {
        perror("Error during file operation");
        return EXIT_FAILURE;
    }

    struct statistics stat = analyze(linearization);

    FILE* out_stream = stdout;

    if(user_in.output_file_selected)
    {
        out_stream = fopen(user_in.output_filename, "w");

        if(out_stream == NULL)
        {
            perror("Error opening the output file; forced output to stdout");
            out_stream = stdout;
        }
    }

    write_statistics(out_stream, stat);

    if(user_in.verbose_selected && out_stream != stdout)
        write_statistics(stdout, stat);

    if(fclose(out_stream) && !user_in.verbose_selected)
    {
        perror("Error to close the output file; forced output to stdout");
        write_statistics(stdout, stat);
    }
    
    return EXIT_SUCCESS;
}