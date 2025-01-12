#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h> // Pre mkdir
#include <sys/types.h> // Pre mkdir
#include <string.h>    // Pre strcat
#include <dirent.h>

#define DEFAULT_MOVEMENT_CHANCE 25
#define DEFAULT_BLOCKADE_CHANCE 50
#define PORT 10023

typedef enum { RUNNING, STOPPED, PAUSED } SimulationState;


typedef struct {
    int OriginalX;
    int OriginalY;
    int x;
    int y;
    int pocetPohybov;
    int chanceUp;
    int chanceRight;
    int chanceLeft;
    int chanceDown;
    char simulationName[100];
} opilec;

typedef struct {
    int** worldOriginal; // 0- prázdne políčko; 1- prekážka; 2- opilec
    int** world; // 0- prázdne políčko; 1- prekážka; 2- opilec
    pthread_mutex_t mutex;
    volatile SimulationState sim_state;
    int mode; // 0-interaktívny; 1-sumárny
    int simType; // 0- bez prekážok; 1- s prekážkami
    int NumOfReplications;
    int pocetSpravenychReplikacii;
    opilec* op;
    int maxPocetKrokov;
    int rozmerX;
    int rozmerY;
    int failed;
    int success;
    int alokovane;
    char simulationName[100];
} simulation;

typedef struct {
    int argc;
    char** argv;
    simulation* sim_c;


} config;

void vypisSim(simulation* sim);
void saveToFile(simulation *sim);
void loadFromFile();

void vypisSouboryKlientovi(int clientSocket) {
    char* folderName = "svety";
    DIR* dir = opendir(folderName);
    if (dir == NULL) {
        perror("Nepodarilo sa otvoriť adresár");
        send(clientSocket, "Nepodarilo sa otvoriť adresár svety/\n", strlen("Nepodarilo sa otvoriť adresár svety/\n"), 0);
        return;
    }

    struct dirent* entry;
    char buffer[1024];
    buffer[0] = '\0'; // Inicializácia bufferu pre správu klientovi

    strcat(buffer, "Zoznam súborov v adresári svety/:\n");

    while ((entry = readdir(dir)) != NULL) {
        // Ignoruj "." a ".."
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            strcat(buffer, entry->d_name);
            strcat(buffer, "\n");
        }
    }

    closedir(dir);

    // Poslanie zoznamu klientovi
    send(clientSocket, buffer, strlen(buffer), 0);
}

void reinitializeWorldForReplication(simulation *sim) {
    for (int i = 0; i < sim->rozmerX; i++) {
        memcpy(sim->world[i], sim->worldOriginal[i], sim->rozmerY * sizeof(int));
    }

    sim->op->x = sim->op->OriginalX;
    sim->op->y = sim->op->OriginalY;

    vypisSim(sim);

}

int pointInBounds(int x, int y, int maxX, int maxY) {
    return x >= 0 && x < maxX && y >= 0 && y < maxY;
}

