"""
builds windows executables from Python scripts

New keywords for distutils' setup function specify what to build:

    console
        list of scripts to convert into console exes

    windows
        list of scripts to convert into gui exes

    service
        list of module names containing win32 service classes

    com_server
        list of module names containing com server classes

    ctypes_com_server
        list of module names containing com server classes

    zipfile
        name of shared zipfile to generate, may specify a subdirectory,
        defaults to 'library.zip'


py2exe options, to be specified in the options keyword to the setup function:

    unbuffered - if true, use unbuffered binary stdout and stderr
    optimize - string or int (0, 1, or 2)

    includes - list of module names to include
    packages - list of packages to include with subpackages
    ignores - list of modules to ignore if they are not found
    excludes - list of module names to exclude
    dll_excludes - list of dlls to exclude

    dist_dir - directory where to build the final files
    typelibs - list of gen_py generated typelibs to include (XXX more text needed)

Items in the console, windows, service or com_server list can also be
dictionaries to further customize the build process.  The following
keys in the dictionary are recognized, most are optional:

    modules (SERVICE, COM) - list of module names (required)
    script (EXE) - list of python scripts (required)
    dest_base - directory and basename for the executable
                if a directory is contained, must be the same for all targets
    create_exe (COM) - boolean, if false, don't build a server exe
    create_dll (COM) - boolean, if false, don't build a server dll
    bitmap_resources - list of 2-tuples (id, pathname)
    icon_resources - list of 2-tuples (id, pathname)
    other_resources - list of 3-tuples (resource_type, id, datastring)
"""
# py2exe/__init__.py

# 'import py2exe' imports this package, and two magic things happen:
#
# - the 'py2exe.build_exe' submodule is imported and installed as
#   'distutils.commands.py2exe' command
#
# - the default distutils Distribution class is replaced by the
# special one contained in this module.
#

__version__ = "0.6.9"

import distutils.dist, distutils.core, distutils.command, build_exe, sys

class Distribution(distutils.dist.Distribution):

    def __init__(self, attrs):
        self.ctypes_com_server = attrs.pop("ctypes_com_server", [])
        self.com_server = attrs.pop("com_server", [])
        self.service = attrs.pop("service", [])
        self.windows = attrs.pop("windows", [])
        self.console = attrs.pop("console", [])
        self.isapi = attrs.pop("isapi", [])
        self.zipfile = attrs.pop("zipfile", "library.zip")

        distutils.dist.Distribution.__init__(self, attrs)

distutils.core.Distribution = Distribution

distutils.command.__all__.append('py2exe')

sys.modules['distutils.command.py2exe'] = build_exe
