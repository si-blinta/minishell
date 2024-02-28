/*
 *  Projet minishell - Licence 3 Info - PSI 2023
 *
 *  Nom :               CROS		BEN AMMAR
 *  Prénom :            Bryan		Nader
 *  Num. étudiant :     22110106	22101740
 *  Groupe de projet :  Groupe 1
 *  Date :              30/10/2023
 *
 *  Dependances : 
 *
 *  Parsing de la ligne de commande entree par l'utilisateur (implementation)
 */

#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void trim( char* str )
{
    // Index du debut et de fin de la chaine resultante
    int startIndex = 0;
    int endIndex = strlen( str ) - 1;

    // On deplace le debut de chaine sur le premier caractere qui n'est pas espace ou tabulation
    for( ; str[startIndex] == ' ' || str[startIndex] == '\t'; ++startIndex );

    // On deplace la fin de chaine sur le dernier caractere qui n'est pas espace ou tabulation
    for( ; endIndex >= startIndex && ( str[endIndex] == ' ' || str[endIndex] == '\t'); --endIndex );

    // Copy de la chaine entre les index de debut et de fin, et ajout du caractere de terminaison de chaine
    strncpy( str, str + startIndex, endIndex - startIndex + 1 );
    str[endIndex - startIndex + 1] = '\0';
}


void clean( char* str )
{
    // Copie de la chaine initiale dans un buffer (la chaine initiale va accueillir le resultat)
    char buff[MAX_LINE_SIZE];
    strcpy( buff, str );

    // Index des caracteres courants dans chaque chaine (buffer et resultat)
    int iBuff = 0;
    int iStr = 0;

    // Flag qui dit si un separateur espace/tabulation est en cours de traitement
    int sepInProgress = 0;

    // Pour chaque caractere du buffer
    while( buff[iBuff] != '\0' )
    {
        // Si le caractere est un espace ou une tabulation
        if( buff[iBuff] == ' ' || buff[iBuff] == '\t' )
        {
            // Si pas de separateur en cours de traitement
            if( ! sepInProgress )
            {
                // On recopie un espace (unique)
                str[iStr++] = ' ';

                // On leve le flag, de sorte a ignorer d'eventuels espaces/tabulations qui suivraient
                sepInProgress = 1;
            }
        }
        else
        {
            // Caractere normal : on le recopie, et on descend le flag
            str[iStr++] = buff[iBuff];
            sepInProgress = 0;
        }

        // Passage au caractere suivant
        ++iBuff;
    }

    // Caractere de terminaison
    str[iStr] = '\0';

}

void showSeparators( char* str )
{
    // Copie de la chaine initiale dans un buffer (la chaine initiale va accueillir le resultat)
    char buff[MAX_LINE_SIZE];
    strcpy( buff, str );

    // Index des caracteres courants dans chaque chaine (buffer et resultat)
    int iBuff = 0;
    int iStr = 0;

    // Tant qu'on est pas a la fin du buffer
    while( buff[iBuff] != '\0' )
    {
        // Separateur eventuel a mettre en evidence (pas plus de 3 caracteres)
        char sep[4] = {'\0'};

        // Suivant le caractere courant, s'il correspond au premier caractere d'un separateur, alors on
        // verifie tous les separateurs potentiels qui commence par ce caractere, du plus long au plus
        // court afin d'eviter de detruire un separateur compose de plusieurs caracteres. Si un separateur
        // est detecte, il est copie dans la variable 'sep'
        switch( buff[iBuff] )
        {
            // On traite uniquement ";"
            case ';':
                strcpy( sep, ";" );
                break;

            // On traite "||" ou "|"
            case '|':
                if( strncmp( buff + iBuff, "||", 2 ) == 0 )
                    strcpy( sep, "||" );
                else
                    strcpy( sep, "|" );
                break;

            // On traite "&>>" ou "&>" ou "&&" ou "&"
            case '&':
                if( strncmp( buff + iBuff, "&>>", 3 ) == 0 )
                    strcpy( sep, "&>>" );
                else if( strncmp( buff + iBuff, "&>", 2 ) == 0 )
                    strcpy( sep, "&>" );
                else if( strncmp( buff + iBuff, "&&", 2 ) == 0 )
                    strcpy( sep, "&&" );
                else
                    strcpy( sep, "&" );
                break;

            // On traite uniquement "<"
            case '<':
                strcpy( sep, "<" );
                break;

            // On traite ">>" ou ">"
            case '>':
                if( strncmp( buff + iBuff, ">>", 2 ) == 0 )
                    strcpy( sep, ">>" );
                else
                    strcpy( sep, ">" );
                break;

            // On traite "1>>" ou "1>"
            case '1':
                if( strncmp( buff + iBuff, "1>>", 3 ) == 0 )
                    strcpy( sep, "1>>" );
                else if( strncmp( buff + iBuff, "1>", 2 ) == 0 )
                    strcpy( sep, "1>" );
                else
                    // Pas de separateur. Caractere suivant...
                    str[iStr++] = buff[iBuff++];
                break;

            // On traite "2>>" ou "2>"
            case '2':
                if( strncmp( buff + iBuff, "2>>", 3 ) == 0 )
                    strcpy( sep, "2>>" );
                else if( strncmp( buff + iBuff, "2>", 2 ) == 0 )
                    strcpy( sep, "2>" );
                else
                    // Pas de separateur. Caractere suivant...
                    str[iStr++] = buff[iBuff++];
                break;

            // Pas de separateur a la position courante. On copie le caractere et on passe au suivant
            default:
                str[iStr++] = buff[iBuff++];
                continue;
        }

        // Si on a un separateur a traiter
        if( strlen( sep ) != 0 )
        {
            // Si le caractere precedent n'est pas un espace, on le rajoute au resultat
            if( iBuff > 0 && buff[iBuff - 1] != ' ' ) str[iStr++] = ' ';

            // On recopie le separateur
            memcpy( str + iStr, sep, strlen( sep ) );
            iStr += strlen( sep ) ;
            iBuff += strlen( sep ) ;

            // S'il n'y a pas d'espace apres le separateur, on le rajoute au resultat
            if( buff[iBuff] != ' ' ) str[iStr++] = ' ';
        }
    }

    // Caractere de terminaison
    str[iStr] = '\0';
}


