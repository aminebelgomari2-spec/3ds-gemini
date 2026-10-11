target      := 3ds-gemini
sources     := .
includes    := include
data        := data

# Utilisation des règles standard de devkitPro
include $(DEVKITPRO)/3ds_rules

# Définition des bibliothèques nécessaires pour libctru et les mathématiques
LIBS        := -lctru -lm
