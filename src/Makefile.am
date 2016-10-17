bin_PROGRAMS = anyconnect
noinst_LTLIBRARIES = libanyconnect.la

AM_CFLAGS = -I$(srcdir)/../include \
-I$(srcdir)/../paho.mqtt.c/src/ \
-I$(srcdir)/../pjproject/pjlib/include/ \
-I$(srcdir)/../pjproject/pjlib-util/include/ \
-I$(srcdir)/../pjproject/pjmedia/include/ \
-I$(srcdir)/../pjproject/pjnath/include/ \
-I$(srcdir)/../ShadowVPN/src/ \
-DPJ_IS_BIG_ENDIAN=0 \
-DPJ_IS_LITTLE_ENDIAN=1

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

libanyconnect_la_LDFLAGS = -L../paho.mqtt.c/build/output/ -lpaho-mqtt3as $(OPENSSL_LIBS)
libanyconnect_la_LDFLAGS += -L../pjproject/pjnath/lib/ -lpjnath-$(host)
libanyconnect_la_LDFLAGS += -L../pjproject/pjmedia/lib/ -lpjsdp-$(host)
libanyconnect_la_LDFLAGS += -L../pjproject/pjlib-util/lib/ -lpjlib-util-$(host)
libanyconnect_la_LDFLAGS += -L../pjproject/pjlib/lib/ -lpj-$(host)
libanyconnect_la_LDFLAGS += -lm -lpthread -lrt

anyconnect_SOURCES = main.c

anyconnect_LDADD = libanyconnect.la
