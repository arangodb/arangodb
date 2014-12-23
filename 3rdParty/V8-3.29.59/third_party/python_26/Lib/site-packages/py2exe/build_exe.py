# Changes:
#
#    can now specify 'zipfile = None', in this case the Python module
#    library archive is appended to the exe.

# Todo:
#
# Make 'unbuffered' a per-target option

from distutils.core import Command
from distutils.spawn import spawn
from distutils.errors import *
import sys, os, imp, types, stat
import marshal
import zipfile
import sets
import tempfile
import struct
import re

is_win64 = struct.calcsize("P") == 8

def _is_debug_build():
    for ext, _, _ in imp.get_suffixes():
        if ext == "_d.pyd":
            return True
    return False

is_debug_build = _is_debug_build()

if is_debug_build:
    python_dll = "python%d%d_d.dll" % sys.version_info[:2]
else:
    python_dll = "python%d%d.dll" % sys.version_info[:2]

# resource constants
RT_BITMAP=2
RT_MANIFEST=24

# Pattern for modifying the 'requestedExecutionLevel' in the manifest.  Groups
# are setup so all text *except* for the values is matched.
pat_manifest_uac = re.compile(r'(^.*<requestedExecutionLevel level=")([^"])*(" uiAccess=")([^"])*(".*$)')

# note: we cannot use the list from imp.get_suffixes() because we want
# .pyc and .pyo, independent of the optimize flag.
_py_suffixes = ['.py', '.pyo', '.pyc', '.pyw']
_c_suffixes = [_triple[0] for _triple in imp.get_suffixes()
               if _triple[2] == imp.C_EXTENSION]

def imp_find_module(name):
    # same as imp.find_module, but handles dotted names
    names = name.split('.')
    path = None
    for name in names:
        result = imp.find_module(name, path)
        path = [result[1]]
    return result

def fancy_split(str, sep=","):
    # a split which also strips whitespace from the items
    # passing a list or tuple will return it unchanged
    if str is None:
        return []
    if hasattr(str, "split"):
        return [item.strip() for item in str.split(sep)]
    return str

def ensure_unicode(text):
    if isinstance(text, unicode):
        return text
    return text.decode("mbcs")

# This loader locates extension modules relative to the library.zip
# file when an archive is used (i.e., skip_archive is not used), otherwise
# it locates extension modules relative to sys.prefix.
LOADER = """
def __load():
    import imp, os, sys
    try:
        dirname = os.path.dirname(__loader__.archive)
    except NameError:
        dirname = sys.prefix
    path = os.path.join(dirname, '%s')
    #print "py2exe extension module", __name__, "->", path
    mod = imp.load_dynamic(__name__, path)
##    mod.frozen = 1
__load()
del __load
"""

# A very loosely defined "target".  We assume either a "script" or "modules"
# attribute.  Some attributes will be target specific.
class Target:
    # A custom requestedExecutionLevel for the User Access Control portion
    # of the manifest for the target. May be a string, which will be used for
    # the 'requestedExecutionLevel' portion and False for 'uiAccess', or a tuple
    # of (string, bool) which specifies both values. If specified and the
    # target's 'template' executable has no manifest (ie, python 2.5 and
    # earlier), then a default manifest is created, otherwise the manifest from
    # the template is copied then updated.
    uac_info = None

    def __init__(self, **kw):
        self.__dict__.update(kw)
        # If modules is a simple string, assume they meant list
        m = self.__dict__.get("modules")
        if m and type(m) in types.StringTypes:
            self.modules = [m]
    def get_dest_base(self):
        dest_base = getattr(self, "dest_base", None)
        if dest_base: return dest_base
        script = getattr(self, "script", None)
        if script:
            return os.path.basename(os.path.splitext(script)[0])
        modules = getattr(self, "modules", None)
        assert modules, "no script, modules or dest_base specified"
        return modules[0].split(".")[-1]

    def validate(self):
        resources = getattr(self, "bitmap_resources", []) + \
                    getattr(self, "icon_resources", [])
        for r_id, r_filename in resources:
            if type(r_id) != type(0):
                raise DistutilsOptionError, "Resource ID must be an integer"
            if not os.path.isfile(r_filename):
                raise DistutilsOptionError, "Resource filename '%s' does not exist" % r_filename

def FixupTargets(targets, default_attribute):
    if not targets:
        return targets
    ret = []
    for target_def in targets:
        if type(target_def) in types.StringTypes :
            # Create a default target object, with the string as the attribute
            target = Target(**{default_attribute: target_def})
        else:
            d = getattr(target_def, "__dict__", target_def)
            if not d.has_key(default_attribute):
                raise DistutilsOptionError, \
                      "This target class requires an attribute '%s'" % default_attribute
            target = Target(**d)
        target.validate()
        ret.append(target)
    return ret

