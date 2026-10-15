# Tableur  application externe NumWorks

Tableur est une application EADK en C pour NumWorks. Elle affiche une grille A1:C3 avec cellule selectionnee, puis un menu secret declenche par la sequence invisible `2, 0, 1, 1`. Le menu donne acces aux cours complets et a leurs sous-sections : Maths, Physique-Chimie chapitre 1 et chapitre 2. Le lecteur utilise une petite police, le retour a la ligne et un defilement borne.

## Construire sans installation locale

1. Creez un depot vide sur GitHub.
2. Dans l'onglet **Code**, utilisez **Add file > Upload files** et glissez-deposez tout le contenu de ce dossier (y compris le dossier cache `.github`).
3. Ouvrez l'onglet **Actions** et attendez la fin de **Build Tableur NumWorks**.
4. Telechargez l'artefact `tableur-app`, puis recuperez `app.nwa`.
5. Installez `app.nwa` sur [my.numworks.com/apps](https://my.numworks.com/apps).

Un push d'un tag `v*` (par exemple `v1.0.0`) construit aussi une GitHub Release et y joint `app.nwa`.

## Fichiers

- `src/main.c` : interface, navigation et lecteur.
- `src/content.c` : cours ASCII generes depuis les trois fichiers TXT.
- `scripts/generate_content.py` : regeneration du contenu.
- `Makefile` : compilation avec `nwlink@0.0.16` via `npx`.
