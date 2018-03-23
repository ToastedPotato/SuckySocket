#define _XOPEN_SOURCE 700   /* So as to allow use of `fdopen` and `getline`.  */

#include "server_thread.h"

#include <netinet/in.h>
#include <netdb.h>

#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <signal.h>

#include <time.h>

enum { NUL = '\0' };

enum {
  /* Configuration constants.  */
  max_wait_time = 30,
  server_backlog_size = 5
};

int server_socket_fd;

// Nombre de client enregistré.
int nb_registered_clients;

// Variable du journal.
// Nombre de requêtes acceptées immédiatement (ACK envoyé en réponse à REQ).
unsigned int count_accepted = 0;

// Nombre de requêtes acceptées après un délai (ACK après REQ, mais retardé).
unsigned int count_wait = 0;

// Nombre de requête erronées (ERR envoyé en réponse à REQ).
unsigned int count_invalid = 0;

// Nombre de clients qui se sont terminés correctement
// (ACK envoyé en réponse à CLO).
unsigned int count_dispatched = 0;

// Nombre total de requête (REQ) traités.
unsigned int request_processed = 0;

// Nombre de clients ayant envoyé le message CLO.
unsigned int clients_ended = 0;

// TODO: Ajouter vos structures de données partagées, ici.

// Number of resources
int nb_resources;

// Maximum resources that can be requested by each process
struct array_t *max;

// Resources currently being holded by each process
struct array_t *allocated;

// Resources available
int *available;


static void sigint_handler(int signum) {
  // Code terminaison.
  accepting_connections = 0;
}

void
st_init ()
{
  // Handle interrupt
  signal(SIGINT, &sigint_handler);

  // Initialise le nombre de clients connecté.
  nb_registered_clients = 0;

  // TODO

  // Attend la connection d'un client et initialise les structures pour
  // l'algorithme du banquier.

  // Attend une connection
  struct sockaddr_in addr;
  socklen_t socket_len = sizeof(addr);
  int socket_fd = -1;
  while(socket_fd < 0) {
   socket_fd = accept(server_socket_fd, (struct sockaddr *)&addr, &socket_len);
  }
  FILE *socket_r = fdopen (socket_fd, "r");
  char cmd[4] = {NUL, NUL, NUL, NUL};
  fread (cmd, 3, 1, socket_r);
  char *args = NULL;
  size_t args_len = 0;
  ssize_t cnt = getline (&args, &args_len, socket_r);

  if(strcmp(cmd, "BEG") == 0) {
    fprintf(stdout, "Init received cmd %s%s", cmd, args);
    // Initialise nombre de resources
    nb_resources = atoi(args);
  }

  fread (cmd, 3, 1, socket_r);
  cnt = getline (&args, &args_len, socket_r);

  if(strcmp(cmd, "PRO") == 0) {
    fprintf(stdout, "Init received cmd %s%s", cmd, args);

    // Initialise quantite initiale de ressources
    available = malloc(nb_resources * sizeof(int));

    available[0] = atoi(strtok(args, " "));
    for(int i=1 ; i < nb_resources; i++) {
      available[i] = atoi(strtok(NULL, " "));
    }
  }

  // Initialise max
  allocated = new_array(2);

  // Initialise allocated
  max = new_array(2);

  free(args);
  fclose(socket_r);
  close(socket_fd);

  char strA[100];
  sprintf(strA,"Available resources:");
  for(int j=0; j < nb_resources; j++) {
    sprintf(strA, "%s %d", strA, available[j]);
  }
  sprintf(strA, "%s\n", strA);
  fprintf(stdout, "%s\n", strA);
  // END TODO
}