class py2exe(Command):
    description = ""
    # List of option tuples: long name, short name (None if no short
    # name), and help string.
    user_options = [
        ('optimize=', 'O',
         "optimization level: -O1 for \"python -O\", "
         "-O2 for \"python -OO\", and -O0 to disable [default: -O0]"),
        ('dist-dir=', 'd',
         "directory to put final built distributions in (default is dist)"),

        ("excludes=", 'e',
         "comma-separated list of modules to exclude"),
        ("dll-excludes=", None,
         "comma-separated list of DLLs to exclude"),
        ("ignores=", None,
         "comma-separated list of modules to ignore if they are not found"),
        ("includes=", 'i',
         "comma-separated list of modules to include"),
        ("packages=", 'p',
         "comma-separated list of packages to include"),

        ("compressed", 'c',
         "create a compressed zipfile"),

        ("xref", 'x',
         "create and show a module cross reference"),

        ("bundle-files=", 'b',
         "bundle dlls in the zipfile or the exe. Valid levels are 1, 2, or 3 (default)"),

        ("skip-archive", None,
         "do not place Python bytecode files in an archive, put them directly in the file system"),

        ("ascii", 'a',
         "do not automatically include encodings and codecs"),

        ('custom-boot-script=', None,
         "Python file that will be run when setting up the runtime environment"),
        ]

    boolean_options = ["compressed", "xref", "ascii", "skip-archive"]

    def initialize_options (self):
        self.xref =0
        self.compressed = 0
        self.unbuffered = 0
        self.optimize = 0
        self.includes = None
        self.excludes = None
        self.ignores = None
        self.packages = None
        self.dist_dir = None
        self.dll_excludes = None
        self.typelibs = None
        self.bundle_files = 3
        self.skip_archive = 0
        self.ascii = 0
        self.custom_boot_script = None

    def finalize_options (self):
        self.optimize = int(self.optimize)
        self.excludes = fancy_split(self.excludes)
        self.includes = fancy_split(self.includes)
        self.ignores = fancy_split(self.ignores)
        self.bundle_files = int(self.bundle_files)
        if self.bundle_files < 1 or self.bundle_files > 3:
            raise DistutilsOptionError, \
                  "bundle-files must be 1, 2, or 3, not %s" % self.bundle_files
        if is_win64 and self.bundle_files < 3:
            raise DistutilsOptionError, \
                  "bundle-files %d not yet supported on win64" % self.bundle_files
        if self.skip_archive:
            if self.compressed:
                raise DistutilsOptionError, \
                      "can't compress when skipping archive"
            if self.distribution.zipfile is None:
                raise DistutilsOptionError, \
                      "zipfile cannot be None when skipping archive"
        # includes is stronger than excludes
        for m in self.includes:
            if m in self.excludes:
                self.excludes.remove(m)
        self.packages = fancy_split(self.packages)
        self.set_undefined_options('bdist',
                                   ('dist_dir', 'dist_dir'))
        self.dll_excludes = [x.lower() for x in fancy_split(self.dll_excludes)]

    def run(self):
        build = self.reinitialize_command('build')
        build.run()
        sys_old_path = sys.path[:]
        if build.build_platlib is not None:
            sys.path.insert(0, build.build_platlib)
        if build.build_lib is not None:
            sys.path.insert(0, build.build_lib)
        try:
            self._run()
        finally:
            sys.path = sys_old_path

    def _run(self):
        self.create_directories()
        self.plat_prepare()
        self.fixup_distribution()

        dist = self.distribution

        # all of these contain module names
        required_modules = []
        for target in dist.com_server + dist.service + dist.ctypes_com_server:
            required_modules.extend(target.modules)
        # and these contains file names
        required_files = [target.script
                          for target in dist.windows + dist.console]

        mf = self.create_modulefinder()

        # These are the name of a script, but used as a module!
        for f in dist.isapi:
            mf.load_file(f.script)

        if self.typelibs:
            print "*** generate typelib stubs ***"
            from distutils.dir_util import mkpath
            genpy_temp = os.path.join(self.temp_dir, "win32com", "gen_py")
            mkpath(genpy_temp)
            num_stubs = collect_win32com_genpy(genpy_temp,
                                               self.typelibs,
                                               verbose=self.verbose,
                                               dry_run=self.dry_run)
            print "collected %d stubs from %d type libraries" \
                  % (num_stubs, len(self.typelibs))
            mf.load_package("win32com.gen_py", genpy_temp)
            self.packages.append("win32com.gen_py")

        # monkey patching the compile builtin.
        # The idea is to include the filename in the error message
        orig_compile = compile
        import __builtin__
        def my_compile(source, filename, *args):
            try:
                result = orig_compile(source, filename, *args)
            except Exception, details:
                raise DistutilsError, "compiling '%s' failed\n    %s: %s" % \
                      (filename, details.__class__.__name__, details)
            return result
        __builtin__.compile = my_compile

        print "*** searching for required modules ***"
        self.find_needed_modules(mf, required_files, required_modules)

        print "*** parsing results ***"
        py_files, extensions, builtins = self.parse_mf_results(mf)

        if self.xref:
            mf.create_xref()

        print "*** finding dlls needed ***"
        dlls = self.find_dlls(extensions)
        self.plat_finalize(mf.modules, py_files, extensions, dlls)
        dlls = [item for item in dlls
                if os.path.basename(item).lower() not in self.dll_excludes]
        # should we filter self.other_depends in the same way?

        print "*** create binaries ***"
        self.create_binaries(py_files, extensions, dlls)

        self.fix_badmodules(mf)

        if mf.any_missing():
            print "The following modules appear to be missing"
            print mf.any_missing()

        if self.other_depends:
            print
            print "*** binary dependencies ***"
            print "Your executable(s) also depend on these dlls which are not included,"
            print "you may or may not need to distribute them."
            print
            print "Make sure you have the license if you distribute any of them, and"
            print "make sure you don't distribute files belonging to the operating system."
            print
            for fnm in self.other_depends:
                print "  ", os.path.basename(fnm), "-", fnm.strip()

    def create_modulefinder(self):
        from modulefinder import ReplacePackage
        from py2exe.mf import ModuleFinder
        ReplacePackage("_xmlplus", "xml")
        return ModuleFinder(excludes=self.excludes)

    def fix_badmodules(self, mf):
        # This dictionary maps additional builtin module names to the
        # module that creates them.
        # For example, 'wxPython.misc' creates a builtin module named
        # 'miscc'.
        builtins = {"clip_dndc": "wxPython.clip_dnd",
                    "cmndlgsc": "wxPython.cmndlgs",
                    "controls2c": "wxPython.controls2",
                    "controlsc": "wxPython.controls",
                    "eventsc": "wxPython.events",
                    "filesysc": "wxPython.filesys",
                    "fontsc": "wxPython.fonts",
                    "framesc": "wxPython.frames",
                    "gdic": "wxPython.gdi",
                    "imagec": "wxPython.image",
                    "mdic": "wxPython.mdi",
                    "misc2c": "wxPython.misc2",
                    "miscc": "wxPython.misc",
                    "printfwc": "wxPython.printfw",
                    "sizersc": "wxPython.sizers",
                    "stattoolc": "wxPython.stattool",
                    "streamsc": "wxPython.streams",
                    "utilsc": "wxPython.utils",
                    "windows2c": "wxPython.windows2",
                    "windows3c": "wxPython.windows3",
                    "windowsc": "wxPython.windows",
                    }

        # Somewhat hackish: change modulefinder's badmodules dictionary in place.
        bad = mf.badmodules
        # mf.badmodules is a dictionary mapping unfound module names
        # to another dictionary, the keys of this are the module names
        # importing the unknown module.  For the 'miscc' module
        # mentioned above, it looks like this:
        # mf.badmodules["miscc"] = { "wxPython.miscc": 1 }
        for name in mf.any_missing():
            if name in self.ignores:
                del bad[name]
                continue
            mod = builtins.get(name, None)
            if mod is not None:
                if mod in bad[name] and bad[name] == {mod: 1}:
                    del bad[name]

    def find_dlls(self, extensions):
        dlls = [item.__file__ for item in extensions]
