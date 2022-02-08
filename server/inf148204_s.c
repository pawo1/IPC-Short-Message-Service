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
    
    
    printLogTime();
    printf("Server joining run loop. Ctrl+C to stop work\n");
    
    while(run) {
        proceedAuth(users, loadedUsers, authQueue, messageQueues);
        proceedMessages(users, loadedUsers, groups, messageQueues);
        proceedCommands(users, &loadedUsers, groups, messageQueues);
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
                            
                            printLogTime();
                            printf("User %s logged in\n", users[i]->login);
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

void proceedMessages(struct user **users, int loadedUsers, struct group **groups, int *messageQueues) {

    struct msgbuf message;
    int result, uid, queue;
    int index;

    for(int i=0; i<loadedUsers; ++i) {
        if(msgrcv(messageQueues[i], &message, sizeof(struct msgbuf)-sizeof(long), MESSAGE_PORT, IPC_NOWAIT) > 0) {
            if(message.msgGroup) {
                index = message.to - GROUP_OFFSET;
                //int size = groups[index]->groupSize;
                //for(int j=0; j<size; ++j) {
                for(int j=0; j<MAX_USERS; ++j) {
                    if(groups[index]->userId[j] == 1)
                        uid = j; //groups[index]->userId[j];
                    else
                        continue;
                    
                    if(uid != i) { // prevent sending messages to itself
                        queue = messageQueues[uid];
                        message.mtype = (message.priority ? PRIORITY_PORT : (index + GROUP_OFFSET)); // information who sent message
                        result = msgsnd(queue, &message, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);
                        if(result == -1) {
                            if(errno == EAGAIN) {
                                sendMessage(messageQueues[i], PRIORITY_PORT, 0, "Couldn't deliver message, queue is full\n");
                                
                                printLogTime();
                                printf("User %s has full queue, message lost\n", users[i]->login);
                                
                            } else {
                                sendMessage(messageQueues[i], PRIORITY_PORT, 0, "Error during sending message\n");
                                
                                printLogTime();
                                printf("Error accessing %s queue\n", users[i]->login);
                            }
                        } else {
                            sendMessage(messageQueues[i], PRIORITY_PORT, 0, "Message delivered\n");
                            
                            printLogTime();
                            printf("Sent Message from %s to %s as group %s message\n", users[i]->login, users[uid]->login, groups[index]->name);
                        }
                    }
                }
            } else {
                uid = message.to-USER_OFFSET;
                queue = messageQueues[uid];
                message.mtype = (message.priority ? PRIORITY_PORT : i+USER_OFFSET); // information who sent message
                result = msgsnd(queue, &message, sizeof(struct msgbuf)-sizeof(long), IPC_NOWAIT);
                if(result == -1) {
                    if(errno == EAGAIN) {
                        sendMessage(messageQueues[i], PRIORITY_PORT, 0, "Couldn't deliver message, queue is full\n");
                        
                        printLogTime();
                        printf("User %s has full queue, message lost\n", users[i]->login);
                    } else {
                        sendMessage(messageQueues[i], PRIORITY_PORT, 0, "Error during sending message\n");
                                
                        printLogTime();
                        printf("Error accessing %s queue\n", users[i]->login);
                    }
                } else {
                    sendMessage(messageQueues[i], PRIORITY_PORT, 0, "Message delivered\n");
                            
                    printLogTime();
                    printf("Sent Message from %s to %s\n", users[i]->login, users[uid]->login);
                }
            }
                
        }
    }
}

void proceedCommands(struct user **users, int *loadedUsers, struct group **groups, int *messageQueues) {
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
                
                } else if(strcmp(cmdmsg.command, "block") == 0) {
/* block */
                } else if(strcmp(cmdmsg.command, "unblock") == 0) {
/* unblock */
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