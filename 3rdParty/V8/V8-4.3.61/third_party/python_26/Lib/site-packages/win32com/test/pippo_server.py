# A little test server, complete with typelib, we can use for testing.
# Originally submitted with bug:
# [ 753154 ] memory leak wrapping object having _typelib_guid_ attribute
# but modified by mhammond for use as part of the test suite.
import sys, os
import pythoncom
import win32com
import winerror
from win32com.server.util import wrap

try:
    __file__ # 2.3 only for __main__
except NameError:
    __file__ = sys.argv[0]

class CPippo:
    #
    # COM declarations    
    #
    _reg_clsid_ = "{05AC1CCE-3F9B-4d9a-B0B5-DFE8BE45AFA8}"
    _reg_desc_ = "Pippo Python test object"
    _reg_progid_ = "Python.Test.Pippo"
    #_reg_clsctx_ = pythoncom.CLSCTX_LOCAL_SERVER    
    ###
    ### Link to typelib
    _typelib_guid_ = '{41059C57-975F-4B36-8FF3-C5117426647A}'
    _typelib_version_ = 1, 0
    _com_interfaces_ = ['IPippo']

    def __init__(self):
        self.MyProp1 = 10

    def Method1(self):
        return wrap(CPippo())

    def Method2(self, in1, inout1):
        return in1, inout1 * 2

def BuildTypelib():
    from distutils.dep_util import newer
    this_dir = os.path.dirname(__file__)
    idl = os.path.abspath(os.path.join(this_dir, "pippo.idl"))
    tlb=os.path.splitext(idl)[0] + '.tlb'
    if newer(idl, tlb):
        print "Compiling %s" % (idl,)
        rc = os.system ('midl "%s"' % (idl,))
        if rc:
            raise RuntimeError, "Compiling MIDL failed!"
        # Can't work out how to prevent MIDL from generating the stubs.
        # just nuke them
        for fname in "dlldata.c pippo_i.c pippo_p.c pippo.h".split():
            os.remove(os.path.join(this_dir, fname))
    
    print "Registering %s" % (tlb,)
    tli=pythoncom.LoadTypeLib(tlb)
    pythoncom.RegisterTypeLib(tli,tlb)

def UnregisterTypelib():
    k = CPippo
    try:
        pythoncom.UnRegisterTypeLib(k._typelib_guid_, 
                                    k._typelib_version_[0], 
                                    k._typelib_version_[1], 
                                    0, 
                                    pythoncom.SYS_WIN32)
        print "Unregistered typelib"
    except pythoncom.error, details:
        if details[0]==winerror.TYPE_E_REGISTRYACCESS:
            pass
        else:
            raise

def main(argv=None):
    if argv is None: argv = sys.argv[1:]
    if '--unregister' in argv:
        # Unregister the type-libraries.
        UnregisterTypelib()
    else:
        # Build and register the type-libraries.
        BuildTypelib()
    import win32com.server.register 
    win32com.server.register.UseCommandLine(CPippo)

if __name__=='__main__':
    main(sys.argv)
