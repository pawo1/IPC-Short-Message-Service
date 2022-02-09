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

#include <sys/shm.h>
#include <sys/sem.h>

#include <errno.h>


#include "inf148204_s.h"

int run;

void run_changer() {
    run = !run;
}

struct sembuf p = { 0, -1, SEM_UNDO}; 
struct sembuf v = { 0, +1, SEM_UNDO};

int main(int argc, char *argv[]) {
    
    int result;
    int loadedUsers;
    
    char * config;
    int threadCounter;
    
    if(argc > 1) {
        if(strcmp(argv[1], "repair") == 0) {
            printf("Repairing locked queues...\n");
            int queue = msgget(99901, 0640 | IPC_CREAT);
            msgctl(queue, IPC_RMID, NULL);
            printf("Rerun server without *repair* option\n");
            return 0;
        }
        
       config = argv[1];
    } else {
        config = strdup("default.txt");
    }
    
    if(argc > 2) {
        threadCounter = atoi(argv[2]);
    } else {
        threadCounter = 1;
    }
    
    int groupSemaphore = semget(ftok(".", getpid()+1), sizeof(union semun), IPC_CREAT | 0640);
    int userSemaphore = semget(ftok(".", getpid()+2), sizeof(union semun), IPC_CREAT | 0640);
    int printSemaphore = semget(ftok(".", getpid()+3), sizeof(union semun), IPC_CREAT | 0640);
    union semun semaphoreUnion;
    semaphoreUnion.val = 1;
    semctl(groupSemaphore, 0, SETVAL, semaphoreUnion);
    semctl(userSemaphore, 0, SETVAL, semaphoreUnion);
    semctl(printSemaphore, 0, SETVAL, semaphoreUnion);
    
    struct user **users;
    int user_id = shmget(ftok(".", getpid()+4), sizeof(struct user*) * MAX_USERS, IPC_CREAT | 0640);
    if(user_id == -1) {
        printLogTime();
        printf("Error %d during user memory allocation\n", errno);
        return -1;
    }
    users  = shmat(user_id, NULL, 0);
    int user_sub_id[MAX_USERS];
    for(int i=0; i<MAX_USERS; ++i) {
        user_sub_id[i] = shmget(ftok(".", getpid()+400+i), sizeof(struct user), IPC_CREAT | 0640);
        if(user_sub_id[i] == -1) {
            printLogTime();
            printf("Error %d during memory allocation, cell %d\n", errno, i);
        }
        users[i] = shmat(user_sub_id[i], NULL, 0);
    }

    struct group **groups;
    int group_id = shmget(ftok(".", getpid()+5), sizeof(struct group*) * MAX_GROUPS, IPC_CREAT | 0640);
    groups = shmat(group_id, NULL, 0);
    int group_sub_id[MAX_GROUPS];
    for(int i=0; i<MAX_GROUPS; ++i) {
        group_sub_id[i] = shmget(ftok(".", getpid()+500+i), sizeof(struct group), IPC_CREAT | 0640);
        groups[i] = shmat(group_sub_id[i], NULL, 0);
    }

    //struct user **users = malloc( sizeof(struct user*) * MAX_USERS );
    //for(int i=0; i<MAX_USERS; ++i)
     //   users[i] = malloc( sizeof(struct user) );
        
    /*struct group **groups = malloc( sizeof(struct group*) * MAX_GROUPS);
    for(int i=0; i<MAX_GROUPS; ++i)
        groups[i] = malloc( sizeof(struct group) ); */
    
    result = loadConfig(config, users, MAX_USERS, groups, MAX_GROUPS);
    
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
    
    int authQueue = msgget(99901, 0640 | IPC_CREAT | IPC_EXCL);
        
    if(authQueue == -1 && errno == EEXIST) {
        printLogTime();
        printf("Server queue already exist\n\t\t\tis there another server process running?\n");
        printLogTime();
        printf("You can reset queues by runing server with *repair* argument\n");
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
    
    
    printLogTime();
    printf("Server joining run loop. Ctrl+C to stop work\n");
    
    int parent = 1;
    for(int i=0; i<threadCounter; ++i) {
        if(parent) {
            if(fork() == 0) {
                parent = 0;
                break;
            }
        }
    }
    
    /* starting pararell part of server */
    
    run = 1;
    signal(SIGINT, run_changer);
    if(!parent) {
        while(run) {
            
            // child workers
            
            proceedAuth(users, loadedUsers, authQueue, messageQueues, printSemaphore, userSemaphore, groupSemaphore);
            proceedMessages(users, loadedUsers, groups, messageQueues, printSemaphore, userSemaphore, groupSemaphore);
            proceedCommands(users, &loadedUsers, groups, messageQueues, printSemaphore, userSemaphore, groupSemaphore);
        
        
        }
    }
    else {
        
        // parent cleanup memory and manage file acess
        
        while(wait(NULL) > 0) {} // wait for all children processes
        
        printf("\n");
        printLogTime();
        printf("Stopping the server. Thanks for using chat\n");
        msgctl(authQueue, IPC_RMID, NULL);
        for(int i=0; i<loadedUsers; ++i) {
            msgctl(messageQueues[i], IPC_RMID, NULL);
        }
        saveConfig(config, users, MAX_USERS, groups, MAX_GROUPS);
        
        semctl(groupSemaphore, 0, IPC_RMID);
        semctl(userSemaphore, 0, IPC_RMID);
        semctl(printSemaphore, 0, IPC_RMID);
        
    }
    
    for(int i=0; i<MAX_USERS; ++i) {
        shmdt(users[i]);
        if(parent)
            shmctl(user_sub_id[i], IPC_RMID, 0);
    }
    shmdt(users);
    if(parent)
        shmctl(user_id, IPC_RMID, 0);
    
    for(int i=0; i<MAX_GROUPS; ++i) {
        shmdt(groups[i]);
        if(parent)
            shmctl(group_sub_id[i], IPC_RMID, 0);
    }
    shmdt(groups);
    if(parent)
        shmctl(group_id, IPC_RMID, 0);
    
    freeMemory(users, groups);
    free(messageQueues);
    
    return 0;
}

void proceedAuth(struct user **users, int loadedUsers, int authQueue, int *messageQueues, int ps, int us, int gs) {
    
    struct authbuf auth;
    struct msgbuf message;
    
    if(msgrcv(authQueue, &auth, sizeof(auth)-sizeof(long), 0, IPC_NOWAIT) > 0) {
            int find = 0;
            
            message.mtype = PRIORITY_PORT;
            message.priority = 0; // messages from server are on priority port but without flag
            message.start = 1;
            message.end = 1;
            message.to = -1;
            
            char login[MAX_LOGIN_LENGTH];
            
            
            for(int i=0; i<loadedUsers; ++i) {
                
                semop(us, &p, 1);
                strcpy(login, users[i]->login);
                semop(us, &v, 1);
                
                if(strcmp(auth.login, login) == 0) {
                    find = 1;
                    semop(us, &p, 1);
                    if(!users[i]->logged) {
                        if(users[i]->tryCounter >= MAX_TRIES) {
                            strncpy(message.msg, "You reached max tries of authentication!\nYour account is blocked, contact with administrator\n", MAX_BUFFER);
                        } else if(strcmp(auth.password, users[i]->password) == 0) {
                            users[i]->logged = 1;
                            sprintf(message.msg, "Welcome back %s!\n", users[i]->login);
                            users[i]->tryCounter = 0;
                            message.to = messageQueues[i]; // adress for private queue
                            
                            semop(ps, &p, 1);
                            printLogTime();
                            printf("User %s logged in\n", users[i]->login);
                            semop(ps, &v, 1);
                        } else {
                            users[i]->tryCounter++;
                            strcpy(message.msg, "Wrong Password!\n");
                            
                            char tries[11];
                            sprintf(tries, "%d", (MAX_TRIES-users[i]->tryCounter));
                            
                            strcpy(message.msg+strlen(message.msg), tries);
                            strcpy(message.msg+strlen(message.msg), " tries left\n"); 
                            
                            semop(ps, &p, 1);
                            printLogTime();
                            printf("User %s tried to login, wrong password. %d tries left before block\n", users[i]->login, MAX_TRIES-users[i]->tryCounter);
                            semop(ps, &v, 1);
                        }
                    }
                    else {
                        strncpy(message.msg, "User already logged on another client\n", MAX_BUFFER);
                    }
                    
                    semop(us, &v, 1); // leave critical section before send message in case of waiting for client 
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

void proceedMessages(struct user **users, int loadedUsers, struct group **groups, int *messageQueues, int ps, int us, int gs) {

    struct msgbuf message;
    int result, uid, queue;
    int index;

    for(int i=0; i<loadedUsers; ++i) {
        if(msgrcv(messageQueues[i], &message, sizeof(struct msgbuf)-sizeof(long), MESSAGE_PORT, IPC_NOWAIT) > 0) {
            if(message.msgGroup) {
                index = message.to - GROUP_OFFSET;
                
                char groupName[MAX_GROUP_NAME];
                semop(gs, &p, 1);
                strcpy(groupName, groups[index]->name);
                semop(gs, &v, 1);
                
                
                for(int j=0; j<MAX_USERS; ++j) {
                    semop(gs, &p, 1);
                    if(groups[index]->userId[j] == 1)
                        uid = j; //groups[index]->userId[j];
                    else
                        uid = -1;
                    semop(gs, &v, 1);

                    if(uid == -1)
                        continue;
                    
                    semop(us, &p, 1);
                    int blocked = users[j]->blockedGroups[index];
                    semop(us, &v, 1);
                    
                    if(blocked) {
                        semop(us, &p, 1);
                        strcpy(message.msg, "Cannot send message to user ");
                        strcpy(message.msg+strlen(message.msg), users[j]->login);
                        strcpy(message.msg+strlen(message.msg), " blocked this group\n");
                        message.mtype = PRIORITY_PORT;
                        message.priority = 0;
                        message.start = 1;
                        message.end = 1;
                        msgsnd(messageQueues[i], &message, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);
                        
                        semop(ps, &p, 1);
                        printLogTime();
                        printf("Message from %s as message to group %s, hasn't sent to %s. Reson: blocked by user\n", users[i]->login, groupName, users[j]->login);
                        semop(ps, &v, 1);
                        
                        semop(us, &v, 1);
                        continue;
                    }
                    
                    if(uid != i) { // prevent sending messages to itself
                        queue = messageQueues[uid];
                        message.mtype = (message.priority ? PRIORITY_PORT : (index + GROUP_OFFSET)); // information who sent message
                        result = msgsnd(queue, &message, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);
                        if(result == -1) {
                            if(errno == EAGAIN) {
                                sendMessage(messageQueues[i], PRIORITY_PORT, 0, "Couldn't deliver message, queue is full\n");
                                
                                semop(us, &p, 1);
                                semop(ps, &p, 1);
                                printLogTime();
                                printf("User %s has full queue, message lost\n", users[i]->login);
                                semop(ps, &v, 1);
                                semop(us, &v, 1);
                                
                            } else {
                                sendMessage(messageQueues[i], PRIORITY_PORT, 0, "Error during sending message\n");
                                
                                semop(us, &p, 1);
                                semop(ps, &p, 1);
                                printLogTime();
                                printf("Error accessing %s queue\n", users[i]->login);
                                semop(ps, &v, 1);
                                semop(us, &v, 1);
                            }
                        } else {
                            sendMessage(messageQueues[i], PRIORITY_PORT, 0, "Message delivered\n");
                            
                            semop(us, &p, 1);
                            semop(ps, &p, 1);
                            printLogTime();
                            printf("Sent Message from %s to %s as group %s message\n", users[i]->login, users[uid]->login, groupName);
                            semop(ps, &v, 1);
                            semop(us, &v, 1);
                        }
                    }
                }
            } else {
                uid = message.to-USER_OFFSET;
                queue = messageQueues[uid];
                message.mtype = (message.priority ? PRIORITY_PORT : i+USER_OFFSET); // information who sent message
                
                semop(us, &p, 1);
                int blocked = users[uid]->blockedUsers[i];
                semop(us, &v, 1);
                
                if(blocked) {
                    strcpy(message.msg, "Cannot send message to user ");
                    semop(us, &p, 1);
                    strcpy(message.msg+strlen(message.msg), users[uid]->login);
                    semop(us, &v, 1);
                    strcpy(message.msg+strlen(message.msg), " because of block\n");
                    message.mtype = PRIORITY_PORT;
                    message.priority = 0;
                    message.start = 1;
                    message.end = 1;
                    msgsnd(messageQueues[i], &message, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);
                        
                    semop(us, &p, 1);
                    semop(ps, &p, 1);
                    printLogTime();
                    printf("Message from %s hasn't sent to %s. Reson: blocked by user\n", users[i]->login, users[uid]->login);
                    semop(ps, &v, 1);
                    semop(us, &v, 1);
                    
                    return;
                }
                
                result = msgsnd(queue, &message, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);
                if(result == -1) {
                    if(errno == EAGAIN) {
                        sendMessage(messageQueues[i], PRIORITY_PORT, 0, "Couldn't deliver message, queue is full\n");
                        
                        semop(us, &p, 1);
                        semop(ps, &p, 1);
                        printLogTime();
                        printf("User %s has full queue, message lost\n", users[i]->login);
                        semop(ps, &v, 1);
                        semop(us, &v, 1);
                        
                    } else {
                        sendMessage(messageQueues[i], PRIORITY_PORT, 0, "Error during sending message\n");
                        
                        semop(us, &p, 1);
                        semop(ps, &p, 1);
                        printLogTime();
                        printf("Error accessing %s queue\n", users[i]->login);
                        semop(us, &v, 1);
                        semop(ps, &v, 1);
                    }
                } else {
                    sendMessage(messageQueues[i], PRIORITY_PORT, 0, "Message delivered\n");
                    
                    semop(us, &p, 1);
                    semop(ps, &p, 1);
                    printLogTime();
                    printf("Sent Message from %s to %s\n", users[i]->login, users[uid]->login);
                    semop(us, &v, 1);
                    semop(ps, &v, 1);
                }
            }
                
        }
    }
}

void proceedCommands(struct user **users, int *loadedUsers, struct group **groups, int *messageQueues, int ps, int us, int gs) {
    struct cmdbuf cmdmsg;
    struct msgbuf message;
    
    message.mtype = PRIORITY_PORT;
    message.priority = 0;
    message.start = 1;
    message.end = 1;
    message.msgGroup = -1;

    int result, find;
    

    for(int i=0; i<(*loadedUsers); ++i) {
        if(msgrcv(messageQueues[i], &cmdmsg, sizeof(struct cmdbuf)-sizeof(long), COMMAND_PORT, IPC_NOWAIT) > 0) {
            if(strcmp(cmdmsg.command, "logout") == 0) {
/* logout */
                users[i]->logged = 0;
                printLogTime();
                printf("User %s logged out\n", users[i]->login);
            } else if(strcmp(cmdmsg.command, "msg") == 0) {
/* switch chat */
                if(cmdmsg.arguments[0][0] != '\0') {
                    if(strcmp(cmdmsg.arguments[0], "group") == 0 && cmdmsg.arguments[1][0] != '\0') {
                        // join group chat
                        find = 0;
                        for(int j=0; j<MAX_GROUPS; ++j) {
                            if(strcmp(cmdmsg.arguments[1], groups[j]->name) == 0) {
                                find = 1;
                                int member = groups[j]->userId[i];
                               /* for(int k=0; k<groups[j]->groupSize; ++k) {
                                    if(groups[j]->userId[k] == i) {
                                        member = 1;
                                        break;
                                    }
                                } */
                                if(!member) {
                                    cmdmsg.result = -1;
                                    strcpy(message.msg, "You are not member of requested group.\nJoin it first by /group join command\n");
                                    result = msgsnd(messageQueues[i], &message, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);
                                    if(result == -1) {
                                        printLogTime();
                                        printf("Cannot send message to %s\n", users[i]->login);
                                    } else {
                                        printLogTime();
                                        printf("Declined request of User %s to join %s group chat\n", users[i]->login, groups[j]->name);
                                        
                                    }
                                } else {
                                    cmdmsg.result = j + GROUP_OFFSET;

                                    strcpy(message.msg, "Switched to new chat\n");
                                    result = msgsnd(messageQueues[i], &message, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);
                                    if(result == -1) {
                                        printLogTime();
                                        printf("Cannot send message to %s\n", users[i]->login);
                                    } else {
                                        printLogTime();
                                        printf("User %s joined Group Chat: %s\n", users[i]->login, groups[j]->name);
                                        
                                    }
                                }
                            }
                        }
                        if(!find) {
                            cmdmsg.result = -1;
                            strcpy(message.msg, "Cannot find specified group\nType /group for list of available groups\n");
                            result = msgsnd(messageQueues[i], &message, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);
                                if(result == -1) {
                                    printLogTime();
                                    printf("Cannot send message to %s\n", users[i]->login);
                                }
                                else {
                                    printLogTime();
                                    printf("User %s requested join %s group chat, but it doesn't exist\n", users[i]->login, cmdmsg.arguments[1]);
                                }
                        }
                        
                        cmdmsg.mtype = ANSWER_PORT;
                        result = msgsnd(messageQueues[i], &cmdmsg, sizeof(struct cmdbuf)-sizeof(long), IPC_NOWAIT);
                        if(result == -1) {
                            printLogTime();
                            printf("Cannot answer %s request\n", users[i]->login);
                            break;
                        }
                        
                    } else {
                        //join user chat
                        find = 0;
                        for(int j=0; j<MAX_USERS; ++j) {
                            if(strcmp(cmdmsg.arguments[0], users[j]->login) == 0) {
                                find = 1;
                                
                                cmdmsg.result = j + USER_OFFSET;

                                strcpy(message.msg, "Switched to new chat\n");
                                result = msgsnd(messageQueues[i], &message, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);
                                if(result == -1) {
                                    printLogTime();
                                    printf("Cannot send message to %s\n", users[i]->login);
                                } else {
                                    printLogTime();
                                    printf("User %s joined Chat: %s\n", users[i]->login, users[j]->login);
                                }
                            
                            }
                        }
                        if(!find) {
                            cmdmsg.result = -1;
                            strcpy(message.msg, "Cannot find specified user\nType /list for logged users\n/list all for full list of users\n");
                            result = msgsnd(messageQueues[i], &message, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);
                            if(result == -1) {
                                printLogTime();
                                printf("Cannot send message to %s\n", users[i]->login);
                            }
                            else {
                                printLogTime();
                                printf("User %s requested join %s user chat, but it's not found\n", users[i]->login, cmdmsg.arguments[0]);
                            }
                        }
                         
                        cmdmsg.mtype = ANSWER_PORT;
                        result = msgsnd(messageQueues[i], &cmdmsg, sizeof(struct cmdbuf)-sizeof(long), IPC_NOWAIT);
                        if(result == -1) {
                            printLogTime();
                            printf("Cannot answer %s request\n", users[i]->login);
                            break;
                        }
                    }
                }
            } else if(strcmp(cmdmsg.command, "list") == 0 || (strcmp(cmdmsg.command, "group") == 0 && cmdmsg.arguments[0][0] == '\0' && 
                                                                                                      cmdmsg.arguments[1][0] == '\0')) {
/* list users or groups*/

                    message.end = 0;
                    int len, all, find;
                    
                    
                    len = 0;
                    if(strcmp(cmdmsg.command, "group") == 0) {
                        strcpy(message.msg, "List of available Groups in system\n---\n");
                        len += strlen(message.msg);
                    } else if(cmdmsg.arguments[0][0] == '\0' || strcmp(cmdmsg.arguments[0], "all") == 0) {

                        
                        
                        if(cmdmsg.arguments[0][0] != '\0') {
                            all = 1;
                            strcpy(message.msg, "List of all users in system\n---\n");
                            len += strlen(message.msg);
                        } else {
                            all = 0;
                            strcpy(message.msg, "List of logged users\n---\n");
                            len += strlen(message.msg);
                        }
                        
                    } else {
                        
                        strcpy(message.msg, "List of users in ");
                        len = strlen(message.msg);
                        strcpy(message.msg+len, cmdmsg.arguments[0]);
                        len = strlen(message.msg);
                        strcpy(message.msg+len, "\n---\n");
                        len = strlen(message.msg);
                    }
                    
                    if(strcmp(cmdmsg.command, "group") == 0) {
                        find = 1;
                        for(int j=0; j<MAX_GROUPS; ++j) {
                            if(groups[j]->name[0] != '\0') {
                                strcpy(message.msg+len, groups[j]->name);
                                len = strlen(message.msg);
                                strcpy(message.msg+len, "\n");
                                len += 1;
                                if(len >= (MAX_BUFFER-(MAX_LOGIN_LENGTH+1))) { // make sure size of message is proper
                                    message.start = 0;
                                    result = msgsnd(messageQueues[i], &message, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);
                                        
                                    if(result == -1) {
                                        printLogTime();
                                        printf("Couldn't send list of groups\n");
                                        break;
                                    }
                                        
                                    message.start = 0;
                                    len = 0;
                                }
                                
                            }
                        }
                        
                    } else if(cmdmsg.arguments[0][0] == '\0' || strcmp(cmdmsg.arguments[0], "all") == 0) {
                        find  = 1;
                        for(int j=0; j<(*loadedUsers); ++j) {
                            if(users[j]->logged || all) {
                                strcpy(message.msg+len, users[j]->login);
                                len += strlen(users[j]->login);
                                strcpy(message.msg+len, "\n");
                                len += 1;
                                if(len >= (MAX_BUFFER-(MAX_LOGIN_LENGTH+1))) { // make sure size of message is proper
                                    message.start = 0;
                                    result = msgsnd(messageQueues[i], &message, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);
                                        
                                    if(result == -1) {
                                        printLogTime();
                                        printf("Couldn't send list of users\n");
                                        break;
                                    }
                                        
                                    message.start = 0;
                                    len = 0;
                                }
                            }
                        }
                    } else {
                        find = 0;
                            
                        for(int j=0; j<MAX_GROUPS; ++j) {
                            if(strcmp(cmdmsg.arguments[0], groups[j]->name) == 0) {
                                find = 1;
                                for(int k=0; k<groups[j]->groupSize; ++k) {
                                    int uid = groups[j]->userId[k];
                                    strcpy(message.msg+len, users[uid]->login);
                                    len += strlen(users[j]->login);
                                    strcpy(message.msg+len, "\n");
                                    len += 1;
                                    if(len >= (MAX_BUFFER-(MAX_LOGIN_LENGTH+1))) { // make sure size of message is proper
                                        
                                        result = msgsnd(messageQueues[i], &message, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);
                                        
                                        if(result == -1) {
                                            printLogTime();
                                            printf("Couldn't send list of users\n");
                                            break;
                                        }
                                        
                                        message.start = 0;
                                        len = 0;
                                    }
                                }
                            }
                        }
                    }
                        
                        
                    message.end = 1;
                    if(find) {
                        strcpy(message.msg+len, "---\n");
                            
                        printLogTime();
                        if(strcmp(cmdmsg.command, "group") == 0) {
                            printf("Passed list of groups to %s\n", users[i]->login);
                        } else if(cmdmsg.arguments[0][0] == '\0' || strcmp(cmdmsg.arguments[0], "all") == 0) {
                            printf("Passed list of users to %s\n", users[i]->login);
                        } else {
                            printf("Passed list of users in %s to %s\n", cmdmsg.arguments[0], users[i]->login);
                        }
                    } else {
                        strcpy(message.msg, "Groupd doens't exist! Type /group for list of available groups\n");
                            
                        printLogTime();
                        printf("User %s requestet list of not valid group: %s\n", users[i]->login, cmdmsg.arguments[0]);
                    }
                    
                    msgsnd(messageQueues[i], &message, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);

                } else if(strcmp(cmdmsg.command, "group") == 0) {
/* group */
                    if(strcmp(cmdmsg.arguments[0], "join") == 0) {
                        /* join group */
                        int find = 0;
                        for(int j=0; j<MAX_GROUPS; ++j) {
                            if(strcmp(cmdmsg.arguments[1], groups[j]->name) == 0) {
                                find = 1;
                                if(groups[j]->userId[i] == 1) { // already in group 
                                    strcpy(message.msg, "You're already in requested group! Join chat by /msg group ");
                                    
                                    printLogTime();
                                    printf("User %s requested join group %s, but already joined\n", users[i]->login, groups[j]->name);
                                } else {
                                    groups[j]->userId[i] = 1;
                                    strcpy(message.msg, "Added to group! Now you can join chat with /msg group ");
                                    
                                    printLogTime();
                                    printf("User %s added to group %s\n", users[i]->login, groups[j]->name);
                                }
                                strcpy(message.msg+strlen(message.msg), cmdmsg.arguments[1]);
                                strcpy(message.msg+strlen(message.msg), "\n");
                            }
                        }
                        
                        if(!find) {
                            printLogTime();
                            printf("User %s wanted to join %s group. Group not found in system\n", users[i]->login, cmdmsg.arguments[1]);
                            
                            strcpy(message.msg, "Requested group doesn't exist.\nType /group to list available groups\n");
                        }
                        
                        msgsnd(messageQueues[i], &message, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);
                        
                    } else if(strcmp(cmdmsg.arguments[0], "leave") == 0) {
                        /* leave group */
                        int find = 0;
                        for(int j=0; j<MAX_GROUPS; ++j) {
                            if(strcmp(cmdmsg.arguments[1], groups[j]->name) == 0) {
                                find = 1;
                                if(groups[j]->userId[i] == 0) { // not in group
                                    strcpy(message.msg, "You're not a member of group ");
                                    
                                    printLogTime();
                                    printf("User %s requested leave group %s, but not a member\n", users[i]->login, groups[j]->name);
                                } else {
                                    groups[j]->userId[i] = 0;
                                    strcpy(message.msg, "Deleted from group ");
                                    
                                    printLogTime();
                                    printf("User %s deleted from group %s\n", users[i]->login, groups[j]->name);
                                }
                                strcpy(message.msg+strlen(message.msg), cmdmsg.arguments[1]);
                                strcpy(message.msg+strlen(message.msg), "\n");
                            }
                        }
                        
                        if(!find) {
                            printLogTime();
                            printf("User %s wanted to leave %s group. Group not found in system\n", users[i]->login, cmdmsg.arguments[1]);
                            
                            strcpy(message.msg, "Requested group doesn't exist.\nType /group to list available groups\n");
                        }
                        
                        msgsnd(messageQueues[i], &message, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);
                    } else {
                        strcpy(message.msg, "Wrong command format, check /help for more info\n");
                        msgsnd(messageQueues[i], &message, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);
                    }
                
                } else if(strcmp(cmdmsg.command, "block") == 0 || strcmp(cmdmsg.command, "unblock") == 0) {
/* block */         

                    int block = 0;
                    if(strcmp(cmdmsg.command, "block") == 0) 
                        block = 1;
                    find  = 0;
                    if(strcmp(cmdmsg.arguments[0], "group") == 0) {
                        for(int j=0; j<MAX_GROUPS; ++j) {
                            
                            if(strcmp(cmdmsg.arguments[1], groups[j]->name) == 0) {
                                find = 1;
                                users[i]->blockedGroups[j] = block;
                                if(block)
                                    strcpy(message.msg, "Group blocked.\n");
                                else
                                    strcpy(message.msg, "Group unblocked.\n");
                                
                                printLogTime();
                                if(block)
                                    printf("User %s blocked messages from group %s\n", users[i]->login, groups[j]->name);
                                else
                                    printf("User %s unblocked messages from group %s\n", users[i]->login, groups[j]->name);
                                    
                                break;
                            }
                        }
                        
                        if(!find) {
                            printLogTime();
                            if(block)
                                printf("User %s tried to block group %s, but register not found\n", users[i]->login, cmdmsg.arguments[1]);
                            else
                                printf("User %s tried to unblock group %s, but register not found\n", users[i]->login, cmdmsg.arguments[1]);
                        }
                        
                        msgsnd(messageQueues[i], &message, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);
                    } else {
                        for(int j=0; j<(*loadedUsers); ++j) {
                            
                            if(strcmp(cmdmsg.arguments[0], users[j]->login) == 0) {
                                find = 1;
                                users[i]->blockedUsers[j] = block;
                                
                                if(block)
                                    strcpy(message.msg, "User blocked.\n");
                                else
                                    strcpy(message.msg, "User unblocked.\n");
                                
                                printLogTime();
                                
                                if(block)
                                    printf("User %s blocked messages from user %s\n", users[i]->login, users[j]->login);
                                else
                                    printf("User %s unblocked messages from user %s\n", users[i]->login, users[j]->login);
                                
                                break;
                            }
                        }
                        
                        if(!find) {
                            printLogTime();
                            if(block)
                                printf("User %s tried to block user %s, but register not found\n", users[i]->login, cmdmsg.arguments[0]);
                            else
                                printf("User %s tried to unblock user %s, but register not found\n", users[i]->login, cmdmsg.arguments[0]);
                        }
                        
                        msgsnd(messageQueues[i], &message, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);
                    }
                    

                
                }

            }
        } 
}


int sendMessage(int queue, int port, int priority, char * message) {
    struct msgbuf answer;
    
    // TODO asnwers longer than 512 characters
    answer.start = 1;
    answer.end = 1;
    answer.priority = priority;
    answer.mtype = port;
    answer.msgGroup = -1;
    strncpy(answer.msg, message, MAX_BUFFER);
    answer.msg[MAX_BUFFER] = '\0';
    
    return msgsnd(queue, &answer, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);
    
}

void freeMemory(struct user **users, struct group **groups) {
        for(int i=0; i<MAX_USERS; ++i)
        free(users[i]);
    free(users);
    
    for(int i=0; i<MAX_GROUPS; ++i)
        free(groups[i]);
    free(groups);
    
}