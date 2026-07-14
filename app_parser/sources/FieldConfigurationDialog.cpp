#include "FieldConfigurationDialog.h"
#include "ui_FieldConfigurationDialog.h"

#include "BitfieldDecoder.h"
#include "BitfieldDecoderDialog.h"
#include "ConditionalBitfieldDecoder.h"
#include "ConditionalBitfieldDecoderDialog.h"
#include "ExcelFieldCodec.h"
#include "InputValidator.h"
#include "MessageJsonCodec.h"
#include "ProjectFile.h"
#include "Themes.h"

#include <QAbstractItemView>
#include <QAction>
#include <QByteArray>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QIODevice>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSet>
#include <QShortcut>
#include <QStringList>
#include <QTableWidgetItem>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVariant>

#include <algorithm>

namespace
{
const int FIELD_COL_NAME = 0;
const int FIELD_COL_BYTE = 1;
const int FIELD_COL_TYPE = 2;
const int FIELD_COL_LENGTH = 3;
const int FIELD_COL_RESOLUTION = 4;
const int FIELD_COL_BIT_DECODER = 5;
const int FIELD_COL_COND_DECODER = 6;

QString resolutionTextForField(const FieldDefinition& field)
{
    if (!field.resolutionExpression.trimmed().isEmpty())
        return field.resolutionExpression.trimmed();
    if (qFuzzyCompare(field.resolution, 1.0))
        return QString();
    return QString::number(field.resolution, 'g', 15);
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
}

FieldConfigurationDialog::FieldConfigurationDialog(QWidget* parent)
    : QDialog(parent),
      m_payloadLengthBytes(0),
      m_offsetUnit("BYTES"),
      ui(new Ui::FieldConfigurationDialog)
{
    ui->setupUi(this);
    Themes::apply(this);
    setAcceptDrops(true);

    ui->tblFields->setColumnCount(7);
    ui->tblFields->setHorizontalHeaderLabels(QStringList() << "Field Name" << "Byte Offset" << "Type" << "Length" << "Resolution" << "Bit Decoder" << "Cond. Decoder");
    ui->tblFields->horizontalHeader()->setStretchLastSection(true);
    ui->tblFields->setSelectionBehavior(QAbstractItemView::SelectRows);
    // Multi-select (Ctrl/Shift) so several fields can be moved or removed at once.
    ui->tblFields->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // Drag a row (grab a text cell) to reorder. Qt's built-in InternalMove
    // corrupts cell widgets, so we let Qt drive the drag visuals but intercept
    // the drop ourselves (eventFilter) and reorder at the data level — matching
    // the simulator's field table.
    ui->tblFields->setDragEnabled(true);
    ui->tblFields->viewport()->setAcceptDrops(true);
    ui->tblFields->setDropIndicatorShown(true);
    ui->tblFields->setDragDropMode(QAbstractItemView::InternalMove);
    ui->tblFields->setDragDropOverwriteMode(false);
    ui->tblFields->viewport()->installEventFilter(this);

    connect(ui->cmbOffsetUnit, SIGNAL(currentIndexChanged(int)), this, SLOT(onOffsetUnitChanged()));

    connect(ui->btnAddField, SIGNAL(clicked()), this, SLOT(onAddFieldClicked()));
    connect(ui->btnEditField, SIGNAL(clicked()), this, SLOT(onEditFieldClicked()));
    connect(ui->btnRemoveField, SIGNAL(clicked()), this, SLOT(onRemoveFieldClicked()));
    connect(ui->btnBitfieldDecoder, SIGNAL(clicked()), this, SLOT(onBitfieldDecoderClicked()));
    connect(ui->btnConditionalDecoder, SIGNAL(clicked()), this, SLOT(onConditionalDecoderClicked()));

    QMenu* jsonMenu = new QMenu(this);
    QAction* jsonImport = jsonMenu->addAction("Import JSON...");
    QAction* jsonExport = jsonMenu->addAction("Export JSON...");
    ui->btnJsonMenu->setMenu(jsonMenu);
    connect(jsonImport, SIGNAL(triggered()), this, SLOT(onImportJsonClicked()));
    connect(jsonExport, SIGNAL(triggered()), this, SLOT(onExportJsonClicked()));

    QMenu* excelMenu = new QMenu(this);
    QAction* excelImport = excelMenu->addAction("Import Excel...");
    QAction* excelExport = excelMenu->addAction("Export Excel...");
    ui->btnExcelMenu->setMenu(excelMenu);
    connect(excelImport, SIGNAL(triggered()), this, SLOT(onImportExcelClicked()));
    connect(excelExport, SIGNAL(triggered()), this, SLOT(onExportExcelClicked()));

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onSaveClicked()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    // Keyboard shortcuts for fast table work (also listed in Help > Keyboard
    // Shortcuts on the main window): Insert adds a row, Ctrl+E edits the
    // selected row, Ctrl+Delete removes it. Arrow keys/Tab navigate natively.
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
    ui->btnEditField->setToolTip("Edit the selected field (Ctrl+E).");
    ui->btnRemoveField->setToolTip("Remove the selected field(s) (Ctrl+Delete). Shift/Ctrl-click to select several.");
    ui->tblFields->setToolTip("Drag a row (grab a text cell) to reorder; Alt+Up / Alt+Down also move the selected row.");
}

FieldConfigurationDialog::~FieldConfigurationDialog()
{
    delete ui;
}

void FieldConfigurationDialog::setPayloadLength(int payloadLengthBytes)
{
    m_payloadLengthBytes = payloadLengthBytes;
    if (m_payloadLengthBytes > 0)
    {
        ui->lblInfo->setText(QString("Configure fields for UDP Payload Length (bytes): %1").arg(m_payloadLengthBytes));
    }
    else
    {
        ui->lblInfo->setText("Configure fields. No fixed payload length is available for this filter mode.");
    }
}

void FieldConfigurationDialog::setFields(const QList<FieldDefinition>& fields)
{
    m_fields = fields;
    refreshFieldTable();
}

QList<FieldDefinition> FieldConfigurationDialog::fields() const
{
    return m_fields;
}

void FieldConfigurationDialog::setOffsetUnit(const QString& unit)
{
    m_offsetUnit = offsetUnitIsWords(unit) ? QString("WORDS") : QString("BYTES");
    const bool wasBlocked = ui->cmbOffsetUnit->blockSignals(true);
    ui->cmbOffsetUnit->setCurrentIndex(offsetUnitIsWords(m_offsetUnit) ? 1 : 0);
    ui->cmbOffsetUnit->blockSignals(wasBlocked);
    updateOffsetColumnHeader();
    refreshFieldTable();
}

QString FieldConfigurationDialog::offsetUnit() const
{
    return m_offsetUnit;
}

void FieldConfigurationDialog::onOffsetUnitChanged()
{
    // Snapshot the table back to canonical bytes under the OLD unit, switch
    // units, then redraw in the new unit so the physical offset is preserved.
    const QList<FieldDefinition> snap = peekFields(); // byteOffset in bytes
    m_offsetUnit = (ui->cmbOffsetUnit->currentIndex() == 1) ? QString("WORDS") : QString("BYTES");
    updateOffsetColumnHeader();
    m_fields = snap;
    refreshFieldTable();
}

void FieldConfigurationDialog::updateOffsetColumnHeader()
{
    const QString label = offsetUnitIsWords(m_offsetUnit) ? QStringLiteral("Word Offset")
                                                          : QStringLiteral("Byte Offset");
    QTableWidgetItem* header = ui->tblFields->horizontalHeaderItem(FIELD_COL_BYTE);
    if (header)
        header->setText(label);
    else
        ui->tblFields->setHorizontalHeaderItem(FIELD_COL_BYTE, new QTableWidgetItem(label));
}

QString FieldConfigurationDialog::tableText(int row, int column) const
{
    QTableWidgetItem* item = ui->tblFields->item(row, column);
    return item ? item->text().trimmed() : QString();
}

int FieldConfigurationDialog::selectedFieldRow() const
{
    QList<QTableWidgetItem*> selectedItems = ui->tblFields->selectedItems();
    if (!selectedItems.isEmpty()) return selectedItems.first()->row();
    return ui->tblFields->currentRow();
}

void FieldConfigurationDialog::setTypeCell(int row, FieldDataType dataType)
{
    if (!isKnownDataType(dataType))
        dataType = FieldDataType::RawUnsignedBE;

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
                    applyLengthStateForType(comboRow, dataTypeForRow(comboRow));
            });

    // On initial load, only ensure the length cell is editable; do NOT overwrite
    // the saved length value — applyLengthStateForType (user-driven type change)
    // handles updating the suggested length.
    QTableWidgetItem* lengthItem = ui->tblFields->item(row, FIELD_COL_LENGTH);
    if (!lengthItem)
    {
        lengthItem = new QTableWidgetItem("1");
        ui->tblFields->setItem(row, FIELD_COL_LENGTH, lengthItem);
    }
    lengthItem->setFlags(lengthItem->flags() | Qt::ItemIsEditable);
}

