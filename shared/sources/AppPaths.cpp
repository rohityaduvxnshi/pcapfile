#include "AppPaths.h"

#include <QDir>
#include <QStandardPaths>

namespace
{
// Create `path` (and any missing parents) and return it. mkpath is a no-op when
// the directory already exists, so this is cheap to call on every access.
QString ensureDir(const QString& path)
{
    QDir().mkpath(path);
    return path;
}
}

QString AppPaths::suiteRootDir()
{
    QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (docs.isEmpty())
        docs = QDir::homePath();
    return ensureDir(QDir(docs).filePath("UniversalDataSuite"));
}

QString AppPaths::outputFilesDir()
{
    return ensureDir(QDir(suiteRootDir()).filePath("Output Files"));
}

QString AppPaths::projectsDir()
{
    return ensureDir(QDir(suiteRootDir()).filePath("Projects"));
}
