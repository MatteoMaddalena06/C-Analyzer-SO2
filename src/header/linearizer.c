#include "header/linearizer.h"
#include <stdio.h>

char** linearize(FILE* fp)
{
    //creo un array di stringhe dinamiche con malloc
    int cap = 8;
    int ind=0;
    char** Token = malloc(cap * 8);

    int c;
    char buffer[100];
    int buffInd = 0;
    
    while ((c = fgetc(fp) != EOF))
    {
        if (ferror(fp))
        {
            printf("An error has been found. Function linearize terminated\n");
            return NULL;
        }
        // trovo i caratteri di spazio, a capo e ; per dire che il token è terminato
        if (c == ' ' || c == '\n' || c == '\t' || c == ';') 
        {
            //trovato il carattere '\n',' ', '\t' o ';' controllo se l'indice è uguale alla grandezza(cap) attuale dell'array Token
            // se si allora aumento la grandezza(cap) dell'array Token con realloc. Aggiungo sempre la nuova stringa in Token
            if(buffInd >0)
            {
                buffer[buffInd] = '\0';
                if (ind == cap)
                {
                    cap *=2;
                    Token = realloc(Token, cap*8);
                }
                Token[ind] = strdup(buffer);
                ind++;  
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