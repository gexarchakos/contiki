CONTIKI_PROJECT = border-router
all: $(CONTIKI_PROJECT) 

TARGET ?= z1

CONTIKI=../../..

CONTIKI_WITH_IPV6 = 1

PROJECTDIRS += .. ../tools
PROJECT_SOURCEFILES += rpl-tools.c slip-bridge.c slip.c

# Set to enable compiler and linker flags to minimize memory.
REDUCE_MEMORY_SIZE = 1

ifeq (REDUCE_MEMORY_SIZE, 1)
CFLAGS += -Os -ffunction-sections
LDFLAGS += -Wl,-Os,--gc-sections,--undefined=_reset_vector__,--undefined=InterruptVectors,--undefined=_copy_data_init__,--undefined=_clear_bss_init__,--undefined=_end_of_init__
endif

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
CFLAGS += -DWITH_COAP
CFLAGS += -DREST=coap_rest_implementation
CFLAGS += -DUIP_CONF_TCP=0
APPS += json
APPS += er-coap
APPS += rest-engine

CFLAGS += -DWITH_TSCH
MODULES += core/net/mac/tsch

# The plexi configuration part
PLEXI_WITH_RPL_DAG_RESOURCE = 1
PLEXI_WITH_SLOTFRAME_RESOURCE = 1
PLEXI_WITH_LINK_RESOURCE = 1
PLEXI_WITH_NEIGHBOR_RESOURCE = 1
PLEXI_WITH_LINK_STATISTICS = 0
PLEXI_WITH_QUEUE_STATISTICS = 0
APPS += plexi

include $(CONTIKI)/Makefile.include

ifeq ($(PREFIX),)
 PREFIX = aaaa::1/64
endif

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