##        extra_path = ["."] # XXX
        extra_path = []
        dlls, unfriendly_dlls, other_depends = \
              self.find_dependend_dlls(dlls,
                                       extra_path + sys.path,
                                       self.dll_excludes)
        self.other_depends = other_depends
        # dlls contains the path names of all dlls we need.
        # If a dll uses a function PyImport_ImportModule (or what was it?),
        # it's name is additionally in unfriendly_dlls.
        for item in extensions:
            if item.__file__ in dlls:
                dlls.remove(item.__file__)
        return dlls

    def create_directories(self):
        bdist_base = self.get_finalized_command('bdist').bdist_base
        self.bdist_dir = os.path.join(bdist_base, 'winexe')

        collect_name = "collect-%d.%d" % sys.version_info[:2]
        self.collect_dir = os.path.abspath(os.path.join(self.bdist_dir, collect_name))
        self.mkpath(self.collect_dir)

        bundle_name = "bundle-%d.%d" % sys.version_info[:2]
        self.bundle_dir = os.path.abspath(os.path.join(self.bdist_dir, bundle_name))
        self.mkpath(self.bundle_dir)

        self.temp_dir = os.path.abspath(os.path.join(self.bdist_dir, "temp"))
        self.mkpath(self.temp_dir)

        self.dist_dir = os.path.abspath(self.dist_dir)
        self.mkpath(self.dist_dir)

        if self.distribution.zipfile is None:
            self.lib_dir = self.dist_dir
        else:
            self.lib_dir = os.path.join(self.dist_dir,
                                        os.path.dirname(self.distribution.zipfile))
        self.mkpath(self.lib_dir)

    def copy_extensions(self, extensions):
        print "*** copy extensions ***"
        # copy the extensions to the target directory
        for item in extensions:
            src = item.__file__
            if self.bundle_files > 2: # don't bundle pyds and dlls
                dst = os.path.join(self.lib_dir, (item.__pydfile__))
                self.copy_file(src, dst, preserve_mode=0)
                self.lib_files.append(dst)
            else:
                # we have to preserve the packages
                package = "\\".join(item.__name__.split(".")[:-1])
                if package:
                    dst = os.path.join(package, os.path.basename(src))
                else:
                    dst = os.path.basename(src)
                self.copy_file(src, os.path.join(self.collect_dir, dst), preserve_mode=0)
                self.compiled_files.append(dst)

    def copy_dlls(self, dlls):
        # copy needed dlls where they belong.
        print "*** copy dlls ***"
        if self.bundle_files < 3:
            self.copy_dlls_bundle_files(dlls)
            return
        # dlls belong into the lib_dir, except those listed in dlls_in_exedir,
        # which have to go into exe_dir (pythonxy.dll, w9xpopen.exe).
        for dll in dlls:
            base = os.path.basename(dll)
            if base.lower() in self.dlls_in_exedir:
                # These special dlls cannot be in the lib directory,
                # they must go into the exe directory.
                dst = os.path.join(self.exe_dir, base)
            else:
                dst = os.path.join(self.lib_dir, base)
            _, copied = self.copy_file(dll, dst, preserve_mode=0)
            if not self.dry_run and copied and base.lower() == python_dll.lower():
                # If we actually copied pythonxy.dll, we have to patch it.
                #
                # Previously, the code did it every time, but this
                # breaks if, for example, someone runs UPX over the
                # dist directory.  Patching an UPX'd dll seems to work
                # (no error is detected when patching), but the
                # resulting dll does not work anymore.
                #
                # The function restores the file times so
                # dependencies still work correctly.
                self.patch_python_dll_winver(dst)

            self.lib_files.append(dst)

    def copy_dlls_bundle_files(self, dlls):
        # If dlls have to be bundled, they are copied into the
        # collect_dir and will be added to the list of files to
        # include in the zip archive 'self.compiled_files'.
        #
        # dlls listed in dlls_in_exedir have to be treated differently:
        #
        for dll in dlls:
            base = os.path.basename(dll)
            if base.lower() in self.dlls_in_exedir:
                # pythonXY.dll must be bundled as resource.
                # w9xpopen.exe must be copied to self.exe_dir.
                if base.lower() == python_dll.lower() and self.bundle_files < 2:
                    dst = os.path.join(self.bundle_dir, base)
                else:
                    dst = os.path.join(self.exe_dir, base)
                _, copied = self.copy_file(dll, dst, preserve_mode=0)
                if not self.dry_run and copied and base.lower() == python_dll.lower():
                    # If we actually copied pythonxy.dll, we have to
                    # patch it.  Well, since it's impossible to load
                    # resources from the bundled dlls it probably
                    # doesn't matter.
                    self.patch_python_dll_winver(dst)
                self.lib_files.append(dst)
                continue

            dst = os.path.join(self.collect_dir, os.path.basename(dll))
            self.copy_file(dll, dst, preserve_mode=0)
            # Make sure they will be included into the zipfile.
            self.compiled_files.append(os.path.basename(dst))

    def create_binaries(self, py_files, extensions, dlls):
        dist = self.distribution

        # byte compile the python modules into the target directory
        print "*** byte compile python files ***"
        self.compiled_files = byte_compile(py_files,
                                           target_dir=self.collect_dir,
                                           optimize=self.optimize,
                                           force=0,
                                           verbose=self.verbose,
                                           dry_run=self.dry_run)

        self.lib_files = []
        self.console_exe_files = []
        self.windows_exe_files = []
        self.service_exe_files = []
        self.comserver_files = []

        self.copy_extensions(extensions)
        self.copy_dlls(dlls)

        # create the shared zipfile containing all Python modules
        if dist.zipfile is None:
            fd, archive_name = tempfile.mkstemp()
            os.close(fd)
        else:
            archive_name = os.path.join(self.lib_dir,
                                        os.path.basename(dist.zipfile))

        arcname = self.make_lib_archive(archive_name,
                                        base_dir=self.collect_dir,
                                        files=self.compiled_files,
                                        verbose=self.verbose,
                                        dry_run=self.dry_run)
        if dist.zipfile is not None:
            self.lib_files.append(arcname)

        for target in self.distribution.isapi:
            print "*** copy isapi support DLL ***"
            # Locate the support DLL, and copy as "_script.dll", just like
            # isapi itself
            import isapi
            src_name = is_debug_build and "PyISAPI_loader_d.dll" or \
                                          "PyISAPI_loader.dll"
            src = os.path.join(isapi.__path__[0], src_name)
            # destination name is "_{module_name}.dll" just like pyisapi does.
            script_base = os.path.splitext(os.path.basename(target.script))[0]
            dst = os.path.join(self.exe_dir, "_" + script_base + ".dll")
            self.copy_file(src, dst, preserve_mode=0)

        if self.distribution.has_data_files():
            print "*** copy data files ***"
            install_data = self.reinitialize_command('install_data')
            install_data.install_dir = self.dist_dir
            install_data.ensure_finalized()
            install_data.run()

            self.lib_files.extend(install_data.get_outputs())

        # build the executables
        for target in dist.console:
            dst = self.build_executable(target, self.get_console_template(),
                                        arcname, target.script)
            self.console_exe_files.append(dst)
        for target in dist.windows:
            dst = self.build_executable(target, self.get_windows_template(),
                                        arcname, target.script)
            self.windows_exe_files.append(dst)
        for target in dist.service:
            dst = self.build_service(target, self.get_service_template(),
                                     arcname)
            self.service_exe_files.append(dst)

        for target in dist.isapi:
            dst = self.build_isapi(target, self.get_isapi_template(), arcname)

        for target in dist.com_server:
            if getattr(target, "create_exe", True):
                dst = self.build_comserver(target, self.get_comexe_template(),
                                           arcname)
                self.comserver_files.append(dst)
            if getattr(target, "create_dll", True):
                dst = self.build_comserver(target, self.get_comdll_template(),
                                           arcname)
                self.comserver_files.append(dst)

        for target in dist.ctypes_com_server:
                dst = self.build_comserver(target, self.get_ctypes_comdll_template(),
                                           arcname, boot_script="ctypes_com_server")
                self.comserver_files.append(dst)

        if dist.zipfile is None:
            os.unlink(arcname)
        else:
            if self.bundle_files < 3 or self.compressed:
                arcbytes = open(arcname, "rb").read()
                arcfile = open(arcname, "wb")

                if self.bundle_files < 2: # bundle pythonxy.dll also
                    print "Adding %s to %s" % (python_dll, arcname)
                    arcfile.write("<pythondll>")
                    bytes = open(os.path.join(self.bundle_dir, python_dll), "rb").read()
                    arcfile.write(struct.pack("i", len(bytes)))
                    arcfile.write(bytes) # python dll

                if self.compressed:
                    # prepend zlib.pyd also
                    zlib_file = imp.find_module("zlib")[0]
                    if zlib_file:
                        print "Adding zlib%s.pyd to %s" % (is_debug_build and "_d" or "", arcname)
                        arcfile.write("<zlib.pyd>")
                        bytes = zlib_file.read()
                        arcfile.write(struct.pack("i", len(bytes)))
                        arcfile.write(bytes) # zlib.pyd

                arcfile.write(arcbytes)

