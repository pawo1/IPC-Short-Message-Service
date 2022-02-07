#ifndef __inf148204_k_defines_h
#define __inf148204_k_defines_h

#define MAX_LOGIN_LENGTH 32
#define MAX_PASSWORD_LENGTH 32
#define MAX_GROUP_NAME 32

#define MAX_USERS 32
#define MAX_GROUPS 10 

#define PRIORITY_PORT (MAX_USERS+MAX_GROUPS+2)
#define COMMAND_PORT PRIORITY_PORT+1
#define MESSAGE_PORT COMMAND_PORT+1

#define MAX_COMMAND 16
#define MAX_BUFFER 512

#endif