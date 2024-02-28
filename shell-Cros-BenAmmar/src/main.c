/*
 *  Projet minishell - Licence 3 Info - PSI 2023
 *
 *  Nom :               CROS		BEN AMMAR
 *  Prénom :            Bryan		Nader
 *  Num. étudiant :     22110106	22101740
 *  Groupe de projet :  Groupe 1
 *  Date :              30/10/2023
 *
 *  Dependances : parser.h cmd.h
 *
 *  Interface du mini-shell
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

#include "parser.h"
#include "cmd.h"


// Codes d'erreur
enum MainError
{
    MAIN_OK = 0,            // Pas d'erreur
    MAIN_BAD_INPUT = 1,     // Erreur de saisie
    MAIN_
};


/*
 * Reinitialisation des variables avant une nouvelle ligne de commande.
 *
 * cmdWords : tableau des mots consituant la ligne de commande
 * cmds : tableau des commandes
 */
static void reinit( char* cmdWords[], cmd_t* cmds )
{
    // Pour chaque element des tableaux (ils ont la meme taille)
    for( int i = 0; i < MAX_CMD_SIZE; ++i ) {

        // Si un mot existe
        if( cmdWords[i] != NULL )
        {
            // Destruction et reinitialisation du mot
            free( cmdWords[i] );
            cmdWords[i] = NULL;
        }

        // Reinitialisation de la commande
        initCmd( cmds + i );
    }
}


/*
 * Saisie d'une ligne de commande, et mise en forme :
 * - Suppression des espaces en debut et en fin de ligne
 * - Ajout d'eventuels espaces autour des connecteurs (; ! || && & ...)
 * - Suppression des doublons d'espaces
 * - Traitement des variables d'environnement
 *
 * cmdLine : la ligne de commande saisie et mise en forme
 *
 * Retourne 0 si la saisie est correcte, sinon un code d'erreur
 */
static int getCmdLine( char* cmdLine )
{
    // Buffer de saisie
    char buff[MAX_LINE_SIZE] = {'\0'};

    // Saisie de la ligne de commande
    if( fgets( buff, MAX_LINE_SIZE, stdin ) == NULL )
    {
        // Erreur de saisie
        return( MAIN_BAD_INPUT );
    }
    else
    {
        // Suppression du '\n' en fin de ligne
        buff[strlen( buff ) - 1] = '\0';
    }

    // Trim de la saisie
    trim( buff );

    // Si rien n'a ete saisi, on retourne
    if( strlen( buff ) == 0 ) return( MAIN_OK );

    // Suppression des doublons
    clean( buff );

    // Mise en evidence des separateurs
    showSeparators( buff );

    // Traitement des variables d'environnement
    const int status = substEnv( buff );
    if( status != PARSER_OK ) return( status );

    // Recopie de la ligne de commande mise en forme
    strcpy( cmdLine, buff );

    return( MAIN_OK );
}


/*
 * Recherche la commande suivante a executer en fonction de la commande courante et du resultat de son execution.
 *
 * current : pointeur sur la commande courante
 *
 * Retourne un pointeur sur la nouvelle commande courante (ou NULL si plus de commande)
 */
static cmd_t* nextCmd( cmd_t* current )
{
    // S'il existe une commande suivante inconditionnelle
    if( current->next != NULL )
    {
        // On utilise cette commande
        return current->next;
    }

    // Sinon, si la commande a echoue et s'il existe une commande suivante en cas d'erreur
    else if( current->status != 0 && current->nextFailure != NULL )
    {
        // On utilise cette commande
        return current->nextFailure;
    }

    // Sinon, si la commande a reussie et s'il existe une commande suivante en cas de succes
    else if( current->status == 0 && current->nextSuccess != NULL )
    {
        // On utilise cette commande
        return current->nextSuccess;
    }

    // Pas de commande suivante disponible
    return( NULL );
}


/*
 * Teste si un file descriptor est deja stocke dans un tableau
 *
 * fd : le file descriptor a rechercher
 * fds : le tableau de file descriptors
 * maxIndex : index de la premiere entree libre dans le tableau
 * retourne 1 si le file descriptor est deja present dans le tableau, 0 sinon
 */
