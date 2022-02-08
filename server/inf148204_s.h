#ifndef __inf148204_s_h
#define __inf148204_s_h

#include "inf148204_defines.h"
#include "inf148204_logUtils.h"
#include "inf148204_fileOperations.h"

struct authbuf {
    long mtype;
    char login[MAX_LOGIN_LENGTH+1];
    char password[MAX_PASSWORD_LENGTH+1];
    int client_queue;
};


struct msgbuf {
    long mtype;
    int priority;
    char from[MAX_LOGIN_LENGTH+1];
    char msg[MAX_BUFFER+1];
    int msgGroup;
    int to;
    int start;
    int end;
};

struct cmdbuf {
  long mtype;
  char command[MAX_COMMAND];
  char arguments[MAX_ARGS][MAX_COMMAND];
  int result;
};

void proceedAuth(struct user **users, int loadedUsers, int authQueue, int *messageQueues);
void proceedMessages(struct user **users, int loadedUsers, struct group **groups, int *messageQueues);
void proceedCommands(struct user **users, int *loadedUsers, struct group **groups, int *messageQueues);

int sendMessage(int queue, int port, int priority, char * message);

void freeMemory(struct user **users, struct group **groups);

#endif