FieldDataType FieldConfigurationDialog::dataTypeForRow(int row) const
{
    QWidget* widget = ui->tblFields->cellWidget(row, FIELD_COL_TYPE);
    QComboBox* combo = qobject_cast<QComboBox*>(widget);
    if (!combo)
        return FieldDataType::RawUnsignedBE;

    const FieldDataType dataType = static_cast<FieldDataType>(combo->currentData().toInt());
    return isKnownDataType(dataType) ? dataType : FieldDataType::RawUnsignedBE;
}

void FieldConfigurationDialog::applyLengthStateForType(int row, FieldDataType dataType)
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

int FieldConfigurationDialog::rowForTypeCombo(const QWidget* combo) const
{
    for (int row = 0; row < ui->tblFields->rowCount(); ++row)
    {
        if (ui->tblFields->cellWidget(row, FIELD_COL_TYPE) == combo)
            return row;
    }

    return -1;
}

void FieldConfigurationDialog::setDecoderCell(int row, const QList<BitDecodeRule>& rules)
{
    // v12: replace the text-only "Yes/No" cell with an inline Edit button so users
    // can open the decoder dialog for any row without first selecting it. Status
    // (rule count) moves into the button label + tooltip.
    QPushButton* button = qobject_cast<QPushButton*>(ui->tblFields->cellWidget(row, FIELD_COL_BIT_DECODER));
    if (!button)
    {
        button = new QPushButton(ui->tblFields);
        ui->tblFields->setCellWidget(row, FIELD_COL_BIT_DECODER, button);
        connect(button, SIGNAL(clicked()), this, SLOT(onBitfieldEditRowClicked()));
    }

    if (rules.isEmpty())
    {
        button->setText("Edit");
        button->setToolTip("No bitfield decoder yet. Click to add rules.");
    }
    else
    {
        button->setText(QString("Edit (%1)").arg(rules.size()));
        button->setToolTip(QString("Bitfield decoder configured with %1 rule(s). Click to edit.").arg(rules.size()));
    }
}