static int fdIsAlreadyStored( int fd, const int fds[], int maxIndex )
{
    // Pour chaque element du tableau
    for( int i = 0; i < maxIndex; ++i )
    {
        // Si l'element correspond a celui recherche, l'element est trouve
        if( fds[i] == fd ) return( 1 );
    }

    // L'element n'existe pas dans le tableau
    return( 0 );
}


/*
 * Mise a jour de la liste des fichier ouverts qui doivent etre refermes.
 *
 * Dans un premier temps, la fonction construit un tableau global qui fusionne les tableaux 'fdclose' de
 * totes les commandes, de sorte a construire la liste globale de tous les fichiers ouverts (et a refermer).
 * Dans un deuxieme temps, cette liste globale vient ecraser le tableau 'fdclose' de chaque commande, de sorte
 * à ce que les processus d'execution associes refermeent egalement ces fichiers ouverts, dont ils ont herites
 * lors du fork.
 *
 * A noter que cette liste ne gere pas les pipes, qui sont traites par ailleurs via le champ 'fdpipe' des commandes.
 *
 * cmds : tableau des commandes
 * cmdCount : nombre de commandes utilisees
 * allFDs : en sortie, tableau des fichiers ouverts
 */
static void updateFDClose( cmd_t cmds[], int cmdCount, int allFDs[] )
{
    // Index de la prochaine entree du tableau a utiliser
    int iNextFD = 0;

    // On construit la liste globale des file descriptors. Pour chaque commande...
    for( int i = 0; i < cmdCount; ++i )
    {
        // On merge la liste de file descriptors de la commande avec la liste globale
        const cmd_t* cmd = cmds + i;
        int iFD = 0;
        while( cmd->fdclose[iFD] != -1 )
        {
            // Si le file descriptor n'est pas deja stocke dans la liste globale
            if( ! fdIsAlreadyStored( cmd->fdclose[iFD], allFDs, iNextFD ) )
            {
                // On rajoute le file descriptor dans la liste globale
                allFDs[iNextFD++] = cmd->fdclose[iFD];
            }

            // File descriptor suivant
            ++iFD;
        }
    }

    // On marque la fin du tableau avec un -1
    allFDs[iNextFD++] = -1;

    // On doit maintenant mettre a jour la liste des fichiers a refermer pour chaque commande
    for( int i = 0; i < cmdCount; ++i )
    {
        // On recopie le tableau global dans la commande
        cmd_t* cmd = cmds + i;
        memcpy( cmd->fdclose, allFDs, iNextFD * sizeof( int ) );
    }
}


/*
 * Callback sur la reception du signal SIGCHLD.
 *
 * Ce callback n'est actif que lorsque le shell n'execute pas de commande en premier plan. Il permet de
 * savoir lorsqu'une commande executee en arriere plan se termine
 *
 * sigNum : nulero du signal a l'origine de l'appel
 */
static void onBgCmdCompletion( int sigNum )
{
    // Si le signal orrespond a la terminaison d'un processus fils, il s'agit d'une commande executee en arriere plan
    if( sigNum == SIGCHLD )
    {
        // On passe a la ligne suivante du prompt
        printf( "\n" );

        // Boucle de synchronisation sur la terminaison du ou des processus (car il peut y en avaoir plusieurs)
        while( 1 )
        {
            // On se synchronise avec la terminaison d'un processus
            int status = 0;
            const pid_t pid = waitpid( -1, &status, WNOHANG );

            // Si plus de processus, on sort de la boucle
            if( pid <= 0 ) break;

            // Si la commande en background correspondante existe
            BgCmd* bgCmd = removeBgCmd( pid );
            if( bgCmd != NULL )
            {
                // Affichage de l'information de terminaison
                printf( "[%d]   Fini (status = %d)           %s\n",
                        bgCmd->number, WEXITSTATUS( status ), bgCmd->cmdLine );

                // Liberation memoire
                free( bgCmd );
            }
        }

        // Reaffichage du prompt
        printf( "$ " );
        fflush( stdout );
    }
}


