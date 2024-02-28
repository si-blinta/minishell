/*
 *  Projet minishell - Licence 3 Info - PSI 2023
 *
 *  Nom :               CROS		BEN AMMAR
 *  Prénom :            Bryan		Nader
 *  Num. étudiant :     22110106	22101740
 *  Groupe de projet :  Groupe 1
 *  Date :              30/10/2023
 *
 *  Dependances : parser.h
 *
 *  Modelisation d'une commande (implementation)
 */


#include "cmd.h"
#include "builtin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/wait.h>


//--- Declaration des types et fonctions locales ---------------------------------------------------------------

// Types de separateurs de commandes :
//
// Parmi les types de separateurs, on distingue ceux qui marquent vraiment la fin d'une commande de ceux qui
// jouent un autre role. Typiquement, les types SEP_SIMPLE, SEP_LOGICAL et SEP_PIPE marquent une vraie fin
// de commande, alors que :
// - Le type SEP_REDIRECT correspond a une redirection de la commande courante, mais ne constitue pas la marque
//   d'une fin de commande proprement dite. De plus, l'argument suivant est cense etre le fichier cible de la
//   redirection, et doit donc etre traite comme tel (et pas comme une nouvelle commande)
// - Le type SEP_BACKGROUND indique que la commande doit etre executee en background, mais comme pour la
//   redirection il ne marque pas vraiment la fin d'une commande
enum SepType
{
    SEP_NONE = -1,          // Pas de separateur
    SEP_SIMPLE = 0,         // Simple separateur de commande (";")
    SEP_REDIRECT,           // Redirections ("<", ">", "1>", ">>", "1>>", "2>", "2>>", "&>", "&>>")
    SEP_LOGICAL,            // Operateurs logiques ("&&", "||")
    SEP_PIPE,               // Pipe ("|")
    SEP_BACKGROUND,         // Execution en background ("&")
    SEP_LAST                // Marque le dernier type disponible
};

// Types de redirections
enum RedirectType
{
    REDIRECT_NONE = -1,     // Pas de redirection
    REDIRECT_IN = 0,        // Redirection de l'entree standard
    REDIRECT_OUT,           // Redirection de la sortie standard
    REDIRECT_ERR            // Redirection de l'erreur standard
};

// Index des files descriptors d'entree et de sortie d'un pipe
#define PIPE_IN  1  // Entree du pipe (cote en ecriture)
#define PIPE_OUT 0  // Sortie du pipe (cote en lecture)


// Liste des commandes en background en cours d'execution
static BgCmd* backgroundCommands = NULL;

/*
 * Teste si le token specifie est un separateur de commande
 *
 * token : token a tester
 * retourne le type de separateur detecte, ou SEP_NONE si le token ne correspond a aucun separateur connu
 */
static int isSeparator( const char* token );

/*
 * Traite un separateur de type SEP_REDIRECT correspondant a une redirection des entree/sortie/erreur d'une
 * commande. Pour cela, le fichier cible de la commande doit etre ouvert selon le mode correspond au type de
 * redirection. Ensuite, le file descriptor obtenu doit etre associe au champs 'in/out/err' de la commande
 * egalement selon le type de redirection. Pour finir, ce file descriptor doit etre memorise dans le tableau
 * 'fdclose' de la commande afin de permettre au minishell de le refermer apres l'execution
 *
 * sep : le separateur de redirection qui doit etre traite
 * cmd : la commande mise a jour
 * fileName : le nom du fichier cible de la redirection (qui doit etre ouvert)
 * retourne 0 en cas de succes, ou sinon un code d'erreur
 */
static int processCmdRedirection( const char* sep, cmd_t* cmd, const char* fileName );

/*
 * Met a jour le chainage des commande dans une sequence de commandes interruptible.
 *
 * Dans le cas ou une commande est liee a la suivante via un pipe ou un operateur logique, l'enchainement
 * entre ces 2 commandes est conditionnel (utilisation de 'nextSuccess' et 'nextFailure'). Cette fonction
 * met a jour le chainage de telles commandes executees en sequence, lorsque la fin de cette sequence est
 * connue.
 *
 * start : premiere commande de la sequence
 * end : premiere commande dont l'execution est inconditionnelle apres la sequence interruptible
 */
