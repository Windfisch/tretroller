PROJECT = tretroller
BUILD_DIR = bin

#SHARED_DIR = ../my-common-code
CFILES = ws2812.c main.c tacho.c usart.c adc.c battery.c color.c math.c ledpattern.c
#AFILES = stuff.S
LDLIBS = -lm
CFLAGS += -DSTM32F1 -std=c99 -pedantic-errors

DEVICE=stm32f103c8t

# You shouldn't have to edit anything below here.
VPATH += $(SHARED_DIR)
INCLUDES += $(patsubst %,-I%, . $(SHARED_DIR))
OPENCM3_DIR=../libopencm3

include $(OPENCM3_DIR)/mk/genlink-config.mk
include rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk
