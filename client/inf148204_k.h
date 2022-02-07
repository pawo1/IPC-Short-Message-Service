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
    int priority;
    char from[MAX_LOGIN_LENGTH];
    char msg[MAX_BUFFER];
    int to;
    int start;
    int end;
};

struct state {
    int mainMenu;
    int choosenUser;
    int choosenGroup;
    char prompt[40];
    int promptSize;
    int privateQueue;
};

struct sembuf p = { 0, -1, SEM_UNDO}; 
struct sembuf v = { 0, +1, SEM_UNDO};

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

void run_changer();

int login(int logged, int authQueue, int printSemaphore);
void logout();

void messageReceiver(struct state *status, int statusSemaphore, int printSemaphore);
void userManager(struct state *status, int statusSemaphore, int printSemaphore);
void proceedMessage(struct msgbuf message, char * prompt, int promptSize);
void printPrompt(char * prompt);

#endif