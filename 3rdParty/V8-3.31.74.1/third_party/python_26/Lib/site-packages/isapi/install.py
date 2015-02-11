"""Installation utilities for Python ISAPI filters and extensions."""

# this code adapted from "Tomcat JK2 ISAPI redirector", part of Apache
# Created July 2004, Mark Hammond.
import sys, os, imp, shutil, stat
from win32com.client import GetObject, Dispatch
from win32com.client.gencache import EnsureModule, EnsureDispatch
import pythoncom
import winerror
import traceback

_APP_INPROC  = 0
_APP_OUTPROC = 1
_APP_POOLED  = 2
_IIS_OBJECT  = "IIS://LocalHost/W3SVC"
_IIS_SERVER  = "IIsWebServer"
_IIS_WEBDIR  = "IIsWebDirectory"
_IIS_WEBVIRTUALDIR  = "IIsWebVirtualDir"
_IIS_FILTERS = "IIsFilters"
_IIS_FILTER  = "IIsFilter"

_DEFAULT_SERVER_NAME = "Default Web Site"
_DEFAULT_HEADERS     = "X-Powered-By: Python"
_DEFAULT_PROTECTION  = _APP_POOLED

# Default is for 'execute' only access - ie, only the extension
# can be used.  This can be overridden via your install script.
_DEFAULT_ACCESS_EXECUTE = True
_DEFAULT_ACCESS_READ = False
_DEFAULT_ACCESS_WRITE = False
_DEFAULT_ACCESS_SCRIPT = False
_DEFAULT_CONTENT_INDEXED = False
_DEFAULT_ENABLE_DIR_BROWSING = False
_DEFAULT_ENABLE_DEFAULT_DOC = False

is_debug_build = False
for imp_ext, _, _ in imp.get_suffixes():
    if imp_ext == "_d.pyd":
        is_debug_build = True
        break

this_dir = os.path.abspath(os.path.dirname(__file__))

class FilterParameters:
    Name = None
    Description = None
    Path = None
    Server = None
    # Params that control if/how AddExtensionFile is called.
    AddExtensionFile = True
    AddExtensionFile_Enabled = True
    AddExtensionFile_GroupID = None # defaults to Name
    AddExtensionFile_CanDelete = True
    AddExtensionFile_Description = None # defaults to Description.

    def __init__(self, **kw):
        self.__dict__.update(kw)

class VirtualDirParameters:
    Name = None # Must be provided.
    Description = None # defaults to Name
    AppProtection = _DEFAULT_PROTECTION
    Headers       = _DEFAULT_HEADERS
    Path          = None # defaults to WWW root.
    AccessExecute  = _DEFAULT_ACCESS_EXECUTE
    AccessRead     = _DEFAULT_ACCESS_READ
    AccessWrite    = _DEFAULT_ACCESS_WRITE
    AccessScript   = _DEFAULT_ACCESS_SCRIPT
    ContentIndexed = _DEFAULT_CONTENT_INDEXED
    EnableDirBrowsing = _DEFAULT_ENABLE_DIR_BROWSING
    EnableDefaultDoc  = _DEFAULT_ENABLE_DEFAULT_DOC
    DefaultDoc = None # Only set in IIS if not None
    ScriptMaps = []
    ScriptMapUpdate = "end" # can be 'start', 'end', 'replace'
    Server = None

    def __init__(self, **kw):
        self.__dict__.update(kw)

class ScriptMapParams:
    Extension = None
    Module = None
    Flags = 5
    Verbs = ""
    # Params that control if/how AddExtensionFile is called.
    AddExtensionFile = True
    AddExtensionFile_Enabled = True
    AddExtensionFile_GroupID = None # defaults to Name
    AddExtensionFile_CanDelete = True
    AddExtensionFile_Description = None # defaults to Description.
    def __init__(self, **kw):
        self.__dict__.update(kw)

