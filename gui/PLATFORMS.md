# Pico-ASHA GUI : Windows, Linux, Android

Le même code C++/Qt sert pour les trois systèmes (liaison USB isolée dans `seriallink.cpp`).
Les trois versions sont compilées par GitHub Actions (`.github/workflows/build.yml`) :

| Système | Artefact | Contenu |
|---|---|---|
| Windows | `pico-asha-gui-win64` | dossier à décompresser |
| Linux | `pico-asha-gui-linux-x86_64` | `Pico-ASHA-GUI-x86_64.AppImage` (un seul fichier) + `99-pico-asha.rules` |
| Android | `pico-asha-gui-android` | fichier `.apk` (un seul fichier) |

## Linux

1. Rendre le fichier exécutable : `chmod +x Pico-ASHA-GUI-x86_64.AppImage`, puis le lancer.
2. Droits d'accès à la Pico (sinon la liaison série est refusée) :
   ```sh
   sudo cp 99-pico-asha.rules /etc/udev/rules.d/
   sudo udevadm control --reload-rules && sudo udevadm trigger
   ```
   Puis débrancher et rebrancher la Pico. La règle empêche aussi ModemManager de sonder la Pico.
   À défaut : `sudo usermod -aG dialout $USER` puis ouvrir une nouvelle session.
3. Le fichier journal `pico-asha.log` est écrit dans `~/.local/share/` (dossier de données de l'application).

## Android

1. Autoriser l'installation d'applications hors Play Store pour le navigateur ou le gestionnaire de fichiers utilisé,
   puis installer le fichier `.apk`.
2. Brancher la Pico au téléphone avec un câble ou un adaptateur **USB-OTG**.
3. Au premier lancement, Android demande l'autorisation d'accéder à l'appareil USB : accepter.
   L'autorisation est redemandée à chaque rebranchement de la Pico.
4. L'affichage est adapté au téléphone : tout est empilé verticalement et la fenêtre défile.

Limites connues :
- Les infobulles (texte d'aide) ne s'affichent pas au toucher.
- Le journal HCI est enregistré dans le dossier de données de l'application : il n'y a pas de bouton « Browse ».
- Android 9 (API 28) minimum.

## Langue

Sur les trois systèmes, la langue du système est détectée automatiquement ; le bouton « Language »
permet d'en choisir une autre ou de revenir à la langue du système (voir `translations/README.md`).
