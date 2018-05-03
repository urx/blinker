PREFIX	?= arm-none-eabi
OPENCM3_DIR ?= $(realpath libopencm3)

LIBNAME		= opencm3_stm32l1
DEFS		+= -DSTM32L1

FP_FLAGS	?= -msoft-float
ARCH_FLAGS	= -mthumb -mcpu=cortex-m3 $(FP_FLAGS) -mfix-cortex-m3-ldrd


CC		:= $(PREFIX)-gcc
CXX		:= $(PREFIX)-g++
LD		:= $(PREFIX)-gcc
AR		:= $(PREFIX)-ar
AS		:= $(PREFIX)-as
OBJCOPY		:= $(PREFIX)-objcopy
OBJDUMP		:= $(PREFIX)-objdump
GDB		:= $(PREFIX)-gdb
STFLASH		= $(shell which st-flash)
STYLECHECK	:= /checkpatch.pl
STYLECHECKFLAGS	:= --no-tree -f --terse --mailback
STYLECHECKFILES	:= $(shell find . -name '*.[ch]')
OPT		:= -Os
DEBUG		:= -ggdb3
CSTD		?= -std=c99
OPENCM3_SCRIPT_DIR = $(OPENCM3_DIR)/scripts

LDSCRIPT = $(OPENCM3_DIR)/lib/stm32/l1/stm32l15xxb.ld

ifeq ($(strip $(DEVICE)),)
# Old style, assume LDSCRIPT exists
DEFS		+= -I$(OPENCM3_DIR)/include
LDFLAGS		+= -L$(OPENCM3_DIR)/lib
LDLIBS		+= -l$(LIBNAME)
LDSCRIPT	?= $(BINARY).ld
else
# New style, assume device is provided, and we're generating the rest.
ifneq ($(strip $(LDSCRIPT)),)
$(error adsfasdfasdfasdf)
endif
include $(OPENCM3_DIR)/mk/genlink-config.mk
endif


###############################################################################
# C flags

TGT_CFLAGS	+= $(OPT) $(CSTD) $(DEBUG)
TGT_CFLAGS	+= $(ARCH_FLAGS)
TGT_CFLAGS	+= -Wextra -Wshadow -Wimplicit-function-declaration
TGT_CFLAGS	+= -Wredundant-decls -Wmissing-prototypes -Wstrict-prototypes
TGT_CFLAGS	+= -fno-common -ffunction-sections -fdata-sections

###############################################################################
# C++ flags

TGT_CXXFLAGS	+= $(OPT) $(CXXSTD) $(DEBUG)
TGT_CXXFLAGS	+= $(ARCH_FLAGS)
TGT_CXXFLAGS	+= -Wextra -Wshadow -Wredundant-decls  -Weffc++
TGT_CXXFLAGS	+= -fno-common -ffunction-sections -fdata-sections

###############################################################################
# C & C++ preprocessor common flags

TGT_CPPFLAGS	+= -MD
TGT_CPPFLAGS	+= -Wall -Wundef
TGT_CPPFLAGS	+= $(DEFS)

###############################################################################
# Linker flags

TGT_LDFLAGS		+= --static -nostartfiles
TGT_LDFLAGS		+= -T$(LDSCRIPT)
TGT_LDFLAGS		+= $(ARCH_FLAGS) $(DEBUG)
TGT_LDFLAGS		+= -Wl,-Map=$(*).map -Wl,--cref
TGT_LDFLAGS		+= -Wl,--gc-sections
ifeq ($(V),99)
TGT_LDFLAGS		+= -Wl,--print-gc-sections
endif


DEFS		+= -I$(OPENCM3_DIR)/include
LDFLAGS		+= -L$(OPENCM3_DIR)/lib
LDLIBS		+= -l$(LIBNAME)
LDSCRIPT	?= $(BINARY).ld

###############################################################################
# Used libraries
LDLIBS		+= -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group



all: build
	

build: lib blinker


lib:
	$(MAKE) -C $(OPENCM3_DIR)


blinker:
	$(CC) $(TGT_CFLAGS) $(CFLAGS) $(TGT_CPPFLAGS) $(CPPFLAGS) -o blinker.o -c blinker.c
	$(LD) $(TGT_LDFLAGS) $(LDFLAGS) blinker.o $(LDLIBS) -o blinker.elf
	$(OBJCOPY) -Obinary blinker.elf blinker.bin
	$(OBJCOPY) -Oihex blinker.elf blinker.hex

clean:
	rm -rf *.hex *.bin *.elf *.d *.o
	$(MAKE) -C $(OPENCM3_DIR) clean
