#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define DEFAULT_WORLD_SIZE 50


typedef struct {
    int** world;
    sem_t mutex;
} simulation;

typedef struct {
    int argc;
    char** argv;
} startupCommands;

void initializeWorld() {

}

void error(const char *msg) {
    errir(msg);
    exit(1);
}

void *clientHandler(void *arg) {
    startupCommands *sc = (startupCommands*)arg;


    if (sc->argc < 2){
        fprint(stderr , "Port nebol zadaný!");
        exit(1);
    }
    int sockfd, newsockfd, portno, n;
    char buffer[255];

    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        error("Nepodarilo sa otvoriť socket!");
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(sc->argv[1]);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if(bind(sockfd , (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 1) {
        error("Binding Failed.");
    }


}

void *simulationManager(void *arg) {

}


int main(int argc, char** argv) {
    startupCommands sc = {.argc = argc, .argv = argv};



    return 0;
}