void FieldConfigurationDialog::setConditionalDecoderCell(int row, const ConditionalBitfieldDecoderConfig& decoder)
{
    // v12: same pattern as setDecoderCell — inline Edit button instead of plain text.
    QPushButton* button = qobject_cast<QPushButton*>(ui->tblFields->cellWidget(row, FIELD_COL_COND_DECODER));
    if (!button)
    {
        button = new QPushButton(ui->tblFields);
        ui->tblFields->setCellWidget(row, FIELD_COL_COND_DECODER, button);
        connect(button, SIGNAL(clicked()), this, SLOT(onConditionalEditRowClicked()));
    }

    if (decoder.profiles.isEmpty())
    {
        button->setText("Edit");
        button->setToolTip("No conditional decoder yet. Click to add profiles.");
    }
    else
    {
        button->setText(QString("Edit (%1)").arg(decoder.profiles.size()));
        button->setToolTip(QString("Conditional decoder with %1 profile(s). Controller: %2. Click to edit.")
                              .arg(decoder.profiles.size())
                              .arg(decoder.controllerFieldName));
    }
}

void FieldConfigurationDialog::refreshFieldTable()
{
    ui->tblFields->setRowCount(0);

    for (int i = 0; i < m_fields.size(); ++i)
    {
        const FieldDefinition& field = m_fields.at(i);
        const int row = ui->tblFields->rowCount();
        ui->tblFields->insertRow(row);

        QTableWidgetItem* nameItem = new QTableWidgetItem(field.name);
        if (field.hasBitfieldDecoder && !field.bitDecodeRules.isEmpty())
            nameItem->setData(Qt::UserRole, BitfieldDecoder::rulesToJson(field.bitDecodeRules));
        if (field.hasConditionalBitfieldDecoder && !field.conditionalDecoder.profiles.isEmpty())
            nameItem->setData(Qt::UserRole + 1, ConditionalBitfieldDecoder::toJson(field.conditionalDecoder));

        ui->tblFields->setItem(row, FIELD_COL_NAME, nameItem);
        ui->tblFields->setItem(row, FIELD_COL_BYTE,
            new QTableWidgetItem(QString::number(byteOffsetToUnit(field.byteOffset, m_offsetUnit))));
        ui->tblFields->setItem(row, FIELD_COL_LENGTH, new QTableWidgetItem(QString::number(field.length)));
        setTypeCell(row, field.dataType);  // reads FIELD_COL_LENGTH; must be set first
        ui->tblFields->setItem(row, FIELD_COL_RESOLUTION, new QTableWidgetItem(resolutionTextForField(field)));
        setDecoderCell(row, field.bitDecodeRules);
        setConditionalDecoderCell(row, field.conditionalDecoder);
    }

    ui->tblFields->resizeColumnsToContents();
    ui->tblFields->horizontalHeader()->setStretchLastSection(true);
}

void FieldConfigurationDialog::onAddFieldClicked()
{
    const int row = ui->tblFields->rowCount();
    ui->tblFields->insertRow(row);
    ui->tblFields->setItem(row, FIELD_COL_NAME, new QTableWidgetItem(QString("Field%1").arg(row + 1)));
    ui->tblFields->setItem(row, FIELD_COL_BYTE, new QTableWidgetItem("1"));
    ui->tblFields->setItem(row, FIELD_COL_LENGTH, new QTableWidgetItem("2"));
    setTypeCell(row, FieldDataType::RawUnsignedBE);
    ui->tblFields->setItem(row, FIELD_COL_RESOLUTION, new QTableWidgetItem("1"));
    setDecoderCell(row, QList<BitDecodeRule>());
    setConditionalDecoderCell(row, ConditionalBitfieldDecoderConfig());
    ui->tblFields->selectRow(row);
}

