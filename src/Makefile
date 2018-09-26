SHELL = /bin/sh
.PHONY: clean
 
srcdir = paho
blddir = ../lib
bindir = ../output
 
SOURCE_FILES = $(wildcard *.c) $(wildcard cjson/*.c) $(wildcard device/*.c) $(wildcard log/*.c) $(wildcard utils/*.c) $(wildcard mqttserver/*.c)
SOURCE_FILES += $(wildcard test/*.c)
MQTT_FILES = $(filter-out $(srcdir)/MQTTClient.c $(srcdir)/MQTTVersion.c $(srcdir)/SSLSocket.c, $(wildcard paho/*.c))
 
HEADERS = $(srcdir)/*.h
MAINHEADERS = utils/*.h log/*.h device/*.h mqttserver/*.h cjson/*.h
 
MQTTLIB_A = paho-mqtt3a
 
CC = mipsel-openwrt-linux-gcc
CFLAGS += -I /home/ubuntu/work/code/openssl-1.1.0h/install/include -ldl
LDFLAGS += -L /home/ubuntu/work/code/openssl-1.1.0h/install/lib -ldl
 
CJSON_TARGET = ${blddir}/libcjson.so
MQTTLIB_A_TARGET = ${blddir}/lib${MQTTLIB_A}.so
MAIN = ${bindir}/smarthome

MQTTASYNC_INIT = MQTTAsync_init
START_GROUP = -Wl,--start-group
END_GROUP = -Wl,--end-group
 
CCFLAGS_SO = -g -fPIC $(CFLAGS) -Wall -fvisibility=hidden
FLAGS_EXE = $(LDFLAGS) -I ${srcdir} -lpthread -L ${blddir}
FLAGS_EXES = $(LDFLAGS) -I ${srcdir} ${START_GROUP} -lpthread -lssl -lcrypto -lcurl ${END_GROUP} -L ${blddir} -D_SVID_SOURCE -lsqlite3
LDFLAGS_A = $(LDFLAGS) -shared -Wl,-init,$(MQTTASYNC_INIT) -lpthread
LDFLAGS_A += -Wl,-soname,lib${MQTTLIB_A}.so
 
all: build
 
build: | ${MQTTLIB_A_TARGET} ${MAIN}
 
clean:
	rm -rf ${blddir}/*
	rm -rf ${bindir}/*
 	
${MQTTLIB_A_TARGET}: ${MQTT_FILES} ${HEADERS}
	${CC} ${CCFLAGS_SO} -o $@ ${MQTT_FILES} ${LDFLAGS_A}
 	
${MAIN}: ${SOURCE_FILES} $(MAINHEADERS) ${MQTTLIB_A_TARGET}
	${CC} -o $@ ${SOURCE_FILES} -l${MQTTLIB_A} ${FLAGS_EXES} -lm -std=c11