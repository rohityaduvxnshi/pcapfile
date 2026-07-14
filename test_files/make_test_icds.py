#!/usr/bin/env python3
"""Generate a dataset of sample ICD .docx files for the Universal Data Suite
test harness. A .docx is a ZIP of OOXML parts; we emit [Content_Types].xml,
_rels/.rels and word/document.xml with field-definition tables.

Each ICD is crafted to exercise specific features of the suite (see FEATURE
notes per ICD). The C++ test harness (tests/run_tests.pro) reads these back
through the real IcdDocxImporter and asserts on the result.
"""
import zipfile, os

NS = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
HERE = os.path.dirname(os.path.abspath(__file__))


def esc(s):
    return s.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')


def cell(text):
    return ('<w:tc><w:tcPr><w:tcW w:w="2000" w:type="dxa"/></w:tcPr>'
            '<w:p><w:r><w:t xml:space="preserve">%s</w:t></w:r></w:p></w:tc>' % esc(text))


def row(cells):
    return '<w:tr>' + ''.join(cell(c) for c in cells) + '</w:tr>'


def table(rows):
    body = ''.join(row(r) for r in rows)
    return ('<w:tbl><w:tblPr><w:tblStyle w:val="TableGrid"/>'
            '<w:tblBorders><w:top w:val="single" w:sz="4" w:space="0" w:color="auto"/>'
            '<w:left w:val="single" w:sz="4" w:space="0" w:color="auto"/>'
            '<w:bottom w:val="single" w:sz="4" w:space="0" w:color="auto"/>'
            '<w:right w:val="single" w:sz="4" w:space="0" w:color="auto"/>'
            '<w:insideH w:val="single" w:sz="4" w:space="0" w:color="auto"/>'
            '<w:insideV w:val="single" w:sz="4" w:space="0" w:color="auto"/></w:tblBorders></w:tblPr>'
            + body + '</w:tbl>')


def heading(text):
    return ('<w:p><w:pPr><w:pStyle w:val="Heading1"/></w:pPr>'
            '<w:r><w:t xml:space="preserve">%s</w:t></w:r></w:p>' % esc(text))


def para(text=''):
    return '<w:p><w:r><w:t xml:space="preserve">%s</w:t></w:r></w:p>' % esc(text)


def write_docx(name, body_parts):
    body = ''.join(body_parts)
    document = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
                '<w:document xmlns:w="%s"><w:body>%s'
                '<w:sectPr><w:pgSz w:w="12240" w:h="15840"/></w:sectPr>'
                '</w:body></w:document>' % (NS, body))
    content_types = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
        '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
        '<Default Extension="xml" ContentType="application/xml"/>'
        '<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>'
        '</Types>')
    rels = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
        '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>'
        '</Relationships>')
    out = os.path.join(HERE, name)
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('[Content_Types].xml', content_types)
        z.writestr('_rels/.rels', rels)
        z.writestr('word/document.xml', document)
    print('wrote', name, os.path.getsize(out), 'bytes')


HDR = ['Field Name', 'Byte Offset', 'Data Type', 'Length', 'Resolution', 'Description']

# 1) Basic multi-message radar ICD — 2 messages on one port.
#    FEATURE: ICD extraction, multi-table/multi-message, column auto-detect.
track = [HDR,
    ['Sync',        '1',  'Uint16',  '2', '1',         'Sync word 0xAA55'],
    ['MessageID',   '3',  'Uint8',   '1', '1',         'Message id (0x10)'],
    ['TrackNumber', '4',  'Uint16',  '2', '1',         'Unique track number'],
    ['Latitude',    '6',  'Int32',   '4', '0.0000001', 'Latitude deg'],
    ['Longitude',   '10', 'Int32',   '4', '0.0000001', 'Longitude deg'],
    ['Altitude',    '14', 'Float32', '4', '1',         'Altitude m'],
    ['Speed',       '18', 'Uint16',  '2', '0.01',      'Ground speed m/s'],
    ['Status',      '20', 'Uint8',   '1', '1',         'Track status'],
]
status = [HDR,
    ['Sync',        '1', 'Uint16', '2', '1',    'Sync word 0xAA55'],
    ['MessageID',   '3', 'Uint8',  '1', '1',    'Message id (0x20)'],
    ['Mode',        '4', 'Uint8',  '1', '1',    'Operating mode'],
    ['Temperature', '5', 'Int16',  '2', '0.1',  'Temperature deg C'],
    ['Voltage',     '7', 'Uint16', '2', '0.01', 'Supply voltage V'],
]
write_docx('icd_radar_basic.docx',
    [heading('ICD - Demo Radar Feed'),
     para('UDP messages on port 5000.'),
     heading('Track Report Message'), table(track), para(),
     heading('System Status Message'), table(status), para()])

# 2) Every supported data type in one message.
#    FEATURE: type mapping for all FieldDataTypes + encode/decode of each.
alltypes = [HDR,
    ['f_u8',   '1',  'Uint8',   '1', '1',    'unsigned char'],
    ['f_i8',   '2',  'Int8',    '1', '1',    'signed char'],
    ['f_u16',  '3',  'Uint16',  '2', '1',    'unsigned short'],
    ['f_i16',  '5',  'Int16',   '2', '1',    'signed short'],
    ['f_u32',  '7',  'Uint32',  '4', '1',    'unsigned int'],
    ['f_i32',  '11', 'Int32',   '4', '1',    'signed int'],
    ['f_u64',  '15', 'Uint64',  '8', '1',    'unsigned long'],
    ['f_i64',  '23', 'Int64',   '8', '1',    'signed long'],
    ['f_f32',  '31', 'Float32', '4', '1',    'float'],
    ['f_f64',  '35', 'Float64', '8', '1',    'double'],
    ['f_bool', '43', 'Bool',    '1', '1',    'boolean'],
    ['f_str',  '44', 'String',  '6', '1',    'text'],
]
write_docx('icd_all_types.docx',
    [heading('ICD - All Data Types'),
     para('One message exercising every supported field type.'),
     heading('AllTypes Message'), table(alltypes), para()])

