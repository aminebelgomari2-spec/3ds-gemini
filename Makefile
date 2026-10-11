FEAURED_3DSX := $(BUILD).3dsx

ifeq ($(strip $(DEVKITPRO)),)
$(error "S'il vous plait, definissez DEVKITPRO dans votre environnement")
endif

include $(DEVKITPRO)/3ds_rules

TARGET      := 3ds-gemini
BUILD       := build
SOURCES     := .
DATA        := data
INCLUDES    := include

ARCH        := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS      := -g -Wall -O2 -mword-relocations $(ARCH)
CXXFLAGS    := $(CFLAGS)
ASFLAGS     := -g $(ARCH)
LDFLAGS     = -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS        := -lctru -lm

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   := $(CURDIR)/$(TARGET)
export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
OBJS        := $(CFILES:.c=.o)

export LD   := $(CC)

.PHONY: all clean

all: $(OUTPUT).cia $(OUTPUT).3dsx

clean:
	@rm -rf $(BUILD) $(OUTPUT).cia $(OUTPUT).3dsx $(OUTPUT).elf

$(OUTPUT).cia: $(OUTPUT).elf
$(OUTPUT).3dsx: $(OUTPUT).elf

$(OUTPUT).elf: $(OBJS)

%:
	@[ -d $(BUILD) ] || mkdir -p $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile $*

else

DEPENDS := $(OBJS:.o=.d)

$(OUTPUT).elf: $(OBJS)

-include $(DEPENDS)

endif
