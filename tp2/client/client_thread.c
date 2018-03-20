/* This `define` tells unistd to define usleep and random.  */
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client_thread.h"

// Socket library
//#include <netdb.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

int port_number = -1;
int num_request_per_client = -1;
int num_resources = -1;
int *provisioned_resources = NULL;

// Variable d'initialisation des threads clients.
unsigned int count = 0;

// Variable du journal.
// Nombre de requête acceptée (ACK reçus en réponse à REQ)
unsigned int count_accepted = 0;

// Nombre de requête en attente (WAIT reçus en réponse à REQ)
unsigned int count_on_wait = 0;

// Nombre de requête refusée (REFUSE reçus en réponse à REQ)
unsigned int count_invalid = 0;

// Nombre de client qui se sont terminés correctement (ACK reçu en réponse à END)
unsigned int count_dispatched = 0;

// Nombre total de requêtes envoyées.
unsigned int request_sent = 0;

// Vous devez modifier cette fonction pour faire l'envoie des requêtes
// Les ressources demandées par la requête doivent être choisies aléatoirement
// (sans dépasser le maximum pour le client). Elles peuvent être positives
// ou négatives.
// Assurez-vous que la dernière requête d'un client libère toute les ressources
// qu'il a jusqu'alors accumulées.
void
send_request (int client_id, int request_id, int socket_fd)
{
  // TP2 TODO
  char str[200];
  sprintf(str, "REQ %d 1 1 1 1 1\n", client_id);
  write(socket_fd, &str, sizeof(str));
  fprintf (stdout, "Client %d is sending its %d request\n", client_id,
      request_id);
  free(str);
  // TP2 TODO:END

}


void *
ct_code (void *param)
{
  int socket_fd = -1;
  client_thread *ct = (client_thread *) param;

  // TP2 TODO
  // Connection au server.
  // Create socket
  socket_fd = socket (AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (socket_fd < 0) {
  	perror ("ERROR opening socket");
  	exit(1);
  }

  // Connect
  struct sockaddr_in serv_addr;
  memset (&serv_addr, 0, sizeof (serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons (port_number);
  if (connect(socket_fd, (struct sockaddr *) &serv_addr, sizeof (serv_addr))  < 0) {
  // wrong way to check for errors with nonblocking sockets...
  //	perror ("ERROR connecting");
  //	exit(1);
  }

  // Initialize server
  // TODO: send only once
  char beg[80];
  sprintf(beg, "BEG %d\n", num_resources);
  write(socket_fd, &beg, strlen(beg));

  char pro[80];
  sprintf(pro,"PRO");
  for(int j=0; j < num_resources; j++) {
    sprintf(pro, "%s %d", pro, provisioned_resources[j]);
  }
  sprintf(pro, "%s\n", pro);
  write(socket_fd, &pro, strlen(pro));

  // Initialize client thread
 // TODO : generate random value for initialization
  char init[80];
  sprintf(init, "INIT %d", ct->id);
  for(int i=0; i < num_resources; i++) {
    sprintf(init, "%s %d", init, 1);
  }
  sprintf(init, "%s\n", init);
  write(socket_fd, &init, strlen(init));

// Write to socket
 // FILE *socket_w = fdopen (socket_fd, "w");

  //First thread initialize server
 // fprintf (socket_w, "BEG %d\n", num_resources);
 // fprintf (socket_w, "PRO 1 1 1 1 1\n");

//  fclose (socket_w);

 // socket_w = fdopen (socket_fd, "w");
 // fprintf (socket_w, "INI %d 0 0 0 0 0\n", ct->id);
 // fclose (socket_w); //probably shouldn't be closed here...

  // Vous devez ici faire l'initialisation des petits clients (`INI`).
  // TP2 TODO:END

  for (unsigned int request_id = 0; request_id < num_request_per_client;
      request_id++)
  {

    // TP2 TODO
    // Vous devez ici coder, conjointement avec le corps de send request,
    // le protocole d'envoi de requête.

    send_request (ct->id, request_id, socket_fd);

    // Last request
    if(request_id == num_request_per_client -1) {
      // TODO: Free resources
      // TODO: Send CLO to server
      close(socket_fd);
    }
    // TP2 TODO:END

    /* Attendre un petit peu (0s-0.1s) pour simuler le calcul.  */
    usleep (random () % (100 * 1000));
    /* struct timespec delay;
     * delay.tv_nsec = random () % (100 * 1000000);
     * delay.tv_sec = 0;
     * nanosleep (&delay, NULL); */
  }

  return NULL;
}


//
// Vous devez changer le contenu de cette fonction afin de régler le
// problème de synchronisation de la terminaison.
// Le client doit attendre que le serveur termine le traitement de chacune
// de ses requêtes avant de terminer l'exécution.
//
void
ct_wait_server ()
{

  // TP2 TODO: IMPORTANT code non valide.

  sleep (4);

  // TP2 TODO:END

}


void
ct_init (client_thread * ct)
{
  ct->id = count++;
}

void
ct_create_and_start (client_thread * ct)
{
  pthread_attr_init (&(ct->pt_attr));
  pthread_create (&(ct->pt_tid), &(ct->pt_attr), &ct_code, ct);
  pthread_detach (ct->pt_tid);
}

//
// Affiche les données recueillies lors de l'exécution du
// serveur.
// La branche else ne doit PAS être modifiée.
//
void
st_print_results (FILE * fd, bool verbose)
{
  if (fd == NULL)
    fd = stdout;
  if (verbose)
  {
    fprintf (fd, "\n---- Résultat du client ----\n");
    fprintf (fd, "Requêtes acceptées: %d\n", count_accepted);
    fprintf (fd, "Requêtes : %d\n", count_on_wait);
    fprintf (fd, "Requêtes invalides: %d\n", count_invalid);
    fprintf (fd, "Clients : %d\n", count_dispatched);
    fprintf (fd, "Requêtes envoyées: %d\n", request_sent);
  }
  else
  {
    fprintf (fd, "%d %d %d %d %d\n", count_accepted, count_on_wait,
        count_invalid, count_dispatched, request_sent);
  }
}
