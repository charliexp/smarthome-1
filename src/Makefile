SHELL = /bin/sh
.PHONY: clean

srcdir = paho
blddir = ../lib
bindir = ../output
cjsonsrcdir = cjson

SOURCE_FILES = $(wildcard $(srcdir)/*.c)
SOURCE_FILES_A = $(filter-out $(srcdir)/MQTTClient.c $(srcdir)/MQTTVersion.c $(srcdir)/SSLSocket.c, $(SOURCE_FILES))
CJSON_FILES = $(wildcard $(cjsonsrcdir)/*.c)

HEADERS = $(srcdir)/*.h
HEADERS_C = $(filter-out $(srcdir)/MQTTAsync.h, $(HEADERS))
HEADERS_A = $(HEADERS)

MQTTLIB_A = paho-mqtt3a

CC = mipsel-openwrt-linux-gcc
CFLAGS += -I /home/ubuntu/work/code/openssl-1.1.0h/install/include -ldl
LDFLAGS += -L /home/ubuntu/work/code/openssl-1.1.0h/install/lib -ldl

CJSON_TARGET = ${blddir}/libcjson.so
MQTTLIB_A_TARGET = ${blddir}/lib${MQTTLIB_A}.so
MQTTVERSION_TARGET = ${bindir}/MQTTVersion
MAIN = ${bindir}/smarthome
  
MQTTCLIENT_INIT = MQTTClient_init
MQTTASYNC_INIT = MQTTAsync_init
START_GROUP = -Wl,--start-group
END_GROUP = -Wl,--end-group

CCFLAGS_SO = -g -fPIC $(CFLAGS) -Wall -fvisibility=hidden
FLAGS_EXE = $(LDFLAGS) -I ${srcdir} -lpthread -L ${blddir}
FLAGS_EXES = $(LDFLAGS) -I ${srcdir} -I ${cjsonsrcdir} ${START_GROUP} -lpthread -lssl -lcrypto ${END_GROUP} -L ${blddir} -I tools

LDFLAGS_A = $(LDFLAGS) -shared -Wl,-init,$(MQTTASYNC_INIT) -lpthread

EXTRA_LIB = -ldl

LDFLAGS_A += -Wl,-soname,lib${MQTTLIB_A}.so


all: build

build: | ${MQTTLIB_A_TARGET} ${MQTTVERSION_TARGET} ${CJSON_TARGET} ${MAIN}

clean:
	rm -rf ${blddir}/*
	rm -rf ${bindir}/*
	
${CJSON_TARGET}: ${CJSON_FILES}
	${CC} -shared -fPIC -g -o $@ ${CJSON_FILES} -lm
	
${MQTTLIB_A_TARGET}: ${SOURCE_FILES_A} ${HEADERS_A}
	${CC} ${CCFLAGS_SO} -o $@ ${SOURCE_FILES_A} ${LDFLAGS_A}

${MQTTVERSION_TARGET}: $(srcdir)/MQTTVersion.c $(srcdir)/MQTTAsync.h ${MQTTLIB_A_TARGET} $(MQTTLIB_CS_TARGET)
	${CC} ${FLAGS_EXE} -o $@ -l${MQTTLIB_A} $(srcdir)/MQTTVersion.c -ldl
	
${MAIN}: main.c tools/tools.c ${MQTTLIB_A_TARGET} ${MQTTVERSION_TARGET} ${CJSON_TARGET}
	${CC} -g -o $@ $<  tools/tools.c -l${MQTTLIB_A} -lcjson ${FLAGS_EXES} -lm