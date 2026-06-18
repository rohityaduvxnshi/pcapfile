#include "SimFieldConfigurationDialog.h"
#include "ui_SimFieldConfigurationDialog.h"

#include "FieldCsvCodec.h"
#include "BitValueEditorDialog.h"
#include "IcdDocxImporter.h"
#include "IcdImportDialog.h"
#include "InputValidator.h"
#include "PayloadBuilder.h"
#include "SimSetupFile.h"
#include "Themes.h"

#include <QAction>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSet>
#include <QShortcut>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

#include <algorithm>

namespace
{
const int FIELD_COL_NAME       = 0;
const int FIELD_COL_BYTE       = 1;
const int FIELD_COL_TYPE       = 2;
const int FIELD_COL_LENGTH     = 3;
const int FIELD_COL_ENDIAN     = 4;
const int FIELD_COL_RESOLUTION = 5;
const int FIELD_COL_VALUE      = 6;
const int FIELD_COL_HEX        = 7;
const int FIELD_COL_BITS       = 8;

// Make `base` unique against names already in `takenLower` (lower-cased),
// suffixing _1, _2, ... Empty names are returned unchanged and not tracked.
QString makeUniqueName(const QString& base, QSet<QString>& takenLower)
{
    if (base.trimmed().isEmpty())
        return base;
    if (!takenLower.contains(base.toLower()))
    {
        takenLower.insert(base.toLower());
        return base;
    }
    for (int n = 1; ; ++n)
    {
        const QString candidate = QString("%1_%2").arg(base).arg(n);
        if (!takenLower.contains(candidate.toLower()))
        {
            takenLower.insert(candidate.toLower());
            return candidate;
        }
    }
}

bool isKnownDataType(FieldDataType dataType)
{
    switch (dataType)
    {
    case FieldDataType::RawUnsignedBE:
    case FieldDataType::Uint8:
    case FieldDataType::Int8:
    case FieldDataType::Uint16:
    case FieldDataType::Int16:
    case FieldDataType::Uint32:
    case FieldDataType::Int32:
    case FieldDataType::Uint64:
    case FieldDataType::Int64:
    case FieldDataType::Float32:
    case FieldDataType::Float64:
    case FieldDataType::Bool:
    case FieldDataType::String:
        return true;
    }

    return false;
}

void addDataTypeItem(QComboBox* combo, const QString& label, FieldDataType dataType)
{
    combo->addItem(label, static_cast<int>(dataType));
}

QString resolutionTextForField(const FieldDefinition& field)
{
    const QString expr = field.resolutionExpression.trimmed();
    if (!expr.isEmpty() && expr != "1")
        return expr;
    return QString::number(field.resolution, 'g', 15);
}
} // namespace

SimFieldConfigurationDialog::SimFieldConfigurationDialog(QWidget* parent)
    : QDialog(parent),
      m_payloadLengthBytes(0),
      m_refreshing(false),
      ui(new Ui::SimFieldConfigurationDialog)
{
    ui->setupUi(this);
    Themes::apply(this);
    setAcceptDrops(true);

    ui->tblFields->setColumnCount(9);
    ui->tblFields->setHorizontalHeaderLabels(QStringList()
        << "Field Name" << "Byte Offset" << "Type" << "Length" << "Endian"
        << "Resolution" << "Value" << "Hex (auto)" << "Bits");
    ui->tblFields->horizontalHeader()->setStretchLastSection(true);
    ui->tblFields->setSelectionBehavior(QAbstractItemView::SelectRows);
    // Multi-select (Ctrl/Shift) so several fields can be deleted at once.
    ui->tblFields->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // Drag a row (grab a text cell) to reorder. Qt's built-in InternalMove
    // corrupts cell widgets, so we let Qt drive the drag visuals but intercept
    // the drop ourselves (eventFilter) and reorder at the data level.
    ui->tblFields->setDragEnabled(true);
    ui->tblFields->viewport()->setAcceptDrops(true);
    ui->tblFields->setDropIndicatorShown(true);
    ui->tblFields->setDragDropMode(QAbstractItemView::InternalMove);
    ui->tblFields->setDragDropOverwriteMode(false);
    ui->tblFields->viewport()->installEventFilter(this);

    connect(ui->btnAddField, SIGNAL(clicked()), this, SLOT(onAddFieldClicked()));
    connect(ui->btnEditField, SIGNAL(clicked()), this, SLOT(onEditFieldClicked()));
    connect(ui->btnRemoveField, SIGNAL(clicked()), this, SLOT(onRemoveFieldClicked()));
    connect(ui->tblFields, SIGNAL(itemChanged(QTableWidgetItem*)),
            this, SLOT(onFieldCellChanged(QTableWidgetItem*)));

    QMenu* csvMenu = new QMenu(this);
    QAction* csvImport   = csvMenu->addAction("Import CSV...");
    QAction* csvExport   = csvMenu->addAction("Export CSV...");
    QAction* csvTemplate = csvMenu->addAction("Template...");
    ui->btnCsvMenu->setMenu(csvMenu);
    connect(csvImport,   SIGNAL(triggered()), this, SLOT(onImportCsvClicked()));
    connect(csvExport,   SIGNAL(triggered()), this, SLOT(onExportCsvClicked()));
    connect(csvTemplate, SIGNAL(triggered()), this, SLOT(onTemplateCsvClicked()));

    QMenu* jsonMenu = new QMenu(this);
    QAction* jsonImport = jsonMenu->addAction("Import JSON...");
    QAction* jsonExport = jsonMenu->addAction("Export JSON...");
    ui->btnJsonMenu->setMenu(jsonMenu);
    connect(jsonImport, SIGNAL(triggered()), this, SLOT(onImportJsonClicked()));
    connect(jsonExport, SIGNAL(triggered()), this, SLOT(onExportJsonClicked()));

    connect(ui->btnImportIcd, SIGNAL(clicked()), this, SLOT(onImportIcdClicked()));

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onSaveClicked()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    // Same fast-table shortcuts as the parser app: Insert adds a row,
    // Ctrl+E edits the selected row, Ctrl+Delete removes it.
    QShortcut* scAdd = new QShortcut(QKeySequence(Qt::Key_Insert), this, SLOT(onAddFieldClicked()));
    scAdd->setContext(Qt::WidgetWithChildrenShortcut);
    QShortcut* scEdit = new QShortcut(QKeySequence("Ctrl+E"), this, SLOT(onEditFieldClicked()));
    scEdit->setContext(Qt::WidgetWithChildrenShortcut);
    QShortcut* scRemove = new QShortcut(QKeySequence("Ctrl+Delete"), this, SLOT(onRemoveFieldClicked()));
    scRemove->setContext(Qt::WidgetWithChildrenShortcut);
    // Secondary keyboard path for reordering (drag-and-drop is the primary way).
    QShortcut* scUp = new QShortcut(QKeySequence("Alt+Up"), this, SLOT(onMoveFieldUpClicked()));
    scUp->setContext(Qt::WidgetWithChildrenShortcut);
    QShortcut* scDown = new QShortcut(QKeySequence("Alt+Down"), this, SLOT(onMoveFieldDownClicked()));
    scDown->setContext(Qt::WidgetWithChildrenShortcut);
    ui->btnAddField->setToolTip("Add a new field row (Insert).");
    ui->btnEditField->setToolTip("Edit the selected field's name (Ctrl+E).");
    ui->btnRemoveField->setToolTip("Remove the selected field(s) (Ctrl+Delete). Shift/Ctrl-click to select several.");
    ui->tblFields->setToolTip("Drag a row (grab a text cell) to reorder; Alt+Up / Alt+Down also move the selected row.");
}

