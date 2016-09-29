noinst_PROGRAMS = anyconnect

noinst_LTLIBRARIES = libanyconnect.la

AM_CFLAGS = -I$(srcdir)/../include -I$(srcdir)/../ShadowVPN/src

libanyconnect_la_SOURCES = callbacks.c \
fsm_event.c \
iceagent.c \
icevpn_common.c \
initiator_api.c \
mqtt_topic.c \
msg_encode.c \
responder_api.c \
transport_ice.c \
common_api.c \
global.c \
ice_strans.c \
icevpn_fsm.c \
mqtt_callbacks.c \
msg_decode.c  \
read_config.c  \
stateMachine.c \
util.c

if WIN32
libanyconnect_la_SOURCES += win32.c win32.h
endif

libanyconnect_la_LIBADD = ../json-c/.libs/libjson-c.la \
../ShadowVPN/src/.libs/libshadowvpn.la 

libanyconnect_la_LDFLAGS = -L../paho.mqtt.c/build/output/ -lpaho-mqtt3as
libanyconnect_la_LDFLAGS += -L../pjproject-2.5.5/pjlib/lib/ -lpj 
libanyconnect_la_LDFLAGS += -L../pjproject-2.5.5/pjlib-util/lib/ -lpjlib-util
libanyconnect_la_LDFLAGS += -L../pjproject-2.5.5/pjmedia/lib/ -lpjsdp
libanyconnect_la_LDFLAGS += -L../pjproject-2.5.5/pjnath/lib/ -lpjnath

anyconnect_SOURCES = main.c

anyconnect_LDADD = libanyconnect.la