static void updateCmdChaining( cmd_t* start, cmd_t* end );

/*
 * Creation d'un pipe entre 2 commandes. L'entree du pipe est associee a la sortie standard de la premiere
 * commande, et sa sortie a l'entree standard de la deuxieme commande
 *
 * firstCmd : premiere commande (avant le pipe)
 * secondCmd : seconde commande (apres le pipe)
 * returne 0 en cas de succes, sinon un code d'erreur
 */
static int createPipe( cmd_t* firstCmd, cmd_t* secondCmd );

/*
 * Rajoute un file descriptor dans la liste des file descriptor a refermer d'une commande
 *
 * cmd : la commande mise a jour
 * fd : le file descriptor a rajouter
 */
static void addFileDescriptor( cmd_t* cmd, int fd );

/*
 * Referme tous les fichiers et pipes ouverts de la commande
 *
 * cmd : commande dont il faut refermer les fichiers/pipes
 */
static void closeCmdFiles( cmd_t* cmd );

/*
 * TODO
 */
static const BgCmd* addBgCmd( cmd_t* cmd );


//--- Implementation des fonctions publiques -------------------------------------------------------------------

int initCmd( cmd_t* p )
{
    // Pas de processus d'execution de la commande
    p->pid = -1;

    // Status OK
    p->status = 0;

    // Par defaut, on utilise stdin, stdout et stderr
    p->in = -1;
    p->out = -1;
    p->err = -1;

    // Pas d'execution en background
    p->wait = 1;

    // Pas de commande
    strcpy( p->path, "" );

    // Initialisation des tableaux (meme tailles)
    for( int i = 0; i < MAX_CMD_SIZE; ++i )
    {
        // Si l'argument existe, on le libere et on le reinitialise (a NULL)
        if( p->argv[i] != NULL )
        {
            free( p->argv[i] );
            p->argv[i] = NULL;
        }

        // Pas de descripteur de fichier valide
        p->fdclose[i] = -1;
    }

    // Pas de pipe ouvert
    p->fdpipe[0] = -1;
    p->fdpipe[1] = -1;

    // Pas de commande suivante
    p->next = NULL;
    p->nextSuccess = NULL;
    p->nextFailure = NULL;
    p->nextCmdLink = LINK_NONE;

    return 0;
}