class ISAPIParameters:
    ServerName     = _DEFAULT_SERVER_NAME
    # Description = None
    Filters = []
    VirtualDirs = []
    def __init__(self, **kw):
        self.__dict__.update(kw)

verbose = 1 # The level - 0 is quiet.
def log(level, what):
    if verbose >= level:
        print what

# Convert an ADSI COM exception to the Win32 error code embedded in it.
def _GetWin32ErrorCode(com_exc):
    hr, msg, exc, narg = com_exc
    # If we have more details in the 'exc' struct, use it.
    if exc:
        hr = exc[-1]
    if winerror.HRESULT_FACILITY(hr) != winerror.FACILITY_WIN32:
        raise
    return winerror.SCODE_CODE(hr)

class InstallationError(Exception): pass
class ItemNotFound(InstallationError): pass
class ConfigurationError(InstallationError): pass

def FindPath(options, server, name):
    if name.lower().startswith("iis://"):
        return name
    else:
        if name and name[0] != "/":
            name = "/"+name
        return FindWebServer(options, server)+"/ROOT"+name

def FindWebServer(options, server_desc):
    # command-line options get first go, and are assumed in 'mbcs' encoding
    # (well, assumed MBCS by the time they got to sys.argv...)
    if options.server:
        server_desc = options.server
        # but someone may have explicitly already set unicode...
        if type(server_desc) != unicode:
            server_desc = server_desc.decode("mbcs")
    # If the config passed by the caller doesn't specify one, use the default
    if not server_desc:
        server = _IIS_OBJECT+"/1"
    else:
        # Assume the user has passed either the instance_id or "server
        # description" - loop over all objects until we find it.
        ob = GetObject(_IIS_OBJECT)
        look = server_desc.lower().strip()
        for sub in ob:
            # ID is generally a number, but no need to assume that.
            this_id = getattr(sub, "Name", "").lower().strip()
            this_comment = getattr(sub, "ServerComment", "").lower().strip()
            if this_id == look or this_comment == look:
                server = sub.AdsPath
                break
        else:
            raise ItemNotFound, \
                  "No web sites match the description '%s'" % (server_desc,)
    # Check it is good.
    try:
        GetObject(server)
    except pythoncom.com_error, details:
        hr, msg, exc, arg_err = details
        if exc and exc[2]:
            msg = exc[2]
        raise ItemNotFound, \
              "WebServer %s: %s" % (server, msg)
    return server

