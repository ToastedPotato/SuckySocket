#include <stdlib.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "client_thread.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

int
main (int argc, char *argv[])
{
  if (argc < 5) {
    fprintf (stderr, "Usage: %s <port-nb> <nb-clients> <nb-requests> <resources>...\n",
        argv[0]);
    exit (1);
  }

  port_number = atoi (argv[1]);
  int num_clients = atoi (argv[2]);
  num_request_per_client = atoi (argv[3]);
  num_resources = argc - 4;

  provisioned_resources = malloc (num_resources * sizeof (int));
  for (unsigned int i = 0; i < num_resources; i++)
    provisioned_resources[i] = atoi (argv[i + 4]);
  
  int socket_fd = -1;
   

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
    if (connect(socket_fd, (struct sockaddr *) &serv_addr, 
    sizeof (serv_addr))  < 0) {
    // wrong way to check for errors with nonblocking sockets...
    //	perror ("ERROR connecting");
    //	exit(1);
    }
       
    // Initialize server
   
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
               
   
  
  client_thread *client_threads
    = malloc (num_clients * sizeof (client_thread));
  for (unsigned int i = 0; i < num_clients; i++)
    ct_init (&(client_threads[i]));

  for (unsigned int i = 0; i < num_clients; i++) {
    ct_create_and_start (&(client_threads[i]));
  }

  ct_wait_server ();

  // Affiche le journal.
  st_print_results (stdout, true);
  FILE *fp = fopen("client.log", "w");
  if (fp == NULL) {
    fprintf(stderr, "Could not print log");
    return EXIT_FAILURE;
  }
  st_print_results (fp, false);
  fclose(fp);

  return EXIT_SUCCESS;
}
