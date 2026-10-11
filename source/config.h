#ifndef CONFIG_H
#define CONFIG_H

/*
 * ============================  CONFIGURATION  ============================
 *
 * 1) Ta clé API Gemini (https://aistudio.google.com/apikey)
 *    Tu peux la mettre ici, OU la laisser telle quelle : l'app la lira alors
 *    dans  sdmc:/3ds/GeminiChat/apikey.txt  ou te la demandera au premier
 *    lancement (elle sera sauvegardée sur la carte SD).
 *
 *    ATTENTION : si tu mets ta vraie clé ici et que le .cia est publié sur
 *    GitHub (release publique), n'importe qui peut la récupérer dans le .cia.
 *    Utilise une clé avec un quota limité, ou préfère la saisie dans l'app.
 */
#define GEMINI_API_KEY "COLLE_TA_CLE_ICI"

/*
 * 2) Modèle utilisé. Les noms changent régulièrement : si tu obtiens une
 *    erreur 404 "modèle introuvable", vérifie la liste à jour sur
 *    https://ai.google.dev/gemini-api/docs/models
 */
#define GEMINI_MODEL "gemini-3.5-flash-lite"

/* 3) Consigne donnée à Gemini à chaque requête (comportement général). */
#define SYSTEM_PROMPT \
    "Tu es un assistant dans une application sur Nintendo 3DS. " \
    "Reponds dans la langue de l'utilisateur. " \
    "Ecris en texte brut : pas de Markdown, pas d'asterisques, pas de tableaux, " \
    "pas d'emojis. Sois concis (quelques phrases) car l'ecran est petit."

/* Fichier où la clé est lue / sauvegardée si elle n'est pas dans ce fichier. */
#define KEY_DIR  "sdmc:/3ds/GeminiChat"
#define KEY_PATH "sdmc:/3ds/GeminiChat/apikey.txt"

#endif
