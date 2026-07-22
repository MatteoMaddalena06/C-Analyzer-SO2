#include "header/linearizer.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

char** linearize(FILE* fp)
{
    //array che contiene gli type e le keywords
    char TypeDict[6] = {"int", "float", "double","char","bool","void"};
    char UserTypeDict[3]={"union","struct","enum"};

    //creo un array di stringhe dinamiche con malloc
    int cap = 8;
    int ind=0;
    char** Token = malloc(cap * sizeof(char*));

    int c;
    char buffer[100];
    int buffInd = 0;
    
    bool skip = false;
    while ((c = fgetc(fp) != EOF))
    {
        if (ferror(fp))
        {
            return NULL;
        }
        //controlla se il carattere dopo è [ oppure = allora salto finche non incontro un " ","\n","\t",";"
        if(c == '[' || c == '=')
        {
            skip = true;
            continue;
        }
        if(skip == true)
        {
            continue;
        }
        // trovo i caratteri di spazio, a capo e ; per dire che il token è terminato
        if (c == ' ' || c == '\n' || c == '\t' || c == ';') 
        {
            skip = false;
            //trovato il carattere '\n',' ', '\t' o ';' controllo se l'indice è uguale alla grandezza(cap) attuale dell'array Token
            // se si allora aumento la grandezza(cap) dell'array Token con realloc. Aggiungo sempre la nuova stringa in Token
            if(buffInd >0)
            {
                buffer[buffInd] = '\0';
                if (ind == cap)
                {
                    cap *=2;
                    Token = realloc(Token, cap*sizeof(char*));
                }
                //controllo che il token sia un tipo e metto dentro l'array
                if(TypePresent(buffer,TypeDict,6) == true)
                {
                    Token[ind] = strdup(buffer);
                    ind++;
                }
                //controllo che il token sia un UserTypeDict e metto dentro l'array
                if(TypePresent(buffer,UserTypeDict,3) == true)
                {
                    Token[ind] = strdup(buffer);
                    ind++;
                }
                //controlla se il token precedente era di un userType or Type, se si allora il buffer è una variabile e viene messo
                //nell'array
                if(TypePresent(Token[ind-1],TypeDict,6)==true || TypePresent(Token[ind-1],UserTypeDict,3)==true)
                {
                    Token[ind] = strdup(buffer);
                    ind++;  
                }
                buffInd =0;
            }
        }
        // continuo a creare il token finche non c'è spazio, a capo o ;
        else
        {
            buffer[buffInd++] = c;
        }
    }
    return Token;
}

void free_linearization(char** linearization)
{
    for(int i = 0; linearization[i] != NULL; i++)
        free(linearization[i]);

    free(linearization);
}

bool TypePresent(char* string, char** vect,int size)
{
    for(int i=0;i<size;i++)
    {
        if(strcmp(string,vect[i])==0)
        {
            return true;
        }
    }
    return false;
}