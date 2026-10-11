ifeq ($(strip $(DEVKITPRO)),)
export DEVKITPRO := /opt/devkitpro
endif

export DEVKITARM := $(DEVKITPRO)/devkitARM
export PATH := $(DEVKITARM)/bin:$(PATH)

TARGET      := 3ds-gemini
BUILD       := build
SOURCES     := .
INCLUDES    := include

# Utilisation des règles officielles de devkitPro
include $(DEVKITPRO)/3ds_rules

LIBS        := -lctru -lm
