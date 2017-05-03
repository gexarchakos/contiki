CONTIKI_PROJECT = node
all: $(CONTIKI_PROJECT)

TARGET ?= z1

CONTIKI=../../..

CONTIKI_WITH_IPV6 = 1

PROJECTDIRS += .. ../tools
PROJECT_SOURCEFILES += rpl-tools.c

# Set to enable compiler and linker flags to minimize memory.
REDUCE_MEMORY_SIZE = 1

ifeq (REDUCE_MEMORY_SIZE, 1)
CFLAGS += -Os -ffunction-sections
LDFLAGS += -Wl,-Os,--gc-sections,--undefined=_reset_vector__,--undefined=InterruptVectors,--undefined=_copy_data_init__,--undefined=_clear_bss_init__,--undefined=_end_of_init__
endif

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
CFLAGS += -DUIP_CONF_TCP=0
CFLAGS += -DWITH_TSCH

MODULES += core/net/mac/tsch

include $(CONTIKI)/Makefile.include
