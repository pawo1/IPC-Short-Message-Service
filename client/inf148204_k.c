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
    
        
    int mem_id = shmget(ftok(".", getpid()), sizeof(struct state), IPC_CREAT | 0640);
    if(mem_id == -1) {
        printf("Cannot create shared memory\n");
        return -1;
    }
    struct state *status = shmat(mem_id, NULL, 0);
    
    int statusSemaphore = semget(ftok(".", getpid()+1), sizeof(union semun), IPC_CREAT | 0640);
    int printSemaphore = semget(ftok(".", getpid()+2), sizeof(union semun), IPC_CREAT | 0640);
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
        status->promptSize = strlen(status->name)+4;
        status->choosenGroup = -1;
        status->choosenUser = -1;
        
    } else {
        printf("Server is not available\n");
        return -1;
    }
    
    run = 1;
    
    int child_id = fork();
    
    if(child_id != 0) {
        signal(SIGINT, run_changer);
        messageReceiver(status, statusSemaphore, printSemaphore);
        shmdt(status);
    } else {
        signal(SIGINT, run_changer);
        userManager(status, statusSemaphore, printSemaphore);
        kill(child_id, SIGINT);
        semctl(statusSemaphore, 0, IPC_RMID);
        semctl(printSemaphore, 0, IPC_RMID);
        shmdt(status);
        shmctl(mem_id, IPC_RMID, 0);
    }
    
    return 0;
}


