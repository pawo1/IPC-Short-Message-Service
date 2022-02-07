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
        status->privateQueue = login(logged, auth_queue, printSemaphore);
        strcpy(status->prompt, "Menu");
        status->promptSize = 4;
        
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
    
    int queue;
    char name[MAX_LOGIN_LENGTH+1];
    char buffer[MAX_BUFFER+1];
    char c;
    int i, start, id;
    
    scanf("%c", &c);
    c = 'a';
    
    struct msgbuf message;
    
    char prompt[MAX_LOGIN_LENGTH+1];
    
    while(run) {
        semop(statusSemaphore, &p, 1);
        queue = status->privateQueue;
        id = (status->choosenUser != -1 ? status->choosenUser : (status->choosenGroup != -1 ? status->choosenGroup : -1));
        strcpy(name, status->name);
        strcpy(prompt, status->prompt);
        semop(statusSemaphore, &v, 1);
        
        semop(printSemaphore, &p, 1);
        printPrompt(prompt);
        semop(printSemaphore, &v, 1);
        
        start = 1;
        i = 0;
        
        message.to = id;
        message.mtype = MESSAGE_PORT;
        message.priority = 0;
        strcpy(message.from, name);
        do {
            scanf("%c", &c);
            buffer[i] = c;
            i++;
            if(i == MAX_BUFFER) {
                if(buffer[0] != '/') {
                    //send partial message
                    buffer[MAX_BUFFER] = '\0';
                    
                    strncpy(message.msg, buffer, MAX_BUFFER);
                    message.start = start;
                    message.end = 0;

                    msgsnd(queue, &message, sizeof(struct msgbuf)-sizeof(long), 0);
                    
                    start = 0;
                    i = 0;
                }
            }
        } while(c != '\n');
        
        if(buffer[0] == '/' && start == 1) { //handle commands
        
        } else if(buffer[0] != '\n') { // prevent sending empty messages
            buffer[i] = '\0';
            strncpy(message.msg, buffer, MAX_BUFFER);
            message.start = start;
            message.end = 1;
            
            msgsnd(queue, &message, sizeof(struct msgbuf)-sizeof(long), 0);
        }
        
    }
}

void messageReceiver(struct state *status, int statusSemaphore, int printSemaphore) {
    
    struct msgbuf message;
    int queue, id, promptSize;
    int result;
    char prompt[20];
    
    while(run) {
        
        semop(statusSemaphore, &p, 1);
            queue = status->privateQueue;
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
        for(int i=0; i<promptSize+3; ++i)
            printf("\b \b"); // backspace prompt symbol 4 more characters for [ and ]> elements
            
        if(message.priority) {
            printf("[\033[5;31m"); // flashing red foreground for priority symbol
            printf("!!!");
            printf("\033[0m]");
        } 
        if(message.mtype == PRIORITY_PORT) {
            // message on priority port without priority flag is message from server
            printf("[\033[32m"); // green foreground for Server messages
            printf("SERVER");
            printf("\033[0m] ");
        } else {
            printf("[%s] ", message.from);
        }
    }    
        
    printf("%s", message.msg);
        
    if(message.end) {
        printPrompt(prompt);
    }
}

void printPrompt(char * prompt) {
    
  //  for(int i=0; i<8; ++i)
   //     printf("\b \b"); // backspace prompt symbol 4 more characters for [ and ]> elements
    
    printf("[\033[34m%s\033[0m]>", prompt);
    fflush(stdout);
}

int login(int logged, int auth_queue, int printSemaphore) {
        char login[MAX_LOGIN_LENGTH];
        char password[MAX_PASSWORD_LENGTH];
    
        if(logged) {
            printf("You're logged in. Logout first, to change user");
            return -1;
        }
        int client_queue = msgget(getpid(), 0640 | IPC_CREAT);
        while(!logged) {
            semop(printSemaphore, &p, 1);
            printf("Login:");
            fflush(stdout);
            scanf("%s", login);
            printf("Password:");
            fflush(stdout);
            scanf("%s", password);
            semop(printSemaphore, &v, 1);
            
            struct authbuf auth;
            struct msgbuf message;
            strcpy(auth.login, login);
            strcpy(auth.password, password);
            auth.mtype = 1;
            auth.client_queue = client_queue;
            
            msgsnd(auth_queue, &auth, sizeof(auth)-sizeof(long), 0);
            msgrcv(client_queue, &message, sizeof(message)-sizeof(long), 0, 0);
            
            semop(printSemaphore, &p, 1);
            printf("       ");
            fflush(stdout);
            proceedMessage(message, "Menu", 4);
            semop(printSemaphore, &v, 1);
            
            semop(printSemaphore, &p, 1);
            for(int i=0; i<7; ++i)
                printf("\b \b");
            fflush(stdout);
            semop(printSemaphore, &v, 1);
            
            
            if(message.to != -1) {
                // succesfull login
                logged = 1;
                msgctl(client_queue, IPC_RMID, NULL);
                return message.to; // private queue for user
            }
            

            
            
        }
        return -1;
}
