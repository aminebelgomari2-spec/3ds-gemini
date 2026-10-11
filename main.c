#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>

int main(int argc, char **argv) {
    // Initialisation de la console et de l'affichage 3DS
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    printf("\x1b[10;2H=== 3DS Gemini Client ===");
    printf("\x1b[12;2HAppuie sur A pour tester la connexion.");
    printf("\x1b[14;2HAppuie sur START pour quitter.");

    // Boucle principale du programme
    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_START) {
            break; // Quitter le programme
        }

        if (kDown & KEY_A) {
            printf("\x1b[16;2HEnvoi de la requete au serveur...");
            // La logique de communication HTTP avec ton serveur Flask sera ici
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
