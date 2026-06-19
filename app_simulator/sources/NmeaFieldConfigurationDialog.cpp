#include "NmeaFieldConfigurationDialog.h"
#include "ui_NmeaFieldConfigurationDialog.h"

#include "NmeaSentenceRegistry.h"
#include "Themes.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QTableWidgetItem>

namespace
{
// Predefined-mode columns.
const int COL_ENABLE = 0;
const int COL_INDEX = 1;
const int COL_DEFAULT = 2;
const int COL_LABEL = 3;
const int COL_KIND = 4;
const int COL_VALUE = 5;

// Custom-mode columns.
const int CUSTOM_COL_INDEX = 0;
const int CUSTOM_COL_NAME = 1;
const int CUSTOM_COL_KIND = 2;
const int CUSTOM_COL_VALUE = 3;

QString kindName(NmeaValueKind kind)
{
    switch (kind)
    {
    case NmeaValueKind::Latitude:  return "Latitude";
    case NmeaValueKind::Longitude: return "Longitude";
    case NmeaValueKind::Time:      return "Time";
    case NmeaValueKind::Date:      return "Date";
    case NmeaValueKind::Status:    return "Status";
    case NmeaValueKind::Char:      return "Char";
    case NmeaValueKind::Numeric:   return "Numeric";
    case NmeaValueKind::Text:
    default:                       return "Text";
    }
}

// Example token per value kind, used as the Value editor's placeholder so the
// user knows the expected NMEA spelling.
QString kindValueExample(NmeaValueKind kind)
{
    switch (kind)
    {
    case NmeaValueKind::Latitude:  return "4807.038";
    case NmeaValueKind::Longitude: return "01131.000";
    case NmeaValueKind::Time:      return "123519";
    case NmeaValueKind::Date:      return "230625";
    case NmeaValueKind::Status:    return "A";
    case NmeaValueKind::Char:      return "M";
    case NmeaValueKind::Numeric:   return "12.5";
    case NmeaValueKind::Text:
    default:                       return "value";
    }
}

// All value kinds, in the order they appear in a custom-field type combo. The
// integer stored as item data is the NmeaValueKind enum value.
void populateKindCombo(QComboBox* combo, int selectedKind)
{
    const NmeaValueKind kinds[] = {
        NmeaValueKind::Text, NmeaValueKind::Numeric, NmeaValueKind::Latitude,
        NmeaValueKind::Longitude, NmeaValueKind::Time, NmeaValueKind::Date,
        NmeaValueKind::Status, NmeaValueKind::Char
    };
    for (int i = 0; i < 8; ++i)
        combo->addItem(kindName(kinds[i]), static_cast<int>(kinds[i]));
    const int idx = combo->findData(selectedKind);
    combo->setCurrentIndex(idx >= 0 ? idx : 0);
}
}

NmeaFieldConfigurationDialog::NmeaFieldConfigurationDialog(QWidget* parent)
    : QDialog(parent),
      m_customMode(false),
      ui(new Ui::NmeaFieldConfigurationDialog)
{
    ui->setupUi(this);
    Themes::apply(this);

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onSaveClicked()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(ui->btnAddRow, SIGNAL(clicked()), this, SLOT(onAddRowClicked()));
    connect(ui->btnRemoveRow, SIGNAL(clicked()), this, SLOT(onRemoveRowClicked()));

    // The Add/Remove buttons only apply to custom sentences; hidden until then.
    ui->btnAddRow->setVisible(false);
    ui->btnRemoveRow->setVisible(false);
}

NmeaFieldConfigurationDialog::~NmeaFieldConfigurationDialog()
{
    delete ui;
}

void NmeaFieldConfigurationDialog::setSentenceType(const QString& formatter)
{
    m_formatter = formatter.trimmed().toUpper();
    const NmeaSentenceDef* def = NmeaSentenceRegistry::lookup(m_formatter);
    m_customMode = (def == 0);

    ui->btnAddRow->setVisible(m_customMode);
    ui->btnRemoveRow->setVisible(m_customMode);

    m_rows.clear();

    if (m_customMode)
    {
        ui->lblSentence->setText(QString(
            "Custom sentence: %1  —  define each field by its comma position and the value to transmit.")
            .arg(m_formatter));
        ui->tblFields->setColumnCount(4);
        QStringList headers;
        headers << "Field #" << "Column Name" << "Type" << "Value";
        ui->tblFields->setHorizontalHeaderLabels(headers);
        ui->tblFields->setRowCount(0);
        // Seed one empty row to make the editor obvious.
        addCustomRow(1, QString(), static_cast<int>(NmeaValueKind::Text), QString());
    }
    else
    {
        ui->lblSentence->setText(def->displayName);
        ui->tblFields->setColumnCount(6);
        QStringList headers;
        headers << "Include" << "Field #" << "Default Name" << "Custom Label" << "Type" << "Value";
        ui->tblFields->setHorizontalHeaderLabels(headers);
        for (int i = 0; i < def->fields.size(); ++i)
            m_rows.append(RowState());
    }

    refreshTable();
}

