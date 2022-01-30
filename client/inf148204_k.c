#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <string.h>

#include <unistd.h>

#include <stdio.h>

#include "inf148204_k.h"


int main() {
    char login[MAX_LOGIN_LENGTH];
    char password[MAX_PASSWORD_LENGTH];
    int logged = 0;
    
    int auth_queue = msgget(99901, 0640);
    int client_queue = msgget(getpid(), 0640 | IPC_CREAT);
    int message_queue;
    
    struct msgbuf message;
    
    if(auth_queue > 0) {
        while(!logged) {
            printf("Login:");
            scanf("%s", login);
            printf("Password:");
            scanf("%s", password);
            
            struct authbuf auth;
            strcpy(auth.login, login);
            strcpy(auth.password, password);
            auth.mtype = 1;
            auth.client_queue = client_queue;
            
            msgsnd(auth_queue, &auth, sizeof(auth)-sizeof(long), 0);
            msgrcv(client_queue, &message, sizeof(message)-sizeof(long), 0, 0);
            
            printf("%s", message.msg);
            if(message.to != -1) {
                // succesfull login
                logged = 1;
                message_queue = message.to;
                msgctl(client_queue, IPC_RMID, NULL);
            }
            
        }
    } else {
        printf("Server is not available");
        msgctl(client_queue, IPC_RMID, NULL);
        return -1;
    }
    
    return 0;
}