####        if self.bundle_files < 2:
####            # remove python dll from the exe_dir, since it is now bundled.
####            os.remove(os.path.join(self.exe_dir, python_dll))


    # for user convenience, let subclasses override the templates to use
    def get_console_template(self):
        return is_debug_build and "run_d.exe" or "run.exe"

    def get_windows_template(self):
        return is_debug_build and "run_w_d.exe" or "run_w.exe"

    def get_service_template(self):
        return is_debug_build and "run_d.exe" or "run.exe"

    def get_isapi_template(self):
        return is_debug_build and "run_isapi_d.dll" or "run_isapi.dll"

    def get_comexe_template(self):
        return is_debug_build and "run_w_d.exe" or "run_w.exe"

    def get_comdll_template(self):
        return is_debug_build and "run_dll_d.dll" or "run_dll.dll"

    def get_ctypes_comdll_template(self):
        return is_debug_build and "run_ctypes_dll_d.dll" or "run_ctypes_dll.dll"

    def fixup_distribution(self):
        dist = self.distribution

        # Convert our args into target objects.
        dist.com_server = FixupTargets(dist.com_server, "modules")
        dist.ctypes_com_server = FixupTargets(dist.ctypes_com_server, "modules")
        dist.service = FixupTargets(dist.service, "modules")
        dist.windows = FixupTargets(dist.windows, "script")
        dist.console = FixupTargets(dist.console, "script")
        dist.isapi = FixupTargets(dist.isapi, "script")

        # make sure all targets use the same directory, this is
        # also the directory where the pythonXX.dll must reside
        paths = sets.Set()
        for target in dist.com_server + dist.service \
                + dist.windows + dist.console + dist.isapi:
            paths.add(os.path.dirname(target.get_dest_base()))

        if len(paths) > 1:
            raise DistutilsOptionError, \
                  "all targets must use the same directory: %s" % \
                  [p for p in paths]
        if paths:
            exe_dir = paths.pop() # the only element
            if os.path.isabs(exe_dir):
                raise DistutilsOptionError, \
                      "exe directory must be relative: %s" % exe_dir
            self.exe_dir = os.path.join(self.dist_dir, exe_dir)
            self.mkpath(self.exe_dir)
        else:
            # Do we allow to specify no targets?
            # We can at least build a zipfile...
            self.exe_dir = self.lib_dir

    def get_boot_script(self, boot_type):
        # return the filename of the script to use for com servers.
        thisfile = sys.modules['py2exe.build_exe'].__file__
        return os.path.join(os.path.dirname(thisfile),
                            "boot_" + boot_type + ".py")

    def build_comserver(self, target, template, arcname, boot_script="com_servers"):
        # Build a dll and an exe executable hosting all the com
        # objects listed in module_names.
        # The basename of the dll/exe is the last part of the first module.
        # Do we need a way to specify the name of the files to be built?

        # Setup the variables our boot script needs.
        vars = {"com_module_names" : target.modules}
        boot = self.get_boot_script(boot_script)
        # and build it
        return self.build_executable(target, template, arcname, boot, vars)

    def get_service_names(self, module_name):
        # import the script with every side effect :)
        __import__(module_name)
        mod = sys.modules[module_name]
        for name, klass in mod.__dict__.items():
            if hasattr(klass, "_svc_name_"):
                break
        else:
            raise RuntimeError, "No services in module"
        deps = ()
        if hasattr(klass, "_svc_deps_"):
            deps = klass._svc_deps_
        return klass.__name__, klass._svc_name_, klass._svc_display_name_, deps

    def build_service(self, target, template, arcname):
        # It should be possible to host many modules in a single service -
        # but this is yet to be tested.
        assert len(target.modules)==1, "We only support one service module"

        cmdline_style = getattr(target, "cmdline_style", "py2exe")
        if cmdline_style not in ["py2exe", "pywin32", "custom"]:
            raise RuntimeError, "cmdline_handler invalid"

        vars = {"service_module_names" : target.modules,
                "cmdline_style": cmdline_style,
            }
        boot = self.get_boot_script("service")
        return self.build_executable(target, template, arcname, boot, vars)

    def build_isapi(self, target, template, arcname):
        target_module = os.path.splitext(os.path.basename(target.script))[0]
        vars = {"isapi_module_name" : target_module,
               }
        return self.build_executable(target, template, arcname, None, vars)

    def build_manifest(self, target, template):
        # Optionally return a manifest to be included in the target executable.
        # Note for Python 2.6 and later, its *necessary* to include a manifest
        # which correctly references the CRT.  For earlier versions, a manifest
        # is optional, and only necessary to customize things like
        # Vista's User Access Control 'requestedExecutionLevel' setting, etc.
        default_manifest = """
            <assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
                <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">
                  <security>
                    <requestedPrivileges>
                      <requestedExecutionLevel level="asInvoker" uiAccess="false"></requestedExecutionLevel>
                    </requestedPrivileges>
                  </security>
                </trustInfo>
            </assembly>
        """
        from py2exe_util import load_resource
        if os.path.splitext(template)[1]==".exe":
            rid = 1
        else:
            rid = 2
        try:
            # Manfiests have resource type of 24, and ID of either 1 or 2.
            mfest = load_resource(ensure_unicode(template), RT_MANIFEST, rid)
            # we consider the manifest 'changed' as we know we clobber all
            # resources including the existing manifest - so this manifest must
            # get written even if we make no other changes.
            changed = True
        except RuntimeError:
            mfest = default_manifest
            # in this case the template had no existing manifest, so its
            # not considered 'changed' unless we make further changes later.
            changed = False
        # update the manifest according to our options.
        # for now a regex will do.
        if target.uac_info:
            changed = True
            if isinstance(target.uac_info, tuple):
                exec_level, ui = target.uac_info
            else:
                exec_level = target.uac_info
                ui = False
            new_lines = []
            for line in mfest.splitlines():
                repl = r'\1%s\3%s\5' % (exec_level, ui)
                new_lines.append(re.sub(pat_manifest_uac, repl, line))
            mfest = "".join(new_lines)
        if not changed:
            return None, None
        return mfest, rid

    def build_executable(self, target, template, arcname, script, vars={}):
        # Build an executable for the target
        # template is the exe-stub to use, and arcname is the zipfile
        # containing the python modules.
        from py2exe_util import add_resource, add_icon
        ext = os.path.splitext(template)[1]
        exe_base = target.get_dest_base()
        exe_path = os.path.join(self.dist_dir, exe_base + ext)
        # The user may specify a sub-directory for the exe - that's fine, we
        # just specify the parent directory for the .zip
        parent_levels = len(os.path.normpath(exe_base).split(os.sep))-1
        lib_leaf = self.lib_dir[len(self.dist_dir)+1:]
        relative_arcname = ((".." + os.sep) * parent_levels)
        if lib_leaf: relative_arcname += lib_leaf + os.sep
        relative_arcname += os.path.basename(arcname)

        src = os.path.join(os.path.dirname(__file__), template)
        # We want to force the creation of this file, as otherwise distutils
        # will see the earlier time of our 'template' file versus the later
        # time of our modified template file, and consider our old file OK.
        old_force = self.force
        self.force = True
        self.copy_file(src, exe_path, preserve_mode=0)
        self.force = old_force

        # Make sure the file is writeable...
        os.chmod(exe_path, stat.S_IREAD | stat.S_IWRITE)
        try:
            f = open(exe_path, "a+b")
            f.close()
        except IOError, why:
            print "WARNING: File %s could not be opened - %s" % (exe_path, why)

        # We create a list of code objects, and write it as a marshaled
        # stream.  The framework code then just exec's these in order.
        # First is our common boot script.
        boot = self.get_boot_script("common")
        boot_code = compile(file(boot, "U").read(),
                            os.path.abspath(boot), "exec")
        code_objects = [boot_code]
        if self.bundle_files < 3:
            code_objects.append(
                compile("import zipextimporter; zipextimporter.install()",
                        "<install zipextimporter>", "exec"))
        for var_name, var_val in vars.items():
            code_objects.append(
                    compile("%s=%r\n" % (var_name, var_val), var_name, "exec")
            )
        if self.custom_boot_script:
            code_object = compile(file(self.custom_boot_script, "U").read() + "\n",
                                  os.path.abspath(self.custom_boot_script), "exec")
            code_objects.append(code_object)
        if script:
            code_object = compile(open(script, "U").read() + "\n",
                                  os.path.basename(script), "exec")
            code_objects.append(code_object)
        code_bytes = marshal.dumps(code_objects)

        if self.distribution.zipfile is None:
            relative_arcname = ""

        si = struct.pack("iiii",
                         0x78563412, # a magic value,
                         self.optimize,
                         self.unbuffered,
                         len(code_bytes),
                         ) + relative_arcname + "\000"

        script_bytes = si + code_bytes + '\000\000'
        self.announce("add script resource, %d bytes" % len(script_bytes))
        if not self.dry_run:
            add_resource(ensure_unicode(exe_path), script_bytes, u"PYTHONSCRIPT", 1, True)

            # add the pythondll as resource, and delete in self.exe_dir
            if self.bundle_files < 2 and self.distribution.zipfile is None:
                # bundle pythonxy.dll
                dll_path = os.path.join(self.bundle_dir, python_dll)
                bytes = open(dll_path, "rb").read()
                # image, bytes, lpName, lpType

                print "Adding %s as resource to %s" % (python_dll, exe_path)
                add_resource(ensure_unicode(exe_path), bytes,
                             # for some reason, the 3. argument MUST BE UPPER CASE,
                             # otherwise the resource will not be found.
                             ensure_unicode(python_dll).upper(), 1, False)

            if self.compressed and self.bundle_files < 3 and self.distribution.zipfile is None:
                zlib_file = imp.find_module("zlib")[0]
                if zlib_file:
                    print "Adding zlib.pyd as resource to %s" % exe_path
                    zlib_bytes = zlib_file.read()
                    add_resource(ensure_unicode(exe_path), zlib_bytes,
                                 # for some reason, the 3. argument MUST BE UPPER CASE,
                                 # otherwise the resource will not be found.
                                 u"ZLIB.PYD", 1, False)

        # Handle all resources specified by the target
        bitmap_resources = getattr(target, "bitmap_resources", [])
        for bmp_id, bmp_filename in bitmap_resources:
            bmp_data = open(bmp_filename, "rb").read()
            # skip the 14 byte bitmap header.
            if not self.dry_run:
                add_resource(ensure_unicode(exe_path), bmp_data[14:], RT_BITMAP, bmp_id, False)
        icon_resources = getattr(target, "icon_resources", [])
        for ico_id, ico_filename in icon_resources:
            if not self.dry_run:
                add_icon(ensure_unicode(exe_path), ensure_unicode(ico_filename), ico_id)

        # a manifest
        mfest, mfest_id = self.build_manifest(target, src)
        if mfest:
            self.announce("add manifest, %d bytes" % len(mfest))
            if not self.dry_run:
                add_resource(ensure_unicode(exe_path), mfest, RT_MANIFEST, mfest_id, False)

        for res_type, res_id, data in getattr(target, "other_resources", []):
            if not self.dry_run:
                if isinstance(res_type, basestring):
                    res_type = ensure_unicode(res_type)
                add_resource(ensure_unicode(exe_path), data, res_type, res_id, False)

        typelib = getattr(target, "typelib", None)
        if typelib is not None:
            data = open(typelib, "rb").read()
            add_resource(ensure_unicode(exe_path), data, u"TYPELIB", 1, False)

        self.add_versioninfo(target, exe_path)

        # Hm, this doesn't make sense with normal executables, which are
        # already small (around 20 kB).
        #
        # But it would make sense with static build pythons, but not
        # if the zipfile is appended to the exe - it will be too slow
        # then (although it is a wonder it works at all in this case).
        #
        # Maybe it would be faster to use the frozen modules machanism
        # instead of the zip-import?
