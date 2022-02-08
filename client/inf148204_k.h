#ifndef __inf148204_k_h
#define __inf148204_k_h

#include "inf148204_k_defines.h"

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

struct state {
    int logged;
    char name[MAX_LOGIN_LENGTH+1];
    int choosenUser;
    int choosenGroup;
    char prompt[MAX_LOGIN_LENGTH+1];
    int promptSize;
    int privateQueue;
    int publicQueue;
};

struct sembuf p = { 0, -1, SEM_UNDO}; 
struct sembuf v = { 0, +1, SEM_UNDO};

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

void run_changer();

int login(char *name, int authQueue, int printSemaphore);
void logout();

void messageReceiver(struct state *status, int statusSemaphore, int printSemaphore);
void userManager(struct state *status, int statusSemaphore, int printSemaphore);
void proceedMessage(struct msgbuf message,char *name, char * prompt, int promptSize);

void printPrompt(char * name, char * prompt);
void printSystemInfo(char * message, int sem);
#endif