#!/usr/bin/env python3
########################################################################
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
########################################################################

import orcus
import json
import os
import os.path
import sys
import enum

from common import config


FORMULAS_JSON_FILENAME = f"{config.prefix_skip}formulas.json"


class JSONEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, orcus.FormulaToken):
            return str(obj)
        if isinstance(obj, orcus.FormulaTokenType):
            n = len("FormulaTokenType.")
            s = str(obj)
            return s[n:].lower()

        return json.JSONEncoder.default(self, obj)


def process_formula_tokens(obj):
    formula_tokens = list()
    for token in obj.formula_tokens:
        formula_tokens.append([token, token.type])
    return formula_tokens


def process_document(filepath, doc):
    """File processor callback function."""

    sheet_names = list()
    formula_cells =  list()
    named_exps = list()

    for name, named_exp in doc.named_expressions.items():
        named_exps.append({
            "name": name,
            "formula": named_exp.formula,
            "formula_tokens": process_formula_tokens(named_exp),
            "scope": "global"
        })

    for sh in doc.sheets:
        sheet_names.append(sh.name)

        for row_pos, row in enumerate(sh.get_rows()):
            for col_pos, cell in enumerate(row):
                data = dict(sheet=sh.name, row=row_pos, column=col_pos)
                if cell.type == orcus.CellType.FORMULA:
                    data.update({
                        "valid": True,
                        "formula": cell.formula,
                    })

                    data["formula_tokens"] = process_formula_tokens(cell)

                elif cell.type == orcus.CellType.FORMULA_WITH_ERROR:
                    data.update({
                        "valid": False,
                        "formula": cell.formula_tokens[1],  # original formula string
                        "error": cell.formula_tokens[2],
                    })
                else:
                    continue

                formula_cells.append(data)

    data = dict()
    data["filepath"] = filepath
    data["sheets"] = sheet_names
    data["formulas"] = formula_cells
    data["named_expressions"] = named_exps

    output_buffer = list()
    for sn in data["sheets"]:
        output_buffer.append(f"* sheet: {sn}")

    n = len(data["formulas"])
    output_buffer.append(f"* formula cell count: {n}")

    dirpath = os.path.dirname(filepath)
    outpath = os.path.join(dirpath, FORMULAS_JSON_FILENAME)
    with open(outpath, "w") as f:
        s = json.dumps(data, cls=JSONEncoder)
        f.write(s)

    return output_buffer


class OutputFormat(enum.Enum):
    JSON = "json"
    XML = "xml"


def dump_json_as_xml(data, stream):
    import xml.etree.ElementTree as ET
    root = ET.Element("docs")
    root.attrib["count"] = str(len(data))

    for doc in data:
        elem_doc = ET.SubElement(root, "doc")
        elem_doc.attrib["filepath"] = doc["filepath"]
        elem_sheets = ET.SubElement(elem_doc, "sheets")
        elem_sheets.attrib["count"] = str(len(doc["sheets"]))
        for sheet in doc["sheets"]:
            elem = ET.SubElement(elem_sheets, "sheet")
            elem.attrib["name"] = sheet

        elem_formulas = ET.SubElement(elem_doc, "formulas")
        elem_formulas.attrib["count"] = str(len(doc["formulas"]))
        for formula_cell in doc["formulas"]:
            elem = ET.SubElement(elem_formulas, "formula")
            elem.attrib["sheet"] = formula_cell["sheet"]
            elem.attrib["row"] = str(formula_cell["row"])
            elem.attrib["column"] = str(formula_cell["column"])
            elem.attrib["s"] = formula_cell["formula"]
            elem.attrib["valid"] = "true" if formula_cell["valid"] else "false"
            if formula_cell["valid"]:
                elem.attrib["token-count"] = str(len(formula_cell["formula_tokens"]))
                for token in formula_cell["formula_tokens"]:
                    elem_token = ET.SubElement(elem, "token")
                    elem_token.attrib["s"] = token[0]
                    elem_token.attrib["type"] = token[1]
            else:
                # invalid formula
                elem.attrib["error"] = formula_cell["error"]

        elem_named_exps = ET.SubElement(elem_doc, "named-expressions")
        elem_named_exps.attrib["count"] = str(len(doc["named_expressions"]))
        for named_exp in doc["named_expressions"]:
            elem = ET.SubElement(elem_named_exps, "named-expressions")
            elem.attrib["name"] = named_exp["name"]
            elem.attrib["formula"] = named_exp["formula"]
            elem.attrib["scope"] = named_exp["scope"]
            elem.attrib["token-count"] = str(len(named_exp["formula_tokens"]))
            for token in named_exp["formula_tokens"]:
                elem_token = ET.SubElement(elem, "token")
                elem_token.attrib["s"] = token[0]
                elem_token.attrib["type"] = token[1]

    s = ET.tostring(root, "utf-8").decode("utf-8")
    from xml.dom import minidom
    s = minidom.parseString(s).toprettyxml(indent="    ")
    stream.write(s)


def dump_batch_to_file(i, batch, args):
    if args.format == OutputFormat.JSON:
        outpath = os.path.join(args.output, f"{i+1:04}.json")
        import pprint
        with open(outpath, "w") as f:
            pprint.pprint(batch, stream=f, width=256)
    elif args.format == OutputFormat.XML:
        outpath = os.path.join(args.output, f"{i+1:04}.xml")
        with open(outpath, "w") as f:
            dump_json_as_xml(batch, f)


def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", "--output", type=str, help="Output directory to write all the formula data to.")
    parser.add_argument("-f", "--format", type=OutputFormat, default=OutputFormat.JSON, help="Output file format.")
    parser.add_argument("-b", "--batch-size", type=int, default=20)
    parser.add_argument("rootdir", help="Root directory from which to traverse for the formula files.")
    args = parser.parse_args()

    os.makedirs(args.output, exist_ok=True)

    data = list()
    args.output = args.output if args.output else sys.stdout

    i = 0
    for root, dir, files in os.walk(args.rootdir):
        for filename in files:
            if filename != FORMULAS_JSON_FILENAME:
                continue

            filepath = os.path.join(root, filename)
            with open(filepath, "r") as f:
                data.append(json.loads(f.read()))

            if len(data) == args.batch_size:
                print(f"dumping batch {i+1}...", flush=True)
                dump_batch_to_file(i, data, args)
                data.clear()
                i += 1

    if data:
        dump_batch_to_file(i, data, args)


if __name__ == "__main__":
    main()
