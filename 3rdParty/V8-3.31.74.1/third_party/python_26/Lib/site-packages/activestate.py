#!/usr/bin/env python
# Copyright (c) 2002-2009 ActiveState Software Inc.  All rights reserved.

"""ActivePython identification module

This can be run as a script to dump version info:
    python .../activestate.py
or to relocate this Python installation appropriately (see relocate_python()
for details):
    python .../activestate.py --relocate
"""

import sys



#---- ActivePython build/configuration info

version_info = {'bsddb_ver': None,
 'build_host': 'activepython-bu',
 'build_num': 2,
 'build_plat_fullname': 'win32-xp-x86',
 'build_plat_name': 'win32-x86',
 'build_time': 'Tue Apr 21 15:38:59 2009',
 'bzip2_ver': (1, 0, 5),
 'compiler': 'vc9-x86',
 'configuration': ['-f',
                   'apyconfig-apy26-rrun-win32-x86.py',
                   '-p',
                   'apy26',
                   '--build-tag',
                   'rrun'],
 'openssl_ver': (0, 9, 8, 'k'),
 'platinfo': {'arch': 'x86',
              'os': 'win32',
              'os_csd': 'SP3',
              'os_name': 'XP',
              'os_ver': '5.1.2600'},
 'platname': 'win32-x86',
 'product_type': 'ActivePython',
 'python_src': ('2.6.2', 'path', 'Python-2.6.2.tgz'),
 'pywin32_build': '211.1',
 'pywin32_src': ('20080624', 'path', 'pywin32-20080624-CRLF.zip'),
 'pywin32_ver': '20080624',
 'scm_revision': 'r2668-branches-sslfixrelease',
 'sqlite3_ver': (3, 5, 9),
 'tcltk_ver': (8, 5, 7),
 'tix_ver': (8, 4, 3),
 'with_bsddb': False,
 'with_bzip2': True,
 'with_ctypes': True,
 'with_docs': True,
 'with_pywin32': True,
 'with_sqlite3': True,
 'with_ssl': True,
 'with_tcltk': True,
 'with_tests': True,
 'zlib_ver': (1, 2, 3)}

compiler_info = """Microsoft (R) 32-bit C/C++ Optimizing Compiler Version 15.00.21022.08 for 80x86"""


# Used for Python install relocation.
prefixes = set([
    # Prefix to which extensions were built
    'C:\\as\\activepython\\branches\\sslfixrelease\\build\\py2_6_2-win32-x86-apy26-rrun\\ExTAcTiVePyThOnPrEfIxExTAcTiVePyThOnPrEfIxExTAcTiVePyThOnPrEfIxExTAcTiVePyThOnPrEfIxExTAcTiVePyThOnPrEfIxExTAcTiVePyThOnPrEfIxExTAcTiVePyThOnPrEfIxExTAcTiVePyThOnPrEfIxExTAcTiVePyThOnPrEfIxExTAcTiVePyThOnPrEfIx',
    # Prefix to which Python sources were built.
    'C:\\as\\activepython\\branches\\sslfixrelease\\build\\py2_6_2-win32-x86-apy26-rrun\\CoReAcTiVePyThOnPrEfIxCoReAcTiVePyThOnPrEfIxCoReAcTiVePyThOnPrEfIxCoReAcTiVePyThOnPrEfIxCoReAcTiVePyThOnPrEfIxCoReAcTiVePyThOnPrEfIxCoReAcTiVePyThOnPrEfIxCoReAcTiVePyThOnPrEfIxCoReAcTiVePyThOnPrEfIxCoReAcTiVePyThOnPrEfIx'
])
shortest_original_prefix_length = 287



#---- relocation code

def _is_path_binary(path):
    """Return true iff the given file is binary.

    Raises an EnvironmentError if the file does not exist or cannot be
    accessed.
    """
    fin = open(path, 'rb')
    try:
        CHUNKSIZE = 1024
        while 1:
            chunk = fin.read(CHUNKSIZE)
            if '\0' in chunk: # found null byte
                return True
            if len(chunk) < CHUNKSIZE:
                break # done
    finally:
        fin.close()
    return False


