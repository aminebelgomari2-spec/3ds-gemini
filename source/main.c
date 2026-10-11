/*
 * Gemini Chat pour Nintendo 3DS
 *
 * - Écran du haut : conversation (défilable, police système => accents OK)
 * - Écran du bas  : boutons tactiles
 * - Saisie        : clavier logiciel de la 3DS
 * - Réseau        : HTTPS (httpc) vers l'API Gemini, dans un thread séparé
 */
#include <3ds.h>
#include <citro2d.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "config.h"

#define API_URL \
    "https://generativelanguage.googleapis.com/v1beta/models/" GEMINI_MODEL ":generateContent"

#define MAX_MSGS    24            /* messages gardés en mémoire (historique) */
#define MAX_INPUT   500           /* caractères max par message saisi        */
#define MAX_RESP    (256 * 1024)  /* taille max d'une réponse HTTP           */
#define TEXT_SCALE  0.5f
#define WRAP_W      384.0f        /* largeur de texte, écran du haut         */
#define MARGIN      5.0f

enum { ROLE_USER, ROLE_MODEL, ROLE_INFO, ROLE_ERROR };

/* ------------------------------------------------------------------------ */
/*  Petit constructeur de chaînes                                           */
/* ------------------------------------------------------------------------ */
typedef struct {
    char  *p;
    size_t len, cap;
    bool   oom;
} Str;

static void sCatN(Str *s, const char *d, size_t n)
{
    if (s->oom || n == 0) return;
    if (s->len + n + 1 > s->cap) {
        size_t ncap = s->cap ? s->cap : 256;
        while (ncap < s->len + n + 1) ncap *= 2;
        char *np = realloc(s->p, ncap);
        if (!np) { s->oom = true; return; }
        s->p = np;
        s->cap = ncap;
    }
    memcpy(s->p + s->len, d, n);
    s->len += n;
    s->p[s->len] = 0;
}

static void sCat(Str *s, const char *t) { sCatN(s, t, strlen(t)); }

static void sCatJson(Str *s, const char *t)
{
    for (; *t; t++) {
        unsigned char c = (unsigned char)*t;
        switch (c) {
            case '"':  sCatN(s, "\\\"", 2); break;
            case '\\': sCatN(s, "\\\\", 2); break;
            case '\n': sCatN(s, "\\n", 2);  break;
            case '\r': sCatN(s, "\\r", 2);  break;
            case '\t': sCatN(s, "\\t", 2);  break;
            default:
                if (c < 0x20) {
                    char b[8];
                    snprintf(b, sizeof b, "\\u%04x", c);
                    sCatN(s, b, 6);
                } else {
                    sCatN(s, (const char *)&c, 1);
                }
        }
    }
}