def CreateDirectory(params, options):
    _CallHook(params, "PreInstall", options)
    if not params.Name:
        raise ConfigurationError, "No Name param"
    slash = params.Name.rfind("/")
    if slash >= 0:
        parent = params.Name[:slash]
        name = params.Name[slash+1:]
    else:
        parent = ""
        name = params.Name
    webDir = GetObject(FindPath(options, params.Server, parent))
    if parent:
        # Note that the directory won't be visible in the IIS UI
        # unless the directory exists on the filesystem.
        keyType = _IIS_WEBDIR
    else:
        keyType = _IIS_WEBVIRTUALDIR
    # We used to go to lengths to keep an existing virtual directory
    # in place.  However, in some cases the existing directories got
    # into a bad state, and an update failed to get them working.
    # So we nuke it first.  If this is a problem, we could consider adding
    # a --keep-existing option.
    try:
        # Also seen the Class change to a generic IISObject - so nuke
        # *any* existing object, regardless of Class
        existing = GetObject(FindPath(options, params.Server, params.Name))
        webDir.Delete(existing.Class, existing.Name)
        log(2, "Deleted old directory '%s'" % (params.Name,))
    except pythoncom.com_error:
        pass

    newDir = webDir.Create(keyType, name)
    log(2, "Creating new directory '%s'..." % (params.Name,))
    
    friendly = params.Description or params.Name
    newDir.AppFriendlyName = friendly
    try:
        path = params.Path or webDir.Path
        newDir.Path = path
    except AttributeError:
        pass
    newDir.AppCreate2(params.AppProtection)
    newDir.HttpCustomHeaders = params.Headers

    log(2, "Setting directory options...")
    newDir.AccessExecute  = params.AccessExecute
    newDir.AccessRead     = params.AccessRead
    newDir.AccessWrite    = params.AccessWrite
    newDir.AccessScript   = params.AccessScript
    newDir.ContentIndexed = params.ContentIndexed
    newDir.EnableDirBrowsing = params.EnableDirBrowsing
    newDir.EnableDefaultDoc  = params.EnableDefaultDoc
    if params.DefaultDoc is not None:
        newDir.DefaultDoc = params.DefaultDoc
    newDir.SetInfo()
    smp_items = []
    for smp in params.ScriptMaps:
        item = "%s,%s,%s" % (smp.Extension, smp.Module, smp.Flags)
        # IIS gets upset if there is a trailing verb comma, but no verbs
        if smp.Verbs:
            item += "," + smp.Verbs
        smp_items.append(item)
    if params.ScriptMapUpdate == "replace":
        newDir.ScriptMaps = smp_items
    elif params.ScriptMapUpdate == "end":
        for item in smp_items:
            if item not in newDir.ScriptMaps:
                newDir.ScriptMaps = newDir.ScriptMaps + (item,)
    elif params.ScriptMapUpdate == "start":
        for item in smp_items:
            if item not in newDir.ScriptMaps:
                newDir.ScriptMaps = (item,) + newDir.ScriptMaps
    else:
        raise ConfigurationError, \
              "Unknown ScriptMapUpdate option '%s'" % (params.ScriptMapUpdate,)
    newDir.SetInfo()
    _CallHook(params, "PostInstall", options, newDir)
    log(1, "Configured Virtual Directory: %s" % (params.Name,))
    return newDir

def CreateISAPIFilter(filterParams, options):
    server = FindWebServer(options, filterParams.Server)
    _CallHook(filterParams, "PreInstall", options)
    try:
        filters = GetObject(server+"/Filters")
    except pythoncom.com_error, (hr, msg, exc, arg):
        # Brand new sites don't have the '/Filters' collection - create it.
        # Any errors other than 'not found' we shouldn't ignore.
        if winerror.HRESULT_FACILITY(hr) != winerror.FACILITY_WIN32 or \
           winerror.HRESULT_CODE(hr) != winerror.ERROR_PATH_NOT_FOUND:
            raise
        server_ob = GetObject(server)
        filters = server_ob.Create(_IIS_FILTERS, "Filters")
        filters.FilterLoadOrder = ""
        filters.SetInfo()

    # As for VirtualDir, delete an existing one.
    try:
        filters.Delete(_IIS_FILTER, filterParams.Name)
        log(2, "Deleted old filter '%s'" % (filterParams.Name,))
    except pythoncom.com_error:
        pass
    newFilter = filters.Create(_IIS_FILTER, filterParams.Name)
    log(2, "Created new ISAPI filter...")
    assert os.path.isfile(filterParams.Path)
    newFilter.FilterPath  = filterParams.Path
    newFilter.FilterDescription = filterParams.Description
    newFilter.SetInfo()
    load_order = [b.strip() for b in filters.FilterLoadOrder.split(",") if b]
    if filterParams.Name not in load_order:
        load_order.append(filterParams.Name)
        filters.FilterLoadOrder = ",".join(load_order)
        filters.SetInfo()
    _CallHook(filterParams, "PostInstall", options, newFilter)
    log (1, "Configured Filter: %s" % (filterParams.Name,))
    return newFilter