SimFieldConfigurationDialog::~SimFieldConfigurationDialog()
{
    delete ui;
}

void SimFieldConfigurationDialog::setPayloadLength(int payloadLengthBytes)
{
    m_payloadLengthBytes = payloadLengthBytes;
    if (m_payloadLengthBytes > 0)
    {
        ui->lblInfo->setText(QString("Configure the fields to transmit. Message payload length: %1 byte(s). "
                                     "Type each field's Value in its own type — the Hex column shows the exact "
                                     "bytes that will be sent, updated as you type.")
                                 .arg(m_payloadLengthBytes));
    }
    else
    {
        ui->lblInfo->setText("Configure the fields to transmit. Type each field's Value in its own type — "
                             "the Hex column shows the exact bytes that will be sent, updated as you type.");
    }
}

void SimFieldConfigurationDialog::setFields(const QList<FieldDefinition>& fields)
{
    m_fields = fields;
    refreshFieldTable();
}

QList<FieldDefinition> SimFieldConfigurationDialog::fields() const
{
    return m_fields;
}

QString SimFieldConfigurationDialog::tableText(int row, int column) const
{
    QTableWidgetItem* item = ui->tblFields->item(row, column);
    return item ? item->text().trimmed() : QString();
}

QString SimFieldConfigurationDialog::valueText(int row) const
{
    QTableWidgetItem* item = ui->tblFields->item(row, FIELD_COL_VALUE);
    return item ? item->text() : QString();
}

int SimFieldConfigurationDialog::selectedFieldRow() const
{
    QList<QTableWidgetItem*> selectedItems = ui->tblFields->selectedItems();
    if (!selectedItems.isEmpty())
        return selectedItems.first()->row();
    return ui->tblFields->currentRow();
}

void SimFieldConfigurationDialog::setTypeCell(int row, FieldDataType dataType)
{
    QComboBox* combo = new QComboBox(ui->tblFields);
    addDataTypeItem(combo, "Raw Unsigned BE", FieldDataType::RawUnsignedBE);
    addDataTypeItem(combo, "bool", FieldDataType::Bool);
    addDataTypeItem(combo, "uchar", FieldDataType::Uint8);
    addDataTypeItem(combo, "char", FieldDataType::Int8);
    addDataTypeItem(combo, "ushort", FieldDataType::Uint16);
    addDataTypeItem(combo, "short", FieldDataType::Int16);
    addDataTypeItem(combo, "uint", FieldDataType::Uint32);
    addDataTypeItem(combo, "int", FieldDataType::Int32);
    addDataTypeItem(combo, "ulong", FieldDataType::Uint64);
    addDataTypeItem(combo, "long", FieldDataType::Int64);
    addDataTypeItem(combo, "float", FieldDataType::Float32);
    addDataTypeItem(combo, "double", FieldDataType::Float64);
    addDataTypeItem(combo, "string", FieldDataType::String);

    const int typeIndex = combo->findData(static_cast<int>(dataType));
    combo->setCurrentIndex(typeIndex >= 0 ? typeIndex : 0);

    ui->tblFields->setCellWidget(row, FIELD_COL_TYPE, combo);

    connect(combo,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [this, combo](int)
            {
                const int comboRow = rowForTypeCombo(combo);
                if (comboRow >= 0)
                {
                    applyLengthStateForType(comboRow, dataTypeForRow(comboRow));
                    refreshHexCell(comboRow);
                    setBitsCell(comboRow);
                }
            });

    // On initial load, only ensure the length cell is editable; do NOT overwrite
    // the saved length value.
    QTableWidgetItem* lengthItem = ui->tblFields->item(row, FIELD_COL_LENGTH);
    if (!lengthItem)
    {
        lengthItem = new QTableWidgetItem("1");
        ui->tblFields->setItem(row, FIELD_COL_LENGTH, lengthItem);
    }
    lengthItem->setFlags(lengthItem->flags() | Qt::ItemIsEditable);
}

