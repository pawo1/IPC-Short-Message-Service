#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "inf148204_defines.h"
#include "inf148204_logUtils.h"
#include "inf148204_fileOperations.h"

int loadConfig(char *filename, struct user **users, int userNum, struct group **groups, int groupNum) {
    printLogTime();
    printf("Loading configuration...\n");
    
    int file = open(filename, O_RDONLY);
    if(file < 0)
        return file;
    
    int user_iterator = 0;
    int group_iterator = 0;
    int result;
    char character;
    
    int bytes = read(file, &character, 1);
    while(bytes) {
        // check type of line
        switch(character) {
            case 'U':
                result = loadUser(file, users[user_iterator]);
                if(result == 0) // loading user was sucessful 
                    user_iterator++;
                break;
            case 'G':
                result = loadGroup(file, groups[group_iterator]);
                if(result == 0) // loading group was sucessful
                    group_iterator++;
                break;
            case 'A':
                result = addGroup(file, users, groups);
                if(result < 0) {
                    printLogTime();
                    printf("Couldn't find user requested to add to group\n");
                }
                else if(result > 0) {
                    printLogTime();
                    printf("Couldn't find requested group\n");
                }
        }
        bytes = read(file, &character, 1);
    }
    
    
    close(file); 
    return user_iterator;
}

int loadUser(int fd, struct user *userRegister) {
    
    char buf[MAX_BUFFER];
    int l = 0;
    int step = 0;
    char character;
    
    int bytes = read(fd, &character, 1);
    while(character != '\n') {
        
        if(!bytes)
            break;
        
        if(character == ';') {
            buf[l] = '\0';
            
            // step == 0 means ';' from mode
            
            if(step == 1)
                strncpy(userRegister->login, buf, MAX_LOGIN_LENGTH);

            step++;
            l=0;
        } else {
            buf[l] = character;
            l++;
        }
        
        read(fd, &character, 1);
    }

    buf[l] = '\0';
    strncpy(userRegister->password, buf, MAX_PASSWORD_LENGTH);
    
    userRegister->logged = 0;
    
    return 0;
}

int loadGroup(int fd, struct group *groupRegister) {
    char buf[MAX_BUFFER];
    int l = 0;
    char character;
    
    int bytes = read(fd, &character, 1);
    while(character != '\n' && bytes) {
        if(character != ';') {
            buf[l] = character;
            l++;
        }
        bytes = read(fd, &character, 1);
    }
    
    buf[l] = '\0';
    strncpy(groupRegister->name, buf, MAX_GROUP_NAME);
    groupRegister->name[MAX_GROUP_NAME] = '\0'; // for trimed names, because strncpy doesn't add it itself
    
    for(int i=0; i<MAX_USERS; ++i) {
        groupRegister->userId[i] = 0;
    }
    groupRegister->groupSize = 0;
    
    return 0;
}

int addGroup(int fd, struct user **users, struct group **groups) {
    
    char userName[MAX_LOGIN_LENGTH+1];
    char groupName[MAX_GROUP_NAME+1];
    int groupFind = 0;
    int userFind = 0;
    
    char buf[MAX_BUFFER];
    int l = 0;
    int step = 0;
    char character;
    
    int bytes = read(fd, &character, 1);
    while(character != '\n') {
        
        if(!bytes)
            break;
        
        if(character == ';') {
            buf[l] = '\0';
            
            // step == 0 means ';' from mode
            
            if(step == 1) {
                strncpy(userName, buf, MAX_LOGIN_LENGTH);
                userName[MAX_LOGIN_LENGTH] = '\0';
            }
            step++;
            l=0;
        } else {
            buf[l] = character;
            l++;
        }
        
        read(fd, &character, 1);
    }

    buf[l] = '\0';
    strncpy(groupName, buf, MAX_GROUP_NAME);
    groupName[MAX_GROUP_NAME] = '\0';
    
    for(int i=0; i<MAX_GROUPS; ++i) {
        
        if(strcmp(groups[i]->name, groupName) == 0) {
            groupFind = 1;
            for(int j=0; j<MAX_USERS; ++j) {
                if(strcmp(users[j]->login, userName) == 0) {
                    userFind = 1;
 //                   int index = groups[i]->groupSize;
 //                   groups[i]->userId[index] = j;
                    groups[i]->userId[j] = 1;
                    groups[i]->groupSize++;
                }
            }
        }
        
    }
    
    if(!userFind) return -1;
    if(!groupFind) return 1;
    
    return 0;
}