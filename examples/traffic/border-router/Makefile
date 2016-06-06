CONTIKI_PROJECT = border-router
all: $(CONTIKI_PROJECT) 

TARGET ?= jn516x

CONTIKI=../../..

CONTIKI_WITH_IPV6 = 1

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
PROJECTDIRS += .. 
PROJECT_SOURCEFILES += slip-bridge.c slip.c
APPS += traffic

CFLAGS += -DUIP_CONF_TCP=0

ifeq ($(PREFIX),)
 PREFIX = aaaa::1/64
endif

include $(CONTIKI)/Makefile.include

ifeq ($(TARGET),jn516x)
 BAUDRATE = 1000000
else
 BAUDRATE = 115200
endif 

#using XON/XOFF flow control
connect-router-sw:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -X -B $(BAUDRATE) $(PREFIX)

#using hw flow control	
connect-router-hw:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -H -B $(BAUDRATE) $(PREFIX)

#using no flow control	
connect-router-no:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -B $(BAUDRATE) $(PREFIX)