def DeleteISAPIFilter(filterParams, options):
    _CallHook(filterParams, "PreRemove", options)
    server = FindWebServer(options, filterParams.Server)
    ob_path = server+"/Filters"
    try:
        filters = GetObject(ob_path)
    except pythoncom.com_error, details:
        # failure to open the filters just means a totally clean IIS install
        # (IIS5 at least has no 'Filters' key when freshly installed).
        log(2, "ISAPI filter path '%s' did not exist." % (ob_path,))
        return
    try:
        filters.Delete(_IIS_FILTER, filterParams.Name)
        log(2, "Deleted ISAPI filter '%s'" % (filterParams.Name,))
    except pythoncom.com_error, details:
        rc = _GetWin32ErrorCode(details)
        if rc != winerror.ERROR_PATH_NOT_FOUND:
            raise
        log(2, "ISAPI filter '%s' did not exist." % (filterParams.Name,))
    # Remove from the load order
    load_order = [b.strip() for b in filters.FilterLoadOrder.split(",") if b]
    if filterParams.Name in load_order:
        load_order.remove(filterParams.Name)
        filters.FilterLoadOrder = ",".join(load_order)
        filters.SetInfo()
    _CallHook(filterParams, "PostRemove", options)
    log (1, "Deleted Filter: %s" % (filterParams.Name,))

def _AddExtensionFile(module, def_groupid, def_desc, params, options):
    group_id = params.AddExtensionFile_GroupID or def_groupid
    desc = params.AddExtensionFile_Description or def_desc
    try:
        ob = GetObject(_IIS_OBJECT)
        ob.AddExtensionFile(module,
                            params.AddExtensionFile_Enabled,
                            group_id,
                            params.AddExtensionFile_CanDelete,
                            desc)
        log(2, "Added extension file '%s' (%s)" % (module, desc))
    except (pythoncom.com_error, AttributeError), details:
        # IIS5 always fails.  Probably should upgrade this to
        # complain more loudly if IIS6 fails.
        log(2, "Failed to add extension file '%s': %s" % (module, details))

def AddExtensionFiles(params, options):
    """Register the modules used by the filters/extensions as a trusted
    'extension module' - required by the default IIS6 security settings."""
    # Add each module only once.
    added = {}
    for vd in params.VirtualDirs:
        for smp in vd.ScriptMaps:
            if not added.has_key(smp.Module) and smp.AddExtensionFile:
                _AddExtensionFile(smp.Module, vd.Name, vd.Description, smp,
                                  options)
                added[smp.Module] = True

    for fd in params.Filters:
        if not added.has_key(fd.Path) and fd.AddExtensionFile:
            _AddExtensionFile(fd.Path, fd.Name, fd.Description, fd, options)
            added[fd.Path] = True

def _DeleteExtensionFileRecord(module, options):
    try:
        ob = GetObject(_IIS_OBJECT)
        ob.DeleteExtensionFileRecord(module)
        log(2, "Deleted extension file record for '%s'" % module)
    except (pythoncom.com_error, AttributeError), details:
        log(2, "Failed to remove extension file '%s': %s" % (module, details))

def DeleteExtensionFileRecords(params, options):
    deleted = {} # only remove each .dll once.
    for vd in params.VirtualDirs:
        for smp in vd.ScriptMaps:
            if not deleted.has_key(smp.Module) and smp.AddExtensionFile:
                _DeleteExtensionFileRecord(smp.Module, options)
                deleted[smp.Module] = True

    for filter_def in params.Filters:
        if not deleted.has_key(filter_def.Path) and filter_def.AddExtensionFile:
            _DeleteExtensionFileRecord(filter_def.Path, options)
            deleted[filter_def.Path] = True

def CheckLoaderModule(dll_name):
    suffix = ""
    if is_debug_build: suffix = "_d"
    template = os.path.join(this_dir,
                            "PyISAPI_loader" + suffix + ".dll")
    if not os.path.isfile(template):
        raise ConfigurationError, \
              "Template loader '%s' does not exist" % (template,)
    # We can't do a simple "is newer" check, as the DLL is specific to the
    # Python version.  So we check the date-time and size are identical,
    # and skip the copy in that case.
    src_stat = os.stat(template)
    try:
        dest_stat = os.stat(dll_name)
    except os.error:
        same = 0
    else:
        same = src_stat[stat.ST_SIZE]==dest_stat[stat.ST_SIZE] and \
               src_stat[stat.ST_MTIME]==dest_stat[stat.ST_MTIME]
    if not same:
        log(2, "Updating %s->%s" % (template, dll_name))
        shutil.copyfile(template, dll_name)
        shutil.copystat(template, dll_name)
    else:
        log(2, "%s is up to date." % (dll_name,))

