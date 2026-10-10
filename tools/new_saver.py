"""Scaffolds the resource files for a screensaver (stdlib only).

Usage: python3 tools/new_saver.py <name> "<Display Name>" "<File description>"

Creates savers/<name>/resource.h and savers/<name>/<name>.rc (name string,
icon, version info; the generic settings dialog comes from common.rc), then
adds <name> to SAVERS in the Makefile. Write savers/<name>/<name>.cpp
yourself (see savers/mystify/mystify.cpp for a compact example) and add an
icon to tools/make_icons.py.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

RESOURCE_H = '''#pragma once
#include "../../common/common_res.h"
'''

RC = '''#include <windows.h>
#include "resource.h"
#include "common.rc"

IDI_MAIN ICON "{name}.ico"

STRINGTABLE
BEGIN
    IDS_DESCRIPTION  "{display}"
END

VS_VERSION_INFO VERSIONINFO
 FILEVERSION 1,0,0,0
 PRODUCTVERSION 1,0,0,0
 FILEOS VOS_NT_WINDOWS32
 FILETYPE VFT_APP
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904B0"
        BEGIN
            VALUE "FileDescription", "{description}"
            VALUE "FileVersion", "1.0.0.0"
            VALUE "InternalName", "{internal}"
            VALUE "OriginalFilename", "{internal}.scr"
            VALUE "ProductName", "Screen Saver Reverse Engineering"
            VALUE "ProductVersion", "1.0.0.0"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END
'''


def main(name, display, description):
    d = os.path.join(ROOT, 'savers', name)
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, 'resource.h'), 'w') as f:
        f.write(RESOURCE_H)
    internal = display.replace(' ', '')
    with open(os.path.join(d, name + '.rc'), 'w') as f:
        f.write(RC.format(name=name, display=display, description=description, internal=internal))
    mk = os.path.join(ROOT, 'Makefile')
    text = open(mk).read()
    m = re.search(r'^SAVERS\s*=(.*)$', text, re.M)
    names = m.group(1).split()
    if name not in names:
        names.append(name)
        text = text[:m.start()] + 'SAVERS   = ' + ' '.join(names) + text[m.end():]
        open(mk, 'w').write(text)
    print('scaffolded', name)


if __name__ == '__main__':
    main(*sys.argv[1:4])
