#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "clients.h"
#include "command.h"
#include "config.h"

void init_clients()
{
    client = malloc(sizeof(client_t *) * maxcli);
    for (int i = 0; i < maxcli; i++) {
        client[i] = malloc(sizeof(client_t));
        client[i]->connfd = 0;
    }
}

void accept_clients()
{
    int connfd;
    struct sockaddr_in cli_addr;
    
    /* accept clients */
    while (1) {
        socklen_t client_len = sizeof(cli_addr);
        connfd = accept(sockfd, (struct sockaddr *)&cli_addr, &client_len);
        
        printf("(srv): connection attempt from: %s\n", inet_ntoa(cli_addr.sin_addr));

        if (connected < maxcli) {
            connected++;
            for (int uid = 0; uid < maxcli; uid++) {
                if (client[uid]->connfd == 0) {
                    pthread_t tid;
                    /* fill in the client struct */
                    client[uid]->addr = cli_addr;
                    client[uid]->connfd = connfd;
                    client[uid]->color = rand() % 5 + 31;
                    client[uid]->id = uid;
                    //sprintf(client[uid]->nick, "%d", uid);
                    pthread_create(&tid, NULL, handle_client, client[uid]);
                    break;
                }
            }
        } else {
             printf("(srv) Max client number reached. Closing descriptor: %s.\n", inet_ntoa(cli_addr.sin_addr));
             close(connfd);
        }
        sleep(1);
    }
}

void *handle_client(void *arg)
{
    client_t *client = (client_t *)arg;

    int read, join = 0;
    char buf_in[buffsize];
    char buf_out[buffsize + 28];

    /* Send motd */ 
    server_send(0, client->id, " \e[34m* \e[35m%s\n\e[34m * \e[35mPlease enter your nick.\e[0m\n", MOTD);

    /* Get input from client */
    while ((read = recv(client->connfd, buf_in, buffsize-1, 0)) > 0) {
        buf_in[read] = '\0';

        for (int i = 0; i < strlen(buf_in); i++) {
            if (buf_in[i] < ' ' || buf_in[i] > '~') buf_in[i] = ' ';

        }

        if (strlen(buf_in) > 1) {
            if (!join) {
                if (cmd_nick(0, client->id, buf_in)) {
                    join = 1;
                    server_send(2, 0, " \e[34m* %s joined. (connected: %d)\e[0m\n", client->nick, connected);
                }
            } else {
                if (buf_in[0] == '/') {
                    char *cmd = strtok(buf_in, " ");
                    if (strcmp("/nick", cmd) == 0) {
                        char *arg = strtok(NULL , " ");
                        cmd_nick(1, client->id, arg);
                    }
                } else {
                    server_send(1, client->id, "\e[1;%dm%s\e[0m: %s\n", client->color, client->nick, buf_in);
                }
            }
        }
        memset(buf_in, 0, sizeof(buf_in));
        memset(buf_out, 0, sizeof(buf_out));
    }

    client->connfd = 0;
    connected--;
    server_send(3, 0, " \e[34m* %s left. (connected: %d)\e[0m\n", client->nick, connected);

    pthread_detach(pthread_self());
    return NULL;
}

/* 
 * Sending mode:
 * 0. Send only to uid
 * 1. Send to everyone except uid
 * 2. Send to everyone (ignores uid)
 */
void server_send(int mode, int uid, const char *format, ...)
{
	char buf[buffsize+28];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

    if (mode == 0) {
        write(client[uid]->connfd, buf, strlen(buf));
    } else {
        for (int i = 0; i < maxcli; i++) {
            if (client[i]->connfd != 0) {
                if (mode == 2 || i != uid) {
                    write(client[i]->connfd, buf, strlen(buf));
                }
            }
        }
    }
}

// !(true && true) false

// false || true   true