int dfs(simulation* sim, int fromStartX, int fromStartY, opilec* op, int** navstivene) {
    // Ak sme mimo mapy, na prekážke alebo už navštívení
    if (!pointInBounds(fromStartX, fromStartY, sim->rozmerX, sim->rozmerY) || sim->world[fromStartX][fromStartY] == 1 || navstivene[fromStartX][fromStartY] == 1) {
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
        for (int row = 0; row < sim->rozmerX; row++) {
            for (int col = 0; col < sim->rozmerY; col++) {
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
    } else {
        printf("\nNesprávny mód!\n");
        return;
    }
}

// bude sa volať v client Handlerovi. Simulation manager začne pracovať až po tom, čo sa ukončí initialize world úspešne
int initializeWorld(simulation* sim) {
    if (sim->alokovane == 0) {
        sim->alokovane = 1;
        sim->world = malloc(sim->rozmerX * sizeof(int*)); // Pole ukazovateľov na riadky
        for (int i = 0; i < sim->rozmerX; i++) {
            sim->world[i] = malloc(sim->rozmerY * sizeof(int)); // Každý riadok
        }

        sim->worldOriginal = malloc(sim->rozmerX * sizeof(int*)); // Pole ukazovateľov na riadky
        for (int i = 0; i < sim->rozmerX; i++) {
            sim->worldOriginal[i] = malloc(sim->rozmerY * sizeof(int)); // Každý riadok
        }
    } else {
        for (int i = 0; i < sim->rozmerX; i++) {
            for (int j = 0; j < sim->rozmerY; j++) {
                sim->world[i][j] = 0;
                sim->worldOriginal[i][j] = 0;
            }
        }
    }
    sim->failed = 0;
    sim->success = 0;

    sim->op->x = rand() % sim->rozmerX;
    sim->op->y = rand() % sim->rozmerY;

    sim->op->OriginalX = sim->op->x;
    sim->op->OriginalY = sim->op->y;

    if (sim->simType == 0) { // setup bez prekážok
        for (int i = 0; i < sim->rozmerX; i++) {
            for (int j = 0; j < sim->rozmerY; j++) {
                sim->world[i][j] = 0;
                sim->worldOriginal[i][j] = 0;
            }
        }
        sim->world[sim->op->x][sim->op->y] = 2;
        sim->worldOriginal[sim->op->x][sim->op->y] = 2;

    } else if (sim->simType == 1) { // setup s prekážkami
        int** navstivene = malloc(sim->rozmerX * sizeof(int*)); // Pole ukazovateľov na riadky
        for (int s = 0; s < sim->rozmerY; s++) {
            navstivene[s] = malloc(sim->rozmerY * sizeof(int)); // Každý riadok
        }


        for (int i = 0; i < sim->rozmerX; i++) {
            for (int j = 0; j < sim->rozmerY; j++) {

                for (int k = 0; k < sim->rozmerX; k++) {
                    for (int l = 0; l < sim->rozmerY; l++) {
                        navstivene[k][l] = 0;
                    }
                }

                if (rand() % 100 < DEFAULT_BLOCKADE_CHANCE && dfs(sim, 0, 0, sim->op, navstivene) && (i != 0 && j != 0) && (i != sim->op->x && j != sim->op->y)) {
                    sim->world[i][j] = 1;
                    sim->worldOriginal[i][j] = 1;
                } else{
                    sim->world[i][j] = 0;
                    sim->worldOriginal[i][j] = 0;
                }

            }
        }
        for (int i = 0; i < sim->rozmerX; i++) {
            free(navstivene[i]);
        }
        free(navstivene);

        sim->world[sim->op->x][sim->op->y] = 2;
        sim->worldOriginal[sim->op->x][sim->op->y] = 2;
    } else {
        printf("Zle zadaný vstup typu simulácie.\n");
        return -1;
    }

    //printf("Debug: Skončil som setup sveta.\n");

    vypisSim(sim);
    return 0;
}

void *clientHandler(void *arg) {

    config* conf = (config*) arg;

    int server_fd, new_socket;
    ssize_t valread;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    char buffer[1024] = { 0 };


    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);


    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *) &address, &addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        printf("New client connected!\n");

        // Kontinuálna komunikácia
        while (1) {
            memset(buffer, 0, sizeof(buffer)); // Vyčistiť buffer
            send(new_socket, "Čo si prajete vykonať?\nMožnosti:\n [1.] začať simuláciu\n [2.] Vypíš Štatistiky z poslednej simulácie\n [3.] Načítaj simuláciui zo súboru\n", strlen("Čo si prajete vykonať?\nMožnosti:\n [1.] začať simuláciu\n [2.] Vypíš Štatistiky z poslednej simulácie\n [3.] Načítaj simuláciui zo súboru\n"), 0);

            valread = recv(new_socket, buffer, sizeof(buffer) - 1, 0); // Čítanie dát od klienta
            if (valread <= 0) {
                if (valread == 0) {
                    printf("Client disconnected.\n");
                } else {
                    perror("recv failed");
                }
                break;
            }

            buffer[valread] = '\0'; // Pridanie null terminátora
            printf("Správa od klienta: %s\n", buffer);
            if (strcmp(buffer, "1") == 0) {

                // ZADANIE NÁZVU SIMULÁCIE
                send(new_socket, "Zadajte názov simulácie: ", strlen("Zadajte názov simulácie: "), 0);
                memset(buffer, 0, sizeof(buffer)); // Vyčistiť buffer
                valread = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
                if (valread > 0) {
                    buffer[valread] = '\0';
                    pthread_mutex_lock(&conf->sim_c->mutex);
                    strncpy(conf->sim_c->simulationName, buffer, sizeof(conf->sim_c->simulationName) - 1);
                    conf->sim_c->simulationName[sizeof(conf->sim_c->simulationName) - 1] = '\0';
                    printf("Názov simulácie: %s\n", conf->sim_c->simulationName);
                    pthread_mutex_unlock(&conf->sim_c->mutex);
                }

                // ZADANIE POČTU REPLIKÁCIÍ
                send(new_socket, "Zadajte počet replikácií simulácie: ",
                     strlen("Zadajte počet replikácií simulácie: "), 0);
                memset(buffer, 0, sizeof(buffer)); // Vyčistiť buffer
                valread = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
                if (valread > 0) {
                    buffer[valread] = '\0';
                    pthread_mutex_lock(&conf->sim_c->mutex);
                    conf->sim_c->NumOfReplications = atoi(buffer);
                    printf("Počet replikácií simulácie: %d\n", conf->sim_c->NumOfReplications);
                    pthread_mutex_unlock(&conf->sim_c->mutex);
                }

                // ZADANIE ROZMEROV
                send(new_socket, "Zadajte rozmer mapy X: ",
                     strlen("Zadajte rozmer mapy X: "), 0);
                memset(buffer, 0, sizeof(buffer)); // Vyčistiť buffer
                valread = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
                if (valread > 0) {
                    buffer[valread] = '\0';
                    pthread_mutex_lock(&conf->sim_c->mutex);
                    conf->sim_c->rozmerX = atoi(buffer);
                    printf("Rozmer mapy X: %d\n", conf->sim_c->rozmerX);
                    pthread_mutex_unlock(&conf->sim_c->mutex);
                }
                send(new_socket, "Zadajte rozmer mapy Y: ",
                     strlen("Zadajte rozmer mapy Y: "), 0);
                memset(buffer, 0, sizeof(buffer)); // Vyčistiť buffer
                valread = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
                if (valread > 0) {
                    buffer[valread] = '\0';
                    pthread_mutex_lock(&conf->sim_c->mutex);
                    conf->sim_c->rozmerY = atoi(buffer);
                    printf("Rozmer mapy Y: %d\n", conf->sim_c->rozmerY);
                    pthread_mutex_unlock(&conf->sim_c->mutex);
                }

                // ZADANIE Pravdepodobnosti
                while(1) {

                    send(new_socket, "Zadajte pravdepodobnosť pohybu Hore: ",
                         strlen("Zadajte pravdepodobnosť pohybu Hore: "), 0);
                    memset(buffer, 0, sizeof(buffer)); // Vyčistiť buffer
                    valread = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
                    if (valread > 0) {
                        buffer[valread] = '\0';
                        pthread_mutex_lock(&conf->sim_c->mutex);
                        conf->sim_c->op->chanceUp = atoi(buffer);
                        printf("Pravdepodonosť pohybu hore: %d\n", conf->sim_c->op->chanceUp);
                        pthread_mutex_unlock(&conf->sim_c->mutex);
                    }

                    send(new_socket, "Zadajte pravdepodobnosť pohybu Vpravo: ",
                         strlen("Zadajte pravdepodobnosť pohybu Vpravo: "), 0);
                    memset(buffer, 0, sizeof(buffer)); // Vyčistiť buffer
                    valread = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
                    if (valread > 0) {
                        buffer[valread] = '\0';
                        pthread_mutex_lock(&conf->sim_c->mutex);
                        conf->sim_c->op->chanceRight = atoi(buffer);
                        printf("Pravdepodonosť pohybu Vpravo: %d\n", conf->sim_c->op->chanceRight);
                        pthread_mutex_unlock(&conf->sim_c->mutex);
                    }

                    send(new_socket, "Zadajte pravdepodobnosť pohybu Dole: ",
                         strlen("Zadajte pravdepodobnosť pohybu Dole: "), 0);
                    memset(buffer, 0, sizeof(buffer)); // Vyčistiť buffer
                    valread = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
                    if (valread > 0) {
                        buffer[valread] = '\0';
                        pthread_mutex_lock(&conf->sim_c->mutex);
                        conf->sim_c->op->chanceDown = atoi(buffer);
                        printf("Pravdepodonosť pohybu Dole: %d\n", conf->sim_c->op->chanceDown);
                        pthread_mutex_unlock(&conf->sim_c->mutex);
                    }

                    send(new_socket, "Zadajte pravdepodobnosť pohybu Vľavo: ",
                         strlen("Zadajte pravdepodobnosť pohybu Vľavo: "), 0);
                    memset(buffer, 0, sizeof(buffer)); // Vyčistiť buffer
                    valread = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
                    if (valread > 0) {
                        buffer[valread] = '\0';
                        pthread_mutex_lock(&conf->sim_c->mutex);
                        conf->sim_c->op->chanceLeft = atoi(buffer);
                        printf("Pravdepodonosť pohybu Vľavo: %d\n", conf->sim_c->op->chanceLeft);
                        pthread_mutex_unlock(&conf->sim_c->mutex);
                    }

                    if (conf->sim_c->op->chanceUp + conf->sim_c->op->chanceRight + conf->sim_c->op->chanceDown + conf->sim_c->op->chanceLeft == 100) {
                        break;
                    } else {
                        send(new_socket, "Zadali ste zlé hodnoty. Pravdepodobnosť musí byť rovná 100",
                             strlen("Zadali ste zlé hodnoty. Pravdepodobnosť musí byť rovná 100"), 0);
                        memset(buffer, 0, sizeof(buffer)); // Vyčistiť buffer
                    }
                }

                // ZADANIE MAXIMÁLNEHO POČTU KROKOV
                send(new_socket, "Zadajte max počet krokov: ",
                     strlen("Zadajte max počet krokov: "), 0);
                memset(buffer, 0, sizeof(buffer)); // Vyčistiť buffer
                valread = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
                if (valread > 0) {
                    buffer[valread] = '\0';
                    pthread_mutex_lock(&conf->sim_c->mutex);
                    conf->sim_c->maxPocetKrokov = atoi(buffer);
                    printf("Max počet krokov: %d\n", conf->sim_c->maxPocetKrokov);
                    pthread_mutex_unlock(&conf->sim_c->mutex);
                }

                // ZADANIE TYPU SIMULACIE
                send(new_socket, "Zadajte typ simulácie \n[0.] Bez prekážok\n[1.] S prekážkami: ",
                     strlen("Zadajte typ simulácie \n[0.] Bez prekážok\n[1.] S prekážkami: "), 0);
                memset(buffer, 0, sizeof(buffer)); // Vyčistiť buffer
                valread = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
                if (valread > 0) {
                    buffer[valread] = '\0';
                    pthread_mutex_lock(&conf->sim_c->mutex);
                    conf->sim_c->simType = atoi(buffer);
                    printf("Typ simulácie: %d\n", conf->sim_c->simType);
                    pthread_mutex_unlock(&conf->sim_c->mutex);
                }

                    // TU MôŽE ZAČAŤ S INICIALIZACIOU SIMULACIE
                    printf("Debug: Začínam inicializovať svet.\n");
                    initializeWorld(conf->sim_c);
                    conf->sim_c->sim_state = RUNNING;



            }
            else if (strcmp(buffer, "STOP") == 0) {
                send(new_socket, "Vypínam server!", strlen("Vypínam server!"), 0);
                pthread_mutex_lock(&conf->sim_c->mutex);
                conf->sim_c->sim_state = STOPPED;
                pthread_mutex_unlock(&conf->sim_c->mutex);
                printf("Vypínam server!\n");
                break;
            }
            else if (strcmp(buffer, "2") == 0) {
                pthread_mutex_lock(&conf->sim_c->mutex);
                size_t outputSize = 1024;
                char* output = malloc(outputSize);
                if (!output) {
                    perror("malloc failed");
                    exit(EXIT_FAILURE);
                }
                output[0] = '\0';

                for (int i = 0; i < conf->sim_c->rozmerX; i++) {
                    for (int j = 0; j < conf->sim_c->rozmerY; j++) {
                        // Ak sa buffer zaplní, zväčšíme ho
                        if (strlen(output) + 32 >= outputSize) { // 32 je rezerva pre ďalšie číslo
                            outputSize *= 2;
                            output = realloc(output, outputSize);
                            if (!output) {
                                perror("realloc failed");
                                exit(EXIT_FAILURE);
                            }
                        }
                        // Pridanie čísla do reťazca
                        char temp[32];
                        char znak;
                        if (conf->sim_c->worldOriginal[i][j] == 0) {
                            znak = '.';
                        } else if (conf->sim_c->worldOriginal[i][j] == 1) {
                            znak = '#';
                        } else if (conf->sim_c->worldOriginal[i][j] == 2) {
                            znak = 'X';
                        }
                        snprintf(temp, sizeof(temp), "%c ", znak);
                        strcat(output, temp);

                    }
                    strcat(output, "\n"); // Riadok ukončíme novým riadkom
                }
                // Pridanie štatistík do výstupu
                char stats[128];
                snprintf(stats, sizeof(stats), "\nÚspešných pokusov: %d\nNeúspešných pokusov: %d\n",conf->sim_c->success, conf->sim_c->failed);

                // Ak sa buffer zaplní, zväčšíme ho
                if (strlen(output) + strlen(stats) >= outputSize) {
                    outputSize += strlen(stats) + 1;
                    output = realloc(output, outputSize);
                    if (!output) {
                        perror("realloc failed");
                        exit(EXIT_FAILURE);
                    }
                }

                strcat(output, stats);
                send(new_socket, output, strlen(output), 0);

                free(output);
                pthread_mutex_unlock(&conf->sim_c->mutex);
            }
            else if (strcmp(buffer, "3") == 0) {
                // načítanie simulácie zo súboru
                pthread_mutex_lock(&conf->sim_c->mutex);
                DIR* dir = opendir("svety");
                if (dir == NULL) {
                    perror("Nepodarilo sa otvoriť adresár svety/");
                    send(new_socket, "Nepodarilo sa otvoriť adresár svety/\n", strlen("Nepodarilo sa otvoriť adresár svety/\n"), 0);
                    continue;
                }

                struct dirent* entry;
                char buffer[1024];
                buffer[0] = '\0'; // Inicializácia bufferu pre zoznam súborov

                strcat(buffer, "Zoznam súborov v adresári svety/:\n");
                while ((entry = readdir(dir)) != NULL) {
                    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                        strcat(buffer, entry->d_name);
                        strcat(buffer, "\n");
                    }
                }
                closedir(dir);
                strcat(buffer, "\nZadajte názov súboru na načítanie:\n");

                // Poslať zoznam klientovi
                send(new_socket, buffer, strlen(buffer), 0);
                memset(buffer, 0, sizeof(buffer));

                ssize_t valread = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
                if (valread <= 0) {
                    perror("recv failed");
                    continue;
                }
                buffer[valread] = '\0';
                buffer[strcspn(buffer, "\r\n")] = 0;

                char filePath[256];
                snprintf(filePath, sizeof(filePath), "svety/%s", buffer);
                FILE* file = fopen(filePath, "r");
                if (!file) {
                    perror("Nepodarilo sa otvoriť súbor");
                    send(new_socket, "Nepodarilo sa otvoriť zadaný súbor.\n", strlen("Nepodarilo sa otvoriť zadaný súbor.\n"), 0);
                    continue;
                }

                char line[256];


                fscanf(file, "%d", &conf->sim_c->maxPocetKrokov);
                fscanf(file, "%d", &conf->sim_c->rozmerX);
                fscanf(file, "%d", &conf->sim_c->rozmerY);

                fscanf(file, "%d", &conf->sim_c->op->OriginalX);
                fscanf(file, "%d", &conf->sim_c->op->OriginalY);

                if (fgets(line, sizeof(line), file) != NULL) {
                    // Vypísanie riadku, ktorý bol práve načítaný
                    printf("Načítaný riadok: %s", line);
                } else {
                    printf("Nepodarilo sa načítať riadok.\n");
                }

                if (conf->sim_c->alokovane == 1) {
                    for (int i = 0; i < conf->sim_c->rozmerX; i++) {
                        free(conf->sim_c->world[i]);
                    }
                    free(conf->sim_c->world);

                    for (int i = 0; i < conf->sim_c->rozmerX; i++) {
                        free(conf->sim_c->worldOriginal[i]);
                    }
                    free(conf->sim_c->worldOriginal);
                }

                conf->sim_c->worldOriginal = malloc(conf->sim_c->rozmerX * sizeof(int*));
                for (int i = 0; i < conf->sim_c->rozmerX; i++) {
                    conf->sim_c->worldOriginal[i] = malloc(conf->sim_c->rozmerY * sizeof(int));
                }
                int i = 0;
                while (fgets(line, sizeof(line), file) != NULL) {
                    // Prejdeme po každom riadku
                    int j = 0;
                    char* token = strtok(line, " \t"); // Rozdelíme riadok podľa medzier a tabulátorov
                    while (token != NULL && j < conf->sim_c->rozmerY) {
                        conf->sim_c->worldOriginal[i][j] = atoi(token); // Konvertujeme na číslo
                        token = strtok(NULL, " \t"); // Získame ďalší token
                        j++;
                    }
                    i++;
                    if (i >= conf->sim_c->rozmerX) break; // Prejdeme len požadovaný počet riadkov
                }

                for (int i = 0; i < conf->sim_c->rozmerX; i++) {
                    for (int j = 0; j < conf->sim_c->rozmerY; j++) {
                        printf("%d ", conf->sim_c->worldOriginal[i][j]);
                    }
                }

                conf->sim_c->world = malloc(conf->sim_c->rozmerX * sizeof(int*)); // Pole ukazovateľov na riadky
                for (int i = 0; i < conf->sim_c->rozmerX; i++) {
                    conf->sim_c->world[i] = malloc(conf->sim_c->rozmerY * sizeof(int)); // Každý riadok
                }

                // ZADANIE NÁZVU SIMULÁCIE
                send(new_socket, "Zadajte názov simulácie: ", strlen("Zadajte názov simulácie: "), 0);
                memset(buffer, 0, sizeof(buffer)); // Vyčistiť buffer
                valread = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
                if (valread > 0) {
                    buffer[valread] = '\0';
                    strncpy(conf->sim_c->simulationName, buffer, sizeof(conf->sim_c->simulationName) - 1);
                    conf->sim_c->simulationName[sizeof(conf->sim_c->simulationName) - 1] = '\0';
                    printf("Názov simulácie: %s\n", conf->sim_c->simulationName);
                }

                // ZADANIE POČTU REPLIKÁCIÍ
                send(new_socket, "Zadajte počet replikácií simulácie: ",
                     strlen("Zadajte počet replikácií simulácie: "), 0);
                memset(buffer, 0, sizeof(buffer)); // Vyčistiť buffer
                valread = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
                if (valread > 0) {
                    buffer[valread] = '\0';
                    conf->sim_c->NumOfReplications = atoi(buffer);
                    printf("Počet replikácií simulácie: %d\n", conf->sim_c->NumOfReplications);
                }

                fclose(file);
                conf->sim_c->pocetSpravenychReplikacii = 0;
                conf->sim_c->sim_state = RUNNING;
                printf("DEBUG: INICIALIZUJEM REPLIKÁCIU");
                printf("RozmerX = %d", conf->sim_c->rozmerX);
                printf("RozmerY = %d", conf->sim_c->rozmerY);
                reinitializeWorldForReplication(conf->sim_c);
                printf("DEBUG: DOKONČIL SOM INICIALIZACIU REPLIKACIE REPLIKÁCIU");

                pthread_mutex_unlock(&conf->sim_c->mutex);
            }
            else {
                send(new_socket, "Neznáma voľba, skúste znova.\n", strlen("Neznáma voľba, skúste znova.\n"), 0);
            }
        }
        if (strcmp(buffer, "STOP") == 0) {
            break;
        }
    }


    // Zatvorenie socketu
    close(new_socket);
    close(server_fd);

    return NULL;
}