##        if self.compressed:
##            import gc
##            gc.collect() # to close all open files!
##            os.system("upx -9 %s" % exe_path)

        if self.distribution.zipfile is None:
            zip_data = open(arcname, "rb").read()
            open(exe_path, "a+b").write(zip_data)

        return exe_path

    def add_versioninfo(self, target, exe_path):
        # Try to build and add a versioninfo resource

        def get(name, md = self.distribution.metadata):
            # Try to get an attribute from the target, if not defined
            # there, from the distribution's metadata, or None.  Note
            # that only *some* attributes are allowed by distutils on
            # the distribution's metadata: version, description, and
            # name.
            return getattr(target, name, getattr(md, name, None))

        version = get("version")
        if version is None:
            return

        from py2exe.resources.VersionInfo import Version, RT_VERSION, VersionError
        version = Version(version,
                          file_description = get("description"),
                          comments = get("comments"),
                          company_name = get("company_name"),
                          legal_copyright = get("copyright"),
                          legal_trademarks = get("trademarks"),
                          original_filename = os.path.basename(exe_path),
                          product_name = get("name"),
                          product_version = get("product_version") or version)
        try:
            bytes = version.resource_bytes()
        except VersionError, detail:
            self.warn("Version Info will not be included:\n  %s" % detail)
            return

        from py2exe_util import add_resource
        add_resource(ensure_unicode(exe_path), bytes, RT_VERSION, 1, False)

    def patch_python_dll_winver(self, dll_name, new_winver = None):
        from py2exe.resources.StringTables import StringTable, RT_STRING
        from py2exe_util import add_resource, load_resource
        from py2exe.resources.VersionInfo import RT_VERSION

        new_winver = new_winver or self.distribution.metadata.name or "py2exe"
        if self.verbose:
            print "setting sys.winver for '%s' to '%s'" % (dll_name, new_winver)
        if self.dry_run:
            return

        # We preserve the times on the file, so the dependency tracker works.
        st = os.stat(dll_name)
        # and as the resource functions silently fail if the open fails,
        # check it explicitly.
        os.chmod(dll_name, stat.S_IREAD | stat.S_IWRITE)
        try:
            f = open(dll_name, "a+b")
            f.close()
        except IOError, why:
            print "WARNING: File %s could not be opened - %s" % (dll_name, why)
        # We aren't smart enough to update string resources in place, so we need
        # to persist other resources we care about.
        unicode_name = ensure_unicode(dll_name)

        # Preserve existing version info (all versions should have this)
        ver_info = load_resource(unicode_name, RT_VERSION, 1)
        # Preserve an existing manifest (only python26.dll+ will have this)
        try:
            # Manfiests have resource type of 24, and ID of either 1 or 2.
            mfest = load_resource(unicode_name, RT_MANIFEST, 2)
        except RuntimeError:
            mfest = None

        # Start putting the resources back, passing 'delete=True' for the first.
        add_resource(unicode_name, ver_info, RT_VERSION, 1, True)
        if mfest is not None:
            add_resource(unicode_name, mfest, RT_MANIFEST, 2, False)

        # OK - do the strings.
        s = StringTable()
        # 1000 is the resource ID Python loads for its winver.
        s.add_string(1000, new_winver)
        for id, data in s.binary():
            add_resource(ensure_unicode(dll_name), data, RT_STRING, id, False)

        # restore the time.
        os.utime(dll_name, (st[stat.ST_ATIME], st[stat.ST_MTIME]))

    def find_dependend_dlls(self, dlls, pypath, dll_excludes):
        import py2exe_util
        sysdir = py2exe_util.get_sysdir()
        windir = py2exe_util.get_windir()
        # This is the tail of the path windows uses when looking for dlls
        # XXX On Windows NT, the SYSTEM directory is also searched
        exedir = os.path.dirname(sys.executable)
        syspath = os.environ['PATH']
        loadpath = ';'.join([exedir, sysdir, windir, syspath])

        # Found by Duncan Booth:
        # It may be possible that bin_depends needs extension modules,
        # so the loadpath must be extended by our python path.
        loadpath = loadpath + ';' + ';'.join(pypath)

        templates = sets.Set()
        if self.distribution.console:
            templates.add(self.get_console_template())
        if self.distribution.windows:
            templates.add(self.get_windows_template())
        if self.distribution.service:
            templates.add(self.get_service_template())
        for target in self.distribution.com_server:
            if getattr(target, "create_exe", True):
                templates.add(self.get_comexe_template())
            if getattr(target, "create_dll", True):
                templates.add(self.get_comdll_template())

        templates = [os.path.join(os.path.dirname(__file__), t) for t in templates]

        # We use Python.exe to track the dependencies of our run stubs ...
        images = dlls + templates

        self.announce("Resolving binary dependencies:")
        excludes_use = dll_excludes[:]
        # The MSVCRT modules are never found when using VS2008+
        if sys.version_info > (2,6):
            excludes_use.append("msvcr90.dll")

        # we add python.exe (aka sys.executable) to the list of images
        # to scan for dependencies, but remove it later again from the
        # results list.  In this way pythonXY.dll is collected, and
        # also the libraries it depends on.
        alldlls, warnings, other_depends = \
                 bin_depends(loadpath, images + [sys.executable], excludes_use)
        alldlls.remove(sys.executable)
        for dll in alldlls:
            self.announce("  %s" % dll)
        # ... but we don't need the exe stubs run_xxx.exe
        for t in templates:
            alldlls.remove(t)

        return alldlls, warnings, other_depends
    # find_dependend_dlls()

    def get_hidden_imports(self):
        # imports done from builtin modules in C code (untrackable by py2exe)
        return {"time": ["_strptime"],
##                "datetime": ["time"],
                "cPickle": ["copy_reg"],
                "parser": ["copy_reg"],
                "codecs": ["encodings"],

                "cStringIO": ["copy_reg"],
                "_sre": ["copy", "string", "sre"],
                }

    def parse_mf_results(self, mf):
        for name, imports in self.get_hidden_imports().items():
            if name in mf.modules.keys():
                for mod in imports:
                    mf.import_hook(mod)

        tcl_src_dir = tcl_dst_dir = None
        if "Tkinter" in mf.modules.keys():
            import Tkinter
            import _tkinter
            tk = _tkinter.create()
            tcl_dir = tk.call("info", "library")
            tcl_src_dir = os.path.split(tcl_dir)[0]
            tcl_dst_dir = os.path.join(self.lib_dir, "tcl")

            self.announce("Copying TCL files from %s..." % tcl_src_dir)
            self.copy_tree(os.path.join(tcl_src_dir, "tcl%s" % _tkinter.TCL_VERSION),
                           os.path.join(tcl_dst_dir, "tcl%s" % _tkinter.TCL_VERSION))
            self.copy_tree(os.path.join(tcl_src_dir, "tk%s" % _tkinter.TK_VERSION),
                           os.path.join(tcl_dst_dir, "tk%s" % _tkinter.TK_VERSION))
            del tk, _tkinter, Tkinter

        # Retrieve modules from modulefinder
        py_files = []
        extensions = []
        builtins = []

        for item in mf.modules.values():
            # There may be __main__ modules (from mf.run_script), but
            # we don't need them in the zipfile we build.
            if item.__name__ == "__main__":
                continue
            if self.bundle_files < 3 and item.__name__ in ("pythoncom", "pywintypes"):
                # these are handled specially in zipextimporter.
                continue
            src = item.__file__
            if src:
                base, ext = os.path.splitext(src)
                suffix = ext
                if sys.platform.startswith("win") and ext in [".dll", ".pyd"] \
                   and base.endswith("_d"):
                    suffix = "_d" + ext

                if suffix in _py_suffixes:
                    py_files.append(item)
                elif suffix in _c_suffixes:
                    extensions.append(item)
                    if not self.bundle_files < 3:
                        loader = self.create_loader(item)
                        if loader:
                            py_files.append(loader)
                else:
                    raise RuntimeError \
                          ("Don't know how to handle '%s'" % repr(src))
            else:
                builtins.append(item.__name__)

        # sort on the file names, the output is nicer to read
        py_files.sort(lambda a, b: cmp(a.__file__, b.__file__))
        extensions.sort(lambda a, b: cmp(a.__file__, b.__file__))
        builtins.sort()
        return py_files, extensions, builtins

    def plat_finalize(self, modules, py_files, extensions, dlls):
        # platform specific code for final adjustments to the file
        # lists
        if sys.platform == "win32":
            # pythoncom and pywintypes are imported via LoadLibrary calls,
            # help py2exe to include the dlls:
            if "pythoncom" in modules.keys():
                import pythoncom
                dlls.add(pythoncom.__file__)
            if "pywintypes" in modules.keys():
                import pywintypes
                dlls.add(pywintypes.__file__)
            self.copy_w9xpopen(modules, dlls)
        else:
            raise DistutilsError, "Platform %s not yet implemented" % sys.platform

    def copy_w9xpopen(self, modules, dlls):
        # Using popen requires (on Win9X) the w9xpopen.exe helper executable.
        if "os" in modules.keys() or "popen2" in modules.keys():
            if is_debug_build:
                fname = os.path.join(os.path.dirname(sys.executable), "w9xpopen_d.exe")
            else:
                fname = os.path.join(os.path.dirname(sys.executable), "w9xpopen.exe")
            # Don't copy w9xpopen.exe if it doesn't exist (64-bit
            # Python build, for example)
            if os.path.exists(fname):
                dlls.add(fname)

    def create_loader(self, item):
        # Hm, how to avoid needless recreation of this file?
        pathname = os.path.join(self.temp_dir, "%s.py" % item.__name__)
        if self.bundle_files > 2: # don't bundle pyds and dlls
            # all dlls are copied into the same directory, so modify
            # names to include the package name to avoid name
            # conflicts and tuck it away for future reference
            fname = item.__name__ + os.path.splitext(item.__file__)[1]
            item.__pydfile__ = fname
        else:
            fname = os.path.basename(item.__file__)
            
        # and what about dry_run?
        if self.verbose:
            print "creating python loader for extension '%s' (%s -> %s)" % (item.__name__,item.__file__,fname)

        source = LOADER % fname
        if not self.dry_run:
            open(pathname, "w").write(source)
        else:
            return None
        from modulefinder import Module
        return Module(item.__name__, pathname)

    def plat_prepare(self):
        self.includes.append("warnings") # needed by Python itself
        if not self.ascii:
            self.packages.append("encodings")
            self.includes.append("codecs")
        if self.bundle_files < 3:
            self.includes.append("zipextimporter")
            self.excludes.append("_memimporter") # builtin in run_*.exe and run_*.dll
        if self.compressed:
            self.includes.append("zlib")

        # os.path will never be found ;-)
        self.ignores.append('os.path')

        # update the self.ignores list to ignore platform specific
        # modules.
        if sys.platform == "win32":
            self.ignores += ['AL',
                             'Audio_mac',
                             'Carbon.File',
                             'Carbon.Folder',
                             'Carbon.Folders',
                             'EasyDialogs',
                             'MacOS',
                             'Mailman',
                             'SOCKS',
                             'SUNAUDIODEV',
                             '_dummy_threading',
                             '_emx_link',
                             '_xmlplus',
                             '_xmlrpclib',
                             'al',
                             'bundlebuilder',
                             'ce',
                             'cl',
                             'dbm',
                             'dos',
                             'fcntl',
                             'gestalt',
                             'grp',
                             'ic',
                             'java.lang',
                             'mac',
                             'macfs',
                             'macostools',
                             'mkcwproject',
                             'org.python.core',
                             'os.path',
                             'os2',
                             'poll',
                             'posix',
                             'pwd',
                             'readline',
                             'riscos',
                             'riscosenviron',
                             'riscospath',
                             'rourl2path',
                             'sgi',
                             'sgmlop',
                             'sunaudiodev',
                             'termios',
                             'vms_lib']
            # special dlls which must be copied to the exe_dir, not the lib_dir
            self.dlls_in_exedir = [python_dll,
                                   "w9xpopen%s.exe" % (is_debug_build and "_d" or ""),
                                   "msvcr71%s.dll" % (is_debug_build and "d" or "")]
        else:
            raise DistutilsError, "Platform %s not yet implemented" % sys.platform

    def find_needed_modules(self, mf, files, modules):
        # feed Modulefinder with everything, and return it.
        for mod in modules:
            mf.import_hook(mod)

        for path in files:
            mf.run_script(path)

        mf.run_script(self.get_boot_script("common"))

        if self.distribution.com_server:
            mf.run_script(self.get_boot_script("com_servers"))

        if self.distribution.ctypes_com_server:
            mf.run_script(self.get_boot_script("ctypes_com_server"))

        if self.distribution.service:
            mf.run_script(self.get_boot_script("service"))

        if self.custom_boot_script:
            mf.run_script(self.custom_boot_script)

        for mod in self.includes:
            if mod[-2:] == '.*':
                mf.import_hook(mod[:-2], None, ['*'])
            else:
                mf.import_hook(mod)

        for f in self.packages:
            def visit(arg, dirname, names):
                if '__init__.py' in names:
                    arg.append(dirname)

            # Try to find the package using ModuleFinders's method to
            # allow for modulefinder.AddPackagePath interactions
            mf.import_hook(f)

            # If modulefinder has seen a reference to the package, then
            # we prefer to believe that (imp_find_module doesn't seem to locate
            # sub-packages)
            if f in mf.modules:
                module = mf.modules[f]
                if module.__path__ is None:
                    # it's a module, not a package, so paths contains just the
                    # file entry
                    paths = [module.__file__]
                else:
                    # it is a package because __path__ is available.  __path__
                    # is actually a list of paths that are searched to import
                    # sub-modules and sub-packages
                    paths = module.__path__
            else:
                # Find path of package
                try:
                    paths = [imp_find_module(f)[1]]
                except ImportError:
                    self.warn("No package named %s" % f)
                    continue

            packages = []
            for path in paths:
                # walk the path to find subdirs containing __init__.py files
                os.path.walk(path, visit, packages)

                # scan the results (directory of __init__.py files)
                # first trim the path (of the head package),
                # then convert directory name in package name,
                # finally push into modulefinder.
                for p in packages:
                    if p.startswith(path):
                        package = f + '.' + p[len(path)+1:].replace('\\', '.')
                        mf.import_hook(package, None, ["*"])

        return mf

    def make_lib_archive(self, zip_filename, base_dir, files,
                         verbose=0, dry_run=0):
        from distutils.dir_util import mkpath
        if not self.skip_archive:
            # Like distutils "make_archive", but we can specify the files
            # to include, and the compression to use - default is
            # ZIP_STORED to keep the runtime performance up.  Also, we
            # don't append '.zip' to the filename.
            mkpath(os.path.dirname(zip_filename), dry_run=dry_run)

            if self.compressed:
                compression = zipfile.ZIP_DEFLATED
            else:
                compression = zipfile.ZIP_STORED

            if not dry_run:
                z = zipfile.ZipFile(zip_filename, "w",
                                    compression=compression)
                for f in files:
                    z.write(os.path.join(base_dir, f), f)
                z.close()

            return zip_filename
        else:
            # Don't really produce an archive, just copy the files.
            from distutils.file_util import copy_file

            destFolder = os.path.dirname(zip_filename)

            for f in files:
                d = os.path.dirname(f)
                if d:
                    mkpath(os.path.join(destFolder, d), verbose=verbose, dry_run=dry_run)
                copy_file(
                          os.path.join(base_dir, f),
                          os.path.join(destFolder, f),
                          preserve_mode=0,
                          verbose=verbose,
                          dry_run=dry_run
                         )
            return '.'


