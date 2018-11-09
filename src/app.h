#ifndef _APP_H_
#define _APP_H_

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <sqlite3.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#include "utils/utils.h"
#include "utils/mytime.h"
#include "device/device.h"
#include "cjson/cJSON.h"
#include "log/log.h"

#endif
