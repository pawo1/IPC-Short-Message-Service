#ifndef __inf148204_s_h
#define __inf148204_s_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#define maxUsers 32
#define maxGroups 10


#define maxLoginLength 32
#define maxPasswordLength 32
#define maxGroupName 32

#define maxBuffer 512

struct user {
    char login[maxLoginLength];
    char password[maxPasswordLength];
    char groups[maxGroups][maxGroupName];
    int groupCounter;
    int logged;
};

int loadConfig(char *filename, struct user **users, int userNum);
int loadUser(int fd, struct user *userRegister);
int loadGroup(int fd);
int addGroup(int fd, struct user **users);

#endif