#pragma once

#include <QString>
#include <QStringList>

class QApplication;

// Gestion de la langue de l'interface.
//
// Les textes de l'interface sont écrits en anglais dans le code (tr("...")).
// Chaque langue est un fichier JSON embarqué dans l'exécutable
// (gui/translations/<code>.json, code ISO 639-1) : { "texte anglais": "traduction" }.
// L'anglais est la langue d'origine : il n'a pas besoin de fichier.
namespace i18n
{

// Codes ISO 639-1 des langues disponibles, triés par nom
QStringList availableLanguages();

// Nom de la langue dans cette langue, ex. "Français"
QString nativeName(QString const& code);

// Écriture de droite à gauche (arabe, hébreu, persan, ourdou...)
bool isRightToLeft(QString const& code);

// Meilleure langue disponible pour la langue de l'ordinateur ("en" à défaut)
QString systemLanguage();

// Langue choisie par l'utilisateur ("" = automatique : langue de l'ordinateur)
QString savedLanguage();
void setSavedLanguage(QString const& code);

// Langue réellement utilisée : choix de l'utilisateur s'il existe, sinon celle de l'ordinateur
QString effectiveLanguage();

// À appeler au démarrage, avant de créer les fenêtres : installe la traduction
// et le sens de lecture (gauche-droite ou droite-gauche)
void install(QApplication& app);

} // namespace i18n