int parseCmd( char* tokens[], cmd_t* cmds, int* cmdCount )
{
    // Commande courante
    cmd_t* current = NULL;

    // Commande precedant la commande courante
    cmd_t* previous = NULL;

    // Premiere commande d'un enchainement conditionnel de commandes (pipe ou operateurs logiques)
    cmd_t* sequenceStart = NULL;

    // Aucune commande au depart
    *cmdCount = 0;

    // Dernier separateur rencontre et type correspondant. On considere ici que les types de separateurs
    // qui marque vraiment une fin de commande (donc pas les redirections ni les executions en background)
    char lastSep[4] = {'\0'};
    int lastSepType = SEP_NONE;

    // Index de l'argument courant de la commande courante
    int iArg = 0;

    // Pointeur sur le token courant
    char** pToken = tokens;

    // Tant qu'on a un token courant
    while( *pToken != NULL )
    {
        // Si on est sur un separateur
        const int sepType = isSeparator( *pToken );
        if( sepType != SEP_NONE )
        {
            // Si pas de commande courante, erreur
            if( current == NULL ) return( CMD_BAD_SEP );

            // Si le separateur est l'execution en background
            if( sepType == SEP_BACKGROUND )
            {
                // On met a jour la commande
                current->wait = 0;
            }

            // Si le separateur est une redirection
            else if( sepType == SEP_REDIRECT )
            {
                // On recupere le separateur et le token suivant, qui doit donner le fichier de redirection
                const char* sep = *pToken++;
                const char* fileName = *pToken;

                // On traite la redirection
                const int status = processCmdRedirection( sep, current, fileName );
                if( status != CMD_OK ) return( status );
            }

            // Sinon, pour tout autre type de separateur
            else
            {
                // La commande courante est terminee et devient la commande precedente
                previous = current;
                current = NULL;

                // Mise a jour du dernier (vrai) separateur rencontre
                strcpy( lastSep, *pToken );
                lastSepType = sepType;
            }
        }

        // Sinon, on est sur un argument de la commande courante
        else
        {
            // Si pas de commande courante, on debute une nouvelle commande
            if( current == NULL )
            {
                // Une commande supplementaire
                ++( *cmdCount );

                // Si on a une commande precedente, on prend la suivante (dans le tableau),
                // sinon la toute premiere
                current = ( previous != NULL ? previous + 1 : cmds );

                // Si on a une commande precedente
                if( previous != NULL )
                {
                    // Suivant le type du dernier separateur
                    switch( lastSepType )
                    {
                        // Simple separateur
                        case SEP_SIMPLE:
                            // La commande precedente est chainee inconditionnellement avec la nouvelle commande
                            previous->next = current;
                            previous->nextCmdLink = LINK_NEXT;

                            // Si on a un sequence interruptible de commandes en cours
                            if( sequenceStart != NULL )
                            {
                                // On met a jour les enchainements de commande dans la sequence (la commande
                                // courante etant la premiere commande apres la sequence)
                                updateCmdChaining( sequenceStart, current );

                                // Pas de sequence interruptible de commandes en cours
                                sequenceStart = NULL;
                            }
                            break;

                        // Pipe
                        case SEP_PIPE:
                            // La commande precedente est chainee en cas de succes avec la nouvelle commande.
                            previous->nextSuccess = current;
                            previous->nextCmdLink = LINK_PIPE;

                            // Creation du pipe entre la commande precedente et la nouvelle commande
                            const int status = createPipe( previous, current );
                            if( status != CMD_OK ) return( status );

                            // S'il n'y a pas de sequence interruptible de commandes en cours, on l'initialise
                            if( sequenceStart == NULL ) sequenceStart = previous;
                            break;

                        // Operateur logique
                        case SEP_LOGICAL:
                            // Si ET logique, les 2 commandes doivent reussir
                            if( strcmp( lastSep, "&&" ) == 0 )
                            {
                                // La commande precedente est chainee en cas de succes avec la nouvelle commande
                                previous->nextSuccess = current;
                                previous->nextCmdLink = LINK_AND;
                            }

                            // Si OU logique, on n'execute la commande suivante qui si la precedente a echoue
                            else if( strcmp( lastSep, "||" ) == 0 )
                            {
                                // La commande precedente est chainee en cas de d'echec avec la nouvelle commande
                                previous->nextFailure = current;
                                previous->nextCmdLink = LINK_OR;
                            }

                            // S'il n'y a pas de sequence interruptible de commandes en cours, on l'initialise
                            if( sequenceStart == NULL ) sequenceStart = previous;
                            break;

                        // Type de separateur non-prevu
                        default:
                            assert( 0 && "Type de separateur non-prevu" );
                    }
                }

                // On recopie le nom de la commande (premier argument)
                iArg = 0;
                strcpy( current->path, *pToken );
            }

            // On rajoute le token dans la liste des arguments de la commande courante
            current->argv[iArg] = (char*)malloc( ( strlen( *pToken ) + 1 ) * sizeof( char ) );
            strcpy( current->argv[iArg++], *pToken );
        }

        // Token suivant
        ++pToken;
    }

    return 0;
}


