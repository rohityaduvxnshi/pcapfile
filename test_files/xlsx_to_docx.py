#!/usr/bin/env python3
"""Convert the Universal Data Suite test report .xlsx to a .docx where each
worksheet becomes exactly one page, preserving the sheet's colours and
formatting (cell fills, font colour/bold/size, per-cell borders, alignment,
merged cells and proportional column widths).

Usage: python xlsx_to_docx.py <input.xlsx> <output.docx> [body_pt]
"""
import sys, os
import openpyxl
from openpyxl.utils import get_column_letter
from docx import Document
from docx.shared import Pt, Inches, RGBColor
from docx.enum.section import WD_ORIENT, WD_SECTION
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_ALIGN_VERTICAL
from docx.oxml.ns import qn
from docx.oxml import OxmlElement

INP = sys.argv[1] if len(sys.argv) > 1 else r'test_files/output/Universal_Data_Suite_Test_Report.xlsx'
OUT = sys.argv[2] if len(sys.argv) > 2 else r'test_files/output/Universal_Data_Suite_Test_Report.docx'
BODY = float(sys.argv[3]) if len(sys.argv) > 3 else 7.0   # base body point size
HEADER = BODY + 1.0
DEFAULT_COLW = 8.43

# Wide, many-column sheets render landscape so the grid is not squeezed.
LANDSCAPE = {'Requirements', 'Test Cases'}


def rgb_of(color):
    """openpyxl Color -> 'RRGGBB' hex (drop alpha) or None for default/theme."""
    if color is None:
        return None
    try:
        if color.type == 'rgb' and color.rgb:
            s = str(color.rgb)
            return s[-6:]  # strip the AA in AARRGGBB
    except Exception:
        pass
    return None


def shade(cell, hexrgb):
    if not hexrgb:
        return
    tcPr = cell._tc.get_or_add_tcPr()
    shd = OxmlElement('w:shd')
    shd.set(qn('w:val'), 'clear')
    shd.set(qn('w:color'), 'auto')
    shd.set(qn('w:fill'), hexrgb)
    tcPr.append(shd)


def cell_borders(cell, sides):
    """sides: dict side->bool (whether that edge is drawn in the xlsx)."""
    if not any(sides.values()):
        return
    tcPr = cell._tc.get_or_add_tcPr()
    borders = OxmlElement('w:tcBorders')
    for side in ('top', 'left', 'bottom', 'right'):
        el = OxmlElement('w:' + side)
        if sides.get(side):
            el.set(qn('w:val'), 'single')
            el.set(qn('w:sz'), '4')
            el.set(qn('w:space'), '0')
            el.set(qn('w:color'), 'BFBFBF')
        else:
            el.set(qn('w:val'), 'nil')
        borders.append(el)
    tcPr.append(borders)


def set_cell_margins(table, top=14, bottom=14, left=54, right=54):
    """Tight cell padding (twips) so dense tables fit one page."""
    tblPr = table._tbl.tblPr
    mar = OxmlElement('w:tblCellMar')
    for side, val in (('top', top), ('bottom', bottom), ('left', left), ('right', right)):
        el = OxmlElement('w:' + side)
        el.set(qn('w:w'), str(val))
        el.set(qn('w:type'), 'dxa')
        mar.append(el)
    tblPr.append(mar)


def fixed_layout(table):
    tblPr = table._tbl.tblPr
    layout = OxmlElement('w:tblLayout')
    layout.set(qn('w:type'), 'fixed')
    tblPr.append(layout)


ALIGN = {'center': WD_ALIGN_PARAGRAPH.CENTER, 'left': WD_ALIGN_PARAGRAPH.LEFT,
         'right': WD_ALIGN_PARAGRAPH.RIGHT, 'justify': WD_ALIGN_PARAGRAPH.JUSTIFY}
