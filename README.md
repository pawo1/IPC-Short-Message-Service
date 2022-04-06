# IPC-Short-Message-Service

Client and Server of simple chat made in C using IPC libraries.
Main aim of this project was dealing with multi-threading and memory managment.


## List of available commands:

 - /help - print this help
 - /logout - logout from current session
 - /login - login to server (called automatically after logout)
 - /msg *user_name* - join chat with specified user
 - /msg group *group_name* - join chat with specified group
 - /menu - back to menu (in menu you receive only priority messages)
 - /p *message* - send priority message. Priority messages are visible even if chat with you is not open
 - /list - list of currently logged users
 - /list all - list all users
 - /list *group_name* - list users added to specified group
 - /group - list of available groups
 - /group join *group_name* - join specified group
 - /group leave *group_name* - leave specified group
 - /block *user_name* - block messages from user
 - /block group *group_name* - block messages from group
 - /unblock *user_name* - unblock messages from user
 - /unblock group *group_name* - unblock messages grom group
 - /exit - logout from session and exit program
