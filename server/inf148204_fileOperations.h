// Module for file operations related to loading configuration

#ifndef __inf148204_fileOperations_h
#define __inf148204_fileOperations_h

struct user {
    char login[MAX_LOGIN_LENGTH+1];
    char password[MAX_PASSWORD_LENGTH+1];
    int logged;
};

struct group {
    char name[MAX_GROUP_NAME+1];
    int userId[MAX_USERS];
    int groupSize;
};

int loadConfig(char *filename, struct user **users, int userNum, struct group **groups, int groupNum);
int loadUser(int fd, struct user *userRegister);
int loadGroup(int fd, struct group *groupRegister);
int addGroup(int fd, struct user **users, struct group **groups);

#endif