void FieldConfigurationDialog::onEditFieldClicked()
{
    const int row = selectedFieldRow();
    if (row < 0 || row >= ui->tblFields->rowCount())
    {
        QMessageBox::warning(this, "Field Configuration", "Select one field to edit.");
        return;
    }

    QTableWidgetItem* item = ui->tblFields->item(row, FIELD_COL_NAME);
    if (item)
        ui->tblFields->editItem(item);
}

void FieldConfigurationDialog::onRemoveFieldClicked()
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
        QMessageBox::warning(this, "Field Configuration", "Select one or more field rows first.");
        return;
    }

    QList<int> rows = rowSet.toList();
    std::sort(rows.begin(), rows.end());

    // Remove high-index-first so earlier indices stay valid.
    for (int i = rows.size() - 1; i >= 0; --i)
        ui->tblFields->removeRow(rows.at(i));
}

void FieldConfigurationDialog::onBitfieldDecoderClicked()
{
    const int row = selectedFieldRow();
    if (row < 0 || row >= ui->tblFields->rowCount())
    {
        QMessageBox::warning(this, "Bitfield Decoder", "Select exactly one field to configure bitfield decoder rules.");
        return;
    }

    const QString fieldName = tableText(row, FIELD_COL_NAME);
    bool lengthOk = false;
    const int fieldLength = tableText(row, FIELD_COL_LENGTH).toInt(&lengthOk, 10);

    if (fieldName.isEmpty())
    {
        QMessageBox::warning(this, "Bitfield Decoder", "Selected field name cannot be empty.");
        return;
    }

    if (!lengthOk || fieldLength <= 0 || fieldLength > 8)
    {
        QMessageBox::warning(this, "Bitfield Decoder", "Selected field length must be between 1 and 8 bytes.");
        return;
    }

    QTableWidgetItem* nameItem = ui->tblFields->item(row, FIELD_COL_NAME);
    if (!nameItem)
    {
        nameItem = new QTableWidgetItem(fieldName);
        ui->tblFields->setItem(row, FIELD_COL_NAME, nameItem);
    }

    QList<BitDecodeRule> existingRules;
    QString error;
    const QString storedJson = nameItem->data(Qt::UserRole).toString();
    if (!storedJson.trimmed().isEmpty() && !BitfieldDecoder::rulesFromJson(storedJson, fieldLength, existingRules, error))
    {
        QMessageBox::warning(this, "Bitfield Decoder", "Existing bitfield decoder data is invalid and will be cleared:\n" + error);
        existingRules.clear();
    }

    BitfieldDecoderDialog dlg(fieldName, fieldLength, existingRules, this);
    if (dlg.exec() == QDialog::Accepted)
    {
        const QList<BitDecodeRule> rules = dlg.rules();
        if (rules.isEmpty())
            nameItem->setData(Qt::UserRole, QVariant());
        else
            nameItem->setData(Qt::UserRole, BitfieldDecoder::rulesToJson(rules));

        setDecoderCell(row, rules);
    }
}

QList<FieldDefinition> FieldConfigurationDialog::peekFields() const
{
    QList<FieldDefinition> fields;
    for (int row = 0; row < ui->tblFields->rowCount(); ++row)
    {
        QTableWidgetItem* nameItem = ui->tblFields->item(row, FIELD_COL_NAME);
        const QString name = nameItem ? nameItem->text().trimmed() : QString();
        if (name.isEmpty()) continue;

        bool lengthOk = false;
        const int length = tableText(row, FIELD_COL_LENGTH).toInt(&lengthOk, 10);

        FieldDefinition field;
        field.name = name;
        field.length = (lengthOk && length > 0) ? length : 1;
        field.byteOffset = unitToByteOffset(tableText(row, FIELD_COL_BYTE).toInt(), m_offsetUnit);
        field.byteOffsetcorrect = field.byteOffset - 1;
        field.dataType = dataTypeForRow(row);
        fields.append(field);
    }
    return fields;
}

