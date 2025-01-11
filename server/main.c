#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>

#define DEFAULT_WORLD_SIZE 10
#define DEFAULT_NUM_OF_REPLICATIONS 1000
#define DEFAULT_MOVEMENT_CHANCE 25
#define DEFAULT_BLOCKADE_CHANCE 50
#define PORT 10023

typedef enum { RUNNING, STOPPED, PAUSED } SimulationState;


typedef struct {
    int x;
    int y;
    int pocetPohybov;
    int chanceUp;
    int chanceRight;
    int chanceLeft;
    int chanceDown;

} opilec;

typedef struct {
    int** world; // 0- prázdne políčko; 1- prekážka; 2- opilec
    pthread_mutex_t mutex;
    volatile SimulationState sim_state;
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

int pointInBounds(int x, int y) {
    return x >= 0 && x < DEFAULT_WORLD_SIZE && y >= 0 && y < DEFAULT_WORLD_SIZE;
}

int dfs(simulation* sim, int fromStartX, int fromStartY, opilec* op, int** navstivene) {
    // Ak sme mimo mapy, na prekážke alebo už navštívení
    if (!pointInBounds(fromStartX, fromStartY) || sim->world[fromStartX][fromStartY] == 1 || navstivene[fromStartX][fromStartY] == 1) {
        return 0;
    }

    // Ak sme dosiahli cieľ
    if (fromStartX == op->x && fromStartY == op->y) {
        return 1;
    }

    // Označiť ako navštívené
    navstivene[fromStartX][fromStartY] = 1;

    // Pohyb hore, dole, vľavo, vpravo
    int dx[] = {1, -1, 0, 0};
    int dy[] = {0, 0, 1, -1};
    // Rekurzívne skúmať susedov
    for (int i = 0; i < 4; i++) {
        if (dfs(sim, fromStartX + dx[i], fromStartY + dy[i], op, navstivene)) {
            return 1;
        }
    }

    return 0; // Žiadna cesta
}

void vypisSim(simulation* sim) {
    printf("\n");
    if(sim->mode == 0){ // interaktívny mód
        for (int row = 0; row < DEFAULT_WORLD_SIZE; row++) {
            for (int col = 0; col < DEFAULT_WORLD_SIZE; col++) {
                if (sim->world[row][col] == 0)
                    printf(".  "); // prázdne miesto
                if (sim->world[row][col] == 1)
                    printf("#  "); // prekážka
                if (sim->world[row][col] == 2)
                    printf("X  "); // opilec
            }
            printf("\n"); // Nový riadok pre každý riadok poľa
        }
        printf("\nLegenda: \n. -> voľné miesto\n# -> prekážka\nX -> opilec\n");
        printf("\nPočet pohybov Opilca: %d\n", sim->op->pocetPohybov);
    } else {
        printf("\nNesprávny mód!\n");
        return;
    }
}

// bude sa volať v client Handlerovi. Simulation manager začne pracovať až po tom, čo sa ukončí initialize world úspešne
int initializeWorld(simulation* sim, int simType, int mode) {
    sim->world = malloc(DEFAULT_WORLD_SIZE * sizeof(int*)); // Pole ukazovateľov na riadky
    for (int i = 0; i < DEFAULT_WORLD_SIZE; i++) {
        sim->world[i] = malloc(DEFAULT_WORLD_SIZE * sizeof(int)); // Každý riadok
    }

    //sem_init(sim->canRun, 0, 1);
    sim->NumOfReplications = DEFAULT_NUM_OF_REPLICATIONS;
    sim->simType = simType;
    sim->mode = mode;

    sim->op->x = rand() % DEFAULT_WORLD_SIZE;
    sim->op->y = rand() % DEFAULT_WORLD_SIZE;

    if (simType == 0) { // setup bez prekážok
        for (int i = 0; i < DEFAULT_WORLD_SIZE; i++) {
            for (int j = 0; j < DEFAULT_WORLD_SIZE; j++) {
                sim->world[i][j] = 0;
            }
        }
        sim->world[sim->op->x][sim->op->y] = 2;

    } else if (simType == 1) { // setup s prekážkami
        int** navstivene = malloc(DEFAULT_WORLD_SIZE * sizeof(int*)); // Pole ukazovateľov na riadky
        for (int s = 0; s < DEFAULT_WORLD_SIZE; s++) {
            navstivene[s] = malloc(DEFAULT_WORLD_SIZE * sizeof(int)); // Každý riadok
        }


        for (int i = 0; i < DEFAULT_WORLD_SIZE; i++) {
            for (int j = 0; j < DEFAULT_WORLD_SIZE; j++) {

                for (int k = 0; k < DEFAULT_WORLD_SIZE; k++) {
                    for (int l = 0; l < DEFAULT_WORLD_SIZE; l++) {
                        navstivene[k][l] = 0;
                    }
                }

                if (rand() % 100 < DEFAULT_BLOCKADE_CHANCE && dfs(sim, 0, 0, sim->op, navstivene) && (i != 0 && j != 0) && (i != sim->op->x && j != sim->op->y)) {
                    sim->world[i][j] = 1;
                } else{
                    sim->world[i][j] = 0;
                }

            }
        }
        for (int i = 0; i < DEFAULT_WORLD_SIZE; i++) {
            free(navstivene[i]);
        }
        free(navstivene);

        sim->world[sim->op->x][sim->op->y] = 2;
    } else {
        printf("Zle zadaný vstup typu simulácie.\n");
        return -1;
    }

    vypisSim(sim);

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

        pthread_mutex_lock(&sim->mutex);

        //akonáhle príde správa od clientHandlera že je spustiteľná, tak sa začne vykonávať.
        if(sim->sim_state == RUNNING) {
            int chance = rand() % (sim->op->chanceDown + sim->op->chanceLeft + sim->op->chanceRight + sim->op->chanceUp);
            int smer;

            if (chance < sim->op->chanceUp) {
                smer = 0;
            } else if (chance < sim->op->chanceRight + sim->op->chanceUp) {
                smer = 1;
            } else if (chance < sim->op->chanceDown + sim->op->chanceRight + sim->op->chanceUp) {
                smer = 2;
            } else {
                smer = 3;
            }

            switch(smer) {
                case 0: // hore
                    if (sim->op->y - 1 < 0) {
                        break;
                    } else if (sim->world[sim->op->x][sim->op->y-1] == 1) {
                        break;
                    } else {
                        sim->world[sim->op->x][sim->op->y] = 0;
                        sim->world[sim->op->x][sim->op->y-1] = 2;
                        sim->op->y--;
                        sim->op->pocetPohybov++;
                    }
                    break;
                case 1: // vpravo
                    if (sim->op->x + 1 >= DEFAULT_WORLD_SIZE) {
                        break;
                    } else if (sim->world[sim->op->x+1][sim->op->y] == 1) {
                        break;
                    } else {
                        sim->world[sim->op->x][sim->op->y] = 0;
                        sim->world[sim->op->x+1][sim->op->y] = 2;
                        sim->op->x++;
                        sim->op->pocetPohybov++;
                    }
                    break;
                case 2: // dole
                    if (sim->op->y + 1 >= DEFAULT_WORLD_SIZE) {
                        break;
                    } else if (sim->world[sim->op->x][sim->op->y+1] == 1) {
                        break;
                    } else {
                        sim->world[sim->op->x][sim->op->y] = 0;
                        sim->world[sim->op->x][sim->op->y+1] = 2;
                        sim->op->y++;
                        sim->op->pocetPohybov++;
                    }
                    break;
                case 3: // vľavo

                    if (sim->op->x - 1 < 0) {
                        break;
                    } else if (sim->world[sim->op->x-1][sim->op->y] == 1) {
                        break;
                    } else {
                        sim->world[sim->op->x][sim->op->y] = 0;
                        sim->world[sim->op->x-1][sim->op->y] = 2;
                        sim->op->x--;
                        sim->op->pocetPohybov++;
                    }
                    break;
            }
            vypisSim(sim);
            if(sim->op->x == 0 && sim->op->y == 0) {
                sim->sim_state = STOPPED;
            }
            pthread_mutex_unlock(&sim->mutex);
        }
        if (sim->sim_state == STOPPED) {
            printf("Simulácia bola ukončená.\n");
            pthread_mutex_unlock(&sim->mutex);
            break;
        }

    }
}


