/*
 *  Projet minishell - Licence 3 Info - PSI 2023
 *
 *  Nom :               CROS		BEN AMMAR
 *  Prénom :            Bryan		Nader
 *  Num. étudiant :     22110106	22101740
 *  Groupe de projet :  Groupe 1
 *  Date :              30/10/2023
 *
 *  Gestion des commandes internes du minishell.
 */

#ifndef _BUILTIN_H_
#define _BUILTIN_H_

#include "cmd.h"


// Codes d'erreurs
enum BuilinError
{
    BUILTIN_OK = 0,             // Pas d'erreur
    BUILTIN_NOT_FOUND = 30,     // Commande builtin non trouvee ou non supportee
    BUILTIN_BAD_ARGS            // Erreur d'utilisation (arguments) d'une commande
};


/*
 * Teste si une commande est builtin
 *
 * cmd : nom de la commande a tester
 * Retourne 1 si la chaîne passée désigne une commande interne, sinon 0
 */
int isBuiltin( const char* cmd );

/*
 * Execute la commande (supposee builtin) specifiee
 *
 * cmd : commande builin a executer
 * Retourne -1 si la commande n'est pas une builtin supporte, et le code de retour d'execution sinon
 */
int execBuiltin( cmd_t* cmd );


#endif // _BUILTIN_H_