void userManager(struct state *status, int statusSemaphore, int printSemaphore) {
    
    int queue;
    char name[MAX_LOGIN_LENGTH+1];
    char buffer[MAX_BUFFER+1];
    char c;
    int i, start, id, msgGroup;
    
    struct msgbuf message;
    struct cmdbuf cmdmsg;
    
    char prompt[MAX_LOGIN_LENGTH+MAX_GROUP_NAME+2+1];
    
    while(run) {
        semop(statusSemaphore, &p, 1);
        queue = status->privateQueue;
        id = (status->choosenUser > 0 ? status->choosenUser : (status->choosenGroup > 0 ? status->choosenGroup : -1));
        msgGroup = (status->choosenGroup > 0 ? 1 : 0);
        
        strcpy(name, status->name);
        if(status->choosenGroup > 0 ) {
            strcpy(name+strlen(status->name), "->");
            strcpy(name+strlen(status->name)+2, status->prompt);
        }
        strcpy(prompt, status->prompt);
        semop(statusSemaphore, &v, 1);
        
        semop(printSemaphore, &p, 1);
        printPrompt(name, prompt);
        semop(printSemaphore, &v, 1);
        
        start = 1;
        i = 0;
        
        message.to = id;
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
        buffer[i] = '\0'; // mark end of current buffer
        
        if(buffer[0] == '/' && buffer[1] == 'p' && buffer[2] == ' ' && buffer[3] != '\n' && start == 1) { // treat priority as message not command
            message.priority = 1;
            strcpy(buffer, buffer + 3);
            i -= 3;
        }
        
        if(buffer[0] == '/' && start == 1 && !message.priority) { //handle commands, priority trim first command
            
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
/* help */ //TODO
            } else if(strcmp(cmdmsg.command, "login") == 0) {
/* login */
                char name[MAX_LOGIN_LENGTH];
                
                
                semop(statusSemaphore, &p, 1);
                int authQueue = status->publicQueue;
                int logged = status->logged;
                semop(statusSemaphore, &v, 1);
                if(logged)
                    printSystemInfo("You're logged in. Logout first, to change user\n", printSemaphore);
                else {
                    result = login(name, authQueue, printSemaphore);
                    
                    if(result != -1) {
                        semop(statusSemaphore, &p, 1);
                        strcpy(status->name, name);
                        status->logged = 1;
                        status->privateQueue = result;
                        status->choosenUser = -1;
                        status->choosenGroup = -1;
                        status->promptSize = strlen(status->name)+4;
                        strcpy(status->prompt, "Menu");
                        semop(statusSemaphore, &v, 1);
                        
                        scanf("%c", &c); // flush newline from login
                    }
                
                }
            } else if(strcmp(cmdmsg.command, "logout") == 0) {
/* logout */
                semop(statusSemaphore, &p, 1);
                status->logged = 0;
                int authQueue = status->publicQueue;
                semop(statusSemaphore, &v, 1);
                result = msgsnd(queue, &cmdmsg, sizeof(struct cmdbuf)-sizeof(long), 0);
                if(result == -1) {
                    printSystemInfo("Cannot pass command to server\n", printSemaphore);
                } else {
                    char name[MAX_LOGIN_LENGTH];
                    
                    result = login(name, authQueue, printSemaphore);
                    if(result != -1) {
                        semop(statusSemaphore, &p, 1);
                        strcpy(status->name, name);
                        status->logged = 1;
                        status->privateQueue = result;
                        status->choosenUser = -1;
                        status->choosenGroup = -1;
                        status->promptSize = strlen(status->name)+4;
                        strcpy(status->prompt, "Menu");
                        semop(statusSemaphore, &v, 1);
                    }
                }
                
            } else if(strcmp(cmdmsg.command, "list") == 0 || strcmp(cmdmsg.command, "group") == 0) {
/* list / group - only passing to server*/
                result = msgsnd(queue, &cmdmsg, sizeof(struct cmdbuf)-sizeof(long), 0);
                if(result == -1) {
                    printSystemInfo("Cannot pass command to server\n", printSemaphore);
                }
            }  else if(strcmp(cmdmsg.command, "msg") == 0) {
/* msg */
                result = msgsnd(queue, &cmdmsg, sizeof(struct cmdbuf)-sizeof(long), 0);
                if(result == -1) {
                    printSystemInfo("Cannot pass command to server\n", printSemaphore);
                }
                
                semop(printSemaphore, &p, 1); // block printing old prompt
                semop(statusSemaphore, &p, 1);

                result = msgrcv(queue, &cmdmsg, sizeof(struct cmdbuf)-sizeof(long), ANSWER_PORT, 0);
                if(result == -1) {
                    printSystemInfo("Error server connection\n", printSemaphore);
                } else {
                    if(cmdmsg.result != -1) {
                        
                        if(strcmp(cmdmsg.arguments[0], "group") == 0 && cmdmsg.arguments[1][0] != '\0') {
                            status->choosenGroup = cmdmsg.result;
                            status->choosenUser = -1;
                            strcpy(status->prompt, cmdmsg.arguments[1]);
                        } else {
                            status->choosenGroup = -1;
                            status->choosenUser =cmdmsg.result;
                            strcpy(status->prompt, cmdmsg.arguments[0]);
                        }
                        status->promptSize = strlen(status->name)+strlen(status->prompt);
                        
                    }
                }
                
                semop(statusSemaphore, &v, 1);
                semop(printSemaphore, &v, 1);

                
            } else if(strcmp(cmdmsg.command, "menu") == 0) {
/* menu */
                semop(statusSemaphore, &p, 1);
                status->choosenGroup = -1;
                status->choosenUser = -1;
                strcpy(status->prompt, "Menu");
                status->promptSize = strlen(status->name)+4;
                semop(statusSemaphore, &v, 1);
            } else if(strcmp(cmdmsg.command, "p") == 0) {
                printSystemInfo("Priority message is empty, type something after /p \n", printSemaphore);
            } else if(strcmp(cmdmsg.command, "exit") == 0) {
/* exit */
                run = 0;
            } /*else if(strcmp(cmdmsg.command, "block") == 0) {
            
            
            } else if(strcmp(cmdmsg.command, "unblock") == 0) {
                
            } */ else {
/* default */
                printSystemInfo("Unsupported command!\nType /help for list of available commands\n", printSemaphore);
            }
            
            
            
        } else if(buffer[0] != '\n' && id != -1) { // prevent sending empty messages
            buffer[i] = '\0';
            strncpy(message.msg, buffer, MAX_BUFFER);
            message.start = start;
            message.end = 1;
            
            msgsnd(queue, &message, sizeof(struct msgbuf)-sizeof(long), 0);
        } else if(buffer[0] != '\n') {
            printSystemInfo("Join chat to send messages!\nType /help for more informations\n", printSemaphore);
        }
        
    }
    
    cmdmsg.mtype = COMMAND_PORT;
    strcpy(cmdmsg.command, "logout");
    semop(statusSemaphore, &p, 1);
    queue = status->privateQueue;
    semop(statusSemaphore, &v, 1);
    msgsnd(queue, &cmdmsg, sizeof(struct cmdbuf)-sizeof(long), 0);
}

