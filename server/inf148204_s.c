#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <errno.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/shm.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <errno.h>

#include "inf148204_s.h"

int run;

void run_changer() {
    run = !run;
}

int main(int argc, char *argv[]) {
    
    int result;
    int loaded_users;
    
    char config[] = "test.txt";
    
    run = 1;
    signal(SIGINT, run_changer);
    
    struct user **users = malloc( sizeof(struct user*) * MAX_USERS );
    for(int i=0; i<MAX_USERS; ++i)
        users[i] = malloc( sizeof(struct user) );
        
    struct group **groups = malloc( sizeof(struct group*) * MAX_GROUPS);
    for(int i=0; i<MAX_GROUPS; ++i)
        groups[i] = malloc( sizeof(struct group) );
    
    result = loadConfig("test.txt", users, MAX_USERS, groups, MAX_GROUPS);
    
    if(result < 0) {
        printLogTime();
        printf("Server cannot load configuration...\n");
        printLogTime();
        printf("Stopping the server...\n");
        freeMemory(users, groups);
        return -1;
    }
    
    loaded_users = result;
    int *message_queues = malloc(sizeof(int)*loaded_users);
    
    
    printLogTime();
    printf("Server loaded %d users\n", loaded_users);
    
    //TODO: IPC_EXCL when all Signals are managed 
    int auth_queue = msgget(99901, 0640 | IPC_CREAT);
        
    if(auth_queue == -1 && errno == EEXIST) {
        printLogTime();
        printf("Server queue already exist\n\t\t\tis there another server process running?\n");
        printLogTime();
        printf("Stopping the server...\n");
        freeMemory(users, groups);
        free(message_queues);
        return -1;
    } else if(auth_queue == -1) {
        printLogTime();
        printf("Uncatched error %d, during creating server queue", errno);
        printLogTime();
        printf("Stopping the server...\n");
        freeMemory(users, groups);
        free(message_queues);
        return -1;
    }
    
    printLogTime();
    printf("Server listening on authentication queue no %d\n", auth_queue);
    
    
    
    for(int i=0; i<loaded_users; ++i) {
        message_queues[i] = msgget(ftok(config, i), 0640 | IPC_CREAT);
        if(message_queues[i] == -1) {
            printLogTime();
            printf("Error during creating queues for users\n");
            freeMemory(users, groups);
            free(message_queues);
            return -1;
        }
    }
    
    struct msgbuf message;
    struct authbuf auth;
    
    printLogTime();
    printf("Server joining run loop. Ctrl+C to stop work\n");
    
    while(run) {
        if(msgrcv(auth_queue, &auth, sizeof(auth)-sizeof(long), 0, IPC_NOWAIT) > 0) {
            for(int i=0; i<loaded_users; ++i) {
                if(strcmp(auth.login, users[i]->login) == 0) {
                    if(!users[i]->logged) {
                        if(strcmp(auth.password, users[i]->password) == 0) {
                            users[i]->logged = 1;
                            sprintf(message.msg, "[Server]\tWelcome back %s!\n", users[i]->login);
                            message.end = 1;
                            message.from = -1;
                            message.to = message_queues[i]; // adress for private queue
                        } else {
                            strncpy(message.msg, "[Server]\tWrong Password!\n", MAX_BUFFER);
                            message.end = 1;
                            message.from = -1;
                            message.to = -1;
                            
                            printLogTime();
                            printf("Password: %s\t Get: %s\n", auth.password, users[i]->password);
                        }
                    }
                    else {
                        strncpy(message.msg, "[Server]\tUser already logged on another client\n", MAX_BUFFER);
                        message.end = 1;
                        message.from = -1;
                        message.to = -1;
                        
                    }
                    msgsnd(auth.client_queue, &message, sizeof(message)-sizeof(long), 0);
                    break;
                }
            }
            // TODO: Nie znaleziono 
            
        }
        
    }
    
    printf("\n");
    printLogTime();
    printf("Stopping the server. Thanks for using chat\n");
    msgctl(auth_queue, IPC_RMID, NULL);
    for(int i=0; i<loaded_users; ++i) {
        msgctl(message_queues[i], IPC_RMID, NULL);
    }
    
    freeMemory(users, groups);
    free(message_queues);
    
    return 0;
}

void freeMemory(struct user **users, struct group **groups) {
        for(int i=0; i<MAX_USERS; ++i)
        free(users[i]);
    free(users);
    
    for(int i=0; i<MAX_GROUPS; ++i)
        free(groups[i]);
    free(groups);
    
}