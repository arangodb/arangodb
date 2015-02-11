# This support script is executed as the entry point for ctypes com servers.
# XXX Currently, this is always run as part of a dll.

import sys
import _ctypes

if 1:
    ################################################################
    # XXX Remove later!
    import ctypes
    class LOGGER:
        def __init__(self):
            self.softspace = None
        def write(self, text):
            if isinstance(text, unicode):
                ctypes.windll.kernel32.OutputDebugStringW(text)
            else:
                ctypes.windll.kernel32.OutputDebugStringA(text)
    sys.stderr = sys.stdout = LOGGER()
##    sys.stderr.write("PATH is %s\n" % sys.path)

################################################################
# tell the win32 COM registering/unregistering code that we're inside
# of an EXE/DLL

if not hasattr(sys, "frozen"):
    # standard exes have none.
    sys.frozen = _ctypes.frozen = 1
else:
    # com DLLs already have sys.frozen set to 'dll'
    _ctypes.frozen = sys.frozen

# Add some extra imports here, just to avoid putting them as "hidden imports"
# anywhere else - this script has the best idea about what it needs.
# (and hidden imports are currently disabled :)
#...

# We assume that py2exe has magically set com_module_names
# to the module names that expose the COM objects we host.
# Note that here all the COM modules for the app are imported - hence any
# environment changes (such as sys.stderr redirection) will happen now.
try:
    com_module_names
except NameError:
    print "This script is designed to be run from inside py2exe % s" % str(details)
    sys.exit(1)
    
com_modules = []
for name in com_module_names:
    __import__(name)
    com_modules.append(sys.modules[name])

def get_classes(module):
    return [ob
            for ob in module.__dict__.values()
            if hasattr(ob, "_reg_progid_")
            ]

def build_class_map():
    # Set _clsid_to_class in comtypes.server.inprocserver.
    #
    # This avoids the need to have registry entries pointing to the
    # COM server class.
    classmap = {}
    for mod in com_modules:
        # dump each class
        for cls in get_classes(mod):
            classmap[cls._reg_clsid_] = cls
    import comtypes.server.inprocserver
    comtypes.server.inprocserver._clsid_to_class = classmap
build_class_map()
del build_class_map

def DllRegisterServer():
    # Enumerate each module implementing an object
    from comtypes.server.register import register
    for mod in com_modules:
        # register each class
        for cls in get_classes(mod):
            register(cls)


def DllUnregisterServer():
    # Enumerate each module implementing an object
    from comtypes.server.register import unregister
    for mod in com_modules:
        # unregister each class
        for cls in get_classes(mod):
            unregister(cls)
