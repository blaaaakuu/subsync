#!/usr/bin/env python3
"""Compile SubSync gettext PO catalogs without an external gettext install."""

import argparse
import ast
import struct
from pathlib import Path


def read_po(path):
    messages = {}
    msgid = None
    msgstr = None
    active = None

    def finish():
        nonlocal msgid, msgstr, active
        if msgid is not None and msgstr is not None:
            messages[msgid] = msgstr
        msgid = msgstr = active = None

    for raw_line in path.read_text(encoding='utf-8').splitlines() + ['']:
        line = raw_line.strip()
        if not line:
            finish()
        elif line.startswith('#'):
            continue
        elif line.startswith('msgid '):
            msgid = ast.literal_eval(line[6:])
            active = 'msgid'
        elif line.startswith('msgstr '):
            msgstr = ast.literal_eval(line[7:])
            active = 'msgstr'
        elif line.startswith('"'):
            value = ast.literal_eval(line)
            if active == 'msgid':
                msgid += value
            elif active == 'msgstr':
                msgstr += value

    return messages


def compile_mo(messages):
    encoded = sorted(
        (msgid.encode('utf-8'), msgstr.encode('utf-8'))
        for msgid, msgstr in messages.items()
    )
    count = len(encoded)
    originals_offset = 7 * 4
    translations_offset = originals_offset + count * 8
    strings_offset = translations_offset + count * 8

    originals = bytearray()
    translations = bytearray()
    original_table = []
    translation_table = []

    for original, _ in encoded:
        original_table.append((len(original), strings_offset + len(originals)))
        originals.extend(original + b'\0')

    translated_strings_offset = strings_offset + len(originals)
    for _, translation in encoded:
        translation_table.append(
            (len(translation), translated_strings_offset + len(translations))
        )
        translations.extend(translation + b'\0')

    header = struct.pack(
        '<7I',
        0x950412DE,
        0,
        count,
        originals_offset,
        translations_offset,
        0,
        0,
    )
    tables = b''.join(
        struct.pack('<2I', *entry)
        for entry in original_table + translation_table
    )
    return header + tables + originals + translations


def compile_catalog(path):
    output = path.with_suffix('.mo')
    output.write_bytes(compile_mo(read_po(path)))
    return output


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        'paths',
        nargs='*',
        type=Path,
        help='PO files (defaults to every subsync locale catalog)',
    )
    args = parser.parse_args()
    paths = args.paths or sorted(
        Path('subsync/locale').glob('*/LC_MESSAGES/messages.po')
    )
    for path in paths:
        print(compile_catalog(path))


if __name__ == '__main__':
    main()
