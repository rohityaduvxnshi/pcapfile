#include "FieldTypeLabels.h"

namespace
{
struct TypeLabel
{
    const char* label;
    FieldDataType type;
};

const TypeLabel kTypeLabels[] = {
    { "Raw Unsigned BE", FieldDataType::RawUnsignedBE },
    { "RawUnsignedBE",   FieldDataType::RawUnsignedBE },
    { "raw",             FieldDataType::RawUnsignedBE },
    { "bool",            FieldDataType::Bool },
    { "Bool",            FieldDataType::Bool },
    { "uchar",           FieldDataType::Uint8 },
    { "Uint8",           FieldDataType::Uint8 },
    { "uint8",           FieldDataType::Uint8 },
    { "char",            FieldDataType::Int8 },
    { "Int8",            FieldDataType::Int8 },
    { "int8",            FieldDataType::Int8 },
    { "ushort",          FieldDataType::Uint16 },
    { "Uint16",          FieldDataType::Uint16 },
    { "uint16",          FieldDataType::Uint16 },
    { "short",           FieldDataType::Int16 },
    { "Int16",           FieldDataType::Int16 },
    { "int16",           FieldDataType::Int16 },
    { "uint",            FieldDataType::Uint32 },
    { "Uint32",          FieldDataType::Uint32 },
    { "uint32",          FieldDataType::Uint32 },
    { "int",             FieldDataType::Int32 },
    { "Int32",           FieldDataType::Int32 },
    { "int32",           FieldDataType::Int32 },
    { "ulong",           FieldDataType::Uint64 },
    { "Uint64",          FieldDataType::Uint64 },
    { "uint64",          FieldDataType::Uint64 },
    { "long",            FieldDataType::Int64 },
    { "Int64",           FieldDataType::Int64 },
    { "int64",           FieldDataType::Int64 },
    { "float",           FieldDataType::Float32 },
    { "Float32",         FieldDataType::Float32 },
    { "float32",         FieldDataType::Float32 },
    { "double",          FieldDataType::Float64 },
    { "Float64",         FieldDataType::Float64 },
    { "float64",         FieldDataType::Float64 },
    { "string",          FieldDataType::String },
    { "String",          FieldDataType::String },
    { "str",             FieldDataType::String },
    { "text",            FieldDataType::String },
    // Verbose / C-style / ICD spellings (exact, case-insensitive).
    { "unsigned char",       FieldDataType::Uint8 },
    { "signed char",         FieldDataType::Int8 },
    { "unsigned short",      FieldDataType::Uint16 },
    { "signed short",        FieldDataType::Int16 },
    { "short int",           FieldDataType::Int16 },
    { "unsigned int",        FieldDataType::Uint32 },
    { "unsigned integer",    FieldDataType::Uint32 },
    { "signed int",          FieldDataType::Int32 },
    { "signed integer",      FieldDataType::Int32 },
    { "unsigned long",       FieldDataType::Uint64 },
    { "unsigned long int",   FieldDataType::Uint64 },
    { "signed long",         FieldDataType::Int64 },
    { "long int",            FieldDataType::Int64 },
    { "byte",                FieldDataType::Uint8 },
    { "word",                FieldDataType::Uint16 },
    { "dword",               FieldDataType::Uint32 },
    { "qword",               FieldDataType::Uint64 },
    { "boolean",             FieldDataType::Bool },
    { "real",                FieldDataType::Float32 },
    { "single",              FieldDataType::Float32 }
};
const int kTypeLabelCount = sizeof(kTypeLabels) / sizeof(kTypeLabels[0]);
}

QStringList FieldTypeLabels::supportedDataTypeLabels()
{
    QStringList out;
    out << "Raw Unsigned BE" << "bool" << "uchar" << "char"
        << "ushort" << "short" << "uint" << "int"
        << "ulong" << "long" << "float" << "double" << "string";
    return out;
}

