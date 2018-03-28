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

//messages envoyés fréquemment par le serveur
const char *acknowledged = "ACK\n";

// Client thread waiting time
int wait_time = 5;

// Number of resources
int nb_resources;

// Maximum resources that can be requested by each process
struct array_t *max;

// Resources currently being holded by each process
struct array_t *allocated;

// Resources available
int *available;

// Client ids by index
struct array_t *client_ids;

int clients_running;

// Mutex pour l'acces aux sections critiques et donnes partagees
pthread_mutex_t critical_mutex = PTHREAD_MUTEX_INITIALIZER;

// Mutex pour l'acces aux donnees du journal
pthread_mutex_t journal_mutex = PTHREAD_MUTEX_INITIALIZER;

// Helper functions
void print_resources();
void print_array(int array[]);
int getClientIdx(int client_id);
int isValid (int client_idx, int req[]);
int isSafe (int client_idx, int req[]);

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
  //FILE *socket_w = fdopen (socket_fd, "w");
  char cmd[4] = {NUL, NUL, NUL, NUL};
  fread (cmd, 3, 1, socket_r);
  char *args = NULL;
  size_t args_len = 0;
  getline (&args, &args_len, socket_r);

  if(strcmp(cmd, "BEG") == 0) {
    fprintf(stdout, "Init received cmd %s%s", cmd, args);
    // Initialise nombre de resources
    nb_resources = atoi(args);
    send (socket_fd, acknowledged, strlen(acknowledged), 0);
  } else {
    char *err_msg = "ERR Server not initialized\n";
    send (socket_fd, err_msg, strlen(err_msg), 0);
  }

  fread (cmd, 3, 1, socket_r);
  getline (&args, &args_len, socket_r);

  if(strcmp(cmd, "PRO") == 0) {
    fprintf(stdout, "Init received cmd %s%s", cmd, args);

    // Initialise quantite initiale de ressources
    available = malloc(nb_resources * sizeof(int));

    available[0] = atoi(strtok(args, " "));
    for(int i=1 ; i < nb_resources; i++) {
      available[i] = atoi(strtok(NULL, " "));
    }
    send (socket_fd, acknowledged, strlen(acknowledged), 0);
  } else {
    char *err_msg = "ERR Please provide resources\n";
    send (socket_fd, err_msg, strlen(err_msg), 0);
  }

  // Initialise structures
  allocated = new_array(2);
  max = new_array(2);
  client_ids = new_array(2);
  clients_running = 0;

  free(args);
  fclose(socket_r);
  //fclose(socket_w);
  close(socket_fd);

  // TODO : DEBUG
  print_resources();
  // END TODO
}

//TODO : for debug  purposes
void print_resources() {
  fflush(stdout);
  char strA[100];
  sprintf(strA,"Available resources:");
  for(int j=0; j < nb_resources; j++) {
    sprintf(strA, "%s %d", strA, available[j]);
  }
  sprintf(strA, "%s\n", strA);
  fprintf(stdout, "%s\n", strA);

}
//TODO : for debug  purposes
void print_array(int array[]) {
  fflush(stdout);
  char strA[100];
  sprintf(strA,"Array:");
  for(int j=0; j < nb_resources; j++) {
    sprintf(strA, "%s %d", strA, array[j]);
  }
  sprintf(strA, "%s\n", strA);
  fprintf(stdout, "%s\n", strA);

}

