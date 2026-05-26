#ifndef PROJECTFILE_H
#define PROJECTFILE_H

#include "AppTypes.h"
#include "FilterTypes.h"
#include "MessageDefinition.h"

#include <QList>
#include <QString>

struct ProjectState
{
    int appSchemaVersion;
    QString savedAtIso;
    QString pcapPath;
    QString inputMode;
    QString filterMode;
    int filterCount;
    FilterConfiguration filterConfig;
    QList< QList<MessageDefinition> > portMessagesByRow;
    QList<FieldDefinition> headerFields;
    QList<FieldDefinition> liveFields;
    FilterConfiguration liveFilterConfig;

    // v12: per-header-row length filters + global live-mode length filters. Empty
    // by default; populated when the user uses the new "Manage Length Filters"
    // affordances inside header / live modes.
    QList< QList<MessageDefinition> > headerMessagesByRow;
    QList<MessageDefinition> liveMessages;

    ProjectState()
        : appSchemaVersion(1),
          filterCount(1)
    {
    }
};

class ProjectFile
{
public:
    static bool save(const ProjectState& state, const QString& path, QString& errorMessage);
    static bool load(const QString& path, ProjectState& state, QString& errorMessage);

    static QString sidecarPathFor(const QString& pcapPath);
    static bool exists(const QString& path);

    // v10: per-field-list JSON for human editing. The exported JSON nests bit decoder
    // and conditional decoder configs as objects (not stringified JSON) so they can be
    // hand-edited. Import accepts either nested objects OR stringified JSON for
    // backward compatibility with raw BitfieldDecoder::rulesToJson output.
    static QString fieldListToJson(const QList<FieldDefinition>& fields);
    static bool fieldListFromJson(const QString& jsonText,
                                  QList<FieldDefinition>& fields,
                                  QString& errorMessage);
};

#endif // PROJECTFILE_H
