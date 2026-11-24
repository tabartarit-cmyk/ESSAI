# Traductions de l'interface

Chaque langue est un fichier `<code>.json` (code ISO 639-1, ex. `fr.json`) de la forme :

```json
{ "Texte anglais exact": "Traduction" }
```

Les fichiers sont embarqués dans l'exécutable (voir `CMakeLists.txt`) : **ajouter une langue = ajouter un fichier ici**, rien d'autre à modifier. L'anglais est la langue d'origine du code et n'a pas de fichier.

## Règles pour traduire

- Garder les marqueurs `%1`, `%2`, `%3` (ils sont remplacés par des valeurs au moment de l'affichage).
- Garder les séparateurs `;;` et les motifs `*.log *.btsnoop` du texte « HCI log (...) ».
- Garder les retours à la ligne `\n` des infobulles.
- Ne pas traduire : Pico-ASHA, ASHA, UAC, HCI, PSM, L2CAP, dB.
- Un texte absent ou vide s'affiche en anglais.

## Outil de contrôle

```sh
python3 tools/i18n_tool.py extract   # régénère _template.json depuis les tr("...") du code
python3 tools/i18n_tool.py check     # signale textes manquants, marqueurs ou séparateurs incorrects
```

## Sens d'écriture

L'écriture de droite à gauche (arabe, hébreu, persan, ourdou, pachto, sindhi, ouïghour, yiddish, maldivien, kashmiri)
est détectée automatiquement : l'interface entière est alors inversée.

## État

Ces traductions n'ont pas été relues par des locuteurs natifs. Merci de corriger toute erreur
(en particulier le vocabulaire technique : UAC = USB Audio Class, atténuation numérique, etc.).
