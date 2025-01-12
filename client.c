#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 10023

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    char stop_command[] = "STOP";
    char stop_command2[] = "C";

    // Vytvorenie soketu
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Nastavenie adresy servera
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Konverzia adresy IP z textu na binárny formát
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        exit(EXIT_FAILURE);
    }

    // Pripojenie k serveru
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connection failed");
        exit(EXIT_FAILURE);
    }

    // Spustenie vlákna na čítanie správy zo servera
    ssize_t valread;
    while (1) {
        // Čítanie odpovede od servera
        valread = read(sock, buffer, sizeof(buffer) - 1); // -1 pre null terminátor
        if (valread >= 0) {
            buffer[valread] = '\0'; // Pridanie null terminátora na koniec správy
            printf("%s\n", buffer);
        } else {
            perror("read failed");
            break;
        }

        // Kontrola na príkaz zo strany klienta
        char input[100];
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = 0; // Odstránenie nového riadku na konci

        if (strcmp(input, stop_command) == 0 || strcmp(input, stop_command2) == 0) {
            printf("Ukončujem spojenie.\n");
            send(sock, input, strlen(input), 0);
            break;
        } else {
            send(sock, input, strlen(input), 0);
        }
        memset(buffer, 0, sizeof(buffer)); // Vyčistiť buffer
    }

    // Zatvorenie soketu
    close(sock);
    return 0;
}
