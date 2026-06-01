#include "NmeaFieldConfigurationDialog.h"
#include "ui_NmeaFieldConfigurationDialog.h"

#include "NmeaSentenceRegistry.h"
#include "Themes.h"

#include <QCheckBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QTableWidgetItem>

namespace
{
const int COL_ENABLE = 0;
const int COL_INDEX = 1;
const int COL_DEFAULT = 2;
const int COL_LABEL = 3;
const int COL_KIND = 4;

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
}

NmeaFieldConfigurationDialog::NmeaFieldConfigurationDialog(QWidget* parent)
    : QDialog(parent),
      ui(new Ui::NmeaFieldConfigurationDialog)
{
    ui->setupUi(this);
    Themes::apply(this);

    ui->tblFields->setColumnCount(5);
    QStringList headers;
    headers << "Include" << "Field #" << "Default Name" << "Custom Label" << "Type";
    ui->tblFields->setHorizontalHeaderLabels(headers);

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onSaveClicked()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

NmeaFieldConfigurationDialog::~NmeaFieldConfigurationDialog()
{
    delete ui;
}

void NmeaFieldConfigurationDialog::setSentenceType(const QString& formatter)
{
    m_formatter = formatter.trimmed().toUpper();
    const NmeaSentenceDef* def = NmeaSentenceRegistry::lookup(m_formatter);

    m_rows.clear();
    if (def)
    {
        for (int i = 0; i < def->fields.size(); ++i)
            m_rows.append(RowState());
    }

    if (def)
        ui->lblSentence->setText(def->displayName);
    else
        ui->lblSentence->setText(QString("Unknown sentence: %1").arg(m_formatter));

    refreshTable();
}

void NmeaFieldConfigurationDialog::setExistingConfig(const QList<FieldDefinition>& fields)
{
    const NmeaSentenceDef* def = NmeaSentenceRegistry::lookup(m_formatter);
    if (!def)
        return;

    // Anything already configured is enabled; everything else defaults to off
    // so re-opening reflects exactly the prior selection.
    for (int i = 0; i < m_rows.size(); ++i)
    {
        m_rows[i].enabled = false;
        m_rows[i].customLabel.clear();
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
                break;
            }
        }
    }

    refreshTable();
}

void NmeaFieldConfigurationDialog::refreshTable()
{
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
    }

    ui->tblFields->resizeColumnsToContents();
    ui->tblFields->horizontalHeader()->setStretchLastSection(true);
}

void NmeaFieldConfigurationDialog::onSaveClicked()
{
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
        m_rows[row].enabled = check && check->isChecked();
        m_rows[row].customLabel = label ? label->text().trimmed() : QString();
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
        out.append(fd);
    }
    return out;
}
