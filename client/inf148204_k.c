#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>

#include <string.h>

#include <unistd.h>

#include <stdio.h>

#include <sys/shm.h>
#include <sys/sem.h>

#include "inf148204_k.h"

int run;

void run_changer() {
    run = !run;
}

int main() {

    int logged = 0;
    
    int auth_queue = msgget(99901, 0640);
    
        
    int mem_id = shmget(ftok(".", 99901), sizeof(struct state), IPC_CREAT | 0640);
    struct state *status = shmat(mem_id, NULL, 0);
    
    int statusSemaphore = semget(ftok(".", 99902), sizeof(union semun), IPC_CREAT | 0640);
    int printSemaphore = semget(ftok(".", 99903), sizeof(union semun), IPC_CREAT | 0640);
    union semun semaphoreUnion;
    semaphoreUnion.val = 1;
    semctl(statusSemaphore, 0, SETVAL, semaphoreUnion);
    semctl(printSemaphore, 0, SETVAL, semaphoreUnion);
    
    if(status < 0) {
        printf("Cannot create shared memory\n");
        return -1;
    }
    
    if(auth_queue > 0) {
        status->privateQueue = login(logged, auth_queue);
    } else {
        printf("Server is not available\n");
        return -1;
    }
    
    run = 1;
    
    if(fork() == 0) {
        signal(SIGINT, run_changer);
        messageReceiver(status, statusSemaphore, printSemaphore);
    } else {
        signal(SIGINT, run_changer);
        userManager(status, statusSemaphore, printSemaphore);
    }
    
    semctl(statusSemaphore, 0, IPC_RMID);
    semctl(printSemaphore, 0, IPC_RMID);
    shmdt(status);
    shmctl(mem_id, IPC_RMID, 0);
    
    return 0;
}


void userManager(struct state *status, int statusSemaphore, int printSemaphore) {
    
    while(run) {
        
    }
}

void messageReceiver(struct state *status, int statusSemaphore, int printSemaphore) {
    
    struct msgbuf message;
    int queue, id, promptSize;
    int result;
    char prompt[20];
    
    semop(statusSemaphore, &p, 1);
    queue = status->privateQueue;
    semop(statusSemaphore, &v, 1);
    
    while(run) {
        
        semop(statusSemaphore, &p, 1);
            id = (status->choosenUser != -1 ? status->choosenUser : (status->choosenGroup != -1 ? status->choosenGroup : -1));
            promptSize = status->promptSize;
            strcpy(prompt, status->prompt);
        semop(statusSemaphore, &v, 1);
        
        // priority messages
        result = msgrcv(queue, &message, sizeof(message)-sizeof(long), PRIORITY_PORT, IPC_NOWAIT);
        if(result > 0) {
            semop(printSemaphore, &p, 1);
            proceedMessage(message, prompt, promptSize);
            while(!message.end) { // keep critical section of printing while receiveing full message
                msgrcv(queue, &message, sizeof(message)-sizeof(long), PRIORITY_PORT, 0); // block for rest of message
                proceedMessage(message, prompt, promptSize);
            }
            semop(printSemaphore, &v, 1);
        }
        // regular messages
        if(id != -1) {
            
            result = msgrcv(queue, &message, sizeof(message)-sizeof(long), id, IPC_NOWAIT);
            if(result > 0) {
                semop(printSemaphore, &p, 1);
            proceedMessage(message, prompt, promptSize);
            while(!message.end) { // keep critical section of printing while receiveing full message
                msgrcv(queue, &message, sizeof(message)-sizeof(long), id, 0); // block for rest of message
                proceedMessage(message, prompt, promptSize);
            }
            semop(printSemaphore, &v, 1);
            }
        }
        
    }
}

void proceedMessage(struct msgbuf message, char *prompt, int promptSize) {
    if(message.start) {
        for(int i; i<promptSize; ++i)
            printf("%c", 8); // backspace prompt symbol
        if(message.priority) {
            printf("[\033[5;31m"); // flashing red foreground for priority symbol
            printf("!!!");
            printf("\033[0m]");
        } else if(message.mtype == PRIORITY_PORT) {
            // message on priority port without priority flag is message from server
            printf("[\033[32m"); // green foreground for Server messages
            printf("SERVER");
            printf("\033[0m]");
        }
    }    
        
    printf("%s", message.msg);
        
    if(message.end) {    
        printf("\n%s", prompt);
    }
}

int login(int logged, int auth_queue) {
        char login[MAX_LOGIN_LENGTH];
        char password[MAX_PASSWORD_LENGTH];
    
        if(logged) {
            printf("You're logged in. Logout first, to change user");
            return -1;
        }
        int client_queue = msgget(getpid(), 0640 | IPC_CREAT);
        while(!logged) {
            printf("Login:");
            scanf("%s", login);
            printf("Password:");
            scanf("%s", password);
            
            struct authbuf auth;
            struct msgbuf message;
            strcpy(auth.login, login);
            strcpy(auth.password, password);
            auth.mtype = 1;
            auth.client_queue = client_queue;
            
            msgsnd(auth_queue, &auth, sizeof(auth)-sizeof(long), 0);
            msgrcv(client_queue, &message, sizeof(message)-sizeof(long), 0, 0);
            
            printf("%s", message.msg);
            if(message.mtype != 1) {
                // succesfull login
                logged = 1;
                msgctl(client_queue, IPC_RMID, NULL);
                return message.mtype; // private queue for user
            }
            
        }
        return -1;
}
