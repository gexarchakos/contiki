CONTIKI_PROJECT = node
all: $(CONTIKI_PROJECT)

TARGET ?= jn516x

CONTIKI=../../..

CONTIKI_WITH_IPV6 = 1

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
PROJECTDIRS += .. ../tools
PROJECT_SOURCEFILES += rpl-tools.c
APPS += traffic

CFLAGS += -DUIP_CONF_TCP=0
APPS += traffic

include $(CONTIKI)/Makefile.include
