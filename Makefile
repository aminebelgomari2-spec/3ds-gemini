PREFIX      := /opt/devkitpro/devkitARM/bin/arm-none-eabi-
CC          := $(PREFIX)gcc

TARGET      := 3ds-gemini
SOURCES     := .
INCLUDES    := include

SPECS       := /opt/devkitpro/devkitARM/arm-none-eabi/lib/3ds.specs

ARCH        := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS      := -g -Wall -O2 -mword-relocations $(ARCH) -I/opt/devkitpro/libctru/include
LDFLAGS     := -g $(ARCH) -specs=$(SPECS) -L/opt/devkitpro/libctru/lib -lctru -lm

CFILES      := $(wildcard *.c)
OBJS        := $(CFILES:.c=.o)

all: $(TARGET).3dsx

$(TARGET).3dsx: $(TARGET).elf
	/opt/devkitpro/tools/bin/3dsxtool $< $@

$(TARGET).elf: $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).3dsx
