#!/usr/bin/env python3
"""Generate a small, realistic sample ICD .docx for documentation screenshots.
A .docx is a ZIP of OOXML parts; we only need [Content_Types].xml, _rels/.rels
and word/document.xml containing a couple of field tables."""
import zipfile, os

NS = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'

def esc(s):
    return (s.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;'))

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

hdr = ['Field Name', 'Byte Offset', 'Data Type', 'Length (bytes)', 'Description']

track = [hdr,
    ['Sync',        '1',  'Uint16',  '2', 'Sync word, fixed 0xAA55'],
    ['MessageID',   '3',  'Uint8',   '1', 'Message identifier (0x10)'],
    ['TrackNumber', '4',  'Uint16',  '2', 'Unique track number'],
    ['Latitude',    '6',  'Int32',   '4', 'Latitude, scale 0.0000001 deg'],
    ['Longitude',   '10', 'Int32',   '4', 'Longitude, scale 0.0000001 deg'],
    ['Altitude',    '14', 'Float32', '4', 'Altitude in metres'],
    ['Speed',       '18', 'Uint16',  '2', 'Ground speed, scale 0.01 m/s'],
    ['TrackStatus', '20', 'Uint8',   '1', '0x00 - LOST / 0x01 - TENTATIVE / 0x02 - CONFIRMED'],
]

status = [hdr,
    ['Sync',        '1', 'Uint16', '2', 'Sync word, fixed 0xAA55'],
    ['MessageID',   '3', 'Uint8',  '1', 'Message identifier (0x20)'],
    ['Mode',        '4', 'Uint8',  '1', '0x00 - OFFLINE / 0x01 - STANDBY / 0x02 - ACTIVE'],
    ['Temperature', '5', 'Int16',  '2', 'Temperature, scale 0.1 deg C'],
    ['Voltage',     '7', 'Uint16', '2', 'Supply voltage, scale 0.01 V'],
]

body = (
    heading('Interface Control Document - Demo Radar Feed')
    + para('This document defines the UDP messages emitted on port 5000.')
    + heading('Track Report Message')
    + table(track)
    + para()
    + heading('System Status Message')
    + table(status)
    + para()
)

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

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'sample_icd.docx')
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
    z.writestr('[Content_Types].xml', content_types)
    z.writestr('_rels/.rels', rels)
    z.writestr('word/document.xml', document)
print('wrote', out, os.path.getsize(out), 'bytes')
