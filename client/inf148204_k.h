#ifndef __inf148204_k_h
#define __inf148204_k_h

#include "inf148204_k_defines.h"

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


#endif