################################################################

class FileSet:
    # A case insensitive but case preserving set of files
    def __init__(self, iterable=None):
        self._dict = {}
        if iterable is not None:
            for arg in iterable:
                self.add(arg)

    def __repr__(self):
        return "<FileSet %s at %x>" % (self._dict.values(), id(self))

    def add(self, fname):
        self._dict[fname.upper()] = fname

    def remove(self, fname):
        del self._dict[fname.upper()]

    def __contains__(self, fname):
        return fname.upper() in self._dict.keys()

    def __getitem__(self, index):
        key = self._dict.keys()[index]
        return self._dict[key]

    def __len__(self):
        return len(self._dict)

    def copy(self):
        res = FileSet()
        res._dict.update(self._dict)
        return res

# class FileSet()

def bin_depends(path, images, excluded_dlls):
    import py2exe_util
    warnings = FileSet()
    images = FileSet(images)
    dependents = FileSet()
    others = FileSet()
    while images:
        for image in images.copy():
            images.remove(image)
            if not image in dependents:
                dependents.add(image)
                abs_image = os.path.abspath(image)
                loadpath = os.path.dirname(abs_image) + ';' + path
                for result in py2exe_util.depends(image, loadpath).items():
                    dll, uses_import_module = result
                    if os.path.basename(dll).lower() not in excluded_dlls:
                        if isSystemDLL(dll):
                            others.add(dll)
                            continue
                        if dll not in images and dll not in dependents:
                            images.add(dll)
                            if uses_import_module:
                                warnings.add(dll)
    return dependents, warnings, others