void SimFieldConfigurationDialog::setEndianCell(int row, FieldEndianness endianness)
{
    QComboBox* combo = new QComboBox(ui->tblFields);
    combo->addItem("Big-endian", static_cast<int>(FieldEndianness::Big));
    combo->addItem("Little-endian", static_cast<int>(FieldEndianness::Little));
    combo->setCurrentIndex(endianness == FieldEndianness::Little ? 1 : 0);
    combo->setToolTip("Byte order on the wire. Big-endian matches the parser app (default); "
                      "Little-endian reverses a numeric field's bytes. No effect on string fields.");
    ui->tblFields->setCellWidget(row, FIELD_COL_ENDIAN, combo);

    connect(combo,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [this, combo](int)
            {
                const int comboRow = rowForEndianCombo(combo);
                if (comboRow >= 0)
                    refreshHexCell(comboRow);
            });
}

FieldDataType SimFieldConfigurationDialog::dataTypeForRow(int row) const
{
    QWidget* widget = ui->tblFields->cellWidget(row, FIELD_COL_TYPE);
    QComboBox* combo = qobject_cast<QComboBox*>(widget);
    if (!combo)
        return FieldDataType::RawUnsignedBE;

    const FieldDataType dataType = static_cast<FieldDataType>(combo->currentData().toInt());
    return isKnownDataType(dataType) ? dataType : FieldDataType::RawUnsignedBE;
}

FieldEndianness SimFieldConfigurationDialog::endiannessForRow(int row) const
{
    QWidget* widget = ui->tblFields->cellWidget(row, FIELD_COL_ENDIAN);
    QComboBox* combo = qobject_cast<QComboBox*>(widget);
    if (!combo)
        return FieldEndianness::Big;
    return static_cast<FieldEndianness>(combo->currentData().toInt());
}

void SimFieldConfigurationDialog::applyLengthStateForType(int row, FieldDataType dataType)
{
    if (row < 0 || row >= ui->tblFields->rowCount())
        return;

    QTableWidgetItem* lengthItem = ui->tblFields->item(row, FIELD_COL_LENGTH);
    if (!lengthItem)
    {
        lengthItem = new QTableWidgetItem("1");
        ui->tblFields->setItem(row, FIELD_COL_LENGTH, lengthItem);
    }

    const int naturalLength = fieldDataTypeNaturalLength(dataType);
    if (naturalLength > 0)
        lengthItem->setText(QString::number(naturalLength));

    lengthItem->setFlags(lengthItem->flags() | Qt::ItemIsEditable);
}

int SimFieldConfigurationDialog::rowForTypeCombo(const QWidget* combo) const
{
    for (int row = 0; row < ui->tblFields->rowCount(); ++row)
    {
        if (ui->tblFields->cellWidget(row, FIELD_COL_TYPE) == combo)
            return row;
    }

    return -1;
}

int SimFieldConfigurationDialog::rowForEndianCombo(const QWidget* combo) const
{
    for (int row = 0; row < ui->tblFields->rowCount(); ++row)
    {
        if (ui->tblFields->cellWidget(row, FIELD_COL_ENDIAN) == combo)
            return row;
    }

    return -1;
}

int SimFieldConfigurationDialog::rowForBitsButton(const QWidget* button) const
{
    for (int row = 0; row < ui->tblFields->rowCount(); ++row)
    {
        if (ui->tblFields->cellWidget(row, FIELD_COL_BITS) == button)
            return row;
    }

    return -1;
}

// Best-effort row -> FieldDefinition for the live hex preview and the bit
// editor. Returns false (with a short reason) when the row is not usable yet.
bool SimFieldConfigurationDialog::fieldFromRow(int row, FieldDefinition& field, QString& problem) const
{
    field = FieldDefinition();
    field.name = tableText(row, FIELD_COL_NAME);
    field.byteOffset = tableText(row, FIELD_COL_BYTE).toInt();
    field.byteOffsetcorrect = field.byteOffset - 1;
    field.dataType = dataTypeForRow(row);
    field.endianness = endiannessForRow(row);
    field.sendValueText = valueText(row);

    bool lengthOk = false;
    const int length = tableText(row, FIELD_COL_LENGTH).toInt(&lengthOk, 10);
    if (!lengthOk || length < 1)
    {
        problem = "Length must be a whole number of bytes (1 or more).";
        return false;
    }
    field.length = length;

    const QString resText = tableText(row, FIELD_COL_RESOLUTION);
    double resolution = 1.0;
    if (!resText.isEmpty())
    {
        QString resError;
        if (!InputValidator::solveResolutionExpression(resText, resolution, resError))
        {
            problem = resError;
            return false;
        }
    }
    field.resolution = resolution;
    field.resolutionExpression = resText.isEmpty() ? QString("1") : resText;

    return true;
}

