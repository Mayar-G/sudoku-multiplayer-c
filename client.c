#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 12345
#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <IP_serveur>\n", argv[0]);
        return 1;
    }

    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char input[256];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(PORT);
    inet_pton(AF_INET, argv[1], &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect"); return 1;
    }

    printf("[CLIENT] Connecté au serveur %s:%d\n", argv[1], PORT);

    // Recevoir le message de bienvenue
    int n = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (n > 0) { buffer[n] = 0; printf("%s", buffer); }

    while (1) {
        printf("> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = '\n';  // garder le \n pour l'envoi

        send(sock, input, strlen(input), 0);

        // Recevoir la réponse
        memset(buffer, 0, BUFFER_SIZE);
        n = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (n <= 0) {
            printf("Connexion fermée par le serveur.\n");
            break;
        }
        buffer[n] = 0;
        printf("%s", buffer);

        // Quitter si QUIT
        if (strncmp(input, "QUIT", 4) == 0) break;
    }

    close(sock);
    return 0;
}
