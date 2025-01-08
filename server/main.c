#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define DEFAULT_WORLD_SIZE 50
#define DEFAULT_NUM_OF_REPLICATIONS 1000

typedef struct {
    int x;
    int y;
} opilec;

typedef struct {
    int** world;
    //sem_t mutex;
    sem_t canRun;
    int mode; // 0-interaktívny; 1-sumárny
    int simType; // 0- bez prekážok; 1- s prekážkami
    int NumOfReplications;
    opilec* op;
} simulation;

typedef struct {
    int argc;
    char** argv;
    simulation* sim_c;
} config;

// bude sa volať v client Handlerovi. Simulation manager začne pracovať až po tom, čo sa ukončí initialize world úspešne
int initializeWorld(simulation* sim, int simType, int mode) {
    sim->world = malloc((DEFAULT_WORLD_SIZE * DEFAULT_WORLD_SIZE) * sizeof(int));
    sem_init(sim->canRun, 0, 1);
    sim->NumOfReplications = DEFAULT_NUM_OF_REPLICATIONS;
    sim->simType = simType;
    sim->mode = mode;

    if (simType == 0) { // setup bez prekážok
        sim->op->x = rand() % DEFAULT_WORLD_SIZE;
        sim->op->y = rand() % DEFAULT_WORLD_SIZE;
    } else if (simType == 1) { // setup s prekážkami
        for (int i = 0; i < DEFAULT_WORLD_SIZE; i++) {

        }
    } else {
        printf("Zle zadaný vstup typu simulácie.");
        return -1;
    }



    return 0;
}

void *clientHandler(void *arg) {
    int server_fd, new_socket;
    ssize_t valread;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    char buffer[1024] = { 0 };
    char* hello = "Hello from server";

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET,
                   SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr*)&address,
             sizeof(address))
        < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    if ((new_socket
                 = accept(server_fd, (struct sockaddr*)&address,
                          &addrlen))
        < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
    valread = read(new_socket, buffer,
                   1024 - 1); // subtract 1 for the null
    // terminator at the end
    printf("%s\n", buffer);
    send(new_socket, hello, strlen(hello), 0);
    printf("Hello message sent\n");

    // closing the connected socket
    close(new_socket);
    // closing the listening socket
    close(server_fd);
    return 0;

}

void *simulationManager(void *arg) {
    simulation *sim = (simulation*) arg;
    while (1) {
        // tu sa bude čakať, kým bude simulácia spustiteľná.
        sem_wait(sim->canRun);
        //akonáhle príde správa od clientHandlera že je spustiteľná, tak sa začne vykonávať.
        break;

    }
}


int main(int argc, char** argv) {
    srand(time(NULL));
    simulation sim;
    opilec opi;
    sim.op = &opi;
    config sc = {.argc = argc, .argv = argv, .sim_c = &sim};
    //initializeWorld(&sim);

    pthread_t clientManager;
    pthread_t simulationManagerT;

    pthread_create(&clientManager, NULL, &clientHandler, &sc );
    pthread_create(&simulationManagerT, NULL, &simulationManager, &sim);

    pthread_join(clientManager, NULL);
    free(sim.world);

    return 0;
}