int execCmd( cmd_t* cmd )
{
    // On traite eventuellement la commande 'exit' qui termine le minishell (et qui doit etre executee
    // dans le processus parent)
    if( strcmp( cmd->path, "exit" ) == 0 )
    {
        // On termine le minishell
        printf( "Bye bye!\n" );
        _exit( 0 );
    }

    // Creation d'un nouveau processus
    cmd->pid = fork();

    // Suivant le PID
    switch( cmd->pid )
    {
        // Erreur
        case -1:
            return( CMD_FORK_FAILED );
            break;

        // Processus fils
        case 0:
            // Redirection des entree/sortie/erreur
            if( cmd->in != -1 ) dup2( cmd->in, STDIN_FILENO );
            if( cmd->out != -1 ) dup2( cmd->out, STDOUT_FILENO );
            if( cmd->err != -1 ) dup2( cmd->err, STDERR_FILENO );

            // Fermeture des fichiers/pipes ouverts
            closeCmdFiles( cmd );

            // Si la commande a executer est builtin
            if( isBuiltin( cmd->path ) )
            {
                // Appel de la fonction builtin
                const int status = execBuiltin( cmd );
                if( status == BUILTIN_NOT_FOUND ) return( CMD_NOT_FOUND );

                // Mise a jour du code de retour de la commande
                cmd->status = status;
            }
            else
            {
                // Execution du binaire de la commande
                execvp( cmd->path, cmd->argv );

                // Ici, on a forcement une erreur d'execution
                fprintf( stderr, "ERREUR - Echec d'execution de la commande %s\n", cmd->path );

                // On force la terminaison du processus fils
                _exit( CMD_EXEC_FAILED );
            }
            break;

        // Processus pere
        default:
            //printf( "INFO - Executing cmd %s (PID = %d)...\n", cmd->path, cmd->pid );

            // On ferme les eventuels pipes ouvert
            if( cmd->fdpipe[0] != -1 ) close( cmd->fdpipe[0] );
            if( cmd->fdpipe[1] != -1 ) close( cmd->fdpipe[1] );

            // Si la commande s'execute au premier plan
            if( cmd->wait )
            {
                // On se synchronise avec la fin du processus d'execution de la commande
                //printf( "INFO - Waiting for process %d to complete...\n", cmd->pid );
                int status = 0;
                if( waitpid( cmd->pid, &status, 0 ) == -1 )
                {
                    fprintf( stderr, "Impossible de se synchroniser avec la fin de la commande %s (PID = %d)\n",
                             cmd->path, cmd->pid );
                    return( CMD_WAIT_FAILED );
                }

                // On met a jour le code de retour de la commande
                cmd->status = WEXITSTATUS( status );
                //printf( "INFO - Process %d has exited with code %d\n", cmd->pid, cmd->status );
            }
            else
            {
                // On enregistre la commande en background
                const BgCmd* bgCmd = addBgCmd( cmd );

                // On affiche le numero et le PID de la nouvelle commande en background
                printf( "[%d] %d\n", bgCmd->number, bgCmd->pid );
            }
            break;
    }

    return( CMD_OK );
}


BgCmd* removeBgCmd( pid_t pid )
{
    // Recherche de la commande avec le meme PID
    BgCmd* bgCmd = backgroundCommands;
    while( bgCmd != NULL )
    {
        // Commande trouvee
        if( bgCmd->pid == pid ) break;

        // Commande suivante
        bgCmd = bgCmd->next;
    }

    // Commande non trouvee
    if( bgCmd == NULL ) return( NULL );

    // On supprime la commande de la liste :
    // - Si pas de commande precedente
    if( bgCmd->previous == NULL )
    {
        // - Si pas de commande suivante
        if( bgCmd->next == NULL )
        {
            // Il n'y a plus de commande en background
            backgroundCommands= NULL;
        }
        else
        {
            // La commande suivante devient la premiere commande
            backgroundCommands = bgCmd->next;
            backgroundCommands->previous = NULL;
        }
    }
    else
    {
        // - Si pas de commande suivante
        if( bgCmd->next == NULL )
        {
            // La commande precedente devient la derniere commande
            bgCmd->previous->next = NULL;
        }
        else
        {
            // On chaine ensemble les commandes precedente et suivante
            bgCmd->previous->next = bgCmd->next;
            bgCmd->next->previous = bgCmd->previous;
        }
    }

    return( bgCmd );
}


void printCmd( const cmd_t* cmd )
{
    // Affichage de champs de la commande
    printf( "- %s:\n", cmd->path );
    printf( "  + in/out/err  = %d/%d/%d\n", cmd->in, cmd->out, cmd->err );
    printf( "  + argv        = " );
    int i = 0;
    while( cmd->argv[i] != NULL ) printf( "'%s' ", cmd->argv[i++] );
    printf( "\n" );
    printf( "  + fdclose     = " );
    i = 0;
    while( cmd->fdclose[i] != -1 ) printf( "%d ", cmd->fdclose[i++] );
    printf( "\n" );
    printf( "  + fpipe       = " );
    if( cmd->fdpipe[0] != -1 ) printf( "%d ", cmd->fdpipe[0] );
    if( cmd->fdpipe[1] != -1 ) printf( "%d ", cmd->fdpipe[1] );
    printf( "\n" );
    printf( "  + wait        = %s\n", cmd->wait ? "true" : "false" );
    printf( "  + next        = %s\n", cmd->next ? cmd->next->path : "NULL" );
    printf( "  + nextSuccess = %s\n", cmd->nextSuccess ? cmd->nextSuccess->path : "NULL" );
    printf( "  + nextFailure = %s\n", cmd->nextFailure ? cmd->nextFailure->path : "NULL" );
}


