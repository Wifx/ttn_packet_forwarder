### Application-specific constants

APP_NAME := poly_pkt_fwd

### Environment constants 

LGW_PATH ?= ../../lora_gateway/libloragw
ARCH ?=
CROSS_COMPILE ?=

### External constant definitions
# must get library build option to know if mpsse must be linked or not

include $(LGW_PATH)/library.cfg
RELEASE_VERSION := $(shell cat ../VERSION)

### Constant symbols

CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar

INC_PATH := inc

CFLAGS := -O2 -Wall -Wextra -std=c99 -I$(INC_PATH) -I.
VFLAG := -D VERSION_STRING="\"$(RELEASE_VERSION)\""

### Constants for Lora concentrator HAL library
# List the library sub-modules that are used by the application

LGW_INC =
ifneq ($(wildcard $(LGW_PATH)/inc/config.h),)
  # only for HAL version 1.3 and beyond
  LGW_INC += $(LGW_PATH)/inc/config.h
endif
LGW_INC += $(LGW_PATH)/inc/loragw_hal.h
LGW_INC += $(LGW_PATH)/inc/loragw_gps.h

INC_FILES := $(wildcard $(INC_PATH)/*.h)
OBJ_FILES := $(patsubst src/%.c,obj/%.o,$(wildcard src/*.c))

### Linking options

ifeq ($(CFG_SPI),native)
  LIBS := -lloragw -lrt -lpthread
else ifeq ($(CFG_SPI),ftdi)
  LIBS := -lloragw -lrt -lpthread -lmpsse
else ifeq ($(CFG_SPI),mac)
    LIBS := -lloragw -lpthread -lmpsse
else
  # keep compatibility with SX1301 HAL version 1.2.x and bellow
  ifeq ($(LGW_PHY),native)
    LIBS := -lloragw -lrt -lpthread
  else ifeq ($(LGW_PHY),ftdi)
    LIBS := -lloragw -lrt -lpthread -lmpsse
  else ifeq ($(LGW_PHY),mac)
    LIBS := -lloragw -lpthread -lmpsse
  else
    $(error [error] Can't find configuration for SPI phy)
  endif
endif

### General build targets

all: $(APP_NAME)

clean:
	rm -f obj/*.o
	rm -f $(APP_NAME)

### Sub-modules compilation
obj/%.o: src/%.c inc/%.h $(INC_FILES)
	$(CC) -I$(LGW_PATH)/inc -c $(CFLAGS) $(CFLAGS2) $< -o $@

### Main program compilation and assembly

obj/$(APP_NAME).o: src/$(APP_NAME).c $(LGW_INC) $(INC_FILES)
	$(CC) -c $(CFLAGS) $(CFLAGS2) $(VFLAG) -I$(LGW_PATH)/inc $< -o $@

$(APP_NAME): $(LGW_PATH)/libloragw.a $(OBJ_FILES)
	@echo $(OBJ_FILES)
	$(CC) -L$(LGW_PATH) $< $(OBJ_FILES) -o $@ $(LIBS)

### EOF