int substEnv( char* str )
{
    // Copie de la chaine initiale dans un buffer (la chaine initiale va accueillir le resultat)
    char buff[MAX_LINE_SIZE];
    strcpy( buff, str );


    // Index des caracteres courants dans chaque chaine (buffer et resultat)
    int iBuff = 0;
    int iStr = 0;

    // Tant qu'on est pas a la fin du buffer
    while( buff[iBuff] != '\0' )
    {
        // Si on est sur le debut d'une variable d'environnement
        if( buff[iBuff] == '$' )
        {
            // Index du debut du nom de la variable (apres le $)
            int iBegin = ++iBuff;

            // Recherche de l'index de la fin du nom de la variable (premier espace ou fin de chaine)
            int iEnd = iBegin;
            while( buff[iEnd] != ' ' && buff[iEnd] != '\0' ) ++iEnd;

            // Si le nom de la variable est vide (longueur nulle), on retourne une erreur
            const int nameLength = iEnd - iBegin;
            if( nameLength == 0 ) return( PARSER_BAD_NAME );

            // Copie du nom de la variable
            char varName[MAX_LINE_SIZE];
            memcpy( varName, buff + iBegin, nameLength );
            varName[nameLength] = '\0';

            // Recherche de la valeur de la variable
            char* varValue = getenv( varName );

            // Si la variable n'existe pas
            if( varValue == NULL )
            {
                // On ignore la variable, et on met a jour l'index du buffer
                iBuff += nameLength;
            }
            else
            {
                // La variable existe, on substitue sa valeur dans la chaine resultat
                const int valueLength= strlen( varValue );
                memcpy( str + iStr, varValue, valueLength );

                // On met a jour les index
                iBuff += nameLength;
                iStr += valueLength;
            }
        }
        else
        {
            // Sinon, on recopie simplement le caractere
            str[iStr++] = buff[iBuff++];
        }
    }

    // Caractere de terminaison
    str[iStr] = '\0';

    return( PARSER_OK );
}


void strcut( char* str, char sepChar, char** tokens )
{
    // Separateur passe a strtok()
    char sep[2] = { sepChar, '\0' };

    // Index du prochain mot a rajouter dans le tableau 'tokens'
    int iLast = 0;

    // Recherche du premier mot de la chaine
    char* word = strtok( str, sep );

    // Tant qu'un mot a ete trouve
    while( word != NULL )
    {
        // On rajoute le mot dans la liste
        const int wordLength = strlen( word );
        tokens[iLast] = (char*)malloc( ( wordLength + 1 ) * sizeof( char ) );
        strcpy( tokens[iLast], word );

        // On passe au mot suivant
        ++iLast;
        word = strtok( NULL, sep );
    }
}
