#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include "sudoku.h"

#define PORT 12345
#define BUFFER_SIZE 4096

int active_clients = 0;
int client_counter = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int socket;
    SudokuGame game;
    struct sockaddr_in addr;
    int client_id;
} ClientData;

void send_msg(int sock, const char *msg) {
    send(sock, msg, strlen(msg), 0);
}

void *handle_client(void *arg) {
    ClientData *client = (ClientData *)arg;
    int sock = client->socket;
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    printf("[SERVEUR] Nouveau client connecté: %s | ID: %d | Thread ID: %lu\n",
           inet_ntoa(client->addr.sin_addr),
           client->client_id,
           (unsigned long)pthread_self());

    char welcome[256];
    snprintf(welcome, sizeof(welcome),
        "Bienvenue sur le serveur Sudoku! [Client #%d | Thread %lu]\n"
        "Commandes: NEW <1|2|3>, PLAY <row> <col> <val>, "
        "CHECK, SOLUTION, QUIT\n",
        client->client_id,
        (unsigned long)pthread_self());
    send_msg(sock, welcome);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int n = recv(sock, buffer, BUFFER_SIZE - 1, 0);

        if (n <= 0) {
            printf("[SERVEUR] Client #%d déconnecté: %s | Thread %lu terminé\n",
                   client->client_id,
                   inet_ntoa(client->addr.sin_addr),
                   (unsigned long)pthread_self());
            break;
        }

        buffer[strcspn(buffer, "\r\n")] = 0;
        printf("[SERVEUR] Client #%d [Thread %lu] reçu: %s\n",
               client->client_id,
               (unsigned long)pthread_self(),
               buffer);

        if (strncmp(buffer, "NEW", 3) == 0) {
            int level = 1;
            sscanf(buffer, "NEW %d", &level);
            if (level < 1 || level > 3) {
                send_msg(sock, "ERREUR: Niveau invalide. Choisir 1, 2 ou 3.\n");
                continue;
            }
            srand(time(NULL));
            generate_grid(&client->game, level);
            grid_to_string(client->game.grid, response);
            char header[64];
            snprintf(header, sizeof(header),
                     "GRILLE (niveau %d):\n", level);
            send_msg(sock, header);
            send_msg(sock, response);
        }

        else if (strncmp(buffer, "PLAY", 4) == 0) {
            int row, col, val;
            if (sscanf(buffer, "PLAY %d %d %d", &row, &col, &val) != 3) {
                send_msg(sock, "ERREUR: Format invalide. Utiliser: PLAY <row> <col> <val>\n");
                continue;
            }
            if (row < 1 || row > 9 || col < 1 || col > 9 || val < 1 || val > 9) {
                send_msg(sock, "ERREUR: Valeurs hors limites (row/col: 1-9, val: 1-9)\n");
                continue;
            }
            row--; col--;

            if (client->game.fixed[row][col]) {
                send_msg(sock, "ERREUR: Cette case est fixe, vous ne pouvez pas la modifier.\n");
                continue;
            }
            if (is_valid_move(&client->game, row, col, val)) {
                client->game.grid[row][col] = val;
                if (is_complete(&client->game))
                    send_msg(sock, "BRAVO! Grille complétée avec succès!\n");
                else
                    send_msg(sock, "OK: Coup valide!\n");
            } else {
                send_msg(sock, "ERREUR: Coup invalide selon les règles du Sudoku.\n");
            }
        }

        else if (strcmp(buffer, "CHECK") == 0) {
            if (is_complete(&client->game))
                send_msg(sock, "GRILLE COMPLETE et CORRECTE!\n");
            else
                send_msg(sock, "Grille incomplète ou incorrecte.\n");
        }

        else if (strcmp(buffer, "SOLUTION") == 0) {
            send_msg(sock, "SOLUTION:\n");
            grid_to_string(client->game.solution, response);
            send_msg(sock, response);
        }

        else if (strcmp(buffer, "QUIT") == 0) {
            send_msg(sock, "Au revoir!\n");
            break;
        }

        else {
            send_msg(sock, "ERREUR: Commande inconnue.\n");
        }
    }

    close(sock);
    free(client);

    pthread_mutex_lock(&mutex);
    active_clients--;
    pthread_mutex_unlock(&mutex);

    printf("[SERVEUR] Thread terminé. Clients actifs: %d\n", active_clients);
    return NULL;
}

int main() {
    int server_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    srand(time(NULL));

    /* ── bind ── */
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(PORT);

    /* ── bind ── */
    if (bind(server_sock, (struct sockaddr*)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("bind"); exit(1);
    }
    printf("[SERVEUR] bind() OK — socket attaché au port %d\n", PORT);

    /* ── listen ── */
    if (listen(server_sock, 20) < 0) { perror("listen"); exit(1); }
    printf("[SERVEUR] listen() OK — en écoute sur le port %d\n", PORT);
    printf("[SERVEUR] Maximum %d clients simultanés.\n", MAX_CLIENTS);

    while (1) {
        /* ── accept ── */
        int client_sock = accept(server_sock,
                                  (struct sockaddr*)&client_addr,
                                  &addr_len);
        if (client_sock < 0) { perror("accept"); continue; }
        printf("[SERVEUR] accept() OK — nouvelle connexion depuis %s\n",
               inet_ntoa(client_addr.sin_addr));

        pthread_mutex_lock(&mutex);
        if (active_clients >= MAX_CLIENTS) {
            pthread_mutex_unlock(&mutex);
            printf("[SERVEUR] Limite atteinte (%d clients max), "
                   "client mis en attente...\n", MAX_CLIENTS);
            char wait_msg[256];
            snprintf(wait_msg, sizeof(wait_msg),
                "ATTENTE: Serveur plein (%d clients max). "
                "Vous serez pris en charge dès qu'une place se libère.\n",
                MAX_CLIENTS);
            send(client_sock, wait_msg, strlen(wait_msg), 0);

            while (1) {
                sleep(1);
                pthread_mutex_lock(&mutex);
                if (active_clients < MAX_CLIENTS) break;
                pthread_mutex_unlock(&mutex);
            }
        }
        active_clients++;
        client_counter++;
        int current_id = client_counter;
        pthread_mutex_unlock(&mutex);

        ClientData *client = malloc(sizeof(ClientData));
        client->socket    = client_sock;
        client->addr      = client_addr;
        client->client_id = current_id;
        memset(&client->game, 0, sizeof(SudokuGame));

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, client) != 0) {
            perror("pthread_create");
            free(client);
            pthread_mutex_lock(&mutex);
            active_clients--;
            pthread_mutex_unlock(&mutex);
            continue;
        }
        pthread_detach(tid);
        printf("[SERVEUR] Thread créé pour Client #%d [Thread ID: %lu]. "
               "Clients actifs: %d/%d\n",
               current_id,
               (unsigned long)tid,
               active_clients, MAX_CLIENTS);
    }

    close(server_sock);
    return 0;
}