void
st_process_requests (server_thread * st, int socket_fd)
{
  // TODO: Remplacer le contenu de cette fonction
  FILE *socket_r = fdopen (socket_fd, "r");
  //FILE *socket_w = fdopen (socket_fd, "w");

    char cmd[4] = {NUL, NUL, NUL, NUL};
    if (!fread (cmd, 3, 1, socket_r))
      return;
    char *args = NULL; size_t args_len = 0;
    ssize_t cnt = getline (&args, &args_len, socket_r);
    if (!args || cnt < 1 || args[cnt - 1] != '\n')
    {
      printf ("Thread %d received incomplete cmd=%s!\n", st->id, cmd);
      return;
    }

    printf ("Thread %d received the command: %s%s\n", st->id, cmd, args);

    int ct_id = atoi(strtok(args, " "));
    // Case 1 : ini
    if(strcmp(cmd, "INI") == 0) {
      // Initialise ressources pour client
      // TODO: check for unique id
      int *max_client = malloc(nb_resources * sizeof(int));
      int *alloc_client = malloc(nb_resources * sizeof(int));
      for(int i=0; i < nb_resources ; i++) {
        max_client[i] = atoi(strtok(NULL, " "));
        alloc_client[i] = 0;
      }

      // Section critique
      pthread_mutex_lock(&critical_mutex);
      push_back(max, max_client);
      push_back(allocated, alloc_client);
      push_back(client_ids, ct_id);
      nb_registered_clients++;
	  clients_running++;
      pthread_mutex_unlock(&critical_mutex);

      printf("Thread %d initialized client %d\n", st->id, ct_id);
      send (socket_fd, acknowledged, strlen(acknowledged), 0);
    } else if(strcmp(cmd, "REQ") == 0) {

      // Case 2 : req
      // Parse request args
      int req[nb_resources];
      for(int i=0; i < nb_resources; i++) {
          req[i] = atoi(strtok(NULL, " "));
      }

      // Section critique
      pthread_mutex_lock(&critical_mutex);

      int idx = getClientIdx(ct_id);
      printf("Thread %d received request from client %d at index %d\n", st->id, ct_id, idx);
      if(idx < 0 || isValid(idx, req) == 0) { // Test request validity
        pthread_mutex_unlock(&critical_mutex);
        printf("Request invalid\n");
        if(idx < 0) {
          char *err_msg =  "ERR invalid id\n";
       	  send (socket_fd, err_msg, strlen(err_msg), 0);
        } else {
          char *err_msg = "ERR invalid resources\n";
          send (socket_fd, err_msg, strlen(err_msg), 0);
        }
        // Update journal
        pthread_mutex_lock(&journal_mutex);
        count_invalid++;
        pthread_mutex_unlock(&journal_mutex);
      } else if(isSafe(idx, req) == 1) {  // Test safe state
        // Grant request
        int *alloc_client = allocated->data[idx];
        for(int i=0; i < nb_resources; i++) {
          alloc_client[i] = req[i] + alloc_client[i];
          available[i] = available[i] - req[i];
        }
        print_resources();
        pthread_mutex_unlock(&critical_mutex);

        printf("Request granted\n");
        send (socket_fd, acknowledged, strlen(acknowledged), 0);

        // Update journal
        //TODO : keep track of waiting clients
        pthread_mutex_lock(&journal_mutex);
        count_accepted++;
        pthread_mutex_unlock(&journal_mutex);
      } else {
        pthread_mutex_unlock(&critical_mutex);
	printf("Request put on wait\n");
        char wait_msg[9];
        sprintf(wait_msg, "WAIT ");
        sprintf(wait_msg, "%s%d\n",wait_msg, wait_time);
        send (socket_fd, &wait_msg, strlen(wait_msg), 0);
               
      }
    } else if(strcmp(cmd, "CLO") == 0) {

      // Case 3 : clo

      //Section critique
      pthread_mutex_lock(&critical_mutex);

      int idx = getClientIdx(ct_id);
      fflush(stdout);
      fprintf(stdout, "Thread %d closed client %d at index %d\n", st->id, ct_id, idx);

      if(idx < 0) {
        char *err_msg = "ERR invalid process id\n";
        send (socket_fd, err_msg, strlen(err_msg), 0);
      } else {
		 clients_running--;
        // Test if there are allocations left
        int *alloc_client = allocated->data[idx];
        int is_free = 1;
        for(int i=0; i < nb_resources; i++) {
          if(alloc_client[i] != 0) {
            is_free = 0;
          }
        }

        if(is_free == 1) {
          // TODO : remove id, deallocate max and allocated
          pthread_mutex_unlock(&critical_mutex);
          send (socket_fd, acknowledged, strlen(acknowledged), 0);

          pthread_mutex_lock(&journal_mutex);
          count_dispatched++;
          pthread_mutex_unlock(&journal_mutex);
        } else {
          pthread_mutex_unlock(&critical_mutex);
          char *err_msg = "ERR Client still holding ressources\n";
          send (socket_fd, err_msg, strlen(err_msg), 0);
        }
        pthread_mutex_lock(&journal_mutex);
        clients_ended++;
        pthread_mutex_unlock(&journal_mutex);
      }
    } else if(strcmp(cmd, "END") == 0) {
      // Case 4 : end
      // Section critique
      pthread_mutex_lock(&critical_mutex);
      if(clients_running == 0) {
	    //TODO : free structures max, allocated, client_ids
        delete_array_callback(&max, free);
        delete_array_callback(&allocated, free);
        delete_array_callback(&client_ids, free);
        free(available);
        pthread_mutex_unlock(&critical_mutex);

        pthread_mutex_destroy(&critical_mutex);
        pthread_mutex_destroy(&journal_mutex);
        send (socket_fd, acknowledged, strlen(acknowledged), 0);
	    accepting_connections = 0;
	  } else {
		pthread_mutex_unlock(&critical_mutex);
        char *err_msg = "ERR Clients still running\n";
        send (socket_fd, err_msg, strlen(err_msg), 0); 
	  }
    } else {
      char *err_msg = "ERR Unknown command\n";
      send (socket_fd, err_msg, strlen(err_msg), 0);
    }
    free (args);

  fclose (socket_r);
  //fclose (socket_w);
  // TODO end
}

