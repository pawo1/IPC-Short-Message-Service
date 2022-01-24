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
#include <time.h>

#include "inf148204_s.h"
#include "inf148204_defines.h"
#include "inf148204_logUtils.h"
#include "inf148204_fileOperations.h"


int main(int argc, char *argv[]) {
    
    int result;
    
    struct user **users = malloc( sizeof(struct user*) * maxUsers );
    for(int i=0; i<maxUsers; ++i)
        users[i] = malloc( sizeof(struct user) );
        
    struct group **groups = malloc( sizeof(struct group*) * maxGroups);
    for(int i=0; i<maxGroups; ++i)
        groups[i] = malloc( sizeof(struct group) );
    
    result = loadConfig("test.txt", users, maxUsers, groups, maxGroups);
    
    if(result < 0) {
        printLogTime();
        printf("Server cannot load configuration...\n");
        printLogTime();
        printf("Stopping the server...\n");
        return -1;
    }
    
    printLogTime();
    printf("Server loaded %d users\n", result);
    
    printf("%s\n%s\n", users[2]->login, users[2]->password);
    
    for(int i=0; i<maxUsers; ++i)
        free(users[i]);
    free(users);
    
    for(int i=0; i<maxGroups; ++i)
        free(groups[i]);
    free(groups);
    
    return 0;
}

