TARGET      := 3ds-gemini
SOURCES     := .
INCLUDES    := include

ARCH        := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS      := -g -Wall -O2 -mword-relocations $(ARCH) -I$(DEVKITPRO)/libctru/include
LDFLAGS     := -g $(ARCH) -specs=$(DEVKITPRO)/libctru/3ds.specs -L$(DEVKITPRO)/libctru/lib -lctru -lm

CFILES      := $(wildcard *.c)
OBJS        := $(CFILES:.c=.o)

all: $(TARGET).3dsx

$(TARGET).3dsx: $(TARGET).elf
	3dsxtool $< $@

$(TARGET).elf: $(OBJS)
	arm-none-eabi-gcc $^ $(LDFLAGS) -o $@

%.o: %.c
	arm-none-eabi-gcc $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).3dsx