void *simulationManager(void *arg) {
    simulation *sim = (simulation*) arg;

    sim->pocetSpravenychReplikacii = 0;
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
                        if (sim->world[sim->op->x][ -1] == 1) {
                            break;
                        } else {
                            sim->world[sim->op->x][sim->op->y] = 0;
                            sim->world[sim->op->x][sim->rozmerY - 1] = 2;
                            sim->op->y = sim->rozmerY - 1;
                            sim->op->pocetPohybov++;
                        }
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
                    if (sim->op->x + 1 >= sim->rozmerX) {
                        if (sim->world[0][sim->op->y] == 1) {
                            break;
                        } else {
                            sim->world[sim->op->x][sim->op->y] = 0;
                            sim->world[0][sim->op->y] = 2;
                            sim->op->x = 0;
                            sim->op->pocetPohybov++;
                        }
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
                    if (sim->op->y + 1 >= sim->rozmerY) {
                        if (sim->world[sim->op->x][0] == 1) {
                            break;
                        } else {
                            sim->world[sim->op->x][sim->op->y] = 0;
                            sim->world[sim->op->x][0] = 2;
                            sim->op->y = 0;
                            sim->op->pocetPohybov++;
                        }
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
                        if (sim->world[sim->rozmerX - 1][sim->op->y] == 1) {
                            break;
                        } else {
                            sim->world[sim->op->x][sim->op->y] = 0;
                            sim->world[sim->rozmerX - 1][sim->op->y] = 2;
                            sim->op->x = sim->rozmerX - 1;
                            sim->op->pocetPohybov++;
                        }
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

            if(sim->op->x == 0 && sim->op->y == 0 || sim->maxPocetKrokov == sim->op->pocetPohybov) {
                if (sim->pocetSpravenychReplikacii == sim->NumOfReplications) {

                    printf("\nSimulácia sa ukončila!\nPočet úspešných replikácií: %d", sim->success);
                    printf("\nPočet neúspešných replikácií: %d\n", sim->failed);
                    sim->sim_state = PAUSED;
                    sim->pocetSpravenychReplikacii = 0;
                    sim->op->pocetPohybov = 0;

                    for (int i = 0; i < sim->rozmerX; i++) {
                        for (int j = 0; j < sim->rozmerY; j++) {
                            printf("%d ", sim->worldOriginal[i][j]);
                        }
                        printf("\n");
                    }

                    saveToFile(sim);
                } else {
                    if (!(sim->op->x == 0 && sim->op->y == 0)) {
                        printf("\n!!Opilcovi sa nepodarilo dostať domov. Odpadol niekde na ulici!!\n");
                        sim->failed++;
                    } else {
                        printf("\nOpilcovi sa podarilo dostať domov! Spravil len %d krokov!\n", sim->op->pocetPohybov);
                        sim->success++;
                    }
                    sim->op->pocetPohybov = 0;
                    reinitializeWorldForReplication(sim);
                    sim->pocetSpravenychReplikacii++;
                }

            }
            pthread_mutex_unlock(&sim->mutex);
        }
        if (sim->sim_state == STOPPED) {
            printf("Simulácia bola ukončená.\n");
            pthread_mutex_unlock(&sim->mutex);
            break;
        }
        pthread_mutex_unlock(&sim->mutex);

    }
}

