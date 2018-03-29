/* This `define` tells unistd to define usleep and random.  */
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client_thread.h"

// Socket library
//#include <netdb.h>

#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

int port_number = -1;
int num_request_per_client = -1;
int num_resources = -1;
int *provisioned_resources = NULL;

// Variable d'initialisation des threads clients.
unsigned int count = 0;

//message de terminaison de l'exécution du serveur 
const char *end_msg = "END\n";
        
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

int server_status = 0;

pthread_mutex_t server_setup = PTHREAD_MUTEX_INITIALIZER;

unsigned int num_running = 0;

int ct_connect (){
      // Create socket
    int socket_fd = socket (AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
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
    	fprintf (stdout, "%s", strerror(errno));
    	exit(1);
    }
  return socket_fd;
}

// Vous devez modifier cette fonction pour faire l'envoie des requêtes
// Les ressources demandées par la requête doivent être choisies aléatoirement
// (sans dépasser le maximum pour le client). Elles peuvent être positives
// ou négatives.
// Assurez-vous que la dernière requête d'un client libère toute les ressources
// qu'il a jusqu'alors accumulées.
void reConnect (int socket_fd){
    close(socket_fd);
    fflush(stdout);
    fprintf(stdout, "Trying to reconnect\n");
    socket_fd = socket (AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (socket_fd < 0) {
        perror ("ERROR opening socket\n");
        exit(1);
    }   
    
    struct sockaddr_in serv_addr;
    memset (&serv_addr, 0, sizeof (serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons (port_number);
    
    connect(socket_fd, (struct sockaddr *) &serv_addr, 
        sizeof (serv_addr));
}

void get_response(int socket_fd, char *buffer, int bufsize){

    memset(buffer, 0, strlen(buffer));
    recv(socket_fd, buffer, bufsize-1, MSG_WAITALL);
    
    if(strchr(buffer, '\n') != NULL){
        buffer[buffer - strchr(buffer, 
            '\n')] = '\0';
    }else{memset(buffer, 0, strlen(buffer));}
}

// Vous devez modifier cette fonction pour faire l'envoie des requêtes
// Les ressources demandées par la requête doivent être choisies aléatoirement
// (sans dépasser le maximum pour le client). Elles peuvent être positives
// ou négatives.
// Assurez-vous que la dernière requête d'un client libère toute les ressources
// qu'il a jusqu'alors accumulées.
void
send_request (int client_id, int request_id, int resend, int req_values[], 
    int max[], int held[], int socket_fd, const struct sockaddr *addr)
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
                    
                   value = (rand() % ((max[j]+1)*2))-(max[j]+1);
                }while(value < (held[j] * -1) || value > max[j]-held[j]);
                
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

    // TODO : to remove
    struct sockaddr_in serv_addr;
    memset (&serv_addr, 0, sizeof (serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons (port_number);
    // TP2 TODO
           
    // Initialize server
    pthread_mutex_lock(&server_setup);
    if(!server_status){
        
		// Connection au server
		socket_fd = ct_connect();
                        
        char beg[80];
        sprintf(beg, "BEG %d\n", num_resources);
		send(socket_fd, &beg, strlen(beg), 0);
		
		char init_response[50]; 
		do{
        //sleep(1);
                
			get_response(socket_fd, init_response, 
            sizeof(init_response));
                
		}while(strlen(init_response) <= 0);
		fprintf(stdout, "Response : %s\n", init_response);
        
        if(strstr(init_response, "ERR") != NULL){exit(1);}
                              
        char pro[80];
        sprintf(pro,"PRO");
        for(int j=0; j < num_resources; j++) {
            sprintf(pro, "%s %d", pro, provisioned_resources[j]);
        }
        sprintf(pro, "%s\n", pro);
        send(socket_fd, &pro, strlen(pro), 0);
		
		do{
        //sleep(1);
                
			get_response(socket_fd, init_response, 
            sizeof(init_response));
                
		}while(strlen(init_response) <= 0);
		fprintf(stdout, "Response : %s\n", init_response);
		
        if(strstr(init_response, "ERR") != NULL){exit(1);}
        
        close(socket_fd);      
        server_status = 1;
        
    }else{fprintf(stdout, "Server ready for requests\n");}
    pthread_mutex_unlock(&server_setup);     
	
    // Initialize client thread
    // Vous devez ici faire l'initialisation des petits clients (`INI`).
	
	// Connection au server
    socket_fd = ct_connect();
    
    int max_resources[num_resources];	
	char init[80];
    sprintf(init, "INI %d", ct->id);
    srand((unsigned int)time(NULL) + ct->pt_tid);  

    for(int i=0; i < num_resources; i++) {
        int value = rand() % provisioned_resources[i];
        max_resources[i] = value;
        sprintf(init, "%s %d", init, value);
    }
    sprintf(init, "%s\n", init);
    
	send(socket_fd, &init, strlen(init), 0);
	
    char response[50];
	do{
        //sleep(1);
                
        get_response(socket_fd, response, 
            sizeof(response));
                
    }while(strlen(response) <= 0);
    fprintf(stdout, "Response : %s\n", response);
	close(socket_fd);
        
    //Ressources allouées au client
    int held[num_resources];
    for (int i = 0; i < num_resources; i++){
        held[i] = 0;    
    }    
    //Valeurs de la requête la plus récente
    int requested[num_resources]; 
    for(int i = 0; i < num_resources; i++){
        requested[i] = 0;
    }
	
	// TP2 TODO:END
    
    for (unsigned int request_id = 0; request_id < num_request_per_client;
      request_id++)
    {

        // TP2 TODO
        // Vous devez ici coder, conjointement avec le corps de send request,
        // le protocole d'envoi de requête.
               
        int request_outcome = 0;
        int wait_time = 0;
        int resend = 0;

        while (request_outcome != 1) {
            // Connection au server
			socket_fd = ct_connect();
			
            // Envoi de la requête                
            send_request (ct->id, request_id, resend, requested, max_resources, 
                held, socket_fd, (struct sockaddr *) &serv_addr);
            
            // Après le renvoi d'une requête suite à un wait
            resend = 0;
                        
            //pour l'instant, j'assume que les réponses du serveur < 50 char
            char server_response[50];
            do{
                //sleep(1);
                
                get_response(socket_fd, server_response, 
                    sizeof(server_response));
                
            }while(strlen(server_response) <= 0);
            fprintf(stdout, "Response : %s\n", server_response);   
			
            if(strstr(server_response, "ACK") != NULL){
                
                request_outcome = 1;                
                //mise à jour des ressources tenues car succès de la requête
                for (int i = 0; i < num_resources; i++){  
                    held[i] = held[i] + requested[i];             
                }
                
                pthread_mutex_lock(&ack_mutex);
                count_accepted++;
                pthread_mutex_unlock(&ack_mutex);
            }else if(strstr(server_response, "WAIT") != NULL){
                
                //la durée de l'attente est le nombre après "WAIT "                
                strtok(server_response, " ");
                char *token =  (strtok(NULL, ""));
                wait_time = atoi(token);
                usleep(wait_time * 100000);
                resend = 1;
                
                pthread_mutex_lock(&req_wait_mutex);
                count_on_wait++;
                pthread_mutex_unlock(&req_wait_mutex);
            }else if(strstr(server_response, "ERR") != NULL){
                
                //"ERR *msg*"
                pthread_mutex_lock(&err_mutex);
                count_invalid++;
                pthread_mutex_unlock(&err_mutex);
            }
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

    // TODO: Send CLO to server
	// Connection au server
	socket_fd = ct_connect();
			
    char clo[10];
    sprintf(clo, "CLO %d\n", ct->id);
	send(socket_fd, &clo, strlen(clo), 0);
    //send(socket_fd, &clo, strlen(clo), MSG_NOSIGNAL);
	do{
        //sleep(1);
                
        get_response(socket_fd, response, 
            sizeof(response));
                
    }while(strlen(response) <= 0);
    fprintf(stdout, "Response : %s\n", response);
	
    //recv(socket_fd, response, 49, MSG_WAITALL);
	fflush(stdout);
    fprintf(stdout, "Sending CLO, response : %s\n", response); 
	close(socket_fd);
    
    pthread_mutex_lock(&dispatch_mutex);
    if(strstr(response, "ACK") != NULL) {
      count_dispatched++;
    }	
    num_running--;

    fprintf(stdout, "num_running : %d\n", num_running);
	// End server
    if(num_running == 0){
        socket_fd = ct_connect();
        fprintf(stdout, "Sending end\n");			
        send(socket_fd, end_msg, strlen(end_msg), 0);
		do{
        //sleep(1);
			get_response(socket_fd, response, 
            sizeof(response));
                
        }while(strlen(response) <= 0);
	fflush(stdout);
	fprintf(stdout, "Sending END, response : %s\n", response);
        
        close(socket_fd);
		server_status = -1;
    }
    pthread_mutex_unlock(&dispatch_mutex);
    
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

    
    // Wait until server status is "ended"
    while(server_status != -1) {
        //busy wait
        sleep (4);
    }
    //TODO: deallocate everything
    //destruction des mutex à la fin de l'exécution car ils ne sont plus requis
    pthread_mutex_destroy(&ack_mutex);
    pthread_mutex_destroy(&req_wait_mutex);
    pthread_mutex_destroy(&err_mutex);
    pthread_mutex_destroy(&dispatch_mutex);
    pthread_mutex_destroy(&sent_mutex);
    pthread_mutex_destroy(&server_setup);
  
    // TP2 TODO:END

}


void
ct_init (client_thread * ct)
{
  ct->id = count++;
  num_running++;
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
    fprintf (fd, "Requêtes en attente: %d\n", count_on_wait);
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