# DLLs to be excluded
# XXX This list is NOT complete (it cannot be)
# Note: ALL ENTRIES MUST BE IN LOWER CASE!
EXCLUDED_DLLS = (
    "advapi32.dll",
    "comctl32.dll",
    "comdlg32.dll",
    "crtdll.dll",
    "gdi32.dll",
    "glu32.dll",
    "opengl32.dll",
    "imm32.dll",
    "kernel32.dll",
    "mfc42.dll",
    "msvcirt.dll",
    "msvcrt.dll",
    "msvcrtd.dll",
    "ntdll.dll",
    "odbc32.dll",
    "ole32.dll",
    "oleaut32.dll",
    "rpcrt4.dll",
    "shell32.dll",
    "shlwapi.dll",
    "user32.dll",
    "version.dll",
    "winmm.dll",
    "winspool.drv",
    "ws2_32.dll",
    "ws2help.dll",
    "wsock32.dll",
    "netapi32.dll",

    "gdiplus.dll",
    )

# XXX Perhaps it would be better to assume dlls from the systemdir are system dlls,
# and make some exceptions for known dlls, like msvcr71, pythonXY.dll, and so on?
def isSystemDLL(pathname):
    if os.path.basename(pathname).lower() in ("msvcr71.dll", "msvcr71d.dll"):
        return 0
    if os.path.basename(pathname).lower() in EXCLUDED_DLLS:
        return 1
    # How can we determine whether a dll is a 'SYSTEM DLL'?
    # Is it sufficient to use the Image Load Address?
    import struct
    file = open(pathname, "rb")
    if file.read(2) != "MZ":
        raise Exception, "Seems not to be an exe-file"
    file.seek(0x3C)
    pe_ofs = struct.unpack("i", file.read(4))[0]
    file.seek(pe_ofs)
    if file.read(4) != "PE\000\000":
        raise Exception, ("Seems not to be an exe-file", pathname)
    file.read(20 + 28) # COFF File Header, offset of ImageBase in Optional Header
    imagebase = struct.unpack("I", file.read(4))[0]
    return not (imagebase < 0x70000000)

