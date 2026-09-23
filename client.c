/* Client TCP : ./client <adresse-serveur> <message-a-transmettre>
 * Une connexion transporte un message. shutdown(SHUT_WR) en marque la fin,
 * tout en laissant le client recevoir la reponse du serveur.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT "5050"
#define MESSAGE_MAX 65536

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage : %s <adresse-serveur> <message>\n", argv[0]);
        return EXIT_FAILURE;
    }
    size_t taille = strlen(argv[2]);
    if (taille > MESSAGE_MAX) {
        fprintf(stderr, "Message trop long : maximum %d octets.\n", MESSAGE_MAX);
        return EXIT_FAILURE;
    }
    /* Une connexion interrompue doit produire une erreur, pas tuer le client. */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        return EXIT_FAILURE;
    }
    struct addrinfo criteres = {0}, *adresses = NULL;
    criteres.ai_family = AF_INET;
    criteres.ai_socktype = SOCK_STREAM;
    int erreur = getaddrinfo(argv[1], PORT, &criteres, &adresses);
    if (erreur != 0) {
        fprintf(stderr, "Adresse du serveur : %s\n", gai_strerror(erreur));
        return EXIT_FAILURE;
    }
    int sock = -1;
    int derniere_erreur = ECONNREFUSED;
    for (struct addrinfo *p = adresses; p != NULL; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0) {
            derniere_erreur = errno;
            continue;
        }
        if (connect(sock, p->ai_addr, p->ai_addrlen) == 0)
            break;
        derniere_erreur = errno;
        close(sock);
        sock = -1;
    }
    freeaddrinfo(adresses);
    if (sock < 0) {
        errno = derniere_erreur;
        perror("Connexion au serveur");
        return EXIT_FAILURE;
    }
    printf("Connexion a %s:%s etablie.\n", argv[1], PORT);
    size_t envoyes = 0;
    while (envoyes < taille) {
        ssize_t n = send(sock, argv[2] + envoyes, taille - envoyes, 0);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            if (n == 0) errno = EIO;
            perror("Envoi du message");
            close(sock);
            return EXIT_FAILURE;
        }
        envoyes += (size_t)n;
    }
    if (shutdown(sock, SHUT_WR) < 0) {
        perror("Fin de l'envoi");
        close(sock);
        return EXIT_FAILURE;
    }
    printf("Reponse du serveur :\n");
    char buffer[4096];
    for (;;) {
        ssize_t n = recv(sock, buffer, sizeof(buffer), 0);
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0) {
            perror("Reception");
            close(sock);
            return EXIT_FAILURE;
        }
        if (n == 0)
            break;
        if (fwrite(buffer, 1, (size_t)n, stdout) != (size_t)n) {
            perror("Affichage");
            close(sock);
            return EXIT_FAILURE;
        }
    }
    printf("\nConnexion fermee.\n");
    close(sock);
    return EXIT_SUCCESS;
}