int getClientIdx(int client_id) {
  for(int i=0; i < nb_registered_clients; i++) {
    if(client_ids->data[i] == client_id) {
      return i;
    }
  }
  return -1;
}

// Test if the request is valid
int isValid (int client_idx, int req[]) {
  int *max_client = max->data[client_idx];
  int *alloc_client = allocated->data[client_idx];

  for(int i=0; i < nb_resources; i++) {
    if(req[i] > max_client[i] - alloc_client[i] || req[i] + alloc_client[i] < 0){
    //  fflush(stdout);
   //   fprintf(stdout, "Req %d > max %d - alloc %d\n", req[i], max_client[i], alloc_client[i]);
    //  fprintf(stdout, "req %d + alloc %d < 0\n", req[i], alloc_client[i]);
      return 0;
    }
  }
  return 1;
}

// Test if the new state following a request is safe
int isSafe (int client_idx, int req[]) {

  // Test if the request can be granted
  int newstate[nb_resources];
  for(int i=0; i < nb_resources; i++) {
    newstate[i] = available[i] - req[i];
    if(newstate[i] < 0) {
      return 0;
    }
  }

  // Compute temp alloc matrix
  int alloc[nb_registered_clients][nb_resources];
  for(int i=0; i < nb_registered_clients; i++) {
    int *alloc_client = allocated->data[i];
    for(int j=0; j < nb_resources; j++) {
      if(client_idx == i) {
        alloc[i][j] = alloc_client[j] + req[j];
      } else {
        alloc[i][j] = alloc_client[j];
      }
    }
  }

  // Test if new state is safe
  int nb_running = nb_registered_clients;
  int running[nb_registered_clients];
  for(int i=0; i < nb_registered_clients; i++) {
    running[i] = 1;
  }
  
 // fprintf(stdout, "Checking safe mode, nb running = %d\n", nb_running);
  //fprintf(stdout, "Initial new state");
  //print_array(newstate);

  int at_least_one_allocated;
  while(nb_running > 0) {
    at_least_one_allocated = 0;
    for(int i=0; i < nb_registered_clients; i++) {
      if(running[i] == 1) {
        int can_finish = 1;
        int *max_ct = max->data[i];
        for(int j=0; j < nb_resources; j++) {
          if(newstate[j] - (max_ct[j] - alloc[i][j]) < 0) {
            can_finish = 0;
          }
        }
        if(can_finish == 1) {
        // fflush(stdout);
        // fprintf(stdout, "Client %d finished, new new state\n", i);
         at_least_one_allocated = 1;
          running[i] = 0;
          nb_running--;
          for(int k=0; k < nb_resources; k++) {
            newstate[k] = newstate[k] + alloc[i][k];
          }
         // print_array(newstate); 
        }
      }
    }
    if(at_least_one_allocated == 0) {
//     fflush(stdout);
//     fprintf(stdout, "Unsafe state with %d still running, none allocated\n", nb_running);
     return 0;
    }
  }
  return 1;
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
    //TODO:
    fflush(stdout);
    fprintf(stdout, "Waiting for client connection\n");
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