/*
 * Fonction principale du programme
 */
int main(int argc, char* argv[])
{
    // Tableau des mots de la ligne de commande entree par l'utilisateur (fini par NULL)
    char* cmdWords[MAX_CMD_SIZE];

    // Tableau des commandes a executer
    cmd_t cmds[MAX_CMD_SIZE];

    // Initialisation du contenu des tableaux
    for( int i = 0; i < MAX_CMD_SIZE; ++i )
    {
        // Tableau de mots
        cmdWords[i] = NULL;

        // Tableau des arguments de la commande #i. Cela est necessaire car la fonction initCmd() libere la memoire
        // des arguments de ce tableau, qui est alloue via malloc().
        cmd_t* cmd = cmds + i;
        for( int iArg = 0; iArg < MAX_CMD_SIZE; ++iArg )
        {
            cmd->argv[iArg] = NULL;
        }
    }

    // Boucle de traitement des lignes de commandes
    while (1)
    {
        // Reinitialisation avant la prochaine ligne de commande
        reinit( cmdWords, cmds );

        // Affichage du prompt
        char cwd[MAX_CMD_SIZE];
        char* statusPrompt = getcwd(cwd, sizeof(cwd));
        if( statusPrompt == NULL )
        {
			// Erreur de saisie, on sort du programme
            fprintf( stderr, "ERREUR - Erreur lors de la récupération du prompte (affichage du prompte classique -> $)\n" );
            printf("$ ");
            fflush( stdout );
		}
		else
		{
			printf("%s $ ", cwd);
			fflush( stdout );
		}

        // Ligne de commande entree par l'utilisateur
        char cmdLine[MAX_LINE_SIZE] = {'\0'};

        // Saisie de la ligne de commande sur l'entree standard
        int status = getCmdLine( cmdLine );
        if( status != 0 )
        {
            // Erreur de saisie, on sort du programme
            fprintf( stderr, "ERREUR - Erreur de saisie [code = %d]\n", status );
            break;
        }

        // Si ligne vide, on recommence la saisie
        if( strlen( cmdLine ) == 0 ) continue;
        //printf( "Saisie : '%s'\n", cmdLine );

        // On decoupe la ligne de commande en mots
        strcut( cmdLine, ' ', cmdWords );
        //printf( "Tokens :\n" );
        //int i = 0;
        //while( cmdWords[i] ) { printf( "- %s\n", cmdWords[i++] ); }

        // Construction des commandes a partir des mots de la ligne de commande
        int cmdCount = 0;
        status = parseCmd( cmdWords, cmds, &cmdCount );
        if( status != 0 )
        {
            // Erreur de parsing, on sort du programme
            fprintf( stderr, "ERREUR - Erreur de parsing [code = %d]\n", status );
            break;
        }
        //printf( "Commandes :\n" );
        //for( int i = 0; i < cmdCount; ++i ) printCmd( cmds + i );

        // Mise a jour de la liste des fichiers ouverts (et a refermer apres execution)
        int allFDs[MAX_CMD_SIZE] = {-1};
        updateFDClose( cmds, cmdCount, allFDs );

        // On desactive le callback sur la terminaison des processus fils (commandes en background)
        signal( SIGCHLD, SIG_DFL );

        // Execution des commandes dans l'ordre etabli lors du parsing
        cmd_t* current = cmds;
        while( current != NULL )
        {
            // Execution de la commande courante
            const int status = execCmd( current );
            if( status != CMD_OK )
            {
                fprintf( stderr, "ERREUR - Erreur d'exécution [code = %d]\n", status );
                //break;
            }

            // Passage a la commande suivante
            current = nextCmd( current );
        }

        // On resactive le callback sur la terminaison des processus fils (commandes en background)
        signal( SIGCHLD, onBgCmdCompletion );

        // On referme tous les fichiers ouverts
        int i = 0;
        while( i < MAX_CMD_SIZE && allFDs[i] != -1 )
        {
            // Fermeture du fichier, et passage au descripteur suivant
            close( allFDs[i++] );
        }
    }

    return( MAIN_OK );
}
