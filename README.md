# Gemini Chat pour Nintendo 3DS

Client **non officiel** pour l'API Gemini : clavier de la 3DS pour écrire, réponses affichées sur l'écran du haut (accents OK), historique de conversation, bouton « Réessayer ».

## Installer sur ta 3DS (avec FBI)

1. Crée un dépôt GitHub (public) et pousse ce dossier dessus (branche `main`).
2. L'onglet **Actions** compile tout automatiquement (2-3 min). Ensuite la page **Releases** contient :
   - `GeminiChat.cia` (l'app)
   - `GeminiChat.3dsx` (version pour le Homebrew Launcher)
   - `qrcode.png` (le QR code)
3. Sur la 3DS : **FBI → Remote Install → Scan QR Code**, puis scanne `qrcode.png` affiché sur ton écran d'ordinateur (luminosité au max, QR bien à plat).

Lien direct (à mettre dans n'importe quel générateur de QR si besoin) :
`https://github.com/TON_PSEUDO/TON_DEPOT/releases/download/latest/GeminiChat.cia`

## Clé API

Clé gratuite : <https://aistudio.google.com/apikey>. Trois façons de la donner à l'app :

1. **Au premier lancement** : l'app la demande et la garde dans `sdmc:/3ds/GeminiChat/apikey.txt` (recommandé).
2. **Dans le code** : remplace `COLLE_TA_CLE_ICI` dans `source/config.h`.
3. **Secret GitHub** : Settings → Secrets → Actions → `GEMINI_API_KEY` (injectée au build).

> ⚠️ Avec les options 2 et 3, la clé est **dans le .cia**, donc récupérable par quiconque télécharge la release publique. Google désactive aussi automatiquement les clés qu'il trouve en clair sur GitHub. Dans le doute, utilise l'option 1.

## Utilisation

| Action | Bouton |
|---|---|
| Écrire un message | A (ou bouton tactile) |
| Réessayer le dernier message | X |
| Nouvelle conversation | Y |
| Défiler | Haut/Bas, pavé circulaire, L/R |
| Quitter | START |

## Modèle

Le modèle est défini dans `source/config.h` (`GEMINI_MODEL`). Si tu as une erreur 404, le nom a probablement changé : <https://ai.google.dev/gemini-api/docs/models>.

## Compiler soi-même

Avec devkitPro (`dkp-pacman -S 3ds-dev`) : `make` pour le `.3dsx`/`.elf`. Pour le `.cia`, il faut aussi `makerom` et `bannertool` : `MAKEROM=... BANNERTOOL=... make cia`.

## Limites connues

- Le HTTPS de la 3DS ne vérifie pas les certificats (les certificats racine de la console sont trop anciens) : n'utilise pas cette app sur un Wi-Fi public.
- Pas de Markdown : le texte est affiché brut (l'app nettoie les `**`, `#`, etc.).
- Historique limité aux 24 derniers messages.
