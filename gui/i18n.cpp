#include "i18n.h"

#include <algorithm>

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QLocale>
#include <QSet>
#include <QSettings>
#include <QTranslator>

namespace
{

constexpr char const* settingsKeyLanguage = "language";

QSettings makeSettings()
{
    return QSettings(QStringLiteral("Pico-ASHA"), QStringLiteral("PicoASHAGui"));
}

// Traducteur lisant un fichier JSON { "texte anglais": "traduction" }
class JsonTranslator : public QTranslator
{
public:
    using QTranslator::QTranslator;

    bool loadLanguage(QString const& code)
    {
        QFile f(QStringLiteral(":/i18n/%1.json").arg(code));
        if (!f.open(QIODevice::ReadOnly)) {
            return false;
        }
        QJsonParseError err;
        auto const doc = QJsonDocument::fromJson(f.readAll(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            return false;
        }
        auto const obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            if (it.key().startsWith(QLatin1Char('@'))) {
                continue;   // métadonnées
            }
            QString const value = it.value().toString();
            if (!value.isEmpty()) {
                m_map.insert(it.key(), value);
            }
        }
        return !m_map.isEmpty();
    }

    QString translate(char const* context, char const* sourceText,
                      char const* disambiguation = nullptr, int n = -1) const override
    {
        Q_UNUSED(context);
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);
        auto const it = m_map.constFind(QString::fromUtf8(sourceText));
        return it == m_map.constEnd() ? QString() : it.value();
    }

    bool isEmpty() const override
    {
        return m_map.isEmpty();
    }

private:
    QHash<QString, QString> m_map;
};

// "fr-FR", "zh_Hans_CN", "iw" -> "fr", "zh", "he"
QString baseCode(QString tag)
{
    tag = tag.toLower().replace(QLatin1Char('_'), QLatin1Char('-'));
    QString const lang = tag.section(QLatin1Char('-'), 0, 0);
    static QHash<QString, QString> const aliases = {
        {"iw", "he"}, {"in", "id"}, {"ji", "yi"}, {"jw", "jv"}, {"mo", "ro"},
    };
    return aliases.value(lang, lang);
}

} // namespace

namespace i18n
{

QStringList availableLanguages()
{
    QStringList codes;
    codes << QStringLiteral("en");
    for (auto const& name : QDir(QStringLiteral(":/i18n")).entryList({"*.json"}, QDir::Files)) {
        QString const code = name.left(name.size() - 5);   // sans ".json"
        if (!code.startsWith(QLatin1Char('_')) && !codes.contains(code)) {
            codes << code;
        }
    }
    std::sort(codes.begin(), codes.end(), [](QString const& a, QString const& b) {
        return nativeName(a).localeAwareCompare(nativeName(b)) < 0;
    });
    return codes;
}

QString nativeName(QString const& code)
{
    QLocale const loc(code);
    QString name = (loc.language() == QLocale::C) ? code : loc.nativeLanguageName();
    if (name.isEmpty()) {
        name = code;
    }
    // Première lettre en majuscule (Qt donne "français", "español"...)
    name[0] = name.at(0).toUpper();
    return name;
}

bool isRightToLeft(QString const& code)
{
    static QSet<QString> const rtl = {"ar", "he", "fa", "ur", "ps", "sd", "ug", "yi", "dv", "ks"};
    return rtl.contains(code) || QLocale(code).textDirection() == Qt::RightToLeft;
}

QString systemLanguage()
{
    QStringList const available = availableLanguages();
    for (auto const& tag : QLocale::system().uiLanguages()) {
        QString code = baseCode(tag);
        if (available.contains(code)) {
            return code;
        }
        // Norvégien : nb / nn / no sont équivalents pour nous
        if (code == "nb" || code == "nn") {
            code = "no";
        } else if (code == "no") {
            code = "nb";
        }
        if (available.contains(code)) {
            return code;
        }
    }
    return QStringLiteral("en");
}

QString savedLanguage()
{
    return makeSettings().value(settingsKeyLanguage).toString();
}

void setSavedLanguage(QString const& code)
{
    auto settings = makeSettings();
    if (code.isEmpty()) {
        settings.remove(settingsKeyLanguage);   // retour à la langue de l'ordinateur
    } else {
        settings.setValue(settingsKeyLanguage, code);
    }
}

QIcon flagIcon(QString const& code)
{
    QString const path = QStringLiteral(":/flags/%1.png").arg(code);
    return QFile::exists(path) ? QIcon(path) : QIcon();
}

QIcon autoIcon()
{
    return QIcon(QStringLiteral(":/flags/auto.png"));
}

QString effectiveLanguage()
{
    QString const saved = savedLanguage();
    if (!saved.isEmpty() && availableLanguages().contains(saved)) {
        return saved;
    }
    return systemLanguage();
}

void install(QApplication& app)
{
    QString const code = effectiveLanguage();

    // Textes de l'application
    if (code != QLatin1String("en")) {
        auto* translator = new JsonTranslator(&app);
        if (translator->loadLanguage(code)) {
            QCoreApplication::installTranslator(translator);
        }
    }

    // Textes propres à Qt (boutons Oui/Non, boîtes de dialogue de fichiers...),
    // si les fichiers qt_xx.qm sont présents (dossier "translations" du zip Windows)
    auto* qtTranslator = new QTranslator(&app);
    QStringList const dirs = {
        QLibraryInfo::path(QLibraryInfo::TranslationsPath),
        QCoreApplication::applicationDirPath() + QStringLiteral("/translations"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../translations"),
    };
    for (auto const& dir : dirs) {
        if (qtTranslator->load(QLocale(code), QStringLiteral("qt"), QStringLiteral("_"), dir)) {
            QCoreApplication::installTranslator(qtTranslator);
            break;
        }
    }

    // Sens de lecture : les dispositions sont inversées pour l'écriture de droite à gauche
    app.setLayoutDirection(isRightToLeft(code) ? Qt::RightToLeft : Qt::LeftToRight);
}

} // namespace i18n
