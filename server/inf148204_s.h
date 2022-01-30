#ifndef __inf148204_s_h
#define __inf148204_s_h

#include "inf148204_defines.h"
#include "inf148204_logUtils.h"
#include "inf148204_fileOperations.h"

struct authbuf {
    long mtype;
    char login[MAX_LOGIN_LENGTH];
    char password[MAX_PASSWORD_LENGTH];
    int client_queue;
};


struct msgbuf {
    long mtype;
    int from;
    int to;
    int id_group;
    char msg[MAX_BUFFER];
    int end;
};

void freeMemory(struct user **users, struct group **groups);

#endif