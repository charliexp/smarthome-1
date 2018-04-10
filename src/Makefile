SHELL = /bin/sh
.PHONY: clean

srcdir = paho
blddir = ../lib
bindir = ../output
cjsonsrcdir = cjson

SOURCE_FILES = $(wildcard $(srcdir)/*.c)
SOURCE_FILES_C = $(filter-out $(srcdir)/MQTTAsync.c $(srcdir)/MQTTVersion.c $(srcdir)/SSLSocket.c, $(SOURCE_FILES))
SOURCE_FILES_CS = $(filter-out $(srcdir)/MQTTAsync.c $(srcdir)/MQTTVersion.c, $(SOURCE_FILES))
SOURCE_FILES_A = $(filter-out $(srcdir)/MQTTClient.c $(srcdir)/MQTTVersion.c $(srcdir)/SSLSocket.c, $(SOURCE_FILES))
SOURCE_FILES_AS = $(filter-out $(srcdir)/MQTTClient.c $(srcdir)/MQTTVersion.c, $(SOURCE_FILES))
CJSON_FILES = $(wildcard $(cjsonsrcdir)/*.c)

HEADERS = $(srcdir)/*.h
HEADERS_C = $(filter-out $(srcdir)/MQTTAsync.h, $(HEADERS))
HEADERS_A = $(HEADERS)

# The names of the four different libraries to be built
MQTTLIB_C = paho-mqtt3c
MQTTLIB_CS = paho-mqtt3cs
MQTTLIB_A = paho-mqtt3a
MQTTLIB_AS = paho-mqtt3as

CC = mipsel-openwrt-linux-gcc
CFLAGS += -I /home/ubuntu/work/code/openssl-1.1.0h/install/include -ldl
LDFLAGS += -L /home/ubuntu/work/code/openssl-1.1.0h/install/lib -ldl

CJSON_TARGET = ${blddir}/libcjson.so
MQTTLIB_C_TARGET = ${blddir}/lib${MQTTLIB_C}.so
MQTTLIB_CS_TARGET = ${blddir}/lib${MQTTLIB_CS}.so
MQTTLIB_A_TARGET = ${blddir}/lib${MQTTLIB_A}.so
MQTTLIB_AS_TARGET = ${blddir}/lib${MQTTLIB_AS}.so
MQTTVERSION_TARGET = ${bindir}/MQTTVersion
MAIN = ${bindir}/smarthome
  
MQTTCLIENT_INIT = MQTTClient_init
MQTTASYNC_INIT = MQTTAsync_init
START_GROUP = -Wl,--start-group
END_GROUP = -Wl,--end-group
  

CCFLAGS_SO = -g -fPIC $(CFLAGS) -Wall -fvisibility=hidden
FLAGS_EXE = $(LDFLAGS) -I ${srcdir} -lpthread -L ${blddir}
FLAGS_EXES = $(LDFLAGS) -I ${srcdir} -I ${cjsonsrcdir} ${START_GROUP} -lpthread -lssl -lcrypto ${END_GROUP} -L ${blddir}

LDFLAGS_C = $(LDFLAGS) -shared -Wl,-init,$(MQTTCLIENT_INIT) -lpthread
LDFLAGS_CS = $(LDFLAGS) -shared $(START_GROUP) -lpthread $(EXTRA_LIB) -lssl -lcrypto $(END_GROUP) -Wl,-init,$(MQTTCLIENT_INIT)
LDFLAGS_A = $(LDFLAGS) -shared -Wl,-init,$(MQTTASYNC_INIT) -lpthread
LDFLAGS_AS = $(LDFLAGS) -shared $(START_GROUP) -lpthread $(EXTRA_LIB) -lssl -lcrypto $(END_GROUP) -Wl,-init,$(MQTTASYNC_INIT)

EXTRA_LIB = -ldl

LDFLAGS_C += -Wl,-soname,lib$(MQTTLIB_C).so
LDFLAGS_CS += -Wl,-soname,lib$(MQTTLIB_CS).so -Wl,-no-whole-archive
LDFLAGS_A += -Wl,-soname,lib${MQTTLIB_A}.so
LDFLAGS_AS += -Wl,-soname,lib${MQTTLIB_AS}.so -Wl,-no-whole-archive


all: build

build: | ${MQTTLIB_C_TARGET} ${MQTTLIB_CS_TARGET} ${MQTTLIB_A_TARGET} ${MQTTLIB_AS_TARGET} ${MQTTVERSION_TARGET} ${CJSON_TARGET} ${MAIN}

clean:
	rm -rf ${blddir}/*
	rm -rf ${bindir}/*
	
${CJSON_TARGET}: ${CJSON_FILES}
	${CC} -shared -fPIC -g -o $@ ${CJSON_FILES} -lm
${MQTTLIB_C_TARGET}: ${SOURCE_FILES_C} ${HEADERS_C}
	${CC} ${CCFLAGS_SO} -o $@ ${SOURCE_FILES_C} ${LDFLAGS_C}

${MQTTLIB_CS_TARGET}: ${SOURCE_FILES_CS} ${HEADERS_C}
	${CC} ${CCFLAGS_SO} -o $@ ${SOURCE_FILES_CS} -DOPENSSL ${LDFLAGS_CS}
	
${MQTTLIB_A_TARGET}: ${SOURCE_FILES_A} ${HEADERS_A}
	${CC} ${CCFLAGS_SO} -o $@ ${SOURCE_FILES_A} ${LDFLAGS_A}

${MQTTLIB_AS_TARGET}: ${SOURCE_FILES_AS} ${HEADERS_A}
	${CC} ${CCFLAGS_SO} -o $@ ${SOURCE_FILES_AS} -DOPENSSL ${LDFLAGS_AS}

${MQTTVERSION_TARGET}: $(srcdir)/MQTTVersion.c $(srcdir)/MQTTAsync.h ${MQTTLIB_A_TARGET} $(MQTTLIB_CS_TARGET)
	${CC} ${FLAGS_EXE} -o $@ -l${MQTTLIB_A} $(srcdir)/MQTTVersion.c -ldl
	
${MAIN}: main.c ${MQTTLIB_C_TARGET} ${MQTTLIB_CS_TARGET} ${MQTTLIB_A_TARGET} ${MQTTLIB_AS_TARGET} ${MQTTVERSION_TARGET} ${CJSON_TARGET}
	${CC} -g -o $@ $< -l${MQTTLIB_C} -lcjson ${FLAGS_EXES} -lm