#include "inf148204_s.h"
#include "inf148204_f.h"


int main() {
    
    int result;
    
    struct user **users = malloc( sizeof(struct user*) * maxUsers );
    for(int i=0; i<maxUsers; ++i)
        users[i] = malloc( sizeof(struct user) );
    
    result = loadConfig("test.txt", users, maxUsers);
    
    if(result < 0) {
        printf("Server cannot load configuration...\n");
        printf("Stopping the server...\n");
        return -1;
    }
    
    printf("%s\n%s\n", users[0]->login, users[0]->password);
    
    for(int i=0; i<maxUsers; ++i)
        free(users[i]);
    free(users);
    
    return 0;
}


int loadConfig(char *filename, struct user **users, int userNum) {
    printf("Loading configuration...\n");
    
    strcpy(users[3]->login,"test");
    strcpy(users[0]->login, "ALALA");
    
    int file = open(filename, O_RDONLY);
    if(file < 0)
        return file;
    
    int i = 0;
    int result;
    char character;
    
    int bytes = read(file, &character, 1);
    while(bytes) {
        // check type of line
        switch(character) {
            case 'U':
                result = loadUser(file, users[i]);
                if(result == 0) // loading user was sucessful 
                    i++;
                break;
            case 'G':
                loadGroup(file);
                break;
            case 'A':
                addGroup(file, users);
            
        }
        
        
        break;
        
        while(character != '\n') {
            
            
        }
    }
    
    
    close(file); 
    return 0;
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
                strcpy(userRegister->login, buf);

            step++;
            l=0;
        } else {
            buf[l] = character;
            l++;
        }
        
        read(fd, &character, 1);
    }

    buf[l] = '\0';
    strcpy(userRegister->password, buf);
    
    return 0;
}

int loadGroup(int fd) {
    
    return 0;
}

int addGroup(int fd, struct user **users) {
    
    return 0;
}