static void sCatUtf8(Str *s, unsigned cp)
{
    char b[4];
    int n;
    if (cp < 0x80)        { b[0] = (char)cp; n = 1; }
    else if (cp < 0x800)  { b[0] = (char)(0xC0 | (cp >> 6));  b[1] = (char)(0x80 | (cp & 0x3F)); n = 2; }
    else if (cp < 0x10000){ b[0] = (char)(0xE0 | (cp >> 12)); b[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                            b[2] = (char)(0x80 | (cp & 0x3F)); n = 3; }
    else                  { b[0] = (char)(0xF0 | (cp >> 18)); b[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
                            b[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); b[3] = (char)(0x80 | (cp & 0x3F)); n = 4; }
    sCatN(s, b, (size_t)n);
}

static char *dupStr(const char *s)
{
    size_t n = strlen(s);
    char *d = malloc(n + 1);
    if (d) memcpy(d, s, n + 1);
    return d;
}

static void trim(char *s)
{
    char *a = s;
    while (*a == ' ' || *a == '\t' || *a == '\r' || *a == '\n') a++;
    size_t n = strlen(a);
    while (n > 0 && (a[n - 1] == ' ' || a[n - 1] == '\t' || a[n - 1] == '\r' || a[n - 1] == '\n')) n--;
    memmove(s, a, n);
    s[n] = 0;
}

/* ------------------------------------------------------------------------ */
/*  Lecture JSON minimaliste (suffisant pour les réponses de Gemini)        */
/* ------------------------------------------------------------------------ */
static int hex4(const char *p)
{
    int v = 0;
    for (int i = 0; i < 4; i++) {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9')      v |= c - '0';
        else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
        else return -1;
    }
    return v;
}

/* p pointe sur le guillemet ouvrant ; renvoie le pointeur après le guillemet fermant. */
static const char *readJsonString(const char *p, Str *out)
{
    p++;
    while (*p && *p != '"') {
        if (*p != '\\') {
            if (out) sCatN(out, p, 1);
            p++;
            continue;
        }
        p++; /* après l'antislash */
        char c = *p;
        if (!c) break;
        if (c == 'u') {
            int cp = hex4(p + 1);
            if (cp < 0) { p++; continue; }
            p += 5; /* 'u' + 4 chiffres */
            if (cp >= 0xD800 && cp < 0xDC00 && p[0] == '\\' && p[1] == 'u') {
                int lo = hex4(p + 2);
                if (lo >= 0xDC00 && lo < 0xE000) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    p += 6;
                }
            }
            if (out) sCatUtf8(out, (unsigned)cp);
            continue;
        }
        char r = c;
        switch (c) {
            case 'n': r = '\n'; break;
            case 't': r = '\t'; break;
            case 'r': r = '\r'; break;
            case 'b': r = '\b'; break;
            case 'f': r = '\f'; break;
            default: break; /* \" \\ \/ => le caractère lui-même */
        }
        if (out) sCatN(out, &r, 1);
        p++;
    }
    return *p ? p + 1 : p;
}

static void skipWs(const char **p)
{
    while (**p == ' ' || **p == '\n' || **p == '\r' || **p == '\t') (*p)++;
}

/* Concatène toutes les valeurs-chaînes de la clé `key` (ou seulement la première). */
static int jsonCollect(const char *json, const char *key, Str *out, bool firstOnly)
{
    size_t klen = strlen(key);
    int found = 0;
    const char *p = json;
    while (*p) {
        if (*p != '"') { p++; continue; }
        bool isKey = (strncmp(p + 1, key, klen) == 0 && p[1 + klen] == '"');
        const char *q = readJsonString(p, NULL);
        if (isKey) {
            const char *r = q;
            skipWs(&r);
            if (*r == ':') {
                r++;
                skipWs(&r);
                if (*r == '"') {
                    q = readJsonString(r, out);
                    found++;
                    if (firstOnly) return found;
                }
            }
        }
        p = q;
    }
    return found;
}

/* Enlève le Markdown le plus gênant (la police de la 3DS l'afficherait tel quel). */
static void cleanReply(char *s)
{
    char *w = s;
    bool lineStart = true;
    for (char *r = s; *r;) {
        if (r[0] == '*' && r[1] == '*') { r += 2; continue; }
        if (r[0] == '`') { r++; continue; }
        if (lineStart) {
            if (*r == '#') {
                while (*r == '#') r++;
                while (*r == ' ') r++;
                lineStart = false;
                continue;
            }
            if ((r[0] == '*' || r[0] == '-') && r[1] == ' ') {
                *w++ = '-';
                *w++ = ' ';
                r += 2;
                lineStart = false;
                continue;
            }
            if (*r == ' ') { *w++ = *r++; continue; }
        }
        lineStart = (*r == '\n');
        *w++ = *r++;
    }
    *w = 0;
    trim(s);
}

/* ------------------------------------------------------------------------ */
/*  Historique de la conversation                                           */
/* ------------------------------------------------------------------------ */
typedef struct {
    int   role;
    bool  skip; /* message non envoyé / échoué : ignoré dans les requêtes */
    char *text;
} Msg;

static Msg g_msgs[MAX_MSGS];
static int g_nmsgs;

static void addMsg(int role, const char *text)
{
    if (g_nmsgs == MAX_MSGS) {
        free(g_msgs[0].text);
        memmove(&g_msgs[0], &g_msgs[1], sizeof(Msg) * (MAX_MSGS - 1));
        g_nmsgs--;
    }
    Msg *m = &g_msgs[g_nmsgs++];
    m->role = role;
    m->skip = false;
    m->text = dupStr(text);
    if (!m->text) m->text = dupStr("(mémoire insuffisante)");
}

static void popMsg(void)
{
    if (g_nmsgs > 0) free(g_msgs[--g_nmsgs].text);
}

/* ------------------------------------------------------------------------ */
/*  Rendu du texte : mesure, retour à la ligne, affichage                   */
/* ------------------------------------------------------------------------ */
typedef struct { int off, len; u32 color; } Line;

static C3D_RenderTarget *g_top, *g_bot;
static C2D_TextBuf g_textBuf, g_measBuf;
static Line *g_lines;
static int   g_nlines, g_capLines;
static char *g_disp;
static float g_lineH = 16.0f;
static int   g_scroll;

#define COL_BG       C2D_Color32(18, 20, 32, 255)
#define COL_USER     C2D_Color32(140, 200, 255, 255)
#define COL_MODEL    C2D_Color32(240, 240, 245, 255)
#define COL_INFO     C2D_Color32(230, 200, 110, 255)
#define COL_ERROR    C2D_Color32(255, 125, 125, 255)
#define COL_SKIPPED  C2D_Color32(140, 140, 150, 255)

static float textWidth(const char *s, int len, float scale)
{
    char tmp[512];
    if (len >= (int)sizeof tmp) return 1e9f;
    memcpy(tmp, s, (size_t)len);
    tmp[len] = 0;
    C2D_TextBufClear(g_measBuf);
    C2D_Text t;
    C2D_TextParse(&t, g_measBuf, tmp);
    float w, h;
    C2D_TextGetDimensions(&t, scale, scale, &w, &h);
    return w;
}

static void drawText(const char *s, int len, float x, float y, float scale, u32 color)
{
    char tmp[512];
    if (len > (int)sizeof tmp - 1) len = (int)sizeof tmp - 1;
    memcpy(tmp, s, (size_t)len);
    tmp[len] = 0;
    C2D_Text t;
    C2D_TextParse(&t, g_textBuf, tmp);
    C2D_TextOptimize(&t);
    C2D_DrawText(&t, C2D_WithColor, x, y, 0.5f, scale, scale, color);
}

static void addLine(int off, int len, u32 color)
{
    if (g_nlines == g_capLines) {
        int nc = g_capLines ? g_capLines * 2 : 256;
        Line *nl = realloc(g_lines, (size_t)nc * sizeof(Line));
        if (!nl) return;
        g_lines = nl;
        g_capLines = nc;
    }
    g_lines[g_nlines].off = off;
    g_lines[g_nlines].len = len;
    g_lines[g_nlines].color = color;
    g_nlines++;
}

static void wrapParagraph(const char *base, int a, int b, u32 color)
{
    if (a >= b) { addLine(a, 0, color); return; }
    int lineStart = a, lineEnd = a, p = a;
    while (p < b) {
        int w = p;
        while (w < b && base[w] != ' ') w++;

        /* le mot ne tient plus sur la ligne courante : on passe à la suivante */
        if (lineEnd > lineStart && textWidth(base + lineStart, w - lineStart, TEXT_SCALE) > WRAP_W) {
            addLine(lineStart, lineEnd - lineStart, color);
            lineStart = p;
        }
        /* mot plus large qu'une ligne entière : on le coupe */
        while (w - lineStart > 1 && textWidth(base + lineStart, w - lineStart, TEXT_SCALE) > WRAP_W) {
            int k = w;
            do {
                k--;
                while (k > lineStart && (base[k] & 0xC0) == 0x80) k--;
            } while (k > lineStart + 1 && textWidth(base + lineStart, k - lineStart, TEXT_SCALE) > WRAP_W);
            if (k <= lineStart) break;
            addLine(lineStart, k - lineStart, color);
            lineStart = k;
        }
        lineEnd = w;
        p = w;
        while (p < b && base[p] == ' ') p++;
    }
    addLine(lineStart, lineEnd - lineStart, color);
}

static void wrapSegment(const char *base, int a, int b, u32 color)
{
    int p = a;
    for (;;) {
        int q = p;
        while (q < b && base[q] != '\n') q++;
        wrapParagraph(base, p, q, color);
        if (q >= b) break;
        p = q + 1;
    }
}

static u32 colorFor(const Msg *m)
{
    switch (m->role) {
        case ROLE_USER:  return m->skip ? COL_SKIPPED : COL_USER;
        case ROLE_MODEL: return COL_MODEL;
        case ROLE_INFO:  return COL_INFO;
        default:         return COL_ERROR;
    }
}

static int visibleLines(void) { return (int)((240.0f - 2 * MARGIN) / g_lineH); }

static void clampScroll(void)
{
    int maxs = g_nlines - visibleLines();
    if (maxs < 0) maxs = 0;
    if (g_scroll > maxs) g_scroll = maxs;
    if (g_scroll < 0) g_scroll = 0;
}

static void relayout(bool snapBottom)
{
    free(g_disp);
    g_disp = NULL;
    g_nlines = 0;

    int starts[MAX_MSGS], ends[MAX_MSGS];
    Str d = {0};
    for (int i = 0; i < g_nmsgs; i++) {
        const Msg *m = &g_msgs[i];
        starts[i] = (int)d.len;
        if (m->role == ROLE_USER)       sCat(&d, "Moi : ");
        else if (m->role == ROLE_MODEL) sCat(&d, "Gemini : ");
        sCat(&d, m->text);
        ends[i] = (int)d.len;
        sCatN(&d, "\n", 1);
    }
    g_disp = d.p;
    if (g_disp) {
        for (int i = 0; i < g_nmsgs; i++) {
            if (i > 0) addLine(0, 0, 0);
            wrapSegment(g_disp, starts[i], ends[i], colorFor(&g_msgs[i]));
        }
    }
    if (snapBottom) g_scroll = 1 << 30;
    clampScroll();
}

/* ------------------------------------------------------------------------ */
/*  Clé API                                                                  */
/* ------------------------------------------------------------------------ */
static char g_apiKey[160];
static bool g_needKey;

static void loadKey(void)
{
    g_apiKey[0] = 0;
    if (GEMINI_API_KEY[0] && strcmp(GEMINI_API_KEY, "COLLE_TA_CLE_ICI") != 0) {
        snprintf(g_apiKey, sizeof g_apiKey, "%s", GEMINI_API_KEY);
    } else {
        FILE *f = fopen(KEY_PATH, "r");
        if (f) {
            if (fgets(g_apiKey, sizeof g_apiKey, f)) trim(g_apiKey);
            fclose(f);
        }
    }
    g_needKey = (g_apiKey[0] == 0);
}

static bool saveKey(const char *key)
{
    mkdir("sdmc:/3ds", 0777);
    mkdir(KEY_DIR, 0777);
    FILE *f = fopen(KEY_PATH, "w");
    if (!f) return false;
    fputs(key, f);
    fclose(f);
    return true;
}

/* ------------------------------------------------------------------------ */
/*  Réseau (thread)                                                          */
/* ------------------------------------------------------------------------ */
static volatile int g_netState; /* 0 = repos, 1 = en cours, 2 = terminé */
static Thread g_netThread;
static char  *g_reqBody;
static char  *g_resp;
static size_t g_respLen;
static u32    g_httpStatus;
static char   g_netErr[200];

static char *buildRequest(void)
{
    Str b = {0};
    sCat(&b, "{\"systemInstruction\":{\"parts\":[{\"text\":\"");
    sCatJson(&b, SYSTEM_PROMPT);
    sCat(&b, "\"}]},\"contents\":[");
    bool first = true, started = false;
    for (int i = 0; i < g_nmsgs; i++) {
        const Msg *m = &g_msgs[i];
        if ((m->role != ROLE_USER && m->role != ROLE_MODEL) || m->skip) continue;
        if (!started && m->role != ROLE_USER) continue; /* doit commencer par l'utilisateur */
        started = true;
        if (!first) sCat(&b, ",");
        first = false;
        sCat(&b, "{\"role\":\"");
        sCat(&b, m->role == ROLE_USER ? "user" : "model");
        sCat(&b, "\",\"parts\":[{\"text\":\"");
        sCatJson(&b, m->text);
        sCat(&b, "\"}]}");
    }
    sCat(&b, "]}");
    if (b.oom) { free(b.p); return NULL; }
    return b.p;
}

static void netThread(void *arg)
{
    (void)arg;
    httpcContext ctx;
    bool opened = false;
    char *buf = NULL;
    size_t len = 0, cap = 0;

    g_resp = NULL;
    g_respLen = 0;
    g_httpStatus = 0;
    g_netErr[0] = 0;

    char url[512];
    snprintf(url, sizeof url, "%s?key=%s", API_URL, g_apiKey); /* clé aussi dans l'URL (méthode documentée) */
    Result rc = httpcOpenContext(&ctx, HTTPC_METHOD_POST, url, 1);
    if (R_FAILED(rc)) {
        snprintf(g_netErr, sizeof g_netErr, "Connexion impossible (code 0x%08lX). Wi-Fi active ?", (unsigned long)rc);
        goto done;
    }
    opened = true;

    httpcSetSSLOpt(&ctx, SSLCOPT_DisableVerify); /* la 3DS n'a pas les certificats racine récents */
    httpcAddRequestHeaderField(&ctx, "User-Agent", "GeminiChat3DS/1.0");
    httpcAddRequestHeaderField(&ctx, "Content-Type", "application/json");
    httpcAddRequestHeaderField(&ctx, "x-goog-api-key", g_apiKey);

    rc = httpcAddPostDataRaw(&ctx, (const u32 *)(const void *)g_reqBody, (u32)strlen(g_reqBody));
    if (R_FAILED(rc)) {
        snprintf(g_netErr, sizeof g_netErr, "Envoi impossible (code 0x%08lX).", (unsigned long)rc);
        goto done;
    }

    rc = httpcBeginRequest(&ctx);
    if (R_FAILED(rc)) {
        snprintf(g_netErr, sizeof g_netErr,
                 "Requête échouée (code 0x%08lX). Wi-Fi / HTTPS indisponible ?", (unsigned long)rc);
        goto done;
    }

    rc = httpcGetResponseStatusCode(&ctx, &g_httpStatus);
    if (R_FAILED(rc)) {
        snprintf(g_netErr, sizeof g_netErr, "Pas de réponse du serveur (code 0x%08lX).", (unsigned long)rc);
        goto done;
    }

    cap = 0x4000;
    buf = malloc(cap + 1);
    if (!buf) {
        snprintf(g_netErr, sizeof g_netErr, "Mémoire insuffisante.");
        goto done;
    }
    do {
        u32 got = 0;
        rc = httpcDownloadData(&ctx, (u8 *)buf + len, (u32)(cap - len), &got);
        len += got;
        if (rc == (Result)HTTPC_RESULTCODE_DOWNLOADPENDING) {
            if (len >= MAX_RESP) {
                snprintf(g_netErr, sizeof g_netErr, "Réponse trop longue.");
                goto done;
            }
            cap = len + 0x4000;
            char *nb = realloc(buf, cap + 1);
            if (!nb) {
                snprintf(g_netErr, sizeof g_netErr, "Mémoire insuffisante.");
                goto done;
            }
            buf = nb;
        }
    } while (rc == (Result)HTTPC_RESULTCODE_DOWNLOADPENDING);

    if (R_FAILED(rc)) {
        snprintf(g_netErr, sizeof g_netErr, "Lecture interrompue (code 0x%08lX).", (unsigned long)rc);
        goto done;
    }
    buf[len] = 0;
    g_resp = buf;
    g_respLen = len;
    buf = NULL;

done:
    free(buf);
    if (opened) httpcCloseContext(&ctx);
    __sync_synchronize();
    g_netState = 2;
}

static bool startRequest(void)
{
    free(g_reqBody);
    g_reqBody = buildRequest();
    if (!g_reqBody) {
        addMsg(ROLE_ERROR, "Mémoire insuffisante.");
        return false;
    }
    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    if (prio - 1 < 0x18) prio = 0x19;
    g_netState = 1;
    g_netThread = threadCreate(netThread, NULL, 64 * 1024, prio - 1, -2, false);
    if (!g_netThread) {
        g_netState = 0;
        addMsg(ROLE_ERROR, "Impossible de lancer le thread réseau.");
        return false;
    }
    return true;
}

static void markLastUserFailed(void)
{
    for (int i = g_nmsgs - 1; i >= 0; i--) {
        if (g_msgs[i].role == ROLE_USER) { g_msgs[i].skip = true; return; }
    }
}

static void finishRequest(void)
{
    threadJoin(g_netThread, U64_MAX);
    threadFree(g_netThread);
    g_netThread = NULL;
    __sync_synchronize();
    g_netState = 0;

    char msg[320];
    if (g_netErr[0]) {
        addMsg(ROLE_ERROR, g_netErr);
        markLastUserFailed();
    } else if (g_httpStatus == 200 && g_resp) {
        Str out = {0};
        jsonCollect(g_resp, "text", &out, false);
        if (out.p && out.len > 0) {
            cleanReply(out.p);
            addMsg(ROLE_MODEL, out.p[0] ? out.p : "(réponse vide)");
        } else {
            addMsg(ROLE_ERROR, "Réponse vide (contenu bloqué par Gemini ?). Reformule ta question.");
            markLastUserFailed();
        }
        free(out.p);
    } else {
        Str em = {0};
        if (g_resp) jsonCollect(g_resp, "message", &em, true);
        const char *hint = "";
        if (g_httpStatus == 404)      hint = " Modèle introuvable : change GEMINI_MODEL dans config.h.";
        else if (g_httpStatus == 429) hint = " Quota dépassé, réessaie plus tard.";
        bool keyProblem = (g_httpStatus == 401 || g_httpStatus == 403 ||
                           (g_httpStatus == 400 && em.p && strstr(em.p, "API key")));
        if (keyProblem) hint = " Clé API refusée : appuie sur A pour la ressaisir.";
        snprintf(msg, sizeof msg, "Erreur HTTP %lu.%s %.150s", (unsigned long)g_httpStatus, hint,
                 em.p ? em.p : "");
        addMsg(ROLE_ERROR, msg);
        markLastUserFailed();
        if (keyProblem) g_needKey = true; /* le bouton A redemande la clé */
        free(em.p);
    }
    free(g_resp);
    g_resp = NULL;
    free(g_reqBody);
    g_reqBody = NULL;
    relayout(true);
}

/* ------------------------------------------------------------------------ */
/*  Actions de l'interface                                                   */
/* ------------------------------------------------------------------------ */
static bool askText(const char *hint, char *out, size_t outSize, int maxChars, const char *okLabel)
{
    SwkbdState sw;
    swkbdInit(&sw, SWKBD_TYPE_NORMAL, 2, maxChars);
    swkbdSetValidation(&sw, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    swkbdSetHintText(&sw, hint);
    swkbdSetButton(&sw, SWKBD_BUTTON_LEFT, "Annuler", false);
    swkbdSetButton(&sw, SWKBD_BUTTON_RIGHT, okLabel, true);
    out[0] = 0;
    SwkbdButton btn = swkbdInputText(&sw, out, outSize);
    if (btn != SWKBD_BUTTON_RIGHT) return false;
    trim(out);
    return out[0] != 0;
}

static void doWrite(void)
{
    if (g_needKey) {
        char key[160];
        if (askText("Tape ta clé API Gemini", key, sizeof key, 120, "Enregistrer")) {
            snprintf(g_apiKey, sizeof g_apiKey, "%s", key);
            g_needKey = false;
            if (saveKey(key)) addMsg(ROLE_INFO, "Clé enregistrée sur la carte SD (3ds/GeminiChat/apikey.txt).");
            else              addMsg(ROLE_INFO, "Clé utilisée, mais impossible de l'enregistrer sur la SD.");
            relayout(true);
        }
        return;
    }
    char in[2048];
    if (askText("Ton message pour Gemini...", in, sizeof in, MAX_INPUT, "Envoyer")) {
        addMsg(ROLE_USER, in);
        relayout(true);
        if (!startRequest()) relayout(true);
    }
}

static bool canRetry(void)
{
    for (int i = g_nmsgs - 1; i >= 0; i--) {
        if (g_msgs[i].role == ROLE_USER || g_msgs[i].role == ROLE_MODEL)
            return g_msgs[i].role == ROLE_USER && g_msgs[i].skip;
    }
    return false;
}

static void doRetry(void)
{
    if (g_nmsgs > 0 && g_msgs[g_nmsgs - 1].role == ROLE_ERROR) popMsg();
    for (int i = g_nmsgs - 1; i >= 0; i--) {
        if (g_msgs[i].role == ROLE_USER) { g_msgs[i].skip = false; break; }
    }
    relayout(true);
    if (!startRequest()) relayout(true);
}

static void clearChat(void)
{
    while (g_nmsgs > 0) popMsg();
    addMsg(ROLE_INFO, "Nouvelle conversation. Appuie sur A pour écrire un message.");
    relayout(true);
}

/* ------------------------------------------------------------------------ */
/*  Dessin                                                                   */
/* ------------------------------------------------------------------------ */
typedef struct { float x, y, w, h; } Rect;

static const Rect BTN_WRITE = { 20,  58, 280, 54 };
static const Rect BTN_RETRY = { 20, 120, 136, 44 };
static const Rect BTN_CLEAR = { 164, 120, 136, 44 };
static const Rect BTN_QUIT  = { 20, 172, 280, 40 };

static bool hit(const Rect *r, int px, int py)
{
    return px >= r->x && px < r->x + r->w && py >= r->y && py < r->y + r->h;
}

static void drawButton(const Rect *r, const char *label, bool enabled)
{
    u32 fill = enabled ? C2D_Color32(66, 99, 235, 255) : C2D_Color32(58, 60, 76, 255);
    u32 text = enabled ? C2D_Color32(255, 255, 255, 255) : C2D_Color32(130, 132, 150, 255);
    C2D_DrawRectSolid(r->x, r->y, 0.0f, r->w, r->h, fill);
    float tw = textWidth(label, (int)strlen(label), 0.55f);
    drawText(label, (int)strlen(label), r->x + (r->w - tw) / 2, r->y + (r->h - g_lineH * 1.1f) / 2, 0.55f, text);
}

static void drawTop(void)
{
    C2D_TargetClear(g_top, COL_BG);
    C2D_SceneBegin(g_top);

    int vis = visibleLines();
    for (int i = 0; i < vis; i++) {
        int li = g_scroll + i;
        if (li >= g_nlines) break;
        const Line *L = &g_lines[li];
        if (L->len > 0) drawText(g_disp + L->off, L->len, MARGIN, MARGIN + i * g_lineH, TEXT_SCALE, L->color);
    }
    if (g_nlines > vis) {
        float trackH = 240.0f - 2 * MARGIN;
        float barH = trackH * (float)vis / (float)g_nlines;
        if (barH < 8) barH = 8;
        float barY = MARGIN + (trackH - barH) * (float)g_scroll / (float)(g_nlines - vis);
        C2D_DrawRectSolid(396, MARGIN, 0.0f, 3, trackH, C2D_Color32(40, 44, 64, 255));
        C2D_DrawRectSolid(396, barY, 0.1f, 3, barH, C2D_Color32(120, 130, 200, 255));
    }
}

static void drawBottom(unsigned frame)
{
    C2D_TargetClear(g_bot, COL_BG);
    C2D_SceneBegin(g_bot);

    drawText("Gemini Chat", 11, 20, 8, 0.75f, C2D_Color32(255, 255, 255, 255));

    char status[96];
    u32 scol = C2D_Color32(150, 155, 180, 255);
    if (g_needKey) {
        snprintf(status, sizeof status, "Clé API manquante");
        scol = COL_ERROR;
    } else if (g_netState) {
        snprintf(status, sizeof status, "Gemini réfléchit%.*s", (int)((frame / 20) % 4), "...");
        scol = COL_INFO;
    } else {
        snprintf(status, sizeof status, "Modèle : %s", GEMINI_MODEL);
    }
    drawText(status, (int)strlen(status), 20, 36, 0.45f, scol);

    bool busy = g_netState != 0;
    drawButton(&BTN_WRITE, g_needKey ? "Saisir la clé API  (A)" : (busy ? "..." : "Écrire un message  (A)"),
               !busy);
    drawButton(&BTN_RETRY, "Réessayer (X)", !busy && canRetry());
    drawButton(&BTN_CLEAR, "Effacer (Y)", !busy);
    drawButton(&BTN_QUIT, "Quitter  (START)", true);

    const char *hint = "Haut/Bas ou L/R : faire défiler";
    drawText(hint, (int)strlen(hint), 20, 220, 0.4f, C2D_Color32(110, 114, 140, 255));
}

/* ------------------------------------------------------------------------ */
/*  main                                                                     */
/* ------------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    g_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    g_bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    g_textBuf = C2D_TextBufNew(4096);
    g_measBuf = C2D_TextBufNew(1024);
    osSetSpeedupEnable(true);

    {   /* hauteur d'une ligne de texte */
        C2D_Text t;
        float w, h;
        C2D_TextParse(&t, g_measBuf, "Ag");
        C2D_TextGetDimensions(&t, TEXT_SCALE, TEXT_SCALE, &w, &h);
        g_lineH = h + 1.0f;
    }

    bool httpOk = R_SUCCEEDED(httpcInit(0x100000));
    loadKey();

    addMsg(ROLE_INFO, g_needKey ? "Bienvenue ! Appuie sur A pour saisir ta clé API Gemini (elle sera gardée sur la SD)."
                                : "Bienvenue ! Appuie sur A pour écrire un message à Gemini.");
    if (!httpOk) addMsg(ROLE_ERROR, "Service HTTP indisponible : impossible de contacter Gemini.");
    relayout(true);

    unsigned frame = 0;
    int holdUp = 0, holdDown = 0;

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        touchPosition tp = {0};
        if (kDown & KEY_TOUCH) hidTouchRead(&tp);
        bool tDown = (kDown & KEY_TOUCH) != 0;

        if ((kDown & KEY_START) || (tDown && hit(&BTN_QUIT, tp.px, tp.py))) break;

        if (g_netState == 2) finishRequest();

        if (g_netState == 0 && httpOk) {
            if ((kDown & KEY_A) || (tDown && hit(&BTN_WRITE, tp.px, tp.py))) doWrite();
            else if ((kDown & KEY_X) || (tDown && hit(&BTN_RETRY, tp.px, tp.py))) { if (canRetry()) doRetry(); }
            else if ((kDown & KEY_Y) || (tDown && hit(&BTN_CLEAR, tp.px, tp.py))) clearChat();
        }

        /* défilement */
        if (kHeld & (KEY_DUP | KEY_CPAD_UP)) {
            if (holdUp == 0 || (holdUp > 15 && holdUp % 2 == 0)) g_scroll--;
            holdUp++;
        } else holdUp = 0;
        if (kHeld & (KEY_DDOWN | KEY_CPAD_DOWN)) {
            if (holdDown == 0 || (holdDown > 15 && holdDown % 2 == 0)) g_scroll++;
            holdDown++;
        } else holdDown = 0;
        if (kDown & KEY_L) g_scroll -= visibleLines() - 1;
        if (kDown & KEY_R) g_scroll += visibleLines() - 1;
        clampScroll();

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TextBufClear(g_textBuf);
        drawTop();
        drawBottom(frame++);
        C3D_FrameEnd(0);
    }

    /* si une requête est encore en cours, on attend sa fin avant de quitter */
    if (g_netState != 0 && g_netThread) {
        threadJoin(g_netThread, U64_MAX);
        threadFree(g_netThread);
    }
    free(g_resp);
    free(g_reqBody);
    free(g_disp);
    free(g_lines);
    while (g_nmsgs > 0) popMsg();

    if (httpOk) httpcExit();
    C2D_TextBufDelete(g_textBuf);
    C2D_TextBufDelete(g_measBuf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
