/*
 *  Projet minishell - Licence 3 Info - PSI 2023
 *
 *  Nom :               CROS		BEN AMMAR
 *  Prénom :            Bryan		Nader
 *  Num. étudiant :     22110106	22101740
 *  Groupe de projet :  Groupe 1
 *  Date :              30/10/2023
 *
 *  Dependances : cmd.h
 *
 *  Gestion des commandes internes du minishell (implementation).
 */

#include "builtin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parser.h"


//--- Declaration des types et fonctions locales --------------------------------------------------------------

// Structure de donnees associees a une commande builtin:
// - name : nom de la commande
// - func : fonction qui execute la commande
typedef struct
{
    char name[MAX_LINE_SIZE];
    int (*func)( cmd_t* cmd );
} BuiltinCmd;

/*
 * Implementations des commandes builtin. Toutes ces fonctions ont la meme signature :
 *
 * cmd : commande a executer
 * retourne le code de retour de l'execution de la commande
 */
static int changeDir( cmd_t* cmd );
static int exportVar( cmd_t* cmd );
static int unsetVar( cmd_t* cmd );

// Liste des commandes builtin supportees
static const BuiltinCmd ALL_BUILTINS[] =
{
    { "cd", changeDir },
    { "export", exportVar },
    { "unset", unsetVar }
};
static const int BUILTIN_COUNT = sizeof( ALL_BUILTINS ) / sizeof( BuiltinCmd );


//--- Implementation des fonctions publiques -------------------------------------------------------------------

int isBuiltin( const char* cmd )
{
    // Pour chaque builtin supportee
    for( int i = 0; i < BUILTIN_COUNT; ++i )
    {
        // Si les noms correspondent, la commande est supportee
        if( strcmp( ALL_BUILTINS[i].name, cmd ) == 0 ) return( 1 );
    }

    // Builtin non trouvee
    return( 0 );
}


int execBuiltin( cmd_t* cmd )
{
    // Recherche de la commande builtin
    BuiltinCmd* builtin = NULL;
    for( int i = 0; i < BUILTIN_COUNT; ++i )
    {
        // Si les noms correspondent, la commande est trouvee
        if( strcmp( ALL_BUILTINS[i].name, cmd->path ) == 0 ) builtin = (BuiltinCmd*)ALL_BUILTINS + i;
    }

    // On verifie que la commande a ete trouvee
    if( builtin == NULL) return( BUILTIN_NOT_FOUND );

    // Execution de la commande
    return( (*builtin->func)( cmd ) );
}


//--- Implementation des fonctions locales ---------------------------------------------------------------------

static int changeDir( cmd_t* cmd )
{
    // On verifie les arguments
    char* dstDir = cmd->argv[1];
    if( cmd->argv[2] != NULL )
    {
        // Erreur d'utilisation
        fprintf( stderr, "ERREUR - Usage: cd [DIR]" );
        return( BUILTIN_BAD_ARGS );
    }

    // Si pas de destination specifiee
    if( dstDir == NULL )
    {
        // Utilisation de $HOME
        dstDir = getenv( "HOME" );

        // Si la variable HOME n'est pas definie, erreur
        if( dstDir == NULL )
        {
            fprintf( stderr, "ERREUR - Usage: cd [DIR] (HOME doit être définie si pas de valeur)" );
            return( BUILTIN_BAD_ARGS );
        }
    }

    // Execution de la commande
    return( chdir( dstDir ) );
}


static int exportVar( cmd_t* cmd )
{
    // On verifie les arguments
    if( cmd->argv[1] == NULL )
    {
        // Erreur d'utilisation
        fprintf( stderr, "ERREUR - Usage: export NAME=VALUE\n" );
        return( BUILTIN_BAD_ARGS );
    }
    if( cmd->argv[2] != NULL )
    {
        // Erreur d'utilisation
        fprintf( stderr, "ERREUR - Usage: export NAME=VALUE\n" );
        return( BUILTIN_BAD_ARGS );
    }
    char* arg = cmd->argv[1];

    // On extrait le nom de la variable
    char varName[MAX_LINE_SIZE] = {'\0'};
    const char* sep = "=";
    char* token = strtok( arg, sep );
    if( token != NULL ) strcpy( varName, token );

    // On extrait la valeur de la variable
    char varValue[MAX_LINE_SIZE] = {'\0'};
    token = strtok( NULL, sep );
    if( token != NULL ) strcpy( varValue, token );

    // On verifie qu'il n'y a plus de '=' (token doit etre NULL )
    token = strtok( NULL, sep );

    // Verification du format
    if( strlen( varName ) == 0 || strlen( varValue ) == 0 || token != NULL )
    {
        // Erreur d'utilisation
        fprintf( stderr, "ERREUR - Usage: export NAME=VALUE\n" );
        return( BUILTIN_BAD_ARGS );
    }

    // Execution de la commande
    return( setenv( varName, varValue, 1 ) );
}


static int unsetVar( cmd_t* cmd )
{
    // On verifie les arguments
    if( cmd->argv[1] == NULL )
    {
        // Erreur d'utilisation
        fprintf( stderr, "ERREUR - Usage: unset VARNAME\n" );
        return( BUILTIN_BAD_ARGS );
    }
    if( cmd->argv[2] != NULL )
    {
        // Erreur d'utilisation
        fprintf( stderr, "ERREUR - Usage: unset VARNAME\n" );
        return( BUILTIN_BAD_ARGS );
    }
    char* varName = cmd->argv[1];

    // Execution de la commande
    return( unsetenv( varName ) );
}


