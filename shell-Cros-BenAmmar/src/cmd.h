/*
 *  Projet minishell - Licence 3 Info - PSI 2023
 *
 *  Nom :               CROS		BEN AMMAR
 *  Prénom :            Bryan		Nader
 *  Num. étudiant :     22110106	22101740
 *  Groupe de projet :  Groupe 1
 *  Date :              30/10/2023
 *
 *  Modelisation d'une commande, et fonctions qui la manipulent:
 *  - Initialisation
 *  - Parsing de la ligne de commande entree par l'operateur
 *  - Execution
 */

#ifndef _CMD_H_
#define _CMD_H_

#include <unistd.h>

#include "parser.h"

// Codes d'erreurs
enum CmdError
{
    CMD_OK = 0,
    CMD_BAD_SEP = 20,       // Separateur de commande au mauvais endroit
    CMD_BAD_REDIRECTION,    // Impossible d'ouvrir le fichier de redirection
    CMD_PIPE_FAILED,        // Echec de creation d'un pipe
    CMD_FORK_FAILED,        // Echec de creation d'un nouveau processus
    CMD_WAIT_FAILED,        // Echec de synchro avec la terminaison du processus d'execution d'une commande
    CMD_EXEC_FAILED,        // Echec de l'execution (via exec) de la commande
    CMD_NOT_FOUND           // Commande builtin non trouvee (ou non supportee)
};

// Type d'enchainements possible entre 2 commandes.
//
// Il est possible de specifier plusieurs commandes sur la ligne de commande. Dans ce cas, les commandes
// s'enchainent. Mais la maniere dont elle s'enchaine depend de l'operateur utilise pour les separer. En
// particulier, certains operateurs creent une sequence de commandes interruptible (au sens ou la commande
// suivante va dependre du resultat). Et lorsqu'on interomp une telle sequence, il faut se rendre a la premiere
// commande qui suit cette sequence.
//
// Concernant les operateurs de separation des commande, on a :
//
// - Le ';' (LINK_NEXT) :
//   Dans ce cas, l'enchainement est inconditionnel
// - Le '|' (LINK_PIPE) :
//   Dans ce cas, on passe a la commande suivante uniquement en cas de success. En cas d'erreur, on passe a
//   la premiere commande apres la sequence interruptible
// - Le '&&' (LINK_AND) :
//   Dans ce cas, on passe a la commande suivante uniquement en cas de success (ET logique). En cas d'erreur,
//   on passe a la premiere commande apres la sequence interruptible
// - Le '||' (LINK_OR) :
//   Dans ce cas, on passe a la commande suivante uniquement en cas d'erreur (OU logique). En cas de success,
//   on passe a la premiere commande apres la sequence interruptible
//
typedef enum
{
    LINK_NEXT = 0,          // Chainage inconditionnel avec la commande suivante
    LINK_PIPE,              // Chainage via un pipe avec la commande suivante
    LINK_AND,               // Chainage via un et logique avec la commande suivante
    LINK_OR,                // Chainage via un ou logique avec la commande suivante
    LINK_NONE = -1          // Pas de commande suivante
} NextCmdLink;

/*
 *  Structure de donnees associee a une commande a executer.
 *
 *  A noter le champ 'fdpipe' qui permet d'eventuellement stocker les descripteurs d'un pipe lorsque
 *  la commande est executee en sortie de ce pipe. En effet, si ce pipe est gere via 'fdclose' comme les
 *  autres fichiers, le pipe n'est referme dans le minishell qu'apres l'execution de toutes les commandes.
 *  Mais alors, il n'est plus possible au niveau du shell d'attendre que la commande se termine, car elle
 *  ne recoit pas de EOF dans le pipe et donc ne se termine jamais. En cas de pipe, il faut donc que le shell
 *  le ferme juste apres le fork, et avant l'appel a waitpid().
 *
 *  pid:            ID du processus qui exécute la commande (ou -1 si pas d'execution)
 *  status:         Code de retour de la commande
 *  in:             Descripteur associe a l'entree standard du processus (ou -1 si par defaut)
 *  out:            Descripteur associe a la sortie standard du processus (ou -1 si par defaut)
 *  err:            Descripteur associe a l'erreur standard du processus (ou -1 si par defaut)
 *  wait:           Flag sur l'execution synchrone du processus (si faux execution en background)
 *  path:           Nom de la commande
 *  argv:           Liste des arguments de la commande (incluant la commande elle-meme)
 *  fdclose:        Liste des descripteurs de fichiers a fermer a la fin de l'execution
 *  fdpipe:         Eventuel pipe a refermer apres le fork du process
 *  next:           Pointeur vers la commande suivante (execution inconditionnelle)
 *  next_success:   Pointeur vers la commande suivante en cas de succes
 *  next_failure:   Pointeur vers la commande suivante en cas d'erreur
 *  nextCmdLink:    Type du separateur avec la prochaine commande
 */
