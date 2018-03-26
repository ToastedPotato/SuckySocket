/* This `define` tells unistd to define usleep and random.  */
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>

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

pthread_mutex_t ack_mutex = PTHREAD_MUTEX_INITIALIZER;

// Nombre de requête en attente (WAIT reçus en réponse à REQ)
unsigned int count_on_wait = 0;

pthread_mutex_t req_wait_mutex = PTHREAD_MUTEX_INITIALIZER;

// Nombre de requête refusée (REFUSE reçus en réponse à REQ)
unsigned int count_invalid = 0;

pthread_mutex_t err_mutex = PTHREAD_MUTEX_INITIALIZER;

// Nombre de client qui se sont terminés correctement (ACK reçu en réponse à END)
unsigned int count_dispatched = 0;

pthread_mutex_t dispatch_mutex = PTHREAD_MUTEX_INITIALIZER;

// Nombre total de requêtes envoyées.
unsigned int request_sent = 0;

pthread_mutex_t sent_mutex = PTHREAD_MUTEX_INITIALIZER;

unsigned int server_ready = 0;

pthread_mutex_t server_setup = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t socket_read = PTHREAD_MUTEX_INITIALIZER;

unsigned int num_running = 0;
// Vous devez modifier cette fonction pour faire l'envoie des requêtes
// Les ressources demandées par la requête doivent être choisies aléatoirement
// (sans dépasser le maximum pour le client). Elles peuvent être positives
// ou négatives.
// Assurez-vous que la dernière requête d'un client libère toute les ressources
// qu'il a jusqu'alors accumulées.
void
send_request (int client_id, int request_id, int resend, int req_values[], 
    int max[], int held[], int socket_fd)
{
    //j'ai dû modifier la signature de la méthode d'envoi de requête pour 
    //satisfiare les contraintes ci-haut

    // TP2 TODO
    char req[200];
    
    if (resend){
        
        //renvoi de la requête précédente suite à un WAIT
        sprintf(req, "REQ %d", client_id);
        for(int j=0; j < num_resources; j++) {
            
            sprintf(req, "%s %d", req, req_values[j]);
        }
        sprintf(req, "%s\n", req);
    }else{
        
        if (request_id < num_request_per_client - 1){
            
            //requêtes normales 
            sprintf(req, "REQ %d", client_id);
            for(int j=0; j < num_resources; j++) {
               
                int value;            
                do{
                    
                   value = (rand() % ((max[j]+1) * 2)) - (max[j] + 1);
                }while(value < (held[j] * -1) && value < 0);
                
                sprintf(req, "%s %d", req, value);
                req_values[j] = value;
            }
            sprintf(req, "%s\n", req);
            
        }else {
            
            //requête finale, on libère toutes les ressources
            sprintf(req, "REQ %d", client_id);
            for(int j=0; j < num_resources; j++) {
                
                int value = (held[j] * -1);
                sprintf(req, "%s %d", req, value);
                req_values[j] = value;
            }
            sprintf(req, "%s\n", req);
        }
    }
    
    send(socket_fd, &req, strlen(req), 0);
        fprintf (stdout, "Client %d is sending request #%d\n", client_id,
            (request_id + 1));
    
    //mise à jour des statistiques
    pthread_mutex_lock(&sent_mutex);
    request_sent++;
    pthread_mutex_unlock(&sent_mutex);
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
    if (connect(socket_fd, (struct sockaddr *) &serv_addr, 
    sizeof (serv_addr))  < 0) {
    // wrong way to check for errors with nonblocking sockets...
    //	perror ("ERROR connecting");
    //	exit(1);
    }
           
    // Initialize server
    pthread_mutex_lock(&server_setup);
    if(!server_ready){
        
        char server_response[50];
            
        char character[2];
            
        int counter = 0;
                        
        char beg[80];
        sprintf(beg, "BEG %d\n", num_resources);
        
        do{
            send(socket_fd, &beg, strlen(beg), 0);
                    
            do{                
                memset(character, 0, strlen(character));
                read(socket_fd, character, 1);
                sprintf(server_response, "%s%s", server_response, character);
                counter++;
            }while(strstr(character, "\n") == NULL);
            server_response[counter+1] = '\0';
        }while(strstr(server_response, "ACK") == NULL);
        
        memset(server_response, 0, strlen(server_response));             
        counter = 0;
        char pro[80];
        sprintf(pro,"PRO");
        for(int j=0; j < num_resources; j++) {
            sprintf(pro, "%s %d", pro, provisioned_resources[j]);
        }
        sprintf(pro, "%s\n", pro);
        
        do{
            send(socket_fd, &pro, strlen(pro), 0);
                    
            do{                
                memset(character, 0, strlen(character));
                read(socket_fd, character, 1);
                sprintf(server_response, "%s%s", server_response, character);
                counter++;
            }while(strstr(character, "\n") == NULL);
            server_response[counter+1] = '\0';
        }while(strstr(server_response, "ACK") == NULL);
                
        server_ready = 1;
        
    }else{fprintf(stdout, "Server ready!\n");}
    pthread_mutex_unlock(&server_setup);       
    // Initialize client thread
    
    char init[80];
    int max_resources[num_resources];
    int needed[num_resources];
    sprintf(init, "INI %d", ct->id);

    srand((unsigned int)time(NULL) + ct->pt_tid);  

    for(int i=0; i < num_resources; i++) {
        int value = rand() % provisioned_resources[i];
        max_resources[i] = value;
        needed[i] = value;
        sprintf(init, "%s %d", init, value);
    }
    sprintf(init, "%s\n", init);
    send(socket_fd, &init, strlen(init), 0);
        

    //TODO: protect with mutex
    num_running++;

    // Vous devez ici faire l'initialisation des petits clients (`INI`).
    // TP2 TODO:END
    
    int held[num_resources];
    for (int i = 0; i < num_resources; i++){
        held[i] = 0;    
    }
    
    for (unsigned int request_id = 0; request_id < num_request_per_client;
      request_id++)
    {

        // TP2 TODO
        // Vous devez ici coder, conjointement avec le corps de send request,
        // le protocole d'envoi de requête.
               
        int requested[num_resources];
        
        int request_outcome = -1;
        
        int wait_time = 0;

        while (request_outcome != 1) {
                  
            int sum = 0;
            for(int i = 0; i < num_resources; i++){
                sum = sum + needed[i];
            }
            
            if(sum == 0){request_id = num_request_per_client -1;}
            
            if (request_outcome == 0){
                
                //renvoi de la même requête sous réception d'un "WAIT"                
                fprintf(stdout, "Wait time: %i seconds\n", wait_time);
                sleep(wait_time);
                send_request (ct->id, request_id, 1, requested, max_resources, 
                    held, socket_fd);
            }else {
            
                //nouvelle requête
                send_request (ct->id, request_id, 0, requested, max_resources, 
                    held, socket_fd);
            }
            
            //pour l'instant, j'assumes que les réponses du serveur < 40 char
            char server_response[50];
            
            char character[2];
            
            int counter = 0;
            
            do{                
                memset(character, 0, strlen(character));
                read(socket_fd, character, 1);
                sprintf(server_response, "%s%s", server_response, character);
                counter++;
            }while(strstr(character, "\n") == NULL);
            server_response[counter+1] = '\0';
            
            fprintf(stdout, "%s\n", server_response);
            
            if(strstr(server_response, "ACK")){
                
                request_outcome = 1;
                
                pthread_mutex_lock(&ack_mutex);
                count_accepted++;
                pthread_mutex_unlock(&ack_mutex);
            }else if(strstr(server_response, "WAIT")){
                
                //la durée de l'attente est le nombre après "WAIT "                
                strtok(server_response, " ");
                wait_time = atoi (strtok(NULL, ""));
                request_outcome = 0;
                
                pthread_mutex_lock(&req_wait_mutex);
                count_on_wait++;
                pthread_mutex_unlock(&req_wait_mutex);
            }else if(strstr(server_response, "ERR")){
                
                //"ERR *msg*"
                request_outcome = -1;
                pthread_mutex_lock(&err_mutex);
                count_invalid++;
                pthread_mutex_unlock(&err_mutex);
                sleep(5);
            }
        }
        
        //mise à jour des ressources si succès de la requête
        for (int i = 0; i < num_resources; i++){
            
            held[i] = held[i] + requested[i];
            if(requested[i] > 0){
                needed[i] = needed[i] - requested[i];
            }
        }
        // TP2 TODO:END

        /* Attendre un petit peu (0s-0.1s) pour simuler le calcul.  */
        usleep (random () % (100 * 1000));
        /* struct timespec delay;
         * delay.tv_nsec = random () % (100 * 1000000);
         * delay.tv_sec = 0;
         * nanosleep (&delay, NULL); */
    }

    // TODO: Send CLO to server
    char close[10];
    sprintf(init, "CLO %d\n", ct->id);
    send(socket_fd, &close, strlen(close), 0);
    
    num_running--;
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
  //TODO : deux solutions : 
  // 1. keep track of num_running threads
  //  send END to server when num_running = 0  
  // **need to establish connection with server

  // Probablement mieux!!
  // 2. keep track of server status
  // send END in last thread to finish + update server status
  // no need to reopen a connection here

  sleep (4);
  while(num_running != 0) {
    //busy wait
  }
  // Send end request
  printf("Ending client %d", num_running);
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