// The real-time "small box next to the input": the read-only Hex cell that
// mirrors the Value cell as the user types.
void SimFieldConfigurationDialog::refreshHexCell(int row)
{
    if (row < 0 || row >= ui->tblFields->rowCount())
        return;

    m_refreshing = true;

    QTableWidgetItem* hexItem = ui->tblFields->item(row, FIELD_COL_HEX);
    if (!hexItem)
    {
        hexItem = new QTableWidgetItem();
        ui->tblFields->setItem(row, FIELD_COL_HEX, hexItem);
    }
    hexItem->setFlags(hexItem->flags() & ~Qt::ItemIsEditable);

    FieldDefinition field;
    QString problem;
    QString display;
    QString tooltip;
    bool error = false;

    if (!fieldFromRow(row, field, problem))
    {
        display = QString::fromUtf8("—");
        tooltip = problem;
        error = true;
    }
    else if (field.sendValueText.trimmed().isEmpty())
    {
        display.clear();
        tooltip = "Type a Value to see the exact bytes that will be transmitted.";
    }
    else
    {
        QString shortError;
        const QString hex = PayloadBuilder::fieldHexPreview(field, field.sendValueText, shortError);
        if (hex.isEmpty())
        {
            display = QString::fromUtf8("—");
            tooltip = shortError;
            error = true;
        }
        else
        {
            display = hex;
            const char* order = (field.endianness == FieldEndianness::Little) ? "little-endian" : "big-endian";
            tooltip = QString("Exactly what will be transmitted for this field (%1, %2 byte(s)).")
                          .arg(order).arg(field.length);
        }
    }

    hexItem->setText(display);
    hexItem->setToolTip(tooltip);
    if (error)
        hexItem->setForeground(QBrush(QColor(231, 76, 60)));
    else
        hexItem->setData(Qt::ForegroundRole, QVariant());

    m_refreshing = false;
}

void SimFieldConfigurationDialog::setBitsCell(int row)
{
    QPushButton* button = qobject_cast<QPushButton*>(ui->tblFields->cellWidget(row, FIELD_COL_BITS));
    if (!button)
    {
        button = new QPushButton("Bits...", ui->tblFields);
        ui->tblFields->setCellWidget(row, FIELD_COL_BITS, button);
        connect(button, SIGNAL(clicked()), this, SLOT(onBitsRowClicked()));
    }

    FieldDefinition field;
    QString problem;
    const bool available = fieldFromRow(row, field, problem)
        && PayloadBuilder::fieldSupportsBitEditing(field);

    button->setEnabled(available);
    button->setToolTip(available
        ? "Open the bit editor: type a value or toggle every bit of every byte individually — "
          "the final value (in this field's type) updates in real time."
        : "Bit editing is available for integer / Raw Unsigned BE / bool fields of 1 to 8 bytes.");
}

void SimFieldConfigurationDialog::refreshFieldTable()
{
    m_refreshing = true;
    ui->tblFields->setRowCount(0);

    for (int i = 0; i < m_fields.size(); ++i)
    {
        const FieldDefinition& field = m_fields.at(i);
        const int row = ui->tblFields->rowCount();
        ui->tblFields->insertRow(row);

        ui->tblFields->setItem(row, FIELD_COL_NAME, new QTableWidgetItem(field.name));
        ui->tblFields->setItem(row, FIELD_COL_BYTE, new QTableWidgetItem(QString::number(field.byteOffset)));
        ui->tblFields->setItem(row, FIELD_COL_LENGTH, new QTableWidgetItem(QString::number(field.length)));
        setTypeCell(row, field.dataType); // reads FIELD_COL_LENGTH; must be set first
        setEndianCell(row, field.endianness);
        ui->tblFields->setItem(row, FIELD_COL_RESOLUTION, new QTableWidgetItem(resolutionTextForField(field)));
        ui->tblFields->setItem(row, FIELD_COL_VALUE, new QTableWidgetItem(field.sendValueText));
        ui->tblFields->setItem(row, FIELD_COL_HEX, new QTableWidgetItem(QString()));
    }

    m_refreshing = false;

    for (int row = 0; row < ui->tblFields->rowCount(); ++row)
    {
        refreshHexCell(row);
        setBitsCell(row);
    }

    ui->tblFields->resizeColumnsToContents();
    ui->tblFields->horizontalHeader()->setStretchLastSection(true);
}

void SimFieldConfigurationDialog::onFieldCellChanged(QTableWidgetItem* item)
{
    if (m_refreshing || !item)
        return;

    const int column = item->column();
    if (column == FIELD_COL_BYTE
        || column == FIELD_COL_LENGTH
        || column == FIELD_COL_RESOLUTION
        || column == FIELD_COL_VALUE)
    {
        refreshHexCell(item->row());
        if (column == FIELD_COL_LENGTH)
            setBitsCell(item->row());
    }
}

void SimFieldConfigurationDialog::onAddFieldClicked()
{
    m_refreshing = true;

    const int row = ui->tblFields->rowCount();
    ui->tblFields->insertRow(row);
    ui->tblFields->setItem(row, FIELD_COL_NAME, new QTableWidgetItem(QString()));
    ui->tblFields->setItem(row, FIELD_COL_BYTE, new QTableWidgetItem("1"));
    ui->tblFields->setItem(row, FIELD_COL_LENGTH, new QTableWidgetItem("1"));
    setTypeCell(row, FieldDataType::RawUnsignedBE);
    setEndianCell(row, FieldEndianness::Big);
    ui->tblFields->setItem(row, FIELD_COL_RESOLUTION, new QTableWidgetItem("1"));
    ui->tblFields->setItem(row, FIELD_COL_VALUE, new QTableWidgetItem(QString()));
    ui->tblFields->setItem(row, FIELD_COL_HEX, new QTableWidgetItem(QString()));

    m_refreshing = false;

    refreshHexCell(row);
    setBitsCell(row);
    ui->tblFields->setCurrentCell(row, FIELD_COL_NAME);
}