typedef struct cmd_t
{
    pid_t pid;
    int status;
    int in, out, err;
    int wait;
    char path[MAX_LINE_SIZE];
    char* argv[MAX_CMD_SIZE];
    int fdclose[MAX_CMD_SIZE];
    int fdpipe[2];
    struct cmd_t* next;
    struct cmd_t* nextSuccess;
    struct cmd_t* nextFailure;
    NextCmdLink nextCmdLink;
} cmd_t;

/*
 * Structure de donnees associee a une commande qui s'execute an arriere plan. Ces commande sont gerees
 * dans une liste doublement chainee
 *
 * pid : PID du processus
 * number : numero attribuee a la commande lors de son lancement (et affiche a sa terminaison)
 * cmdLine : ligne de commande correspondante
 * next : pointeur sur la commande suivante
 * previous : pointeur sur la commande precedente
 */
typedef struct BgCmd
{
    pid_t pid;
    int number;
    char cmdLine[MAX_LINE_SIZE];
    struct BgCmd* next;
    struct BgCmd* previous;
} BgCmd;


/*
 *  Initialiser une structure cmd_t avec les valeurs par défaut.
 *
 *  p : pointeur sur la commande à initialiser.
 *
 *  Retourne 0 en cas de succes, ou un code d'erreur.
 */
int initCmd( cmd_t* p );

/*
 *  Remplit le tableau de commandes en fonction du contenu de tokens.
 *  Ex : {"ls", "-l", "|", "grep", "^a", NULL} =>
 *       {
 *          {
 *              path = "ls",
 *              argv = {"ls", "-l", NULL},
 *              wait = 0,
 *              ...
 *          },
 *          {
 *              path = "grep",
 *              argv = {"grep", "^a", NULL},
 *              wait = 1,
 *              ...
 *          }
 *      }
 *
 *  tokens : le tableau des elements de la ligne de commandes (peut être modifie).
 *  cmds : le tableau dans lequel sont stockes les commandes.
 *  cmdCount : en sortie, nombre de commandes utilisees
 *
 *  Retourne 0 ou un code d'erreur.
 */
int parseCmd( char* tokens[], cmd_t* cmds, int* cmdCount );

/*
 *  Lance la commande en fonction de ses attributs et initialise les champs manquants.
 *
 *  On cree un nouveau processus, on detourne eventuellement les entree/sorties.
 *  On conserve le PID dans la structure du processus appelant (le minishell).
 *  On attend si la commande est lancee en "avant-plan" et on initialise le code de retour.
 *  On rend la main immédiatement sinon.
 *
 *  cmd : pointeur sur la commande a executer.
 *
 *  Retourne 0 ou un code d'erreur.
 */
int execCmd( cmd_t* cmd );

/*
 * Recherche et retourne la commande en background correspondant au PID specifie.
 *
 * Si la commande est trouvee, elle est retiree de la liste globale des commandes en background en cours
 * d'execution, et retournee a l'appelant (qui est responable de la liberation de la memoire associee)
 *
 * pid : PID de la commande
 * retourne un pointeur sur la commande si trouvee, sinon NULL
 */
BgCmd* removeBgCmd( pid_t pid );

/*
 * Affiche le contenu d'une commande.
 *
 * cmd : la commande que l'on souhaite afficher.
 */
void printCmd( const cmd_t* cmd );


#endif // _CMD_H_