int main(int argc, char** argv) {
    srand(time(NULL));
    if (argc < 2) {
        printf("\nNezadal si číslo socektu!\n");
        return -1;
    }
    simulation sim;
    opilec opi;
    sim.op = &opi;
    config sc = {.argc = argc, .argv = argv, .sim_c = &sim};
    pthread_mutex_init(&sim.mutex, NULL);
    initializeWorld(&sim, 1, 0);
    sim.sim_state = RUNNING;
    opi.chanceDown = DEFAULT_MOVEMENT_CHANCE;
    opi.chanceUp = DEFAULT_MOVEMENT_CHANCE;
    opi.chanceRight = DEFAULT_MOVEMENT_CHANCE;
    opi.chanceLeft = DEFAULT_MOVEMENT_CHANCE;

    pthread_t clientManager;
    pthread_t simulationManagerT;

    pthread_create(&clientManager, NULL, &clientHandler, &sc );
    pthread_create(&simulationManagerT, NULL, &simulationManager, &sim);

    pthread_join(simulationManagerT, NULL);
    pthread_join(clientManager, NULL);

    for (int i = 0; i < DEFAULT_WORLD_SIZE; i++) {
        free(sim.world[i]);
    }
    free(sim.world);
    pthread_mutex_destroy(&sim.mutex);

    return 0;
}