//--- Implementation des fonctions locales ---------------------------------------------------------------------

static int isSeparator( const char* token )
{
    // Liste des separateurs de commandes supportes, tries par type
    static char* SEPS_SIMPLE[] = { ";", NULL };
    static char* SEPS_REDIRECT[] = { "<", ">", "1>", ">>", "1>>", "2>", "2>>", "&>", "&>>", NULL };
    static char* SEPS_LOGICAL[] = { "&&", "||", NULL };
    static char* SEPS_PIPE[] = { "|", NULL };
    static char* SEPS_BACKGROUND[] = { "&", NULL };
    static char** ALL_SEPS[SEP_LAST] =
    {
        SEPS_SIMPLE,
        SEPS_REDIRECT,
        SEPS_LOGICAL,
        SEPS_PIPE,
        SEPS_BACKGROUND
    };

    // Pour chaque type de separateurs de commandes
    for( int type = SEP_SIMPLE; type < SEP_LAST; ++type )
    {
        // Pour chaque separateur correspondant au type
        char** sep = ALL_SEPS[type];
        while( *sep != NULL )
        {
            // Si le token correspond
            if( strcmp( token, *sep ) == 0 )
            {
                // Test positif, on retourne le type du separateur
                return( type );
            }

            // Separateur suivant
            ++sep;
        }
    }

    // Test negatif
    return( SEP_NONE );
}


static int processCmdRedirection( const char* sep, cmd_t* cmd, const char* fileName )
{
    // Flags d'ouverture du fichier (en fonction du type de redirection)
    int flags = 0;

    // Flags qui donnes les redirections a traiter
    int redirections[3] = {0, 0, 0};

    // Selon le type de redirection
    if( strcmp( sep, "<" ) == 0 )
    {
        // Redirection entree standard
        flags |= O_RDONLY;
        redirections[REDIRECT_IN] = 1;
    }
    else if( strcmp( sep, ">" ) == 0 || strcmp( sep, "1>" ) == 0 )
    {
        // Redirection sortie standard (ecrasement du fichier)
        flags |= O_WRONLY | O_CREAT;
        redirections[REDIRECT_OUT] = 1;
    }
    else if( strcmp( sep, ">>" ) == 0 || strcmp( sep, "1>>" ) == 0 )
    {
        // Redirection sortie standard (ajout en fin de fichier)
        flags |= O_WRONLY | O_CREAT | O_APPEND;
        redirections[REDIRECT_OUT] = 1;
    }
    else if( strcmp( sep, "2>" ) == 0 )
    {
        // Redirection erreur standard (ecrasement du fichier)
        flags |= O_WRONLY | O_CREAT;
        redirections[REDIRECT_ERR] = 1;
    }
    else if( strcmp( sep, "2>>" ) == 0 )
    {
        // Redirection erreur standard (ajout en fin de fichier)
        flags |= O_WRONLY | O_CREAT | O_APPEND;
        redirections[REDIRECT_ERR] = 1;
    }
    else if( strcmp( sep, "&>" ) == 0 )
    {
        // Redirection sortie et erreur standards (ecrasement du fichier)
        flags |= O_WRONLY | O_CREAT;
        redirections[REDIRECT_OUT] = 1;
        redirections[REDIRECT_ERR] = 1;
    }
    else if( strcmp( sep, "&>>" ) == 0 )
    {
        // Redirection sortie et erreur standards (ajout en fin de fichier)
        flags |= O_WRONLY | O_CREAT | O_APPEND;
        redirections[REDIRECT_OUT] = 1;
        redirections[REDIRECT_ERR] = 1;
    }

    // Ouverture du fichier avec les flags positionne
    int fd = open( fileName, flags, 0644 );
    if( fd == -1 )
    {
       // Erreur d'ouverture du fichier
       fprintf( stderr, "ERREUR - Echec d'ouverture du fichier %s\n", fileName );
       return( CMD_BAD_REDIRECTION );
    }

    // Mise a jour de la commande (qui de in/out/err doit etre redirigee vers le fichier)
    if( redirections[REDIRECT_IN] ) cmd->in = fd;
    if( redirections[REDIRECT_OUT] ) cmd->out = fd;
    if( redirections[REDIRECT_ERR] ) cmd->err = fd;

    // Ajout du file descriptor dans la liste des fichiers a fermer par la commande
    addFileDescriptor( cmd, fd );

    return( CMD_OK );
}


