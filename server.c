/* Serveur TCP sequentiel : ./serveur
 * Remplace les deux premiers octets du message par RE et ajoute #.
 * Pour un message de moins de deux octets, la reponse est RE#.
 * Le client termine son envoi avec shutdown(SHUT_WR).
 */
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 5050
#define MESSAGE_MAX 65536

static int envoyer_tout(int sock, const char *buffer, size_t taille) {
    size_t envoyes = 0;
    while (envoyes < taille) {
        ssize_t n = send(sock, buffer + envoyes, taille - envoyes, 0);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            if (n == 0) errno = EIO;
            perror("Envoi de la reponse");
            return -1;
        }
        envoyes += (size_t)n;
    }
    return 0;
}

static void renvoi(int sock) {
    /* Un octet supplementaire permet de detecter un message trop long ;
     * les autres reservent la place de # et du terminateur de chaine.
     */
    char buffer[MESSAGE_MAX + 3];
    size_t longueur = 0;
    for (;;) {
        ssize_t n = recv(sock, buffer + longueur,
                         MESSAGE_MAX + 1 - longueur, 0);
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0) {
            perror("Reception du message");
            return;
        }
        if (n == 0)
            break;
        longueur += (size_t)n;
        if (longueur > MESSAGE_MAX) {
            static const char erreur[] = "ERR message trop long\n";
            envoyer_tout(sock, erreur, sizeof(erreur) - 1);
            return;
        }
    }
    printf("Message recu : %zu octets.\n", longueur);
    buffer[0] = 'R';
    buffer[1] = 'E';
    if (longueur < 2)
        longueur = 2;
    buffer[longueur++] = '#';
    buffer[longueur] = '\0';
    if (envoyer_tout(sock, buffer, longueur) == 0)
        printf("Reponse envoyee : %zu octets.\n", longueur);
}

int main(void) {
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        return EXIT_FAILURE;
    }
    int serveur = socket(AF_INET, SOCK_STREAM, 0);
    if (serveur < 0) {
        perror("Creation de la socket");
        return EXIT_FAILURE;
    }
    int option = 1;
    if (setsockopt(serveur, SOL_SOCKET, SO_REUSEADDR,
                   &option, sizeof(option)) < 0) {
        perror("SO_REUSEADDR");
        close(serveur);
        return EXIT_FAILURE;
    }
    struct sockaddr_in adresse = {0};
    adresse.sin_family = AF_INET;
    adresse.sin_port = htons(PORT);
    adresse.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(serveur, (struct sockaddr *)&adresse, sizeof(adresse)) < 0) {
        perror("Association au port 5050");
        close(serveur);
        return EXIT_FAILURE;
    }
    if (listen(serveur, 5) < 0) {
        perror("Mise en ecoute");
        close(serveur);
        return EXIT_FAILURE;
    }
    printf("Serveur en ecoute sur le port %d. Arret : Ctrl+C.\n", PORT);
    fflush(stdout);
    for (;;) {
        struct sockaddr_in client = {0};
        socklen_t longueur_adresse = sizeof(client);
        int connexion = accept(serveur, (struct sockaddr *)&client,
                               &longueur_adresse);
        if (connexion < 0) {
            if (errno == EINTR)
                continue;
            perror("Acceptation du client");
            close(serveur);
            return EXIT_FAILURE;
        }
        renvoi(connexion);
        close(connexion);
    }
}