def _CallHook(ob, hook_name, options, *extra_args):
    func = getattr(ob, hook_name, None)
    if func is not None:
        args = (ob,options) + extra_args
        func(*args)

def Install(params, options):
    _CallHook(params, "PreInstall", options)
    for vd in params.VirtualDirs:
        CreateDirectory(vd, options)
        
    for filter_def in params.Filters:
        CreateISAPIFilter(filter_def, options)

    AddExtensionFiles(params, options)

    _CallHook(params, "PostInstall", options)

def Uninstall(params, options):
    _CallHook(params, "PreRemove", options)
    
    DeleteExtensionFileRecords(params, options)
    
    for vd in params.VirtualDirs:
        _CallHook(vd, "PreRemove", options)
        try:
            directory = GetObject(FindPath(options, vd.Server, vd.Name))
        except pythoncom.com_error, details:
            rc = _GetWin32ErrorCode(details)
            if rc != winerror.ERROR_PATH_NOT_FOUND:
                raise
            log(2, "VirtualDirectory '%s' did not exist" % vd.Name)
            directory = None
        if directory is not None:
            # Be robust should IIS get upset about unloading.
            try:
                directory.AppUnLoad()
            except:
                exc_val = sys.exc_info()[1]
                log(2, "AppUnLoad() for %s failed: %s" % (vd.Name, exc_val))
            # Continue trying to delete it.
            try:
                parent = GetObject(directory.Parent)
                parent.Delete(directory.Class, directory.Name)
                log (1, "Deleted Virtual Directory: %s" % (vd.Name,))
            except:
                exc_val = sys.exc_info()[1]
                log(1, "Failed to remove directory %s: %s" % (vd.Name, exc_val))

        _CallHook(vd, "PostRemove", options)

    for filter_def in params.Filters:
        DeleteISAPIFilter(filter_def, options)
    _CallHook(params, "PostRemove", options)

# Patch up any missing module names in the params, replacing them with
# the DLL name that hosts this extension/filter.
def _PatchParamsModule(params, dll_name, file_must_exist = True):
    if file_must_exist:
        if not os.path.isfile(dll_name):
            raise ConfigurationError, "%s does not exist" % (dll_name,)

    # Patch up all references to the DLL.
    for f in params.Filters:
        if f.Path is None: f.Path = dll_name
    for d in params.VirtualDirs:
        for sm in d.ScriptMaps:
            if sm.Module is None: sm.Module = dll_name

def GetLoaderModuleName(mod_name, check_module = None):
    # find the name of the DLL hosting us.
    # By default, this is "_{module_base_name}.dll"
    if hasattr(sys, "frozen"):
        # What to do?  The .dll knows its name, but this is likely to be
        # executed via a .exe, which does not know.
        base, ext = os.path.splitext(mod_name)
        path, base = os.path.split(base)
        # handle the common case of 'foo.exe'/'foow.exe'
        if base.endswith('w'):
            base = base[:-1]
        # For py2exe, we have '_foo.dll' as the standard pyisapi loader - but
        # 'foo.dll' is what we use (it just delegates).
        # So no leading '_' on the installed name.
        dll_name = os.path.abspath(os.path.join(path, base + ".dll"))
    else:
        base, ext = os.path.splitext(mod_name)
        path, base = os.path.split(base)
        dll_name = os.path.abspath(os.path.join(path, "_" + base + ".dll"))
    # Check we actually have it.
    if check_module is None: check_module = not hasattr(sys, "frozen")
    if check_module:
        CheckLoaderModule(dll_name)
    return dll_name

