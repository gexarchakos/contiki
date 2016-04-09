CONTIKI_PROJECT = node
all: $(CONTIKI_PROJECT)

TARGET ?= jn516x

CONTIKI=../../..

CONTIKI_WITH_IPV6 = 1

PROJECTDIRS += .. ../tools
PROJECT_SOURCEFILES += rpl-tools.c

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
CFLAGS += -DWITH_COAP
CFLAGS += -DREST=coap_rest_implementation
CFLAGS += -DUIP_CONF_TCP=0
APPS += json
APPS += er-coap
APPS += rest-engine

CFLAGS += -DWITH_TSCH
CFLAGS += -DWITH_PLEXI
MODULES += core/net/mac/tsch
APPS += plexi

include $(CONTIKI)/Makefile.include
