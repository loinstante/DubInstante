// CHECK-based tests over translations/*.ts: every language is complete, keeps
// the placeholders its source uses, and every source text is English.
// Build: cmake --build build --target test_translations && ./build/test_translations

#include <QDir>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QStringList>
#include <QXmlStreamReader>

#include "check.h"
#include <algorithm>
#include <cstdio>

namespace {

struct Message {
  QString source;
  bool numerus = false;
  QString type; // "unfinished", "vanished", "obsolete" or empty
  QStringList forms;
};

QList<Message> readTs(const QString &path) {
  QList<Message> messages;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return messages;
  QXmlStreamReader xml(&file);
  Message current;
  while (!xml.atEnd()) {
    xml.readNext();
    if (xml.isStartElement()) {
      if (xml.name() == u"message") {
        current = Message();
        current.numerus = xml.attributes().value("numerus") == u"yes";
      } else if (xml.name() == u"source") {
        current.source = xml.readElementText();
      } else if (xml.name() == u"translation") {
        current.type = xml.attributes().value("type").toString();
        if (!current.numerus)
          current.forms << xml.readElementText();
      } else if (xml.name() == u"numerusform") {
        current.forms << xml.readElementText();
      }
    } else if (xml.isEndElement() && xml.name() == u"message") {
      messages << current;
    }
  }
  return xml.hasError() ? QList<Message>() : messages; // main() checks for empty
}

// %1..%99, %L1 and %n: a lost one silently drops a value from the message
QStringList placeholders(const QString &text, bool withCount) {
  static const QRegularExpression re("%(L?\\d+|n)");
  QStringList found;
  for (auto it = re.globalMatch(text); it.hasNext();) {
    const QString token = it.next().captured(0);
    if (withCount || token != "%n")
      found << token;
  }
  found.sort();
  return found;
}

// A French word left in a tr() source shows untranslated to every other language
bool hasLetterBeyondAscii(const QString &text) {
  for (const QChar c : text) {
    if (c.unicode() > 127 && c.isLetter())
      return true;
  }
  return false;
}

} // namespace

int main() {
  // Plural forms Qt expects per language (see Qt Linguist numerus rules)
  const QHash<QString, int> pluralForms = {
      {"en", 2}, {"fr", 2}, {"es", 2}, {"pt_BR", 2}, {"de", 2},
      {"it", 2}, {"ja", 1}, {"zh_CN", 1}, {"ru", 3},   {"ar", 6}};

  const QDir dir(TRANSLATIONS_DIR);
  const QStringList files = dir.entryList({"dubinstante_*.ts"}, QDir::Files);
  CHECK(files.size() == pluralForms.size());

  for (const QString &fileName : files) {
    const QString lang = fileName.mid(12, fileName.size() - 12 - 3);
    CHECK(pluralForms.contains(lang));
    const QList<Message> messages = readTs(dir.filePath(fileName));
    CHECK(!messages.isEmpty());
    int problems = 0;

    for (const Message &m : messages) {
      CHECK(m.type != "vanished" && m.type != "obsolete");
      if (hasLetterBeyondAscii(m.source)) {
        printf("%s: non-English source: %s\n", qPrintable(lang), qPrintable(m.source));
        ++problems;
      }
      // English only fills its plural forms: the source is the text
      if (lang == "en" && !m.numerus)
        continue;

      const bool complete = m.type.isEmpty() && !m.forms.isEmpty() &&
                            std::none_of(m.forms.begin(), m.forms.end(),
                                         [](const QString &f) { return f.isEmpty(); });
      if (!complete || (m.numerus && m.forms.size() != pluralForms.value(lang))) {
        printf("%s: untranslated or wrong plural count: %s\n", qPrintable(lang),
               qPrintable(m.source));
        ++problems;
        continue;
      }
      for (const QString &form : m.forms) {
        // A plural form may spell the number out ("Every minute") and drop %n
        if (placeholders(form, !m.numerus) != placeholders(m.source, !m.numerus) ||
            form.count('\n') != m.source.count('\n')) {
          printf("%s: placeholders or line breaks differ: %s -> %s\n", qPrintable(lang),
                 qPrintable(m.source), qPrintable(form));
          ++problems;
        }
      }
    }
    CHECK(problems == 0);
  }

  printf("test_translations: OK\n");
  return 0;
}