def InstallModule(conf_module_name, params, options):
    if not hasattr(sys, "frozen"):
        conf_module_name = os.path.abspath(conf_module_name)
        if not os.path.isfile(conf_module_name):
            raise ConfigurationError, "%s does not exist" % (conf_module_name,)

    loader_dll = GetLoaderModuleName(conf_module_name)
    _PatchParamsModule(params, loader_dll)
    Install(params, options)

def UninstallModule(conf_module_name, params, options):
    loader_dll = GetLoaderModuleName(conf_module_name, False)
    _PatchParamsModule(params, loader_dll, False)
    Uninstall(params, options)

standard_arguments = {
    "install" : "Install the extension",
    "remove"  : "Remove the extension"
}

# We support 2 ways of extending our command-line/install support.
# * Many of the installation items allow you to specify "PreInstall",
#   "PostInstall", "PreRemove" and "PostRemove" hooks
#   All hooks are called with the 'params' object being operated on, and
#   the 'optparser' options for this session (ie, the command-line options)
#   PostInstall for VirtualDirectories and Filters both have an additional
#   param - the ADSI object just created.
# * You can pass your own option parser for us to use, and/or define a map
#   with your own custom arg handlers.  It is a map of 'arg'->function.
#   The function is called with (options, log_fn, arg).  The function's
#   docstring is used in the usage output.
def HandleCommandLine(params, argv=None, conf_module_name = None,
                      default_arg = "install",
                      opt_parser = None, custom_arg_handlers = {}):
    """Perform installation or removal of an ISAPI filter or extension.
    
    This module handles standard command-line options and configuration
    information, and installs, removes or updates the configuration of an
    ISAPI filter or extension.
    
    You must pass your configuration information in params - all other
    arguments are optional, and allow you to configure the installation
    process.
    """
    global verbose
    from optparse import OptionParser

    argv = argv or sys.argv
    conf_module_name = conf_module_name or sys.argv[0]
    if opt_parser is None:
        # Build our own parser.
        parser = OptionParser(usage='')
    else:
        # The caller is providing their own filter, presumably with their
        # own options all setup.
        parser = opt_parser

    # build a usage string if we don't have one.
    if not parser.get_usage():
        all_args = standard_arguments.copy()
        for arg, handler in custom_arg_handlers.items():
            all_args[arg] = handler.__doc__
        arg_names = "|".join(all_args.keys())
        usage_string = "%prog [options] [" + arg_names + "]\n"
        usage_string += "commands:\n"
        for arg, desc in all_args.items():
            usage_string += " %-10s: %s" % (arg, desc) + "\n"
        parser.set_usage(usage_string[:-1])

    parser.add_option("-q", "--quiet",
                      action="store_false", dest="verbose", default=True,
                      help="don't print status messages to stdout")
    parser.add_option("-v", "--verbosity", action="count",
                      dest="verbose", default=1,
                      help="increase the verbosity of status messages")
    parser.add_option("", "--server", action="store",
                      help="Specifies the IIS server to install/uninstall on." \
                           " Default is '%s/1'" % (_IIS_OBJECT,))

    (options, args) = parser.parse_args(argv[1:])
    verbose = options.verbose
    if not args:
        args = [default_arg]
    try:
        for arg in args:
            if arg == "install":
                InstallModule(conf_module_name, params, options)
                log(1, "Installation complete.")
            elif arg in ["remove", "uninstall"]:
                UninstallModule(conf_module_name, params, options)
                log(1, "Uninstallation complete.")
            else:
                handler = custom_arg_handlers.get(arg, None)
                if handler is None:
                    parser.error("Invalid arg '%s'" % (arg,))
                handler(options, log, arg)
    except (ItemNotFound, InstallationError), details:
        if options.verbose > 1:
            traceback.print_exc()
        print "%s: %s" % (details.__class__.__name__, details)
