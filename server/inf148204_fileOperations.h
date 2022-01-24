// Module for file operations related to loading configuration

#ifndef __inf148204_fileOperations_h
#define __inf148204_fileOperations_h

struct user {
    char login[maxLoginLength+1];
    char password[maxPasswordLength+1];
    int logged;
};

struct group {
    char name[maxGroupName+1];
    int userId[maxUsers];
    int groupSize;
};

int loadConfig(char *filename, struct user **users, int userNum, struct group **groups, int groupNum);
int loadUser(int fd, struct user *userRegister);
int loadGroup(int fd, struct group *groupRegister);
int addGroup(int fd, struct user **users, struct group **groups);

#endif