void saveToFile(simulation *sim) {
    // Kontrola a vytvorenie adresára
    char* folderName = "svety";
    char* fileName = sim->simulationName;
    struct stat st = {0};
    if (stat(folderName, &st) == -1) {
        if (mkdir(folderName, 0700) != 0) {
            perror("Nepodarilo sa vytvoriť adresár");
            return;
        }
    }

    // Vytvorenie cesty k súboru
    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", folderName, fileName);

    // Otvorenie súboru
    FILE* file = fopen(fullPath, "w");
    if (file == NULL) {
        perror("Nepodarilo sa otvoriť súbor");
        return;
    }

    // Zápis základných atribútov
    fprintf(file, "%d\n", sim->maxPocetKrokov);
    fprintf(file, "%d\n", sim->rozmerX);
    fprintf(file, "%d\n", sim->rozmerY);
    fprintf(file, "%d\n", sim->op->OriginalX);
    fprintf(file, "%d\n", sim->op->OriginalY);


    // Zápis matice worldOriginal
    fprintf(file, "Počiatočný stav worldOriginal:\n");
    for (int i = 0; i < sim->rozmerX; i++) {
        for (int j = 0; j < sim->rozmerY; j++) {
            fprintf(file, "%d ", sim->worldOriginal[i][j]); // Zápis jednotlivých hodnôt
        }
        fprintf(file, "\n"); // Nový riadok po každom riadku matice
    }


    fclose(file);
    printf("Údaje boli úspešne uložené do súboru %s\n", fullPath);

}


int main(int argc, char** argv) {
    srand(time(NULL));
    simulation sim;
    opilec opi;
    sim.op = &opi;
    config sc = {.argc = argc, .argv = argv, .sim_c = &sim};
    pthread_mutex_init(&sim.mutex, NULL);
    sim.sim_state = PAUSED;
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

    for (int i = 0; i < sim.rozmerX; i++) {
        free(sim.world[i]);
    }
    free(sim.world);

    for (int i = 0; i < sim.rozmerX; i++) {
        free(sim.worldOriginal[i]);
    }
    free(sim.worldOriginal);
    pthread_mutex_destroy(&sim.mutex);

    return 0;
}
