TARGET      := 3ds-gemini
BUILD       := build
SOURCES     := .
INCLUDES    := include

ARCH        := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS      := -g -Wall -O2 -mword-relocations $(ARCH) -I/opt/devkitpro/libctru/include
LDFLAGS     := -g $(ARCH) -L/opt/devkitpro/libctru/lib -lctru -lm

CFILES      := $(wildcard *.c)
OBJS        := $(CFILES:.c=.o)

all: $(TARGET).3dsx

$(TARGET).3dsx: $(TARGET).elf
	3dsxtool $< $@

$(TARGET).elf: $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).3dsx
