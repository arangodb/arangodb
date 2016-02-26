# boot_service.py
import sys
import os
import servicemanager
import win32service
import win32serviceutil
import winerror
# We assume that py2exe has magically set service_module_names
# to the module names that expose the services we host.
service_klasses = []
try:
    service_module_names
except NameError:
    print "This script is designed to be run from inside py2exe"
    sys.exit(1)

for name in service_module_names:
    # Use the documented fact that when a fromlist is present,
    # __import__ returns the innermost module in 'name'.
    # This makes it possible to have a dotted name work the
    # way you'd expect.
    mod = __import__(name, globals(), locals(), ['DUMMY'])
    for ob in mod.__dict__.values():
        if hasattr(ob, "_svc_name_"):
            service_klasses.append(ob)

if not service_klasses:
    raise RuntimeError, "No service classes found"

# Event source records come from servicemanager
evtsrc_dll = os.path.abspath(servicemanager.__file__)

# Tell the Python servicemanager what classes we host.
if len(service_klasses)==1:
    k = service_klasses[0]
    # One service - make the event name the same as the service.
    servicemanager.Initialize(k._svc_name_, evtsrc_dll)
    # And the class that hosts it.
    servicemanager.PrepareToHostSingle(k)
else:
    # Multiple services (NOTE - this hasn't been tested!)
    # Use the base name of the exe as the event source
    servicemanager.Initialize(os.path.basename(sys.executable), evtsrc_dll)
    for k in service_klasses:
        servicemanager.PrepareToHostMultiple(k._svc_name_, k)

################################################################

if cmdline_style == "py2exe":
    # Simulate the old py2exe service command line handling (to some extent)
    # This could do with some re-thought

    class GetoptError(Exception):
        pass
    
    def w_getopt(args, options):
        """A getopt for Windows style command lines.
    
        Options may start with either '-' or '/', the option names may
        have more than one letter (examples are /tlb or -RegServer), and
        option names are case insensitive.
    
        Returns two elements, just as getopt.getopt.  The first is a list
        of (option, value) pairs in the same way getopt.getopt does, but
        there is no '-' or '/' prefix to the option name, and the option
        name is always lower case.  The second is the list of arguments
        which do not belong to any option.
    
        Different from getopt.getopt, a single argument not belonging to an option
        does not terminate parsing.
        """
        opts = []
        arguments = []
        while args:
            if args[0][:1] in "/-":
                arg = args[0][1:] # strip the '-' or '/'
                arg = arg.lower()
                if arg + ':' in options:
                    try:
                        opts.append((arg, args[1]))
                    except IndexError:
                        raise GetoptError, "option '%s' requires an argument" % args[0]
                    args = args[1:]
                elif arg in options:
                    opts.append((arg, ''))
                else:
                    raise GetoptError, "invalid option '%s'" % args[0]
                args = args[1:]
            else:
                arguments.append(args[0])
                args = args[1:]
    
        return opts, arguments

    options = "help install remove auto disabled interactive user: password:".split()
    
    def usage():
        print "Services are supposed to be run by the system after they have been installed."
        print "These command line options are available for (de)installation:"
        for opt in options:
            if opt.endswith(":"):
                print "\t-%s <arg>" % opt
            else:
                print "\t-%s" % opt
        print
    
    try:
        opts, args = w_getopt(sys.argv[1:], options)
    except GetoptError, detail:
        print detail
        usage()
        sys.exit(1)
    
    if opts:
        startType = None
        bRunInteractive = 0
        serviceDeps = None
        userName = None
        password = None
    
        do_install = False
        do_remove = False
    
        done = False
    
        for o, a in opts:
            if o == "help":
                usage()
                done = True
            elif o == "install":
                do_install = True
            elif o == "remove":
                do_remove = True
            elif o == "auto":
                startType = win32service.SERVICE_AUTO_START
            elif o == "disabled":
                startType = win32service.SERVICE_DISABLED
            elif o == "user":
                userName = a
            elif o == "password":
                password = a
            elif o == "interactive":
                bRunInteractive = True
    
        if do_install:
            for k in service_klasses:
                svc_display_name = getattr(k, "_svc_display_name_", k._svc_name_)
                svc_deps = getattr(k, "_svc_deps_", None)
                win32serviceutil.InstallService(None,
                                                k._svc_name_,
                                                svc_display_name,
                                                exeName = sys.executable,
                                                userName = userName,
                                                password = password,
                                                startType = startType,
                                                bRunInteractive = bRunInteractive,
                                                serviceDeps = svc_deps,
                                                description = getattr(k, "_svc_description_", None),
                                                )
            done = True
    
        if do_remove:
            for k in service_klasses:
                win32serviceutil.RemoveService(k._svc_name_)
            done = True
    
        if done:
            sys.exit(0)
    else:
        usage()
        
    print "Connecting to the Service Control Manager"
    servicemanager.StartServiceCtrlDispatcher()

elif cmdline_style == "pywin32":
    assert len(service_klasses) == 1, "Can only handle 1 service!"
    k = service_klasses[0]
    if len(sys.argv) == 1:
        try:
            servicemanager.StartServiceCtrlDispatcher()
        except win32service.error, details:
            if details[0] == winerror.ERROR_FAILED_SERVICE_CONTROLLER_CONNECT:
                win32serviceutil.usage()
    else:
        win32serviceutil.HandleCommandLine(k)

elif cmdline_style == "custom":
    assert len(service_module_names) == 1, "Can only handle 1 service!"
    # Unlike services implemented in .py files, when a py2exe service exe is
    # executed without args, it may mean the service is being started.
    if len(sys.argv) == 1:
        try:
            servicemanager.StartServiceCtrlDispatcher()
        except win32service.error, details:
            if details[0] == winerror.ERROR_FAILED_SERVICE_CONTROLLER_CONNECT:
                win32serviceutil.usage()
    else:
        # assume/insist that the module provides a HandleCommandLine function.
        mod = sys.modules[service_module_names[0]]
        mod.HandleCommandLine()
