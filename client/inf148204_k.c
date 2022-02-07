#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>

#include <string.h>

#include <unistd.h>

#include <stdio.h>

#include <sys/shm.h>
#include <sys/sem.h>

#include <errno.h>

#include "inf148204_k.h"



int run;

void run_changer() {
    run = !run;
}

int main() {

    int authQueue = msgget(99901, 0640);
    
        
    int mem_id = shmget(ftok(".", 99901), sizeof(struct state), IPC_CREAT | 0640);
    if(mem_id == -1) {
        printf("Cannot create shared memory\n");
        return -1;
    }
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
    
    if(authQueue >= 0) {
        status->publicQueue = authQueue;
        status->privateQueue = login(status->name, authQueue, printSemaphore);
        status->logged = 1;
        strcpy(status->prompt, "Menu");
        status->promptSize = 4;
        status->choosenGroup = -1;
        status->choosenUser = -1;
        
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
    int i, start, id, msgGroup;
    
    scanf("%c", &c);
    c = 'a';
    
    struct msgbuf message;
    
    char prompt[MAX_LOGIN_LENGTH+1];
    
    while(run) {
        semop(statusSemaphore, &p, 1);
        queue = status->privateQueue;
        id = (status->choosenUser > 0 ? status->choosenUser : (status->choosenGroup > 0 ? status->choosenGroup : -1));
        msgGroup = (status->choosenGroup > 0 ? 1 : 0);
        strcpy(name, status->name);
        strcpy(prompt, status->prompt);
        semop(statusSemaphore, &v, 1);
        
        semop(printSemaphore, &p, 1);
        printPrompt(prompt);
        semop(printSemaphore, &v, 1);
        
        start = 1;
        i = 0;
        
        message.to = 0;// id;
        message.msgGroup = msgGroup; // flag for group messages
        message.mtype = MESSAGE_PORT;
        message.priority = 0;
        strcpy(message.from, name);
        
        
        do {
            scanf("%c", &c);
            buffer[i] = c;
            i++;
            if(i == MAX_BUFFER) {
                
                if(buffer[0] == '/' && buffer[1] == 'p' && buffer[2] == ' ' && start == 1) {
                    message.priority = 1;
                    strcpy(buffer, buffer + 3);
                    buffer[i-3] = '\0';
                }
                
                if( (buffer[0] != '/' || message.priority) && id != -1) {
                    //send partial message
                    buffer[MAX_BUFFER] = '\0';
                    
                    strncpy(message.msg, buffer, MAX_BUFFER);
                    message.start = start;
                    message.end = 0;

                    msgsnd(queue, &message, sizeof(struct msgbuf)-sizeof(long), 0);
                    
                    start = 0;
                    i = 0;
                } else if(buffer[0] == '/' || id == -1){
                    
                    semop(printSemaphore, &p, 1);
                    if(buffer[0] == '/')
                        printf("\033[31mCommand line is too long\n\033[0m");
                    else if(id == -1)
                        printf("\033[31mJoin chat to send messages!\nType /help for more informations\033[0m");
                    semop(printSemaphore, &v, 1);
                    
                    while(c != '\n') { // clear command from buffer
                        scanf("%c", &c);
                    }
                }
            }
        } while(c != '\n');
        
        if(buffer[0] == '/' && buffer[1] == 'p' && buffer[2] == ' ' && buffer[3] != '\n' && start == 1) { // treat priority as message not command
            message.priority = 1;
            strcpy(buffer, buffer + 3);
            i -= 3;
        }
        
        if(buffer[0] == '/' && start == 1 && !message.priority) { //handle commands, priority trim first command
            
            struct cmdbuf cmdmsg;
            cmdmsg.mtype = COMMAND_PORT;
            semop(statusSemaphore, &p, 1);
            int queue = status->privateQueue;
            semop(statusSemaphore, &v, 1);
            
            int result;
            char *split;
            buffer[i-1] = '\0'; // delete newline
            split = strtok(buffer, " ");
            strcpy(cmdmsg.command, split + 1); // copy without '/'
            
            for(int j=0; j<MAX_ARGS; ++j) {
                split = strtok(NULL, " ");
                if(split != NULL)
                    strcpy(cmdmsg.arguments[j], split);
                else
                    cmdmsg.arguments[j][0] = '\0';
            }
            
            if(strcmp(cmdmsg.command, "help") == 0) {
                
            } else if(strcmp(cmdmsg.command, "login") == 0) {
                
                char name[MAX_LOGIN_LENGTH];
                
                
                semop(statusSemaphore, &p, 1);
                int authQueue = status->publicQueue;
                int logged = status->logged;
                semop(statusSemaphore, &v, 1);
                if(logged)
                    printf("You're logged in. Logout first, to change user\n");
                else {
                    result = login(name, authQueue, printSemaphore);
                    
                    if(result != -1) {
                        semop(statusSemaphore, &p, 1);
                        status->logged = 1;
                        status->privateQueue = result;
                        semop(statusSemaphore, &v, 1);
                    }
                
                }
            } else if(strcmp(cmdmsg.command, "logout") == 0) {
                semop(statusSemaphore, &p, 1);
                status->logged = 0;
                int authQueue = status->publicQueue;
                semop(statusSemaphore, &v, 1);
                result = msgsnd(queue, &cmdmsg, sizeof(struct msgbuf)-sizeof(long), 0);
                if(result == -1) {
                    printSystemInfo("Cannot pass command to server\n", printSemaphore);
                } else {
                    char name[MAX_LOGIN_LENGTH];
                    
                    result = login(name, authQueue, printSemaphore);
                    if(result != -1) {
                        semop(statusSemaphore, &p, 1);
                        status->logged = 1;
                        status->privateQueue = result;
                        semop(statusSemaphore, &v, 1);
                    }
                }
                
            } else if(strcmp(cmdmsg.command, "list") == 0) {
                
            } else if(strcmp(cmdmsg.command, "group") == 0) {
                
            } else if(strcmp(cmdmsg.command, "msg") == 0) {
                
            } else if(strcmp(cmdmsg.command, "menu") == 0) {
                
            } else if(strcmp(cmdmsg.command, "p") == 0) {
                printSystemInfo("Priority message is empty, type something after /p \n", printSemaphore);
            } /*else if(strcmp(cmdmsg.command, "block") == 0) {
            
            } else if(strcmp(cmdmsg.command, "unblock") == 0) {
                
            } */ else {
                printSystemInfo("Unsupported command!\nType /help for list of available commands\n", printSemaphore);
            }
            
            
            
        } else if(buffer[0] != '\n' && start == 1 && id != -1) { // prevent sending empty messages
            buffer[i] = '\0';
            strncpy(message.msg, buffer, MAX_BUFFER);
            message.start = start;
            message.end = 1;
            
            msgsnd(queue, &message, sizeof(struct msgbuf)-sizeof(long), 0);
        } else {
            printSystemInfo("Join chat to send messages!\nType /help for more informations\n", printSemaphore);
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
            id = (status->choosenUser > 0 ? status->choosenUser : (status->choosenGroup > 0 ? status->choosenGroup : -1));
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
        if(message.mtype == PRIORITY_PORT && !message.priority) {
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

void printSystemInfo(char * message, int sem) {
    semop(sem, &p, 1);
    printf("\033[31m%s\033[0m", message);
    semop(sem, &v, 1);
}

int login(char * name, int auth_queue, int printSemaphore) {
        char login[MAX_LOGIN_LENGTH+1];
        char password[MAX_PASSWORD_LENGTH+1];
        
        int client_queue = msgget(getpid(), 0640 | IPC_CREAT);
        while(1) {
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
                strncpy(name, login, MAX_LOGIN_LENGTH);
                msgctl(client_queue, IPC_RMID, NULL);
                return message.to; // private queue for user
            }
            
        }
        return -1;
}