void SimFieldConfigurationDialog::onEditFieldClicked()
{
    const int row = selectedFieldRow();
    if (row < 0 || row >= ui->tblFields->rowCount())
    {
        QMessageBox::warning(this, "Edit Field", "Select a field row first.");
        return;
    }

    QTableWidgetItem* nameItem = ui->tblFields->item(row, FIELD_COL_NAME);
    if (nameItem)
    {
        ui->tblFields->setCurrentItem(nameItem);
        ui->tblFields->editItem(nameItem);
    }
}

void SimFieldConfigurationDialog::onRemoveFieldClicked()
{
    // Collect the distinct selected rows (multi-select). Fall back to the
    // current row when nothing is selected.
    QSet<int> rowSet;
    const QList<QTableWidgetItem*> selected = ui->tblFields->selectedItems();
    for (int i = 0; i < selected.size(); ++i)
        rowSet.insert(selected.at(i)->row());
    if (rowSet.isEmpty() && ui->tblFields->currentRow() >= 0)
        rowSet.insert(ui->tblFields->currentRow());

    if (rowSet.isEmpty())
    {
        QMessageBox::warning(this, "Remove Field", "Select one or more field rows first.");
        return;
    }

    QList<int> rows = rowSet.toList();
    std::sort(rows.begin(), rows.end());

    QString prompt;
    if (rows.size() == 1)
    {
        const QString name = tableText(rows.first(), FIELD_COL_NAME);
        prompt = QString("Remove field '%1'?").arg(name.isEmpty() ? QString("Row %1").arg(rows.first() + 1) : name);
    }
    else
    {
        prompt = QString("Remove %1 selected field(s)?").arg(rows.size());
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(this,
        "Remove Field", prompt, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    // Remove high-index-first so earlier indices stay valid.
    for (int i = rows.size() - 1; i >= 0; --i)
        ui->tblFields->removeRow(rows.at(i));
}

QList<FieldDefinition> SimFieldConfigurationDialog::snapshotAllRows() const
{
    QList<FieldDefinition> out;
    for (int row = 0; row < ui->tblFields->rowCount(); ++row)
    {
        FieldDefinition f;
        f.name = tableText(row, FIELD_COL_NAME);
        f.byteOffset = tableText(row, FIELD_COL_BYTE).toInt();
        f.byteOffsetcorrect = f.byteOffset - 1;
        const int len = tableText(row, FIELD_COL_LENGTH).toInt();
        f.length = (len >= 1) ? len : 1;
        f.dataType = dataTypeForRow(row);
        f.endianness = endiannessForRow(row);
        const QString resText = tableText(row, FIELD_COL_RESOLUTION);
        f.resolutionExpression = resText.isEmpty() ? QString("1") : resText;
        double resolution = 1.0;
        QString resErr;
        if (!resText.isEmpty() && InputValidator::solveResolutionExpression(resText, resolution, resErr))
            f.resolution = resolution;
        else
            f.resolution = 1.0;
        f.sendValueText = valueText(row);
        out.append(f);
    }
    return out;
}

void SimFieldConfigurationDialog::reorderRows(QList<int> sourceRows, int targetRow)
{
    const int count = ui->tblFields->rowCount();
    if (sourceRows.isEmpty() || count <= 1)
        return;

    std::sort(sourceRows.begin(), sourceRows.end());

    QList<FieldDefinition> rows = snapshotAllRows();
    if (rows.size() != count)
        return;

    // Pull out the moved rows (keeping their order) and remember where to drop.
    QList<FieldDefinition> moved;
    for (int i = 0; i < sourceRows.size(); ++i)
        moved.append(rows.at(sourceRows.at(i)));

    // How many removed rows sit before the target → shift the insertion point.
    int insertAt = targetRow;
    for (int i = 0; i < sourceRows.size(); ++i)
        if (sourceRows.at(i) < targetRow)
            --insertAt;

    for (int i = sourceRows.size() - 1; i >= 0; --i)
        rows.removeAt(sourceRows.at(i));

    if (insertAt < 0) insertAt = 0;
    if (insertAt > rows.size()) insertAt = rows.size();
    for (int i = 0; i < moved.size(); ++i)
        rows.insert(insertAt + i, moved.at(i));

    setFields(rows);

    // Re-select the moved block at its new home for a smooth feel.
    ui->tblFields->clearSelection();
    for (int i = 0; i < moved.size(); ++i)
        ui->tblFields->selectRow(insertAt + i);
}

void SimFieldConfigurationDialog::moveSelectedRows(int delta)
{
    QSet<int> rowSet;
    const QList<QTableWidgetItem*> selected = ui->tblFields->selectedItems();
    for (int i = 0; i < selected.size(); ++i)
        rowSet.insert(selected.at(i)->row());
    if (rowSet.isEmpty() && ui->tblFields->currentRow() >= 0)
        rowSet.insert(ui->tblFields->currentRow());
    if (rowSet.isEmpty())
        return;

    QList<int> rows = rowSet.toList();
    std::sort(rows.begin(), rows.end());

    const int count = ui->tblFields->rowCount();
    if (delta < 0)
    {
        if (rows.first() <= 0) return;          // already at top
        reorderRows(rows, rows.first() - 1);
    }
    else
    {
        if (rows.last() >= count - 1) return;   // already at bottom
        reorderRows(rows, rows.last() + 2);     // +2: target is past the row below
    }
}

void SimFieldConfigurationDialog::onMoveFieldUpClicked()
{
    moveSelectedRows(-1);
}

void SimFieldConfigurationDialog::onMoveFieldDownClicked()
{
    moveSelectedRows(+1);
}

bool SimFieldConfigurationDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->tblFields->viewport()
        && (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove))
    {
        // Let file drags reach the Drop handler (the item view would otherwise
        // reject them); internal row drags fall through to Qt's move indicator.
        QDropEvent* move = static_cast<QDropEvent*>(event);
        if (move->mimeData() && move->mimeData()->hasUrls())
        {
            move->acceptProposedAction();
            return true;
        }
        return false;
    }

    if (watched == ui->tblFields->viewport() && event->type() == QEvent::Drop)
    {
        QDropEvent* drop = static_cast<QDropEvent*>(event);

        // A file dragged in from outside still imports (works over the table too).
        if (drop->mimeData() && drop->mimeData()->hasUrls())
        {
            const QList<QUrl> urls = drop->mimeData()->urls();
            for (int i = 0; i < urls.size(); ++i)
            {
                const QString path = urls.at(i).toLocalFile();
                if (path.endsWith(".csv", Qt::CaseInsensitive)) { drop->acceptProposedAction(); importCsvFromPath(path); return true; }
                if (path.endsWith(".json", Qt::CaseInsensitive)) { drop->acceptProposedAction(); importJsonFromPath(path); return true; }
            }
            return true;
        }

        // Internal row move: reorder at the data level and consume the event so
        // Qt's default move never runs (it would corrupt the cell widgets).
        QSet<int> rowSet;
        const QList<QTableWidgetItem*> selected = ui->tblFields->selectedItems();
        for (int i = 0; i < selected.size(); ++i)
            rowSet.insert(selected.at(i)->row());
        if (rowSet.isEmpty())
            return true;

        int targetRow = ui->tblFields->rowAt(drop->pos().y());
        if (targetRow < 0)
            targetRow = ui->tblFields->rowCount(); // dropped past the last row → append

        const QList<int> rows = rowSet.toList();
        drop->acceptProposedAction();
        // Rebuild the table AFTER the drag machinery unwinds (modifying the model
        // inside the drop handler is unsafe), so defer to the next event loop turn.
        QTimer::singleShot(0, this, [this, rows, targetRow]() { reorderRows(rows, targetRow); });
        return true;
    }
    return QDialog::eventFilter(watched, event);
}