void NmeaFieldConfigurationDialog::setExistingConfig(const QList<FieldDefinition>& fields)
{
    if (m_customMode)
    {
        // Existing fields ARE the custom definition; rebuild the editor from them.
        ui->tblFields->setRowCount(0);
        for (int f = 0; f < fields.size(); ++f)
        {
            const FieldDefinition& fd = fields.at(f);
            addCustomRow(fd.nmeaFieldIndex > 0 ? fd.nmeaFieldIndex : 1,
                         fd.name, fd.nmeaValueKind, fd.sendValueText);
        }
        if (fields.isEmpty())
            addCustomRow(1, QString(), static_cast<int>(NmeaValueKind::Text), QString());
        return;
    }

    const NmeaSentenceDef* def = NmeaSentenceRegistry::lookup(m_formatter);
    if (!def)
        return;

    // Anything already configured is enabled; everything else defaults to off
    // so re-opening reflects exactly the prior selection.
    for (int i = 0; i < m_rows.size(); ++i)
    {
        m_rows[i].enabled = false;
        m_rows[i].customLabel.clear();
        m_rows[i].valueText.clear();
    }

    for (int f = 0; f < fields.size(); ++f)
    {
        const FieldDefinition& fd = fields.at(f);
        for (int i = 0; i < def->fields.size(); ++i)
        {
            if (def->fields.at(i).index == fd.nmeaFieldIndex)
            {
                m_rows[i].enabled = true;
                if (fd.name != def->fields.at(i).name)
                    m_rows[i].customLabel = fd.name;
                m_rows[i].valueText = fd.sendValueText;
                break;
            }
        }
    }

    refreshTable();
}

void NmeaFieldConfigurationDialog::addCustomRow(int fieldIndex, const QString& name, int valueKind, const QString& value)
{
    const int row = ui->tblFields->rowCount();
    ui->tblFields->insertRow(row);

    QSpinBox* idx = new QSpinBox(ui->tblFields);
    idx->setRange(1, 64);
    idx->setValue(fieldIndex > 0 ? fieldIndex : 1);
    ui->tblFields->setCellWidget(row, CUSTOM_COL_INDEX, idx);

    QLineEdit* nameEdit = new QLineEdit(ui->tblFields);
    nameEdit->setPlaceholderText("Column name");
    nameEdit->setText(name);
    ui->tblFields->setCellWidget(row, CUSTOM_COL_NAME, nameEdit);

    QComboBox* kind = new QComboBox(ui->tblFields);
    populateKindCombo(kind, valueKind);
    ui->tblFields->setCellWidget(row, CUSTOM_COL_KIND, kind);

    QLineEdit* valueEdit = new QLineEdit(ui->tblFields);
    valueEdit->setPlaceholderText(kindValueExample(static_cast<NmeaValueKind>(valueKind)));
    valueEdit->setText(value);
    valueEdit->setToolTip("The token transmitted at this comma position. Leave blank for a null field (,,). "
                          "Must not contain , * $ or !.");
    ui->tblFields->setCellWidget(row, CUSTOM_COL_VALUE, valueEdit);

    ui->tblFields->resizeColumnsToContents();
    ui->tblFields->horizontalHeader()->setStretchLastSection(true);
}

void NmeaFieldConfigurationDialog::refreshTable()
{
    if (m_customMode)
        return;   // custom rows are managed directly via addCustomRow

    const NmeaSentenceDef* def = NmeaSentenceRegistry::lookup(m_formatter);
    ui->tblFields->setRowCount(0);
    if (!def)
        return;

    for (int i = 0; i < def->fields.size() && i < m_rows.size(); ++i)
    {
        const NmeaFieldDef& fld = def->fields.at(i);
        const int row = ui->tblFields->rowCount();
        ui->tblFields->insertRow(row);

        QCheckBox* check = new QCheckBox(ui->tblFields);
        check->setChecked(m_rows.at(i).enabled);
        ui->tblFields->setCellWidget(row, COL_ENABLE, check);

        QTableWidgetItem* idxItem = new QTableWidgetItem(QString::number(fld.index));
        idxItem->setFlags(idxItem->flags() & ~Qt::ItemIsEditable);
        ui->tblFields->setItem(row, COL_INDEX, idxItem);

        QTableWidgetItem* defItem = new QTableWidgetItem(fld.name);
        defItem->setFlags(defItem->flags() & ~Qt::ItemIsEditable);
        ui->tblFields->setItem(row, COL_DEFAULT, defItem);

        QLineEdit* label = new QLineEdit(ui->tblFields);
        label->setPlaceholderText(fld.name);
        label->setText(m_rows.at(i).customLabel);
        ui->tblFields->setCellWidget(row, COL_LABEL, label);

        QTableWidgetItem* kindItem = new QTableWidgetItem(kindName(fld.kind));
        kindItem->setFlags(kindItem->flags() & ~Qt::ItemIsEditable);
        ui->tblFields->setItem(row, COL_KIND, kindItem);

        QLineEdit* valueEdit = new QLineEdit(ui->tblFields);
        valueEdit->setPlaceholderText(kindValueExample(fld.kind));
        valueEdit->setText(m_rows.at(i).valueText);
        valueEdit->setToolTip("The token transmitted at this comma position when the sentence is built. "
                              "Leave blank for a null field (,,). Must not contain , * $ or !.");
        ui->tblFields->setCellWidget(row, COL_VALUE, valueEdit);
    }

    ui->tblFields->resizeColumnsToContents();
    ui->tblFields->horizontalHeader()->setStretchLastSection(true);
}

