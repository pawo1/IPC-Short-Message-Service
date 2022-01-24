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
    
    char buf[maxBuffer];
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
                strncpy(userRegister->login, buf, maxLoginLength);

            step++;
            l=0;
        } else {
            buf[l] = character;
            l++;
        }
        
        read(fd, &character, 1);
    }

    buf[l] = '\0';
    strncpy(userRegister->password, buf, maxPasswordLength);
    
    userRegister->logged = 0;
    
    return 0;
}

int loadGroup(int fd, struct group *groupRegister) {
    char buf[maxBuffer];
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
    strncpy(groupRegister->name, buf, maxGroupName);
    groupRegister->name[maxGroupName] = '\0'; // for trimed names, because strncpy doesn't add it itself
    
    /*for(int i=0; i<maxUsers; ++i) {
        groupRegister->userId[i] = -1;
    }*/
    groupRegister->groupSize = 0;
    
    return 0;
}

int addGroup(int fd, struct user **users, struct group **groups) {
    
    char userName[maxLoginLength+1];
    char groupName[maxGroupName+1];
    int groupFind = 0;
    int userFind = 0;
    
    char buf[maxBuffer];
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
                strncpy(userName, buf, maxLoginLength);
                userName[maxLoginLength] = '\0';
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
    strncpy(groupName, buf, maxGroupName);
    groupName[maxGroupName] = '\0';
    
    for(int i=0; i<maxGroups; ++i) {
        
        if(strcmp(groups[i]->name, groupName) == 0) {
            groupFind = 1;
            for(int j=0; j<maxUsers; ++j) {
                if(strcmp(users[j]->login, userName) == 0) {
                    userFind = 1;
                    int index = groups[i]->groupSize;
                    groups[i]->userId[index] = j;
                    groups[i]->groupSize++;
                }
            }
        }
        
    }
    
    if(!userFind) return -1;
    if(!groupFind) return 1;
    
    return 0;
}