def byte_compile(py_files, optimize=0, force=0,
                 target_dir=None, verbose=1, dry_run=0,
                 direct=None):

    if direct is None:
        direct = (__debug__ and optimize == 0)

    # "Indirect" byte-compilation: write a temporary script and then
    # run it with the appropriate flags.
    if not direct:
        from tempfile import mktemp
        from distutils.util import execute
        script_name = mktemp(".py")
        if verbose:
            print "writing byte-compilation script '%s'" % script_name
        if not dry_run:
            script = open(script_name, "w")
            script.write("""\
from py2exe.build_exe import byte_compile
from modulefinder import Module
files = [
""")

            for f in py_files:
                script.write("Module(%s, %s, %s),\n" % \
                (`f.__name__`, `f.__file__`, `f.__path__`))
            script.write("]\n")
            script.write("""
byte_compile(files, optimize=%s, force=%s,
             target_dir=%s,
             verbose=%s, dry_run=0,
             direct=1)
""" % (`optimize`, `force`, `target_dir`, `verbose`))

            script.close()

        cmd = [sys.executable, script_name]
        if optimize == 1:
            cmd.insert(1, "-O")
        elif optimize == 2:
            cmd.insert(1, "-OO")
        spawn(cmd, verbose=verbose, dry_run=dry_run)
        execute(os.remove, (script_name,), "removing %s" % script_name,
                verbose=verbose, dry_run=dry_run)


    else:
        from py_compile import compile
        from distutils.dir_util import mkpath
        from distutils.dep_util import newer
        from distutils.file_util import copy_file

        for file in py_files:
            # Terminology from the py_compile module:
            #   cfile - byte-compiled file
            #   dfile - purported source filename (same as 'file' by default)
            cfile = file.__name__.replace('.', '\\')

            if file.__path__:
                dfile = cfile + '\\__init__.py' + (__debug__ and 'c' or 'o')
            else:
                dfile = cfile + '.py' + (__debug__ and 'c' or 'o')
            if target_dir:
                cfile = os.path.join(target_dir, dfile)

            if force or newer(file.__file__, cfile):
                if verbose:
                    print "byte-compiling %s to %s" % (file.__file__, dfile)
                if not dry_run:
                    mkpath(os.path.dirname(cfile))
                    suffix = os.path.splitext(file.__file__)[1]
                    if suffix in (".py", ".pyw"):
                        compile(file.__file__, cfile, dfile)
                    elif suffix in _py_suffixes:
                        # Minor problem: This will happily copy a file
                        # <mod>.pyo to <mod>.pyc or <mod>.pyc to
                        # <mod>.pyo, but it does seem to work.
                        copy_file(file.__file__, cfile, preserve_mode=0)
                    else:
                        raise RuntimeError \
                              ("Don't know how to handle %r" % file.__file__)
            else:
                if verbose:
                    print "skipping byte-compilation of %s to %s" % \
                          (file.__file__, dfile)
    compiled_files = []
    for file in py_files:
        cfile = file.__name__.replace('.', '\\')

        if file.__path__:
            dfile = cfile + '\\__init__.py' + (optimize and 'o' or 'c')
        else:
            dfile = cfile + '.py' + (optimize and 'o' or 'c')
        compiled_files.append(dfile)
    return compiled_files

# byte_compile()

# win32com makepy helper.
def collect_win32com_genpy(path, typelibs, verbose=0, dry_run=0):
    import win32com
    from win32com.client import gencache, makepy
    from distutils.file_util import copy_file
    
    old_gen_path = win32com.__gen_path__
    num = 0
    try:
        win32com.__gen_path__ = path
        win32com.gen_py.__path__ = [path]
        gencache.__init__()
        for info in typelibs:
            guid, lcid, major, minor = info[:4]
            # They may provide an input filename in the tuple - in which case
            # they will have pre-generated it on a machine with the typelibs
            # installed, and just want us to include it.
            fname_in = None
            if len(info) > 4:
                fname_in = info[4]
            if fname_in is not None:
                base = gencache.GetGeneratedFileName(guid, lcid, major, minor)
                fname_out = os.path.join(path, base) + ".py"
                copy_file(fname_in, fname_out, verbose=verbose, dry_run=dry_run)
                num += 1
                # That's all we gotta do!
                continue

            # It seems bForDemand=True generates code which is missing
            # at least sometimes an import of DispatchBaseClass.
            # Until this is resolved, set it to false.
            # What's the purpose of bForDemand=True? Thomas
            # bForDemand is supposed to only generate stubs when each
            # individual object is referenced.  A side-effect of that is
            # that each object gets its own source file.  The intent of
            # this code was to set bForDemand=True, meaning we get the
            # 'file per object' behaviour, but then explicitly walk all
            # children forcing them to be built - so the entire object model
            # is included, but not in a huge .pyc.
            # I'm not sure why its not working :) I'll debug later.
            # bForDemand=False isn't really important here - the overhead for
            # monolithic typelib stubs is in the compilation, not the loading
            # of an existing .pyc. Mark.
##            makepy.GenerateFromTypeLibSpec(info, bForDemand = True)
            tlb_info = (guid, lcid, major, minor)
            makepy.GenerateFromTypeLibSpec(tlb_info, bForDemand = False)
            # Now get the module, and build all sub-modules.
            mod = gencache.GetModuleForTypelib(*tlb_info)
            for clsid, name in mod.CLSIDToPackageMap.items():
                try:
                    gencache.GetModuleForCLSID(clsid)
                    num += 1
                    #print "", name
                except ImportError:
                    pass
        return num
    finally:
        # restore win32com, just in case.
        win32com.__gen_path__ = old_gen_path
        win32com.gen_py.__path__ = [old_gen_path]
        gencache.__init__()

# utilities hacked from distutils.dir_util

def _chmod(file):
    os.chmod(file, 0777)

# Helper for force_remove_tree()
def _build_cmdtuple(path, cmdtuples):
    for f in os.listdir(path):
        real_f = os.path.join(path,f)
        if os.path.isdir(real_f) and not os.path.islink(real_f):
            _build_cmdtuple(real_f, cmdtuples)
        else:
            cmdtuples.append((_chmod, real_f))
            cmdtuples.append((os.remove, real_f))
    cmdtuples.append((os.rmdir, path))

def force_remove_tree (directory, verbose=0, dry_run=0):
    """Recursively remove an entire directory tree.  Any errors are ignored
    (apart from being reported to stdout if 'verbose' is true).
    """
    import distutils
    from distutils.util import grok_environment_error
    _path_created = distutils.dir_util._path_created

    if verbose:
        print "removing '%s' (and everything under it)" % directory
    if dry_run:
        return
    cmdtuples = []
    _build_cmdtuple(directory, cmdtuples)
    for cmd in cmdtuples:
        try:
            cmd[0](cmd[1])
            # remove dir from cache if it's already there
            abspath = os.path.abspath(cmd[1])
            if _path_created.has_key(abspath):
                del _path_created[abspath]
        except (IOError, OSError), exc:
            if verbose:
                print grok_environment_error(
                    exc, "error removing %s: " % directory)
