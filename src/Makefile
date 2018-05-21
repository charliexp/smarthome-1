SHELL = /bin/sh
.PHONY: clean

srcdir = utils device log cjson mqttserver
blddir = ../lib
bindir = ../output

SOURCE_FILES = main.c $(wildcard cjson/*.c) $(wildcard device/*.c) $(wildcard log/*.c) $(wildcard utils/*.c) $(wildcard mqttserver/*.c) 
MQTT_FILES = $(filter-out $(srcdir)/MQTTClient.c $(srcdir)/SSLSocket.c, $(wildcard paho/*.c))

MQTT_HEADERS = paho/*.h
HEADERS = $(srcdir)/*.h

MQTTLIB = paho-mqtt3a

CC = mipsel-openwrt-linux-gcc
CFLAGS += -I /home/ubuntu/work/code/openssl-1.1.0h/install/include -ldl
LDFLAGS += -L /home/ubuntu/work/code/openssl-1.1.0h/install/lib -ldl

CJSON_TARGET = ${blddir}/libcjson.so
MQTTLIB_SO = ${blddir}/lib${MQTTLIB}.so
MAIN = ${bindir}/smarthome

MQTTASYNC_INIT = MQTTAsync_init
START_GROUP = -Wl,--start-group
END_GROUP = -Wl,--end-group

CCFLAGS_SO = -g -fPIC $(CFLAGS) -Wall -fvisibility=hidden
FLAGS_EXE = $(LDFLAGS) -I paho ${START_GROUP} -lpthread -lssl -lcrypto ${END_GROUP} -L ${blddir} -D_SVID_SOURCE -lsqlite3
LDFLAGS_A = $(LDFLAGS) -shared -Wl,-init,$(MQTTASYNC_INIT) -lpthread
LDFLAGS_A += -Wl,-soname,lib${MQTTLIB}.so


all: build

build: | ${MQTTLIB_A_TARGET} ${MAIN}

clean:
	rm -rf ${blddir}/*
	rm -rf ${bindir}/*
	
${MQTTLIB_SO}: ${MQTT_FILES} ${MQTT_HEADERS}
	${CC} ${CCFLAGS_SO} -o $@ ${MQTT_FILES} ${LDFLAGS}
	
${MAIN}: ${SOURCE_FILES} ${HEADERS} ${MQTTLIB_SO} 
	${CC} -g -o $@ ${SOURCE_FILES} -l${MQTTLIB_SO} ${FLAGS_EXE} -lm -std=c11