def _relocate_path(path, from_prefix, to_prefix, log):
    import sys
    import os
    from os.path import join
    import stat
    import re

    # Determine if this file needs to be relocated.
    fin = open(path, 'rb')
    try:
        content = fin.read()
    finally:
        fin.close()
    is_binary = _is_path_binary(path)
    if is_binary:
        from_str = join(from_prefix, "lib")
        to_str = join(to_prefix, "lib")
    else:
        from_str = from_prefix
        to_str = to_prefix
    if sys.version_info[0] >= 3:
        from_str = bytes(from_str, 'utf-8')
        to_str = bytes(to_str, 'utf-8')
    if from_str not in content:
        return

    # Relocate this file.
    log("relocate '%s'" % path)
    perm = stat.S_IMODE(os.stat(path).st_mode)
    if is_binary:
        if sys.platform.startswith("aix"):
            # On AIX the lib path _list_ is stored as one string, rather
            # than just the one path. This means that the integrity of
            # the path list must be maintained by separating with ':'.
            # We also change the remainder to all x's to ensure it is
            # a bogus path.
            to_str = join(to_prefix, "lib") \
                + ':' + "x"*(len(from_prefix)-len(to_prefix)-1)
            if sys.version_info[0] >= 3:
                to_str = bytes(to_str, 'utf-8')
            #log("replace (length %d)\n\t%s\nwith (length %d)\n\t%s",
            #    % (len(from_str), from_str, len(to_str), to_str))
            content = content.replace(from_str, to_str)
        else:
            # Replace 'from_str' with 'to_str' in a null-terminated string.
            # Make sure to properly correct for trailing content in the
            # same string because:
            # - on HP-UX sometimes a full path to the shared lib is stored:
            #      <from_str>/libtcl8.4.sl\0
            # - on AIX a path _list_ is stored:
            #      <from_str>:other/lib/paths\0
            #   NOTE: This *should* work on AIX, AFAICT, but it does
            #   *not*. See above for special handling for AIX.
            #TODO: should this regex use re.DOTALL flag?
            pattern = re.compile(re.escape(from_str) + "([^\0]*)\0")
            def c_string_replace(match, before=from_str, after=to_str):
                lendiff = len(before) - len(after)
                s = after + match.group(1) + ("\0" * lendiff) + "\0"
                # Encode nulls as '0' instead of '\x00' so one can see
                # the before and after strings line up.
                #log("replace (length %d)\n\t%s\nwith (length %d)\n\t%s",
                #    % (len(match.group(0)),
                #        repr(match.group(0)).replace("\\x00", '0'),
                #        len(s),
                #        repr(s).replace("\\x00", '0')))
                return s
            content = pattern.sub(c_string_replace, content)
    else:
        #log("replace (length %d)\n\t%s\nwith (length %d)\n\t%s",
        #    % (len(from_str), from_str, len(to_str), to_str))
        content = content.replace(from_str, to_str)
    # Sometimes get the following error. Avoid it by removing file first.
    #   IOError: [Errno 26] Text file busy: '$path'
    os.remove(path)
    fout = open(path, 'wb')
    try:
        fout.write(content)
    finally:
        fout.close()
    os.chmod(path, perm) # restore permissions


def relocate_python(install_prefix, verbose=False):
    """Relocate this Python installation.
    
    "Relocation" involves updating hardcoded shebang lines in Python scripts
    and (on some platforms) binary patching of built-in runtime-lib-paths
    to point to the given install prefix.
    """
    import sys
    import os
    from os.path import isabs, join, splitext
    
    if verbose:
        def log(s):
            sys.stderr.write(s+"\n")
    else:
        def log(s):
            pass
    
    assert isabs(install_prefix)

    if len(install_prefix) > shortest_original_prefix_length:
        raise RuntimeError("cannot properly relocate this Python "
                           "installation (prefix='%s') because install "
                           "path (%d chars) is longer than the original "
                           "build prefix (%d chars)"
                           % (install_prefix, len(install_prefix),
                               shortest_original_prefix_length))

    log("relocate this Python to '%s'" % install_prefix)
    for prefix in prefixes:
        if prefix == install_prefix:
            continue
        for dirpath, dirnames, filenames in os.walk(install_prefix):
            for filename in filenames:
                if splitext(filename)[1] in (".pyo", ".pyc"):
                    continue
                _relocate_path(join(dirpath, filename),
                               prefix, install_prefix, log)


#---- mainline

if __name__ == "__main__":
    if "--relocate" in sys.argv:
        # Determine the install_prefix holding this module and relocate
        # that Python installation.
        if sys.platform == "win32":
            raise RuntimeError("relocating a Python install isn't "
                               "necessary on Windows")

        # <prefix>\lib\pythonX.Y\site-packages\activestate.py
        from os.path import dirname, exists, join, basename, abspath
        install_prefix = dirname(dirname(dirname(dirname(abspath(__file__)))))
        python_exe = join(install_prefix, "bin", "python")
        if not exists(python_exe):
            raise RuntimeError("'%s' does not exist: it doesn't look like "
                               "'%s' is in a Python site-packages dir"
                               % (python_exe, basename(__file__)))
        del python_exe, dirname, exists, join, basename, abspath
        
        relocate_python(install_prefix, True)

    else:
        for key, value in sorted(version_info.items()):
            if value is None: continue
            if key.endswith("_src"): continue
            if key in ("platinfo", "configuration"): continue
            print("%s: %s" % (key, value))
    