void NmeaFieldConfigurationDialog::onAddRowClicked()
{
    addCustomRow(ui->tblFields->rowCount() + 1, QString(),
                 static_cast<int>(NmeaValueKind::Text), QString());
}

void NmeaFieldConfigurationDialog::onRemoveRowClicked()
{
    const int row = ui->tblFields->currentRow();
    if (row < 0)
    {
        QMessageBox::warning(this, "NMEA Fields", "Select a field row to remove.");
        return;
    }
    ui->tblFields->removeRow(row);
}

void NmeaFieldConfigurationDialog::onSaveClicked()
{
    if (m_customMode)
    {
        if (ui->tblFields->rowCount() == 0)
        {
            QMessageBox::warning(this, "NMEA Fields", "Add at least one field.");
            return;
        }
        for (int row = 0; row < ui->tblFields->rowCount(); ++row)
        {
            QLineEdit* nameEdit = qobject_cast<QLineEdit*>(
                ui->tblFields->cellWidget(row, CUSTOM_COL_NAME));
            if (!nameEdit || nameEdit->text().trimmed().isEmpty())
            {
                QMessageBox::warning(this, "NMEA Fields",
                    QString("Field row %1 needs a column name.").arg(row + 1));
                return;
            }
        }
        accept();
        return;
    }

    const NmeaSentenceDef* def = NmeaSentenceRegistry::lookup(m_formatter);
    if (!def)
    {
        QMessageBox::warning(this, "NMEA Fields",
            QString("Unknown sentence formatter '%1'.").arg(m_formatter));
        return;
    }

    // Pull the live widget state back into m_rows before validating/accepting.
    for (int row = 0; row < ui->tblFields->rowCount() && row < m_rows.size(); ++row)
    {
        QCheckBox* check = qobject_cast<QCheckBox*>(ui->tblFields->cellWidget(row, COL_ENABLE));
        QLineEdit* label = qobject_cast<QLineEdit*>(ui->tblFields->cellWidget(row, COL_LABEL));
        QLineEdit* value = qobject_cast<QLineEdit*>(ui->tblFields->cellWidget(row, COL_VALUE));
        m_rows[row].enabled = check && check->isChecked();
        m_rows[row].customLabel = label ? label->text().trimmed() : QString();
        m_rows[row].valueText = value ? value->text().trimmed() : QString();
    }

    bool anyEnabled = false;
    for (int i = 0; i < m_rows.size(); ++i)
    {
        if (m_rows.at(i).enabled) { anyEnabled = true; break; }
    }
    if (!anyEnabled)
    {
        QMessageBox::warning(this, "NMEA Fields", "Enable at least one field.");
        return;
    }

    accept();
}

QList<FieldDefinition> NmeaFieldConfigurationDialog::fieldConfig() const
{
    QList<FieldDefinition> out;

    if (m_customMode)
    {
        for (int row = 0; row < ui->tblFields->rowCount(); ++row)
        {
            QSpinBox* idx = qobject_cast<QSpinBox*>(
                ui->tblFields->cellWidget(row, CUSTOM_COL_INDEX));
            QLineEdit* nameEdit = qobject_cast<QLineEdit*>(
                ui->tblFields->cellWidget(row, CUSTOM_COL_NAME));
            QComboBox* kind = qobject_cast<QComboBox*>(
                ui->tblFields->cellWidget(row, CUSTOM_COL_KIND));
            QLineEdit* valueEdit = qobject_cast<QLineEdit*>(
                ui->tblFields->cellWidget(row, CUSTOM_COL_VALUE));
            if (!idx || !nameEdit || !kind)
                continue;
            if (nameEdit->text().trimmed().isEmpty())
                continue;

            FieldDefinition fd;
            fd.name = nameEdit->text().trimmed();
            fd.nmeaFieldIndex = idx->value();
            fd.nmeaValueKind = kind->currentData().toInt();
            fd.sendValueText = valueEdit ? valueEdit->text().trimmed() : QString();
            out.append(fd);
        }
        return out;
    }

    const NmeaSentenceDef* def = NmeaSentenceRegistry::lookup(m_formatter);
    if (!def)
        return out;

    for (int i = 0; i < def->fields.size() && i < m_rows.size(); ++i)
    {
        if (!m_rows.at(i).enabled)
            continue;

        const NmeaFieldDef& fld = def->fields.at(i);
        FieldDefinition fd;
        fd.name = m_rows.at(i).customLabel.isEmpty() ? fld.name : m_rows.at(i).customLabel;
        fd.nmeaFieldIndex = fld.index;
        fd.nmeaValueKind = static_cast<int>(fld.kind);
        fd.sendValueText = m_rows.at(i).valueText;
        out.append(fd);
    }
    return out;
}