void messageReceiver(struct state *status, int statusSemaphore, int printSemaphore) {
    
    struct msgbuf message;
    int queue, id, promptSize;
    int result;
    char prompt[72];
    char name[MAX_LOGIN_LENGTH+1];
    
    while(run) {
        
        
                semop(statusSemaphore, &p, 1); 
                queue = status->privateQueue;
                id = (status->choosenUser > 0 ? status->choosenUser : (status->choosenGroup > 0 ? status->choosenGroup : -1));
                strcpy(name, status->name);
                semop(statusSemaphore, &v, 1); 
        
        // priority messages
        result = msgrcv(queue, &message, sizeof(message)-sizeof(long), PRIORITY_PORT, IPC_NOWAIT);
        if(result > 0) {
            semop(printSemaphore, &p, 1);

            semop(statusSemaphore, &p, 1); // update after claim printing, because /msg could update status
            promptSize = status->promptSize;
            strcpy(prompt, status->prompt);
            semop(statusSemaphore, &v, 1); 
            
            proceedMessage(message, name, prompt, promptSize);
            while(!message.end) { // keep critical section of printing while receiveing full message
                msgrcv(queue, &message, sizeof(message)-sizeof(long), PRIORITY_PORT, 0); // block for rest of message
                proceedMessage(message, name, prompt, promptSize);
            }
            semop(printSemaphore, &v, 1);
        }
        // regular messages
        if(id != -1) {
            
            result = msgrcv(queue, &message, sizeof(message)-sizeof(long), id, IPC_NOWAIT);
            if(result > 0) {
                semop(printSemaphore, &p, 1);
                
                
                semop(statusSemaphore, &p, 1); // update after claim printing, because /msg could update status
                promptSize = status->promptSize;
                strcpy(prompt, status->prompt);
                semop(statusSemaphore, &v, 1);
                
                proceedMessage(message, name, prompt, promptSize);
                while(!message.end) { // keep critical section of printing while receiveing full message
                    msgrcv(queue, &message, sizeof(message)-sizeof(long), id, 0); // block for rest of message
                    proceedMessage(message, name, prompt, promptSize);
                }
                semop(printSemaphore, &v, 1);
            }
        }
        
    }
}

void proceedMessage(struct msgbuf message, char *name, char *prompt, int promptSize) {
    if(message.start) {
        for(int i=0; i<promptSize+5; ++i)
            printf("\b \b"); // backspace prompt symbol 5 more characters for [. -> and ]> elements
            
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
            printf("[\033[36m"); // cyan foreground for message labels
            printf("%s", message.from);
            printf("\033[0m] ");
        }
    }    
        
    printf("%s", message.msg);
        
    if(message.end) {
        printPrompt(name, prompt);
    }
}

void printPrompt(char *name, char * prompt) {
    
  //  for(int i=0; i<8; ++i)
   //     printf("\b \b"); // backspace prompt symbol 4 more characters for [ and ]> elements
  //  if(name != NULL)
   //     printf("[\033[34m%s->%s\033[0m]>", name, prompt);
   // else
    printf("[\033[1;34m%s->%s\033[0m]>", name, prompt);
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
            for(int i=0; i<strlen(login)+9; ++i)
                printf(" ");
            fflush(stdout);
            proceedMessage(message, login, "Menu", strlen(login)+4);
            for(int i=0; i<strlen(login)+9; ++i)
                printf("\b \b");
            fflush(stdout);
            char c;
            scanf("%c", &c); // flush newline from stdin
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
