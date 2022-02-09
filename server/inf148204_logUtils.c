#include <time.h>
#include <stdio.h>
#include <unistd.h>

#include "inf148204_logUtils.h"

void printLogTime() {
    char formatted[20];
    time_t now = time(NULL);
    strftime(formatted, 20, "%Y-%m-%d %H:%M:%S", localtime(&now));
    printf("[%d][%s]\t", getpid(), formatted);
}