# 3) Scaled fields with resolution expressions.
#    FEATURE: resolution parsing (decimals + fractions) + scaled decode.
scaled = [HDR,
    ['Pressure',    '1',  'Uint16', '2', '0.01',   'Pressure, 0.01 hPa'],
    ['Heading',     '3',  'Uint16', '2', '0.0055', 'Heading, scale 0.0055 deg'],
    ['RateOfTurn',  '5',  'Int16',  '2', '1/3',    'Rate, scale 1/3'],
    ['Depth',       '7',  'Int32',  '4', '0.001',  'Depth, 0.001 m'],
]
write_docx('icd_scaled.docx',
    [heading('ICD - Scaled Sensor Block'),
     para('Fields with non-unity resolution, including a fractional scale.'),
     heading('Sensor Message'), table(scaled), para()])

# 4) Wide fields beyond 8 bytes (reader-only wide decode).
#    FEATURE: wide-field decode (>8 bytes) via base-256->base-10 conversion.
wide = [HDR,
    ['Counter',     '1',  'Uint32',  '4',  '1', 'message counter'],
    ['BigId',       '5',  'RawUnsignedBE', '12', '1', '12-byte unsigned id'],
    ['SerialBlob',  '17', 'RawUnsignedBE', '16', '1', '16-byte serial'],
    ['Tail',        '33', 'Uint16',  '2',  '1', 'tail marker'],
]
write_docx('icd_wide_fields.docx',
    [heading('ICD - Wide Identifier Block'),
     para('Fields wider than 8 bytes, decoded by the reader exactly.'),
     heading('WideBlock Message'), table(wide), para()])

# 5) Length + Message-ID fields (compare-options derivation in parser review).
#    FEATURE: data-length / message-id ICD fields recognised on review.
lenmsg = [HDR,
    ['MessageID',  '1', 'Uint16', '2', '1', 'Message identifier'],
    ['DataLength', '3', 'Uint16', '2', '1', 'Payload data length in bytes'],
    ['PayloadA',   '5', 'Uint32', '4', '1', 'first data word'],
    ['PayloadB',   '9', 'Uint32', '4', '1', 'second data word'],
]
write_docx('icd_length_msgid.docx',
    [heading('ICD - Framed Message'),
     para('Carries an explicit message id and data-length field.'),
     heading('Framed Message'), table(lenmsg), para()])

# 6) Lenient rows: missing cells + duplicate names.
#    FEATURE: lenient import (<=1 missing key kept, dup-name _2 suffix).
lenient = [HDR,
    ['Alpha',   '1', 'Uint8',  '1', '1', 'ok row'],
    ['',        '2', 'Uint8',  '1', '1', 'missing name only -> auto-named, kept'],
    ['Beta',    '3', '',       '1', '1', 'missing type only -> default, kept'],
    ['Alpha',   '4', 'Uint8',  '1', '1', 'duplicate name -> renamed _2'],
    ['Gamma',   '5', 'Uint8',  '1', '1', 'ok row'],
]
write_docx('icd_lenient.docx',
    [heading('ICD - Imperfect Table'),
     para('Some rows are missing a single key; importer should keep them.'),
     heading('Loose Message'), table(lenient), para()])

# 7) Continuation table: one message split across two adjacent tables.
#    FEATURE: continuation auto-grouping + grouped draft concatenation.
cont_head = [HDR,
    ['Sync',     '1', 'Uint16', '2', '1', 'sync'],
    ['Id',       '3', 'Uint8',  '1', '1', 'id'],
    ['FieldA',   '4', 'Uint16', '2', '1', 'A'],
]
cont_tail = [HDR,
    ['FieldB',   '6',  'Uint16', '2', '1', 'B'],
    ['FieldC',   '8',  'Uint32', '4', '1', 'C'],
    ['FieldD',   '12', 'Uint16', '2', '1', 'D'],
]
write_docx('icd_continuation.docx',
    [heading('ICD - Split Message'),
     para('A single message whose fields continue in a second table.'),
     heading('Long Message'), table(cont_head),
     heading('Long Message (cont.)'), table(cont_tail), para()])

# 8) Word-offset labelled ICD (offset entry/display unit context).
#    FEATURE: offsets meant to be read as 16-bit words (offsetUnit context).
words = [['Field Name', 'Word Offset', 'Data Type', 'Length', 'Resolution', 'Description'],
    ['WordA', '1', 'Uint16', '2', '1', 'first word'],
    ['WordB', '2', 'Uint16', '2', '1', 'second word'],
    ['WordC', '3', 'Uint16', '2', '1', 'third word'],
    ['WordD', '4', 'Uint16', '2', '1', 'fourth word'],
]
write_docx('icd_word_offsets.docx',
    [heading('ICD - Word Addressed Block'),
     para('Offsets are given in 16-bit words (1 word = 2 bytes).'),
     heading('WordBlock Message'), table(words), para()])

print('done')