void SimFieldConfigurationDialog::onBitsRowClicked()
{
    const int row = rowForBitsButton(qobject_cast<QWidget*>(sender()));
    if (row < 0)
        return;

    FieldDefinition field;
    QString problem;
    if (!fieldFromRow(row, field, problem))
    {
        QMessageBox::warning(this, "Bit Editor",
                             QString("This row cannot be bit-edited yet: %1").arg(problem));
        return;
    }

    if (!PayloadBuilder::fieldSupportsBitEditing(field))
    {
        QMessageBox::warning(this, "Bit Editor",
                             "Bit editing is available for integer / Raw Unsigned BE / bool fields of 1 to 8 bytes.\n"
                             "Solution: change the field's Type or Length, or type the value directly.");
        return;
    }

    const QString fieldLabel = field.name.isEmpty() ? QString("Row %1").arg(row + 1) : field.name;
    BitValueEditorDialog dlg(fieldLabel, field.dataType, field.length, field.resolution,
                             field.sendValueText, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QTableWidgetItem* item = ui->tblFields->item(row, FIELD_COL_VALUE);
    if (!item)
    {
        item = new QTableWidgetItem();
        ui->tblFields->setItem(row, FIELD_COL_VALUE, item);
    }
    item->setText(dlg.resultValueText()); // triggers onFieldCellChanged -> live hex refresh
}

bool SimFieldConfigurationDialog::collectFields(QList<FieldDefinition>& fields, QStringList& problems) const
{
    fields.clear();
    const int initialProblemCount = problems.size();

    for (int row = 0; row < ui->tblFields->rowCount(); ++row)
    {
        const QString name = tableText(row, FIELD_COL_NAME);
        const QString byteText = tableText(row, FIELD_COL_BYTE);
        const QString lengthText = tableText(row, FIELD_COL_LENGTH);
        const QString resolutionText = tableText(row, FIELD_COL_RESOLUTION);
        const QString value = valueText(row);

        if (name.isEmpty() && byteText.isEmpty() && lengthText.isEmpty()
            && resolutionText.isEmpty() && value.trimmed().isEmpty())
            continue; // fully blank row

        const QString prefix = QString("Row %1%2: ")
                                   .arg(row + 1)
                                   .arg(name.isEmpty() ? QString() : QString(" ('%1')").arg(name));

        if (name.isEmpty())
        {
            problems.append(prefix + "the field has no name. Solution: give every field a short unique name.");
            continue;
        }

        bool byteOk = false;
        const int byteOffset = byteText.toInt(&byteOk, 10);
        if (!byteOk || byteOffset < 1)
        {
            problems.append(prefix + QString("Byte Offset '%1' is not valid. Solution: use a whole number of 1 or more (the first payload byte is byte 1).").arg(byteText));
            continue;
        }

        bool lengthOk = false;
        const int length = lengthText.toInt(&lengthOk, 10);
        if (!lengthOk || length < 1)
        {
            problems.append(prefix + QString("Length '%1' is not valid. Solution: use a whole number of bytes (1 or more).").arg(lengthText));
            continue;
        }

        const FieldDataType dataType = dataTypeForRow(row);
        if (dataType != FieldDataType::String && length > 8)
        {
            problems.append(prefix + QString("Length %1 is more than 8 bytes, which only string fields support. Solution: choose the string type, or split the data into fields of 8 bytes or less.").arg(length));
            continue;
        }

        double resolution = 1.0;
        if (!resolutionText.isEmpty())
        {
            QString resError;
            if (!InputValidator::solveResolutionExpression(resolutionText, resolution, resError))
            {
                problems.append(prefix + resError + " Solution: use a number or a constant expression such as 0.01 or 1/3.");
                continue;
            }
        }

        FieldDefinition field;
        field.name = name;
        field.byteOffset = byteOffset;
        field.byteOffsetcorrect = byteOffset - 1;
        field.length = length;
        field.dataType = dataType;
        field.endianness = endiannessForRow(row);
        field.resolution = resolution;
        field.resolutionExpression = resolutionText.isEmpty() ? QString("1") : resolutionText;
        field.sendValueText = value;

        if (m_payloadLengthBytes > 0
            && (field.byteOffsetcorrect < 0 || field.byteOffsetcorrect + field.length > m_payloadLengthBytes))
        {
            problems.append(prefix + QString("the field ends at byte %1 but the message payload is only %2 byte(s). Solution: increase the message's Payload Length to at least %1, or reduce Byte Offset / Length.")
                                         .arg(field.byteOffsetcorrect + field.length)
                                         .arg(m_payloadLengthBytes));
            continue;
        }

        // An empty Value is allowed at save time (it can be filled in later);
        // a non-empty value must encode. The Send button re-verifies everything.
        if (!field.sendValueText.trimmed().isEmpty() || field.dataType == FieldDataType::String)
        {
            QByteArray bytes;
            QString reason;
            QString solution;
            if (!PayloadBuilder::encodeFieldValue(field, field.sendValueText, bytes, reason, solution))
            {
                problems.append(prefix + reason + " Solution: " + solution);
                continue;
            }
        }

        fields.append(field);
    }

    QString duplicateError;
    if (!InputValidator::validateFields(fields, duplicateError))
        problems.append(duplicateError + " Solution: make every field name unique.");

    return problems.size() == initialProblemCount;
}

void SimFieldConfigurationDialog::showProblems(const QString& title, const QStringList& problems)
{
    if (problems.size() <= 4)
    {
        QMessageBox::warning(this, title, problems.join("\n\n"));
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(title);
    box.setText(QString("%1 problem(s) found. Press Show Details for the full list with solutions.")
                    .arg(problems.size()));
    box.setDetailedText(problems.join("\n\n"));
    box.exec();
}

void SimFieldConfigurationDialog::onSaveClicked()
{
    QList<FieldDefinition> collectedFields;
    QStringList problems;
    if (!collectFields(collectedFields, problems))
    {
        showProblems("Invalid Fields", problems);
        return;
    }

    m_fields = collectedFields;
    accept();
}

void SimFieldConfigurationDialog::onImportCsvClicked()
{
    const QString path = QFileDialog::getOpenFileName(this,
        "Import Field Definitions from CSV",
        QString(),
        "CSV Files (*.csv);;All Files (*.*)");
    if (path.isEmpty()) return;

    importCsvFromPath(path);
}

void SimFieldConfigurationDialog::importCsvFromPath(const QString& path)
{
    QList<FieldDefinition> imported;
    QStringList warnings;
    QString error;
    if (!FieldCsvCodec::importFromCsv(path, m_payloadLengthBytes, imported, warnings, error))
    {
        QMessageBox::warning(this, "Import CSV",
            QString("Import failed:\n\n%1").arg(error));
        return;
    }
    if (imported.isEmpty())
    {
        QMessageBox::information(this, "Import CSV", "CSV contained no valid field rows.");
        return;
    }

    applyImportedFields(imported, warnings, "CSV");
}

void SimFieldConfigurationDialog::onExportCsvClicked()
{
    QList<FieldDefinition> collected;
    QStringList problems;
    if (!collectFields(collected, problems))
    {
        showProblems("Export CSV — fix the fields first", problems);
        return;
    }
    if (collected.isEmpty())
    {
        QMessageBox::information(this, "Export CSV", "No fields to export.");
        return;
    }

    const QString path = QFileDialog::getSaveFileName(this,
        "Export Field Definitions to CSV",
        "fields.csv",
        "CSV Files (*.csv)");
    if (path.isEmpty()) return;

    QString error;
    if (!FieldCsvCodec::exportToCsv(path, collected, error))
    {
        QMessageBox::warning(this, "Export CSV",
            QString("Export failed:\n%1").arg(error));
        return;
    }
    QMessageBox::information(this, "Export CSV",
        QString("Exported %1 field(s) to:\n%2\n\nThe Value column (the value to transmit) is included.")
            .arg(collected.size()).arg(path));
}

void SimFieldConfigurationDialog::onTemplateCsvClicked()
{
    const QString path = QFileDialog::getSaveFileName(this,
        "Save CSV Template",
        "field_template.csv",
        "CSV Files (*.csv)");
    if (path.isEmpty()) return;

    QString error;
    if (!FieldCsvCodec::writeTemplate(path, error))
    {
        QMessageBox::warning(this, "Save Template",
            QString("Failed to write template:\n%1").arg(error));
        return;
    }
    QMessageBox::information(this, "Save Template",
        QString("Template written to:\n%1").arg(path));
}

void SimFieldConfigurationDialog::onImportJsonClicked()
{
    const QString path = QFileDialog::getOpenFileName(this,
        "Import Field Definitions from JSON",
        QString(),
        "JSON Files (*.json);;All Files (*.*)");
    if (path.isEmpty()) return;

    importJsonFromPath(path);
}

void SimFieldConfigurationDialog::importJsonFromPath(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Import JSON",
            QString("Cannot open file:\n%1").arg(file.errorString()));
        return;
    }
    const QString jsonText = QString::fromUtf8(file.readAll());
    file.close();

    QList<FieldDefinition> imported;
    QString error;
    if (!SimSetupFile::fieldListFromJson(jsonText, imported, error))
    {
        QMessageBox::warning(this, "Import JSON",
            QString("Import failed:\n\n%1").arg(error));
        return;
    }

    applyImportedFields(imported, QStringList(), "JSON");
}