static void updateCmdChaining( cmd_t* start, cmd_t* end )
{
    // Pour chaque commande de la sequence
    cmd_t* current = start;
    while( current != end )
    {
        // Suivant le type de separateur avec la commande suivante
        switch( current->nextCmdLink )
        {
            // Pipe ou ET logique, en cas d'erreur on va en fin de sequence
            case LINK_PIPE:
            case LINK_AND:
                current->nextFailure = end;
                break;

            // OU logique, en cas de succes on va en fin de sequence
            case LINK_OR:
                current->nextSuccess = end;
                break;

            // Tout autre valeur, pas de traitement
            default:
                break;
        }

        // Commande suivante
        ++current;
    }
}


static int createPipe( cmd_t* firstCmd, cmd_t* secondCmd )
{
    // Creation du pipe
    int pipeFD[2] = {-1, -1};
    if( pipe( pipeFD ) == -1 )
    {
        return( CMD_PIPE_FAILED );
    }

    // La sortie standard de la premiere commande est associee a l'entree du pipe
    firstCmd->out = pipeFD[PIPE_IN];

    // L'entree standard de la deuxieme commande est associee à la sortie du pipe
    secondCmd->in = pipeFD[PIPE_OUT];

    // Ajout du pipe a refermer dans la deuxieme commande (en sortie du pipe)
    secondCmd->fdpipe[0] = pipeFD[0];
    secondCmd->fdpipe[1] = pipeFD[1];

    return( CMD_OK );
}


static void addFileDescriptor( cmd_t* cmd, int fd )
{
    // Recherche de la premiere entree libre du tableau 'fdclose'
    int index = 0;
    while( cmd->fdclose[index] != -1 ) ++index;

    // On rajoute le file descriptor a cette entree
    cmd->fdclose[index] = fd;
}


static void closeCmdFiles( cmd_t* cmd )
{
    // Fermeture des fichiers ouverts
    int i = 0;
    while( i < MAX_CMD_SIZE && cmd->fdclose[i] != -1 )
    {
        // Fermeture du fichier, et passage au descripteur suivant
        close( cmd->fdclose[i++] );
    }

    // Fermeture du pipe (si ouvert)
    if( cmd->fdpipe[0] != -1 ) close( cmd->fdpipe[0] );
    if( cmd->fdpipe[1] != -1 ) close( cmd->fdpipe[1] );
}


static const BgCmd* addBgCmd( cmd_t* cmd )
{
    // Numero attribue a la derniere commande en background qui a ete rajoute
    static int lastCmdNumber = 1;

    // Creation d'une nouvelle commande qui s'execute en background
    BgCmd* bgCmd = (BgCmd*)malloc( sizeof( BgCmd ) );
    bgCmd->pid = cmd->pid;
    bgCmd->number = 0;
    bgCmd->cmdLine[0] = '\0';
    for( int i = 0; i < MAX_CMD_SIZE; ++i )
    {
        if( cmd->argv[i] == NULL ) break;
        strcat( bgCmd->cmdLine, cmd->argv[i] );
        strcat( bgCmd->cmdLine, " " );
    }
    bgCmd->next = NULL;
    bgCmd->previous = NULL;

    // Si premiere commande
    if( backgroundCommands == NULL )
    {
        // Unique commande enregistree
        backgroundCommands = bgCmd;
        bgCmd->number = 1;
        lastCmdNumber = 1;
    }
    else
    {
        // On va sur la derniere commande enregistree
        BgCmd* lastCmd = backgroundCommands;
        while( lastCmd->next != NULL )
        {
            lastCmd = lastCmd->next;
        }

        // On rajoute la commande en fin de liste
        lastCmd->next = bgCmd;
        bgCmd->previous = lastCmd;

        // On attribue le numero suivant a la commande
        bgCmd->number = ++lastCmdNumber;
    }

    return( bgCmd );
}
