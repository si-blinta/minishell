/*
 *  Projet minishell - Licence 3 Info - PSI 2023
 *
 *  Nom :               CROS		BEN AMMAR
 *  Prénom :            Bryan		Nader
 *  Num. étudiant :     22110106	22101740
 *  Groupe de projet :  Groupe 1
 *  Date :              30/10/2023
 *
 *  Parsing de la ligne de commande entree par l'utilisateur.
 */

#ifndef _PARSER_H_
#define _PARSER_H_

#include <stddef.h>

// Taille max d'un chaine de caracteres
#define MAX_LINE_SIZE   1024

// Nombre max de mots dans une ligne de commande
#define MAX_CMD_SIZE    256

// Code d'erreur
enum ParserError
{
    PARSER_OK = 0,              // Pas d'erreur
    PARSER_BAD_NAME = 10        // Nom de variable d'environnement incorrect
};


/*
 * Supprime les espaces et tabulations en debut et fin de ligne
 *
 * str : chaine de caracteres a traiter
 */
void trim( char* str );

/*
 * Supprimer les doublons d'espaces et de tabulations
 *
 * str : chaine de caracteres a traiter
 */
void clean(char* str);

/*
 * Rajoute si besoin des espaces autour des separateurs de commandes afin de les mettre en evidence lorsque
 * la ligne de commande sera divisee en mots. Les separateurs pris en compte sont :
 * - ";"    : separateur de commandes
 * - "|"    : pipe entre 2 commandes
 * - "&&"   : ET logique
 * - "||"   : OU logique
 * - "<"    : redirection de STDIN
 * - ">"    : redirection de STDOUT
 * - "1>"   : redirection de STDOUT
 * - ">>"   : redirection de STDOUT (mode concatenation)
 * - "1>>"  : redirection de STDOUT (mode concatenation)
 * - "2>"   : redirection de STDERR
 * - "2>>"  : redirection de STDERR (mode concatenation)
 * - "&>"   : redirection de STDOUT et STDERR
 * - "&>>"  : redirection de STDOUT et STDERR (mode concatenation)
 * - "&"    : execution en background
 *
 * str : chaine de caracteres a traiter
 */
void showSeparators( char* str );

/*
 * Pour chaque variable d'environnement (de la forme $NOM) rencontree dans la chaine de caracteres, on
 * substitue la variable par sa valeur si cette variable existe (dans le cas contraire on ne met rien)
 *
 * str : chaine de caracteres a traiter
 * retourne 0 en cas de succes, sinon un code d'erreur
 */
int substEnv(char* str );

/*
 * Decoupe la chaine de caracteres specifiee en mots.
 *
 * str : chaine de caracteres a traiter
 * sepChar : caractere de separation des mots
 * tokens : tableau des mots extraits de la chaine de caractere
 */
void strcut( char* str, char sepChar, char** tokens );


#endif // _PARSER_H_