void SimFieldConfigurationDialog::onImportIcdClicked()
{
    const QString path = QFileDialog::getOpenFileName(this,
        "Import fields from an ICD (Word .docx)", QString(),
        "Word documents (*.docx);;All files (*.*)");
    if (path.isEmpty())
        return;

    IcdDocument doc;
    QString error;
    if (!IcdDocxImporter::extract(path, doc, error))
    {
        QMessageBox::warning(this, "Import ICD",
            QString("Could not read the ICD:\n\n%1").arg(error));
        return;
    }
    if (doc.tables.isEmpty())
    {
        QMessageBox::information(this, "Import ICD",
            "No tables were found in this .docx. Solution: make sure the ICD's field "
            "definitions are laid out in Word tables.");
        return;
    }

    IcdImportDialog dlg(this);
    dlg.setDocument(doc);
    if (dlg.exec() != QDialog::Accepted)
        return;

    // Flatten the chosen messages' fields into this message's field list.
    const QList<MessageDefinition> messages = dlg.selectedMessages();
    QList<FieldDefinition> imported;
    for (int i = 0; i < messages.size(); ++i)
        imported.append(messages.at(i).fields);

    if (imported.isEmpty())
    {
        QMessageBox::information(this, "Import ICD",
            "No complete fields were selected to import.");
        return;
    }

    applyImportedFields(imported, QStringList(), "ICD");
}