void FieldConfigurationDialog::onConditionalDecoderClicked()
{
    const int row = selectedFieldRow();
    if (row < 0 || row >= ui->tblFields->rowCount())
    {
        QMessageBox::warning(this, "Conditional Decoder", "Select exactly one field to configure its conditional decoder.");
        return;
    }

    const QString fieldName = tableText(row, FIELD_COL_NAME);
    bool lengthOk = false;
    const int fieldLength = tableText(row, FIELD_COL_LENGTH).toInt(&lengthOk, 10);

    if (fieldName.isEmpty())
    {
        QMessageBox::warning(this, "Conditional Decoder", "Selected field name cannot be empty.");
        return;
    }

    if (!lengthOk || fieldLength <= 0 || fieldLength > 8)
    {
        QMessageBox::warning(this, "Conditional Decoder", "Selected field length must be between 1 and 8 bytes.");
        return;
    }

    const QList<FieldDefinition> allFields = peekFields();
    if (allFields.size() < 2)
    {
        QMessageBox::warning(this, "Conditional Decoder", "At least two fields are required: the dependent field and a controller field.");
        return;
    }

    QTableWidgetItem* nameItem = ui->tblFields->item(row, FIELD_COL_NAME);
    if (!nameItem)
    {
        nameItem = new QTableWidgetItem(fieldName);
        ui->tblFields->setItem(row, FIELD_COL_NAME, nameItem);
    }

    ConditionalBitfieldDecoderConfig existingDecoder;
    const QString storedJson = nameItem->data(Qt::UserRole + 1).toString();
    if (!storedJson.trimmed().isEmpty())
    {
        QString parseError;
        if (!ConditionalBitfieldDecoder::fromJson(storedJson, existingDecoder, parseError))
        {
            QMessageBox::warning(this, "Conditional Decoder", "Existing conditional decoder data is invalid and will be cleared:\n" + parseError);
            existingDecoder = ConditionalBitfieldDecoderConfig();
        }
    }

    ConditionalBitfieldDecoderDialog dlg(fieldName, fieldLength, allFields, existingDecoder, this);
    if (dlg.exec() == QDialog::Accepted)
    {
        const ConditionalBitfieldDecoderConfig decoder = dlg.decoder();
        if (decoder.profiles.isEmpty())
            nameItem->setData(Qt::UserRole + 1, QVariant());
        else
            nameItem->setData(Qt::UserRole + 1, ConditionalBitfieldDecoder::toJson(decoder));

        setConditionalDecoderCell(row, decoder);
    }
}

bool FieldConfigurationDialog::collectFields(QList<FieldDefinition>& fields, QString& errorMessage) const
{
    fields.clear();

    for (int row = 0; row < ui->tblFields->rowCount(); ++row)
    {
        const QString name = tableText(row, FIELD_COL_NAME);
        const QString byteText = tableText(row, FIELD_COL_BYTE);
        const QString lengthText = tableText(row, FIELD_COL_LENGTH);
        const QString resolutionText = tableText(row, FIELD_COL_RESOLUTION);

        if (name.isEmpty() && byteText.isEmpty() && lengthText.isEmpty() && resolutionText.isEmpty())
            continue;

        if (!InputValidator::validateField(name, byteText, lengthText, resolutionText, errorMessage,
                                           InputValidator::kNoNumericLengthCap))
        {
            errorMessage = QString("Row %1: %2").arg(row + 1).arg(errorMessage);
            return false;
        }

        const QString trimmedResolution = resolutionText.trimmed();
        double solvedResolution = 1.0;
        if (!trimmedResolution.isEmpty()
            && !InputValidator::solveResolutionExpression(trimmedResolution, solvedResolution, errorMessage))
        {
            errorMessage = QString("Row %1: %2").arg(row + 1).arg(errorMessage);
            return false;
        }

        FieldDefinition field;
        field.name = name;
        field.byteOffset = unitToByteOffset(byteText.toInt(), m_offsetUnit);
        field.byteOffsetcorrect = field.byteOffset - 1;
        field.length = lengthText.toInt();
        field.dataType = dataTypeForRow(row);
        field.resolution = solvedResolution;
        field.resolutionExpression = trimmedResolution;

        if (m_payloadLengthBytes > 0
            && (field.byteOffsetcorrect < 0 || field.byteOffsetcorrect + field.length > m_payloadLengthBytes))
        {
            errorMessage = QString("Field '%1' exceeds payload length %2 bytes.").arg(field.name).arg(m_payloadLengthBytes);
            return false;
        }

        QTableWidgetItem* nameItem = ui->tblFields->item(row, FIELD_COL_NAME);
        if (nameItem)
        {
            const QString storedJson = nameItem->data(Qt::UserRole).toString();
            if (!storedJson.trimmed().isEmpty())
            {
                QList<BitDecodeRule> rules;
                QString ruleError;
                if (!BitfieldDecoder::rulesFromJson(storedJson, field.length, rules, ruleError))
                {
                    errorMessage = QString("Row %1 bitfield decoder error: %2").arg(row + 1).arg(ruleError);
                    return false;
                }
                field.bitDecodeRules = rules;
                field.hasBitfieldDecoder = !rules.isEmpty();
            }

            const QString condJson = nameItem->data(Qt::UserRole + 1).toString();
            if (!condJson.trimmed().isEmpty())
            {
                ConditionalBitfieldDecoderConfig condDecoder;
                QString condError;
                if (!ConditionalBitfieldDecoder::fromJson(condJson, condDecoder, condError))
                {
                    errorMessage = QString("Row %1 conditional decoder error: %2").arg(row + 1).arg(condError);
                    return false;
                }
                field.conditionalDecoder = condDecoder;
                field.hasConditionalBitfieldDecoder = !condDecoder.profiles.isEmpty();
            }
        }

        fields.append(field);
    }

    return InputValidator::validateFields(fields, errorMessage, InputValidator::kNoNumericLengthCap);
}

