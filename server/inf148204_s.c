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
    int loadedUsers;
    
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
    
    loadedUsers = result;
    int *messageQueues = malloc(sizeof(int)*loadedUsers);
    
    
    printLogTime();
    printf("Server loaded %d users\n", loadedUsers);
    
    //TODO: IPC_EXCL when all Signals are managed 
    int authQueue = msgget(99901, 0640 | IPC_CREAT);
        
    if(authQueue == -1 && errno == EEXIST) {
        printLogTime();
        printf("Server queue already exist\n\t\t\tis there another server process running?\n");
        printLogTime();
        printf("Stopping the server...\n");
        freeMemory(users, groups);
        free(messageQueues);
        return -1;
    } else if(authQueue == -1) {
        printLogTime();
        printf("Uncatched error %d, during creating server queue", errno);
        printLogTime();
        printf("Stopping the server...\n");
        freeMemory(users, groups);
        free(messageQueues);
        return -1;
    }
    
    printLogTime();
    printf("Server listening on authentication queue no %d\n", authQueue);
    
    
    
    for(int i=0; i<loadedUsers; ++i) {
        messageQueues[i] = msgget(ftok(config, i), 0640 | IPC_CREAT);
        if(messageQueues[i] == -1) {
            printLogTime();
            printf("Error during creating queues for users\n");
            freeMemory(users, groups);
            free(messageQueues);
            return -1;
        }
    }
    
    struct msgbuf message;
    struct authbuf auth;
    
    printLogTime();
    printf("Server joining run loop. Ctrl+C to stop work\n");
    
    while(run) {
        proceedAuth(users, loadedUsers, authQueue, messageQueues);
        
    }
    
    printf("\n");
    printLogTime();
    printf("Stopping the server. Thanks for using chat\n");
    msgctl(authQueue, IPC_RMID, NULL);
    for(int i=0; i<loadedUsers; ++i) {
        msgctl(messageQueues[i], IPC_RMID, NULL);
    }
    
    freeMemory(users, groups);
    free(messageQueues);
    
    return 0;
}

void proceedAuth(struct user **users, int loadedUsers, int authQueue, int *messageQueues) {
    
    struct authbuf auth;
    struct msgbuf message;
    
    if(msgrcv(authQueue, &auth, sizeof(auth)-sizeof(long), 0, IPC_NOWAIT) > 0) {
            int find = 0;
            
            message.mtype = PRIORITY_PORT;
            message.priority = 0; // messages from server are on priority port but without flag
            message.start = 1;
            message.end = 1;
            message.to = -1;
            
            for(int i=0; i<loadedUsers; ++i) {
                
                if(strcmp(auth.login, users[i]->login) == 0) {
                    find = 1;
                    if(!users[i]->logged) {
                        if(strcmp(auth.password, users[i]->password) == 0) {
                            users[i]->logged = 1;
                            sprintf(message.msg, "Welcome back %s!\n", users[i]->login);
                            message.to = messageQueues[i]; // adress for private queue
                        } else {
                            strncpy(message.msg, "Wrong Password!\n", MAX_BUFFER);
                        }
                    }
                    else {
                        strncpy(message.msg, "User already logged on another client\n", MAX_BUFFER);
                    }
                    msgsnd(auth.client_queue, &message, sizeof(message)-sizeof(long), 0);
                    break;
                }
            }
            if(!find) {
                // wrong username
                strncpy(message.msg, "Wrong Login\n", MAX_BUFFER);
                msgsnd(auth.client_queue, &message, sizeof(message)-sizeof(long), 0);
            }
        }
}

void freeMemory(struct user **users, struct group **groups) {
        for(int i=0; i<MAX_USERS; ++i)
        free(users[i]);
    free(users);
    
    for(int i=0; i<MAX_GROUPS; ++i)
        free(groups[i]);
    free(groups);
    
}