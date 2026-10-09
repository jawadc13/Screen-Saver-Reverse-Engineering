"""Dumps the parts of a Windows screensaver (.scr) that matter for reverse
engineering: header, imports, resource tree, string table and readable
strings.  Requires `pip install pefile`.

Usage: python3 -I tools/pe_inspect.py path/to/file.scr
"""
import re
import struct
import sys

import pefile


def main(path):
    pe = pefile.PE(path)
    print('Machine 0x%x  Subsystem %d  Timestamp 0x%x' % (
        pe.FILE_HEADER.Machine, pe.OPTIONAL_HEADER.Subsystem, pe.FILE_HEADER.TimeDateStamp))
    for s in pe.sections:
        print('  section %-8s raw 0x%x' % (s.Name.rstrip(b'\0').decode(), s.SizeOfRawData))

    print('\nImports:')
    for e in getattr(pe, 'DIRECTORY_ENTRY_IMPORT', []):
        names = [i.name.decode() if i.name else '#%d' % i.ordinal for i in e.imports]
        print('  %s: %s' % (e.dll.decode(), ', '.join(names)))

    print('\nResources:')
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        tname = pefile.RESOURCE_TYPE.get(t.id, t.name)
        print('  %s: %s' % (tname, [str(n.name or n.id) for n in t.directory.entries]))
        if t.id == pefile.RESOURCE_TYPE['RT_STRING']:
            for n in t.directory.entries:
                d = n.directory.entries[0].data.struct
                b = pe.get_data(d.OffsetToData, d.Size)
                o = 0
                for i in range(16):
                    ln = struct.unpack_from('<H', b, o)[0]
                    o += 2
                    if ln:
                        print('    STRING %5d %r' % ((n.id - 1) * 16 + i, b[o:o + 2 * ln].decode('utf-16le')))
                    o += 2 * ln

    data = open(path, 'rb').read()
    print('\nInteresting ASCII strings:')
    for m in re.finditer(rb'[\x20-\x7e]{6,}', data):
        s = m.group().decode()
        if re.search(r'(?i)pwd|password|\.dll|\.cpl|wndclass|screen|control|\.pdb|\.scr', s):
            print('  ', s)
    print('\nInteresting UTF-16 strings:')
    for m in re.finditer(rb'(?:[\x20-\x7e]\x00){6,}', data):
        s = m.group().decode('utf-16le')
        if re.search(r'(?i)software\\|screen|control|black|adapter|speed|texture|joint|pipes', s):
            print('  ', s)


if __name__ == '__main__':
    main(sys.argv[1])
