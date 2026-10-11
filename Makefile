ifeq ($(strip $(DEVKITPRO)),)
$(error "DEVKITPRO n'est pas défini")
endif

include $(DEVKITPRO)/3ds_rules

TARGET      := 3ds-gemini
BUILD       := build
SOURCES     := .
INCLUDES    := include

ARCH        := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS      := -g -Wall -O2 -mword-relocations $(ARCH)
CXXFLAGS    := $(CFLAGS)
ASFLAGS     := -g $(ARCH)

LIBS        := -lctru -lm

CFILES      := $(wildcard *.c)
OBJS        := $(CFILES:.c=.o)

all: $(TARGET).3dsx

$(TARGET).3dsx: $(TARGET).elf
	3dsxtool $< $@

$(TARGET).elf: $(OBJS)
	$(CC) $(ARCH) -specs=3ds.specs $^ $(LIBDIRS) $(LIBS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).3dsx