void
st_process_requests (server_thread * st, int socket_fd)
{
  // TODO: Remplacer le contenu de cette fonction
  FILE *socket_r = fdopen (socket_fd, "r");
  FILE *socket_w = fdopen (socket_fd, "w");

  while (true)
  {
    char cmd[4] = {NUL, NUL, NUL, NUL};
    if (!fread (cmd, 3, 1, socket_r))
      break;
    char *args = NULL; size_t args_len = 0;
    ssize_t cnt = getline (&args, &args_len, socket_r);
    if (!args || cnt < 1 || args[cnt - 1] != '\n')
    {
      printf ("Thread %d received incomplete cmd=%s!\n", st->id, cmd);
      break;
    }

    printf ("Thread %d received the command: %s%s\n", st->id, cmd, args);

    int ctId = atoi(strtok(args, " "));
    // Case 1 : ini
    if(strcmp(cmd, "INI") == 0) {
      nb_registered_clients++;
      // Initialise ressources max pour client
      // TODO: increment max/allocated dynamically
      // TODO: test validity : max <= provisionned resources?? (can that be done?)
      int *resources = malloc(nb_resources * sizeof(int));
      for(int i=0; i < nb_resources ; i++) {
        resources[i] = atoi(strtok(NULL, " "));
      }
      push_back(max, resources);
      int *pointer = max->data[0];
      printf("Thread %d initialized client %d with %d\n", st->id, ctId, pointer[0]);
      // reply
    } else if(strcmp(cmd, "REQ") == 0) {
      // Case 2 : req
      printf("Thread %d received request from client %d\n", st->id, ctId);
      // TODO: process request
      // Parse request args
      int req[nb_resources];
      for(int i=0; i < nb_resources; i++) {
          req[i] = atoi(strtok(NULL, " "));
      }
      // Test request validity
      // req <= max - allocated for client
      // req + allocated >= 0 for client
      // reply with error if invalid 
      // Test if request can be granted
      // newstate = available - req >= 0 
      // if(cannot be granted)
      // reply with wait
      // Test new state
      // if(isSafe)
      // grant request
      // reply with ack
      // else reply with wait
    } else if(strcmp(cmd, "CLO") == 0) {
      // Case 3 : clo
      printf("Thread %d closed client %d", st->id, ctId);
      // TODO : deallocate max and allocated
      nb_registered_clients--;
      // reply ack
    } else if(strcmp(cmd, "END") == 0) {
      // Case 4 : end
      //TODO : Close server / free structures
      // reply ACK
    } else {
      fprintf (socket_w, "ERR Unknown command\n");
    }
    free (args);
  }

  fclose (socket_r);
  fclose (socket_w);
  // TODO end
}
// Test if a state is safe
int isSafe (int state[]) {
    // 1. Let work and finish vectors of m and n length
    // initialize work = available
    // finish = false

    // 2. find i such that finish[i] = false & need[i][] <= work
    // if !E, go to 4.

    // 3. if E, work = work + allocated[i][]
    // finish[i] = true
    // go to 2.

    // 4. if finish=true for all i
    // isSafe = true
    return 0;
}


/*void
st_signal ()
{
  // TODO: Remplacer le contenu de cette fonction



  // TODO end
}*/

int st_wait() {
  struct sockaddr_in thread_addr;
  socklen_t socket_len = sizeof (thread_addr);
  int thread_socket_fd = -1;
  int end_time = time (NULL) + max_wait_time;

  while(thread_socket_fd < 0 && accepting_connections) {
    thread_socket_fd = accept(server_socket_fd,
        (struct sockaddr *)&thread_addr,
        &socket_len);
    if (time(NULL) >= end_time) {
      break;
    }
  }
  return thread_socket_fd;
}

void *
st_code (void *param)
{
  server_thread *st = (server_thread *) param;

  int thread_socket_fd = -1;

  // Boucle de traitement des requêtes.
  while (accepting_connections)
  {
    // Wait for a I/O socket.
    thread_socket_fd = st_wait();
    if (thread_socket_fd < 0)
    {
      fprintf (stderr, "Time out on thread %d.\n", st->id);
      continue;
    }

    if (thread_socket_fd > 0)
    {
      st_process_requests (st, thread_socket_fd);
      close (thread_socket_fd);
    }
  }
  return NULL;
}


//
// Ouvre un socket pour le serveur.
//
void
st_open_socket (int port_number)
{
  server_socket_fd = socket (AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (server_socket_fd < 0) {
    perror ("ERROR opening socket");
    exit(1);
  }
  if (setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0) {
    perror("setsockopt()");
    exit(1);
  }

  struct sockaddr_in serv_addr;
  memset (&serv_addr, 0, sizeof (serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons (port_number);

  if (bind
      (server_socket_fd, (struct sockaddr *) &serv_addr,
       sizeof (serv_addr)) < 0)
    perror ("ERROR on binding");

  listen (server_socket_fd, server_backlog_size);
}


//
// Affiche les données recueillies lors de l'exécution du
// serveur.
// La branche else ne doit PAS être modifiée.
//
void
st_print_results (FILE * fd, bool verbose)
{
  if (fd == NULL) fd = stdout;
  if (verbose)
  {
    fprintf (fd, "\n---- Résultat du serveur ----\n");
    fprintf (fd, "Requêtes acceptées: %d\n", count_accepted);
    fprintf (fd, "Requêtes en attente: %d\n", count_wait);
    fprintf (fd, "Requêtes invalides: %d\n", count_invalid);
    fprintf (fd, "Clients : %d\n", count_dispatched);
    fprintf (fd, "Requêtes traitées: %d\n", request_processed);
  }
  else
  {
    fprintf (fd, "%d %d %d %d %d\n", count_accepted, count_wait,
        count_invalid, count_dispatched, request_processed);
  }
}