QString FieldTypeLabels::dataTypeToLabel(FieldDataType dataType)
{
    switch (dataType)
    {
    case FieldDataType::RawUnsignedBE: return QString("Raw Unsigned BE");
    case FieldDataType::Bool:          return QString("bool");
    case FieldDataType::Uint8:         return QString("uchar");
    case FieldDataType::Int8:          return QString("char");
    case FieldDataType::Uint16:        return QString("ushort");
    case FieldDataType::Int16:         return QString("short");
    case FieldDataType::Uint32:        return QString("uint");
    case FieldDataType::Int32:         return QString("int");
    case FieldDataType::Uint64:        return QString("ulong");
    case FieldDataType::Int64:         return QString("long");
    case FieldDataType::Float32:       return QString("float");
    case FieldDataType::Float64:       return QString("double");
    case FieldDataType::String:        return QString("string");
    }
    return QString("Raw Unsigned BE");
}

bool FieldTypeLabels::dataTypeFromLabel(const QString& label, FieldDataType& dataType)
{
    const QString trimmed = label.trimmed();
    for (int i = 0; i < kTypeLabelCount; ++i)
    {
        if (trimmed.compare(QString::fromLatin1(kTypeLabels[i].label), Qt::CaseInsensitive) == 0)
        {
            dataType = kTypeLabels[i].type;
            return true;
        }
    }
    return false;
}

bool FieldTypeLabels::dataTypeFromLabelAndSize(const QString& label, int sizeBytes, FieldDataType& dataType)
{
    // Exact alias wins (covers Uint16/ushort/float/double/"unsigned integer"/...).
    if (dataTypeFromLabel(label, dataType))
        return true;

    const QString s = label.trimmed().toLower();
    if (s.isEmpty())
        return false;

    // Floating point.
    if (s.contains("double"))
    {
        dataType = FieldDataType::Float64;
        return true;
    }
    if (s.contains("float") || s.contains("real") || s.contains("single"))
    {
        dataType = (sizeBytes == 8) ? FieldDataType::Float64 : FieldDataType::Float32;
        return true;
    }
    if (s.contains("bool"))
    {
        dataType = FieldDataType::Bool;
        return true;
    }
    if (s.contains("string") || s.contains("text") || s.contains("ascii") || s.contains("utf"))
    {
        dataType = FieldDataType::String;
        return true;
    }

    // Integer family. Only proceed if an integer-ish word is present.
    const bool integerWord =
        s.contains("int") || s.contains("char") || s.contains("short") ||
        s.contains("long") || s.contains("byte") || s.contains("word");
    if (!integerWord)
        return false;

    // Width: trust the Size column when it is a natural width; else infer from word.
    int width = sizeBytes;
    if (width != 1 && width != 2 && width != 4 && width != 8)
    {
        if      (s.contains("char") || s.contains("byte"))  width = 1;
        else if (s.contains("dword"))                       width = 4;
        else if (s.contains("qword"))                       width = 8;
        else if (s.contains("short") || s.contains("word")) width = 2;
        else if (s.contains("long"))                        width = 8;
        else if (s.contains("int"))                         width = 4;
        else                                                width = 0;
    }

    // Signedness: ICD integers are unsigned unless explicitly "signed".
    const bool explicitSigned = s.contains("signed") && !s.contains("unsigned");
    const bool isUnsigned = !explicitSigned;

    switch (width)
    {
    case 1: dataType = isUnsigned ? FieldDataType::Uint8  : FieldDataType::Int8;  return true;
    case 2: dataType = isUnsigned ? FieldDataType::Uint16 : FieldDataType::Int16; return true;
    case 4: dataType = isUnsigned ? FieldDataType::Uint32 : FieldDataType::Int32; return true;
    case 8: dataType = isUnsigned ? FieldDataType::Uint64 : FieldDataType::Int64; return true;
    default:
        dataType = FieldDataType::RawUnsignedBE;   // unusual width -> generic BE unsigned
        return true;
    }
}