void FieldConfigurationDialog::onSaveClicked()
{
    QList<FieldDefinition> collectedFields;
    QString error;
    if (!collectFields(collectedFields, error))
    {
        QMessageBox::warning(this, "Invalid Field", error);
        return;
    }

    // Re-validate conditional decoders against the final collected field list
    // to catch cases where the controller field was deleted after decoder was configured.
    for (int i = 0; i < collectedFields.size(); ++i)
    {
        const FieldDefinition& field = collectedFields.at(i);
        if (!field.hasConditionalBitfieldDecoder) continue;

        QString condError;
        if (!ConditionalBitfieldDecoder::validate(field.conditionalDecoder, collectedFields,
                                                   field.name, field.length, condError))
        {
            QMessageBox::warning(this, "Invalid Conditional Decoder",
                QString("Field '%1': %2").arg(field.name).arg(condError));
            return;
        }
    }

    m_fields = collectedFields;
    accept();
}

void FieldConfigurationDialog::onImportExcelClicked()
{
    const QString path = QFileDialog::getOpenFileName(this,
        "Import Field Definitions from Excel", QString(),
        "Excel Files (*.xlsx);;All Files (*.*)");
    if (path.isEmpty()) return;

    QList<FieldDefinition> imported;
    QStringList warnings;
    QString error;
    if (!ExcelFieldCodec::importFields(path, m_payloadLengthBytes, imported, warnings, error))
    {
        QMessageBox::warning(this, "Import Excel", QString("Import failed:\n\n%1").arg(error));
        return;
    }
    if (imported.isEmpty())
    {
        QMessageBox::information(this, "Import Excel", "The workbook contained no valid field rows.");
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle("Import Excel");
    box.setText(QString("Imported %1 field(s). Replace the current field list, or append?")
                    .arg(imported.size()));
    QPushButton* replaceBtn = box.addButton("Replace", QMessageBox::AcceptRole);
    QPushButton* appendBtn  = box.addButton("Append",  QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(replaceBtn);
    box.exec();

    if (box.clickedButton() == replaceBtn)
        m_fields = imported;
    else if (box.clickedButton() == appendBtn)
        m_fields.append(imported);
    else
        return;

    refreshFieldTable();

    QString summary = QString("Imported %1 field(s).\nBitfield and Conditional decoders are NOT carried in Excel \xe2\x80\x94 use JSON for those.")
                          .arg(imported.size());
    if (!warnings.isEmpty())
        summary += "\n\nWarnings:\n" + warnings.join("\n");
    QMessageBox::information(this, "Import Excel", summary);
}

void FieldConfigurationDialog::onExportExcelClicked()
{
    QList<FieldDefinition> collected;
    QString collectError;
    if (!collectFields(collected, collectError))
    {
        QMessageBox::warning(this, "Export Excel",
            "Cannot export: current fields are not valid.\n" + collectError);
        return;
    }
    if (collected.isEmpty())
    {
        QMessageBox::information(this, "Export Excel", "No fields to export.");
        return;
    }

    const QString path = QFileDialog::getSaveFileName(this,
        "Export Field Definitions to Excel", "fields.xlsx", "Excel Files (*.xlsx)");
    if (path.isEmpty()) return;

    QString error;
    if (!ExcelFieldCodec::exportFields(path, collected, error))
    {
        QMessageBox::warning(this, "Export Excel", QString("Export failed:\n%1").arg(error));
        return;
    }
    QMessageBox::information(this, "Export Excel",
        QString("Exported %1 field(s) to:\n%2\n\nNote: Bitfield and Conditional decoders are NOT included in Excel (use JSON for those).")
            .arg(collected.size()).arg(path));
}

void FieldConfigurationDialog::onImportJsonClicked()
{
    const QString path = QFileDialog::getOpenFileName(this,
        "Import Field Definitions from JSON",
        QString(),
        "JSON Files (*.json);;All Files (*.*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Import JSON",
            QString("Cannot open file:\n%1").arg(file.errorString()));
        return;
    }
    const QByteArray bytes = file.readAll();
    file.close();

    // Accept BOTH the shared suite format (exported here or by the simulator)
    // and the legacy reader field-list format. The suite format is tagged by its
    // "kind"; older files fall through to the legacy reader for full fidelity of
    // their bit-decoder serialisation.
    const QString jsonText = QString::fromUtf8(bytes);
    const bool unified = jsonText.contains(QLatin1String("UniversalDataSuiteMessages"));
    QList<FieldDefinition> imported;
    QString errorOrWarnings;
    const bool ok = unified
        ? MessageJsonCodec::fieldsFromJson(jsonText, imported, errorOrWarnings)
        : ProjectFile::fieldListFromJson(jsonText, imported, errorOrWarnings);
    if (!ok)
    {
        QMessageBox::warning(this, "Import JSON",
            QString("Import failed:\n\n%1").arg(errorOrWarnings));
        return;
    }
    if (imported.isEmpty())
    {
        QMessageBox::information(this, "Import JSON", "JSON contained no fields.");
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle("Import JSON");
    box.setText(QString("Imported %1 field(s). Replace the current field list, or append?")
                    .arg(imported.size()));
    QPushButton* replaceBtn = box.addButton("Replace", QMessageBox::AcceptRole);
    QPushButton* appendBtn  = box.addButton("Append",  QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(replaceBtn);
    box.exec();

    if (box.clickedButton() == replaceBtn)
        m_fields = imported;
    else if (box.clickedButton() == appendBtn)
        m_fields.append(imported);
    else
        return;

    refreshFieldTable();

    QString summary = QString("Imported %1 field(s) (with any bit / conditional decoders found in the JSON).")
                          .arg(imported.size());
    if (!errorOrWarnings.isEmpty())
        summary += "\n\nWarnings:\n" + errorOrWarnings;
    QMessageBox::information(this, "Import JSON", summary);
}

void FieldConfigurationDialog::onExportJsonClicked()
{
    QList<FieldDefinition> collected;
    QString collectError;
    if (!collectFields(collected, collectError))
    {
        QMessageBox::warning(this, "Export JSON",
            "Cannot export: current fields are not valid.\n" + collectError);
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

    // Unified suite format (shared MessageJsonCodec) so the simulator can open
    // it too. Carries bit + conditional decoders, so no manual editing needed.
    const QString jsonText = MessageJsonCodec::fieldsToJson(collected);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        QMessageBox::warning(this, "Export JSON",
            QString("Cannot open file for writing:\n%1").arg(file.errorString()));
        return;
    }
    const QByteArray bytes = jsonText.toUtf8();
    const qint64 written = file.write(bytes);
    file.close();
    if (written != bytes.size())
    {
        QMessageBox::warning(this, "Export JSON", "Write incomplete.");
        return;
    }

    QMessageBox::information(this, "Export JSON",
        QString("Exported %1 field(s) to:\n%2\n\n"
                "This is the shared suite JSON format — the simulator can import it directly.")
            .arg(collected.size()).arg(path));
}

// v12: per-row Edit button slots. They resolve the clicked row by scanning the
// table for the cellWidget that fired the signal (robust to row insertion /
// removal — no stale row indices), select that row, and delegate to the
// existing onBitfieldDecoderClicked / onConditionalDecoderClicked logic.
void FieldConfigurationDialog::onBitfieldEditRowClicked()
{
    QObject* obj = sender();
    int row = -1;
    for (int r = 0; r < ui->tblFields->rowCount(); ++r)
    {
        if (ui->tblFields->cellWidget(r, FIELD_COL_BIT_DECODER) == obj)
        {
            row = r;
            break;
        }
    }
    if (row < 0) return;
    ui->tblFields->selectRow(row);
    onBitfieldDecoderClicked();
}

void FieldConfigurationDialog::onConditionalEditRowClicked()
{
    QObject* obj = sender();
    int row = -1;
    for (int r = 0; r < ui->tblFields->rowCount(); ++r)
    {
        if (ui->tblFields->cellWidget(r, FIELD_COL_COND_DECODER) == obj)
        {
            row = r;
            break;
        }
    }
    if (row < 0) return;
    ui->tblFields->selectRow(row);
    onConditionalDecoderClicked();
}

// ============================================================================
// Field-table row reordering (drag-and-drop + Alt+Up/Down) and drag-and-drop
// import of JSON field-definition files.
// ============================================================================

namespace
{
QString firstFieldDefFile(const QMimeData* mime)
{
    if (!mime || !mime->hasUrls()) return QString();
    const QList<QUrl> urls = mime->urls();
    for (int i = 0; i < urls.size(); ++i)
    {
        const QString local = urls.at(i).toLocalFile();
        if (local.isEmpty()) continue;
        const QString suffix = QFileInfo(local).suffix().toLower();
        if (suffix == "json")
            return local;
    }
    return QString();
}
}

void FieldConfigurationDialog::dragEnterEvent(QDragEnterEvent* event)
{
    if (!firstFieldDefFile(event->mimeData()).isEmpty())
        event->acceptProposedAction();
    else
        event->ignore();
}

void FieldConfigurationDialog::dropEvent(QDropEvent* event)
{
    const QString path = firstFieldDefFile(event->mimeData());
    if (path.isEmpty())
    {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
    importJsonFromPath(path);
}

QList<FieldDefinition> FieldConfigurationDialog::snapshotAllRows() const
{
    QList<FieldDefinition> out;
    for (int row = 0; row < ui->tblFields->rowCount(); ++row)
    {
        FieldDefinition field;
        field.name = tableText(row, FIELD_COL_NAME);
        field.byteOffset = unitToByteOffset(tableText(row, FIELD_COL_BYTE).toInt(), m_offsetUnit);
        field.byteOffsetcorrect = field.byteOffset - 1;
        const int len = tableText(row, FIELD_COL_LENGTH).toInt();
        field.length = (len >= 1) ? len : 1;
        field.dataType = dataTypeForRow(row);
        const QString resText = tableText(row, FIELD_COL_RESOLUTION);
        field.resolutionExpression = resText;
        double resolution = 1.0;
        QString resErr;
        if (!resText.trimmed().isEmpty()
            && InputValidator::solveResolutionExpression(resText, resolution, resErr))
            field.resolution = resolution;
        else
            field.resolution = 1.0;

        // Preserve the per-row bit / conditional decoders stored on the name item
        // so they survive a reorder.
        QTableWidgetItem* nameItem = ui->tblFields->item(row, FIELD_COL_NAME);
        if (nameItem)
        {
            const QString bitJson = nameItem->data(Qt::UserRole).toString();
            if (!bitJson.trimmed().isEmpty())
            {
                QList<BitDecodeRule> rules;
                QString err;
                if (BitfieldDecoder::rulesFromJson(bitJson, field.length, rules, err))
                {
                    field.bitDecodeRules = rules;
                    field.hasBitfieldDecoder = !rules.isEmpty();
                }
            }
            const QString condJson = nameItem->data(Qt::UserRole + 1).toString();
            if (!condJson.trimmed().isEmpty())
            {
                ConditionalBitfieldDecoderConfig cfg;
                QString err;
                if (ConditionalBitfieldDecoder::fromJson(condJson, cfg, err))
                {
                    field.conditionalDecoder = cfg;
                    field.hasConditionalBitfieldDecoder = !cfg.profiles.isEmpty();
                }
            }
        }
        out.append(field);
    }
    return out;
}

void FieldConfigurationDialog::reorderRows(QList<int> sourceRows, int targetRow)
{
    const int count = ui->tblFields->rowCount();
    if (sourceRows.isEmpty() || count <= 1)
        return;

    std::sort(sourceRows.begin(), sourceRows.end());

    QList<FieldDefinition> rows = snapshotAllRows();
    if (rows.size() != count)
        return;

    QList<FieldDefinition> moved;
    for (int i = 0; i < sourceRows.size(); ++i)
        moved.append(rows.at(sourceRows.at(i)));

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

    ui->tblFields->clearSelection();
    for (int i = 0; i < moved.size(); ++i)
        ui->tblFields->selectRow(insertAt + i);
}

void FieldConfigurationDialog::moveSelectedRows(int delta)
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

void FieldConfigurationDialog::onMoveFieldUpClicked()
{
    moveSelectedRows(-1);
}

void FieldConfigurationDialog::onMoveFieldDownClicked()
{
    moveSelectedRows(+1);
}

bool FieldConfigurationDialog::eventFilter(QObject* watched, QEvent* event)
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

        // A JSON file dragged in from outside still imports (works over the table too).
        if (drop->mimeData() && drop->mimeData()->hasUrls())
        {
            const QString path = firstFieldDefFile(drop->mimeData());
            if (!path.isEmpty())
            {
                drop->acceptProposedAction();
                importJsonFromPath(path);
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

void FieldConfigurationDialog::importJsonFromPath(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Import JSON",
            QString("Cannot open file:\n%1").arg(file.errorString()));
        return;
    }
    const QByteArray bytes = file.readAll();
    file.close();

    // Accept BOTH the shared suite format (exported here or by the simulator)
    // and the legacy reader field-list format. The suite format is tagged by its
    // "kind"; older files fall through to the legacy reader for full fidelity of
    // their bit-decoder serialisation.
    const QString jsonText = QString::fromUtf8(bytes);
    const bool unified = jsonText.contains(QLatin1String("UniversalDataSuiteMessages"));
    QList<FieldDefinition> imported;
    QString errorOrWarnings;
    const bool ok = unified
        ? MessageJsonCodec::fieldsFromJson(jsonText, imported, errorOrWarnings)
        : ProjectFile::fieldListFromJson(jsonText, imported, errorOrWarnings);
    if (!ok)
    {
        QMessageBox::warning(this, "Import JSON",
            QString("Import failed:\n\n%1").arg(errorOrWarnings));
        return;
    }
    if (imported.isEmpty())
    {
        QMessageBox::information(this, "Import JSON", "JSON contained no fields.");
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle("Import JSON");
    box.setText(QString("Imported %1 field(s) from the dropped file. Replace the current field list, or append?")
                    .arg(imported.size()));
    QPushButton* replaceBtn = box.addButton("Replace", QMessageBox::AcceptRole);
    QPushButton* appendBtn  = box.addButton("Append",  QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(replaceBtn);
    box.exec();

    if (box.clickedButton() == replaceBtn)
        m_fields = imported;
    else if (box.clickedButton() == appendBtn)
        m_fields.append(imported);
    else
        return;

    refreshFieldTable();

    QString summary = QString("Imported %1 field(s) (with any bit / conditional decoders found in the JSON).")
                          .arg(imported.size());
    if (!errorOrWarnings.isEmpty())
        summary += "\n\nWarnings:\n" + errorOrWarnings;
    QMessageBox::information(this, "Import JSON", summary);
}
