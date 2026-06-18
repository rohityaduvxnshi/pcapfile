#ifndef ICDREVIEWDRAFTBUILDER_H
#define ICDREVIEWDRAFTBUILDER_H

#include "IcdImportTypes.h"

#include <QList>
#include <QStringList>

// Builds ICD review drafts without treating incomplete field properties as
// row-fatal. Missing/unmapped/invalid Name, ByteOffset, DataType or Length are
// carried as empty editable cells in IcdFieldDraftRow; final MessageDefinition
// validation remains in IcdImportDialog::onAccept().
class IcdReviewDraftBuilder
{
public:
    static void buildGroupedDrafts(const IcdDocument& doc,
                                   const QList<IcdTableGroup>& groups,
                                   QList<IcdMessageDraft>& drafts,
                                   QStringList& globalWarnings);
};

#endif // ICDREVIEWDRAFTBUILDER_H