VALIGN = {'center': WD_ALIGN_VERTICAL.CENTER, 'top': WD_ALIGN_VERTICAL.TOP,
          'bottom': WD_ALIGN_VERTICAL.BOTTOM}


def write_cell(cell, xl, scale):
    para = cell.paragraphs[0]
    para.paragraph_format.space_before = Pt(0)
    para.paragraph_format.space_after = Pt(0)
    para.paragraph_format.line_spacing = 1.0
    halign = xl.alignment.horizontal if xl.alignment else None
    para.alignment = ALIGN.get(halign, WD_ALIGN_PARAGRAPH.LEFT)
    valign = (xl.alignment.vertical if xl.alignment else None) or 'top'
    cell.vertical_alignment = VALIGN.get(valign, WD_ALIGN_VERTICAL.TOP)

    text = '' if xl.value is None else str(xl.value)
    run = para.add_run(text)
    f = xl.font
    sz = (f.size or 11.0) if f else 11.0
    run.font.size = Pt(max(5.5, sz * scale))
    if f and f.bold:
        run.font.bold = True
    fc = rgb_of(f.color) if f else None
    if fc and fc != '000000':
        run.font.color.rgb = RGBColor.from_string(fc)

    # fill + borders mirror the spreadsheet cell
    fill = None
    if xl.fill is not None and xl.fill.patternType:
        fill = rgb_of(xl.fill.fgColor)
    shade(cell, fill)
    b = xl.border
    cell_borders(cell, {
        'top': bool(b and b.top and b.top.style),
        'bottom': bool(b and b.bottom and b.bottom.style),
        'left': bool(b and b.left and b.left.style),
        'right': bool(b and b.right and b.right.style),
    })


def set_section(section, landscape, content_in):
    if landscape:
        section.orientation = WD_ORIENT.LANDSCAPE
        section.page_width, section.page_height = Inches(11), Inches(8.5)
    else:
        section.orientation = WD_ORIENT.PORTRAIT
        section.page_width, section.page_height = Inches(8.5), Inches(11)
    section.top_margin = section.bottom_margin = Inches(0.4)
    section.left_margin = section.right_margin = Inches(0.45)
    return content_in


def build(inp, out, body_pt):
    wb = openpyxl.load_workbook(inp)
    doc = Document()
    # base scale so the title (xlsx ~13-16pt) shrinks proportionally
    scale = body_pt / 10.0

    for si, name in enumerate(wb.sheetnames):
        ws = wb[name]
        nrows, ncols = ws.max_row, ws.max_column
        landscape = name in LANDSCAPE

        if si == 0:
            section = doc.sections[0]
        else:
            section = doc.add_section(WD_SECTION.NEW_PAGE)
        set_section(section, landscape, None)

        usable = (11 - 0.9) if landscape else (8.5 - 0.9)  # inches available for the table

        # proportional column widths from the xlsx
        widths = []
        for c in range(1, ncols + 1):
            dim = ws.column_dimensions.get(get_column_letter(c))
            widths.append(dim.width if (dim and dim.width) else DEFAULT_COLW)
        tot = sum(widths) or 1.0
        col_in = [usable * w / tot for w in widths]

        table = doc.add_table(rows=nrows, cols=ncols)
        table.autofit = False
        table.allow_autofit = False
        fixed_layout(table)
        set_cell_margins(table)

        for r in range(nrows):
            for c in range(ncols):
                cell = table.cell(r, c)
                cell.width = Inches(col_in[c])
                write_cell(cell, ws.cell(r + 1, c + 1), scale)

        # apply merges (anchor keeps its content)
        for mr in ws.merged_cells.ranges:
            a = table.cell(mr.min_row - 1, mr.min_col - 1)
            b = table.cell(mr.max_row - 1, mr.max_col - 1)
            try:
                a.merge(b)
            except Exception:
                pass

    doc.save(out)
    print('wrote', out, '(body', body_pt, 'pt)')


if __name__ == '__main__':
    build(INP, OUT, BODY)