void SimFieldConfigurationDialog::onExportJsonClicked()
{
    QList<FieldDefinition> collected;
    QStringList problems;
    if (!collectFields(collected, problems))
    {
        showProblems("Export JSON — fix the fields first", problems);
        return;
    }
    if (collected.isEmpty())
    {
        QMessageBox::information(this, "Export JSON", "No fields to export.");
        return;
    }

    const QString path = QFileDialog::getSaveFileName(this,
        "Export Field Definitions to JSON",
        "fields.json",
        "JSON Files (*.json)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        QMessageBox::warning(this, "Export JSON",
            QString("Cannot open file for writing:\n%1").arg(file.errorString()));
        return;
    }
    file.write(SimSetupFile::fieldListToJson(collected).toUtf8());
    file.close();

    QMessageBox::information(this, "Export JSON",
        QString("Exported %1 field(s) to:\n%2").arg(collected.size()).arg(path));
}

void SimFieldConfigurationDialog::applyImportedFields(const QList<FieldDefinition>& imported,
                                                      const QStringList& warnings,
                                                      const QString& sourceLabel)
{
    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QString("Import %1").arg(sourceLabel));
    box.setText(QString("Imported %1 field(s). Replace the current field list, or append?")
                    .arg(imported.size()));
    QPushButton* replaceBtn = box.addButton("Replace", QMessageBox::AcceptRole);
    QPushButton* appendBtn  = box.addButton("Append",  QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(replaceBtn);
    box.exec();

    const bool append = (box.clickedButton() == appendBtn);
    if (!append && box.clickedButton() != replaceBtn)
        return; // Cancel

    // De-duplicate imported names. On Append they must also avoid clashing with
    // the fields already present; either way a collision is renamed name -> name_1
    // (then _2, ...) rather than dropped, and the renames are reported.
    QSet<QString> taken;
    if (append)
        for (int i = 0; i < m_fields.size(); ++i)
            if (!m_fields.at(i).name.trimmed().isEmpty())
                taken.insert(m_fields.at(i).name.toLower());

    QStringList renames;
    QList<FieldDefinition> incoming = imported;
    for (int i = 0; i < incoming.size(); ++i)
    {
        const QString original = incoming[i].name;
        const QString unique = makeUniqueName(original, taken);
        if (unique != original)
        {
            incoming[i].name = unique;
            renames << QString("'%1' → '%2'").arg(original).arg(unique);
        }
    }

    if (append)
        m_fields.append(incoming);
    else
        m_fields = incoming;

    refreshFieldTable();

    QString summary = QString("Imported %1 field(s). The Value column is imported when present; "
                              "fill in any missing values before sending.")
                          .arg(imported.size());
    if (!renames.isEmpty())
        summary += QString("\n\nRenamed %1 field(s) to avoid duplicate names:\n%2")
                       .arg(renames.size()).arg(renames.join("\n"));
    if (!warnings.isEmpty())
        summary += "\n\nWarnings:\n" + warnings.join("\n");
    QMessageBox::information(this, QString("Import %1").arg(sourceLabel), summary);
}

void SimFieldConfigurationDialog::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void SimFieldConfigurationDialog::dropEvent(QDropEvent* event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    for (int i = 0; i < urls.size(); ++i)
    {
        const QString path = urls.at(i).toLocalFile();
        if (path.isEmpty())
            continue;

        if (path.endsWith(".csv", Qt::CaseInsensitive))
        {
            event->acceptProposedAction();
            importCsvFromPath(path);
            return;
        }
        if (path.endsWith(".json", Qt::CaseInsensitive))
        {
            event->acceptProposedAction();
            importJsonFromPath(path);
            return;
        }
    }
}
