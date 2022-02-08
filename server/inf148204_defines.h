#ifndef __inf148204_defines_h
#define __inf148204_defines_h


#define MAX_USERS 32
#define MAX_GROUPS 10

#define USER_OFFSET 1
#define GROUP_OFFSET (MAX_USERS+USER_OFFSET)

#define PRIORITY_PORT (MAX_USERS+MAX_GROUPS+2)
#define COMMAND_PORT (PRIORITY_PORT+1)
#define ANSWER_PORT (COMMAND_PORT +1)
#define MESSAGE_PORT (COMMAND_PORT+2)


#define MAX_LOGIN_LENGTH 32
#define MAX_PASSWORD_LENGTH 32
#define MAX_GROUP_NAME 32

#define MAX_COMMAND 32
#define MAX_ARGS 2

#define MAX_BUFFER 512

#define OFFSET 10

#define MAX_TRIES 3

#endif