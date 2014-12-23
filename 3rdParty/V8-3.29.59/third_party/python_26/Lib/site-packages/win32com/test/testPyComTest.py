# NOTE - Still seems to be a leak here somewhere
# gateway count doesnt hit zero.  Hence the print statements!

import sys; sys.coinit_flags=0 # Must be free-threaded!
import win32api, types, pythoncom, time
import sys, os, win32com, win32com.client.connect
from win32com.test.util import CheckClean
from win32com.client import constants
import win32com
from util import RegisterPythonServer

importMsg = "**** PyCOMTest is not installed ***\n  PyCOMTest is a Python test specific COM client and server.\n  It is likely this server is not installed on this machine\n  To install the server, you must get the win32com sources\n  and build it using MS Visual C++"

error = Exception

# This test uses a Python implemented COM server - ensure correctly registered.
RegisterPythonServer(os.path.join(os.path.dirname(__file__), '..', "servers", "test_pycomtest.py"))

from win32com.client import gencache
try:
    gencache.EnsureModule('{6BCDCB60-5605-11D0-AE5F-CADD4C000000}', 0, 1, 1)
except pythoncom.com_error:
    print "The PyCOMTest module can not be located or generated."
    print importMsg
    raise RuntimeError, importMsg

# We had a bg where RegisterInterfaces would fail if gencache had 
# already been run - exercise that here
from win32com import universal
universal.RegisterInterfaces('{6BCDCB60-5605-11D0-AE5F-CADD4C000000}', 0, 1, 1)

verbose = 0

def progress(*args):
    if verbose:
        for arg in args:
            print arg,
        print

def TestApplyResult(fn, args, result):
    try:
        import string
        fnName = string.split(str(fn))[1]
    except:
        fnName = str(fn)
    progress("Testing ", fnName)
    pref = "function " + fnName
    rc  = apply(fn, args)
    if rc != result:
        raise error, "%s failed - result not %r but %r" % (pref, result, rc)

def TestConstant(constName, pyConst):
    try: 
        comConst = getattr(constants, constName)
    except:
        raise error, "Constant %s missing" % (constName,)
    if comConst != pyConst:
        raise error, "Constant value wrong for %s - got %s, wanted %s" % (constName, comConst, pyConst)

# Simple handler class.  This demo only fires one event.
class RandomEventHandler:
    def _Init(self):
        self.fireds = {}
    def OnFire(self, no):
        try:
            self.fireds[no] = self.fireds[no] + 1
        except KeyError:
            self.fireds[no] = 0
    def OnFireWithNamedParams(self, no, a_bool, out1, out2):
        # This test exists mainly to help with an old bug, where named
        # params would come in reverse.
        Missing = pythoncom.Missing
        if no is not Missing:
            # We know our impl called 'OnFire' with the same ID
            assert no in self.fireds
            assert no+1==out1, "expecting 'out1' param to be ID+1"
            assert no+2==out2, "expecting 'out2' param to be ID+2"
        # The middle must be a boolean.
        assert a_bool is Missing or type(a_bool)==bool, "middle param not a bool"
        return out1+2, out2+2

    def _DumpFireds(self):
        if not self.fireds:
            print "ERROR: Nothing was received!"
        for firedId, no in self.fireds.items():
            progress("ID %d fired %d times" % (firedId, no))

def TestDynamic():
    progress("Testing Dynamic")
    import win32com.client.dynamic
    o = win32com.client.dynamic.DumbDispatch("PyCOMTest.PyCOMTest")

    progress("Getting counter")
    counter = o.GetSimpleCounter()
    TestCounter(counter, 0)

    progress("Checking default args")
    rc = o.TestOptionals()
    if  rc[:-1] != ("def", 0, 1) or abs(rc[-1]-3.14)>.01:
        print rc
        raise error, "Did not get the optional values correctly"
    rc = o.TestOptionals("Hi", 2, 3, 1.1)
    if  rc[:-1] != ("Hi", 2, 3) or abs(rc[-1]-1.1)>.01:
        print rc
        raise error, "Did not get the specified optional values correctly"
    rc = o.TestOptionals2(0)
    if  rc != (0, "", 1):
        print rc
        raise error, "Did not get the optional2 values correctly"
    rc = o.TestOptionals2(1.1, "Hi", 2)
    if  rc[1:] != ("Hi", 2) or abs(rc[0]-1.1)>.01:
        print rc
        raise error, "Did not get the specified optional2 values correctly"

#       if verbose: print "Testing structs"
    r = o.GetStruct()
    assert r.int_value == 99 and str(r.str_value)=="Hello from C++"
    counter = win32com.client.dynamic.DumbDispatch("PyCOMTest.SimpleCounter")
    TestCounter(counter, 0)
    assert o.DoubleString("foo") == "foofoo"

    l=[]
    TestApplyResult(o.SetVariantSafeArray, (l,), len(l))
    l=[1,2,3,4]
    TestApplyResult(o.SetVariantSafeArray, (l,), len(l))
#       TestApplyResult(o.SetIntSafeArray, (l,), len(l))       Still fails, and probably always will.
    TestApplyResult(o.CheckVariantSafeArray, ((1,2,3,4,),), 1)
    o.LongProp = 3
    if o.LongProp != 3 or o.IntProp != 3:
        raise error, "Property value wrong - got %d/%d" % (o.LongProp,o.IntProp)
    o.LongProp = o.IntProp = -3
    if o.LongProp != -3 or o.IntProp != -3:
        raise error, "Property value wrong - got %d/%d" % (o.LongProp,o.IntProp)
    # This number fits in an unsigned long.  Attempting to set it to a normal
    # long will involve overflow, which is to be expected. But we do
    # expect it to work in a property explicitly a VT_UI4.
    check = 3 *10 **9
    o.ULongProp = check
    if o.ULongProp != check:
        raise error, "Property value wrong - got %d (expected %d)" % (o.ULongProp, check)
    # currency.
    pythoncom.__future_currency__ = 1
    if o.CurrencyProp != 0:
        raise error, "Expecting 0, got %r" % (o.CurrencyProp,)
    try:
        import decimal
    except ImportError:
        import win32com.decimal_23 as decimal
    o.CurrencyProp = decimal.Decimal("1234.5678")
    if o.CurrencyProp != decimal.Decimal("1234.5678"):
        raise error, "got %r" % (o.CurrencyProp,)
    v1 = decimal.Decimal("1234.5678")
    # can't do "DoubleCurrencyByVal" in dynamic files.
    TestApplyResult(o.DoubleCurrency, (v1,), v1*2)
    v2 = decimal.Decimal("9012.3456")
    TestApplyResult(o.AddCurrencies, (v1, v2), v1+v2)

    # damn - props with params don't work for dynamic objects :(
    # o.SetParamProp(0, 1)
    # if o.ParamProp(0) != 1:
    #    raise RuntimeError, o.paramProp(0)

    try:
        import datetime
        now = datetime.datetime.now()
        expect = pythoncom.MakeTime(now)
        TestApplyResult(o.EarliestDate, (now, now), expect)
    except ImportError:
        pass # py 2.2 - no datetime

def TestGenerated():
    # Create an instance of the server.
    from win32com.client.gencache import EnsureDispatch
    o = EnsureDispatch("PyCOMTest.PyCOMTest")
    counter = o.GetSimpleCounter()
    TestCounter(counter, 1)

    counter = EnsureDispatch("PyCOMTest.SimpleCounter")
    TestCounter(counter, 1)

    i1, i2 = o.GetMultipleInterfaces()
    if type(i1) != types.InstanceType or type(i2) != types.InstanceType:
        # Yay - is now an instance returned!
        raise error,  "GetMultipleInterfaces did not return instances - got '%s', '%s'" % (i1, i2)
    del i1
    del i2

    progress("Checking default args")
    rc = o.TestOptionals()
    if  rc[:-1] != ("def", 0, 1) or abs(rc[-1]-3.14)>.01:
        print rc
        raise error, "Did not get the optional values correctly"
    rc = o.TestOptionals("Hi", 2, 3, 1.1)
    if  rc[:-1] != ("Hi", 2, 3) or abs(rc[-1]-1.1)>.01:
        print rc
        raise error, "Did not get the specified optional values correctly"
    rc = o.TestOptionals2(0)
    if  rc != (0, "", 1):
        print rc
        raise error, "Did not get the optional2 values correctly"
    rc = o.TestOptionals2(1.1, "Hi", 2)
    if  rc[1:] != ("Hi", 2) or abs(rc[0]-1.1)>.01:
        print rc
        raise error, "Did not get the specified optional2 values correctly"

    progress("Checking var args")
    o.SetVarArgs("Hi", "There", "From", "Python", 1)
    if o.GetLastVarArgs() != ("Hi", "There", "From", "Python", 1):
        raise error, "VarArgs failed -" + str(o.GetLastVarArgs())
    progress("Checking getting/passing IUnknown")
    if o.GetSetUnknown(o) != o:
        raise error, "GetSetUnknown failed"
    progress("Checking getting/passing IDispatch")
    if type(o.GetSetDispatch(o)) !=types.InstanceType:
        raise error, "GetSetDispatch failed"
    progress("Checking getting/passing IDispatch of known type")
    if o.GetSetInterface(o).__class__ != o.__class__:
        raise error, "GetSetDispatch failed"
    if o.GetSetVariant(4) != 4:
        raise error, "GetSetVariant (int) failed"
    if o.GetSetVariant("foo") != "foo":
        raise error, "GetSetVariant (str) failed"
    if o.GetSetVariant(o) != o:
        raise error, "GetSetVariant (dispatch) failed"
    for l in sys.maxint, sys.maxint+1, 1 << 65L:
        if o.GetSetVariant(l) != l:
            raise error, "GetSetVariant (long) failed"
    if o.TestByRefVariant(2) != 4:
        raise error, "TestByRefVariant failed"
    if o.TestByRefString("Foo") != "FooFoo":
        raise error, "TestByRefString failed"

    # Pass some non-sequence objects to our array decoder, and watch it fail.
    try:
        o.SetVariantSafeArray("foo")
        raise error, "Expected a type error"
    except TypeError:
        pass
    try:
        o.SetVariantSafeArray(666)
        raise error, "Expected a type error"
    except TypeError:
        pass

    o.GetSimpleSafeArray(None)
    TestApplyResult(o.GetSimpleSafeArray, (None,), tuple(range(10)))
    resultCheck = tuple(range(5)), tuple(range(10)), tuple(range(20))
    TestApplyResult(o.GetSafeArrays, (None, None, None), resultCheck)

    l=[1,2,3,4]
    TestApplyResult(o.SetVariantSafeArray, (l,), len(l))
    TestApplyResult(o.SetIntSafeArray, (l,), len(l))
    l=[]
    TestApplyResult(o.SetVariantSafeArray, (l,), len(l))
    TestApplyResult(o.SetIntSafeArray, (l,), len(l))
    # Tell the server to do what it does!
    TestApplyResult(o.Test, ("Unused", 99), 1) # A bool function
    TestApplyResult(o.Test, ("Unused", -1), 1) # A bool function
    TestApplyResult(o.Test, ("Unused", 1==1), 1) # A bool function
    TestApplyResult(o.Test, ("Unused", 0), 0)
    TestApplyResult(o.Test, ("Unused", 1==0), 0)
    TestApplyResult(o.Test2, (constants.Attr2,), constants.Attr2)
    TestApplyResult(o.Test3, (constants.Attr2,), constants.Attr2)
    TestApplyResult(o.Test4, (constants.Attr2,), constants.Attr2)
    TestApplyResult(o.Test5, (constants.Attr2,), constants.Attr2)

    TestApplyResult(o.Test6, (constants.WideAttr1,), constants.WideAttr1)
    TestApplyResult(o.Test6, (constants.WideAttr2,), constants.WideAttr2)
    TestApplyResult(o.Test6, (constants.WideAttr3,), constants.WideAttr3)
    TestApplyResult(o.Test6, (constants.WideAttr4,), constants.WideAttr4)
    TestApplyResult(o.Test6, (constants.WideAttr5,), constants.WideAttr5)

    TestConstant("ULongTest1", 0xFFFFFFFFL)
    TestConstant("ULongTest2", 0x7FFFFFFFL)
    TestConstant("LongTest1", -0x7FFFFFFFL)
    TestConstant("LongTest2", 0x7FFFFFFFL)
    TestConstant("UCharTest", 255)
    TestConstant("CharTest", -1)
    TestConstant("StringTest", "Hello Loraine")

    now = pythoncom.MakeTime(time.gmtime(time.time()))
    later = pythoncom.MakeTime(time.gmtime(time.time()+1))
    TestApplyResult(o.EarliestDate, (now, later), now)
    try:
        import datetime
        now = datetime.datetime.now()
        expect = pythoncom.MakeTime(now)
        TestApplyResult(o.EarliestDate, (now, now), expect)
    except ImportError:
        pass # py 2.2 - no datetime

    assert o.DoubleString("foo") == "foofoo"
    assert o.DoubleInOutString("foo") == "foofoo"

    o.LongProp = 3
    if o.LongProp != 3 or o.IntProp != 3:
        raise error, "Property value wrong - got %d/%d" % (o.LongProp,o.IntProp)

    o.LongProp = o.IntProp = -3
    if o.LongProp != -3 or o.IntProp != -3:
        raise error, "Property value wrong - got %d/%d" % (o.LongProp,o.IntProp)

    check = 3 *10 **9
    o.ULongProp = check
    if o.ULongProp != check:
        raise error, "Property value wrong - got %d (expected %d)" % (o.ULongProp, check)

    # currency.
    pythoncom.__future_currency__ = 1
    if o.CurrencyProp != 0:
        raise error, "Expecting 0, got %r" % (o.CurrencyProp,)
    try:
        import decimal
    except ImportError:
        import win32com.decimal_23 as decimal
    for val in ("1234.5678", "1234.56", "1234"):
        o.CurrencyProp = decimal.Decimal(val)
        if o.CurrencyProp != decimal.Decimal(val):
            raise error, "%s got %r" % (val, o.CurrencyProp)
    v1 = decimal.Decimal("1234.5678")
    TestApplyResult(o.DoubleCurrency, (v1,), v1*2)
    TestApplyResult(o.DoubleCurrencyByVal, (v1,), v1*2)
    v2 = decimal.Decimal("9012.3456")
    TestApplyResult(o.AddCurrencies, (v1, v2), v1+v2)

    o.SetParamProp(0, 1)
    if o.ParamProp(0) != 1:
        raise RuntimeError, o.paramProp(0)

    # Do the connection point thing...
    # Create a connection object.
    progress("Testing connection points")
    sessions = []
    o = win32com.client.DispatchWithEvents( o, RandomEventHandler)
    o._Init()

    try:
        for i in range(3):
            session = o.Start()
            sessions.append(session)
        time.sleep(.5)
    finally:
        # Stop the servers
        for session in sessions:
            o.Stop(session)
        o._DumpFireds()
    progress("Finished generated .py test.")

def TestCounter(counter, bIsGenerated):
    # Test random access into container
    progress("Testing counter", `counter`)
    import random
    for i in xrange(50):
        num = int(random.random() * len(counter))
        try:
            ret = counter[num]
            if ret != num+1:
                raise error, "Random access into element %d failed - return was %s" % (num,`ret`)
        except IndexError:
            raise error, "** IndexError accessing collection element %d" % num

    num = 0
    if bIsGenerated:
        counter.SetTestProperty(1)
        counter.TestProperty = 1 # Note this has a second, default arg.
        counter.SetTestProperty(1,2)
        if counter.TestPropertyWithDef != 0:
            raise error, "Unexpected property set value!"
        if counter.TestPropertyNoDef(1) != 1:
            raise error, "Unexpected property set value!"
    else:
        pass
        # counter.TestProperty = 1

    counter.LBound=1
    counter.UBound=10
    if counter.LBound <> 1 or counter.UBound<>10:
        print "** Error - counter did not keep its properties"

    if bIsGenerated:
        bounds = counter.GetBounds()
        if bounds[0]<>1 or bounds[1]<>10:
            raise error, "** Error - counter did not give the same properties back"
        counter.SetBounds(bounds[0], bounds[1])

    for item in counter:
        num = num + 1
    if num <> len(counter):
        raise error, "*** Length of counter and loop iterations dont match ***"
    if num <> 10:
        raise error, "*** Unexpected number of loop iterations ***"

    counter = counter._enum_.Clone() # Test Clone() and enum directly
    counter.Reset()
    num = 0
    for item in counter:
        num = num + 1
    if num <> 10:
        raise error, "*** Unexpected number of loop iterations - got %d ***" % num
    progress("Finished testing counter")

def TestLocalVTable(ob):
    # Python doesn't fully implement this interface.
    if ob.DoubleString("foo") != "foofoo":
        raise error("couldn't foofoo")

###############################
##
## Some vtable tests of the interface
##
def TestVTable(clsctx=pythoncom.CLSCTX_ALL):
    # Any vtable interfaces marked as dual *should* be able to be
    # correctly implemented as IDispatch.
    ob = win32com.client.Dispatch("Python.Test.PyCOMTest")
    TestLocalVTable(ob)
    # Now test it via vtable - use some C++ code to help here as Python can't do it directly yet.
    tester = win32com.client.Dispatch("PyCOMTest.PyCOMTest")
    testee = pythoncom.CoCreateInstance("Python.Test.PyCOMTest", None, clsctx, pythoncom.IID_IUnknown)
    # check we fail gracefully with None passed.
    try:
        tester.TestMyInterface(None)
    except pythoncom.com_error, details:
        pass
    # and a real object.
    tester.TestMyInterface(testee)

def TestVTable2():
    # We once crashed creating our object with the native interface as
    # the first IID specified.  We must do it _after_ the tests, so that
    # Python has already had the gateway registered from last run.
    ob = win32com.client.Dispatch("Python.Test.PyCOMTest")
    iid = pythoncom.InterfaceNames["IPyCOMTest"]
    clsid = "Python.Test.PyCOMTest"
    clsctx = pythoncom.CLSCTX_SERVER
    try:
        testee = pythoncom.CoCreateInstance(clsid, None, clsctx, iid)
    except TypeError:
        # Python can't actually _use_ this interface yet, so this is
        # "expected".  Any COM error is not.
        pass

def TestVTableMI():
    clsctx = pythoncom.CLSCTX_SERVER
    ob = pythoncom.CoCreateInstance("Python.Test.PyCOMTestMI", None, clsctx, pythoncom.IID_IUnknown)
    # This inherits from IStream.
    ob.QueryInterface(pythoncom.IID_IStream)
    # This implements IStorage, specifying the IID as a string
    ob.QueryInterface(pythoncom.IID_IStorage)
    # IDispatch should always work
    ob.QueryInterface(pythoncom.IID_IDispatch)
    
    iid = pythoncom.InterfaceNames["IPyCOMTest"]
    try:
        ob.QueryInterface(iid)
    except TypeError:
        # Python can't actually _use_ this interface yet, so this is
        # "expected".  Any COM error is not.
        pass

def TestQueryInterface(long_lived_server = 0, iterations=5):
    tester = win32com.client.Dispatch("PyCOMTest.PyCOMTest")
    if long_lived_server:
        # Create a local server
        t0 = win32com.client.Dispatch("Python.Test.PyCOMTest", clsctx=pythoncom.CLSCTX_LOCAL_SERVER)
    # Request custom interfaces a number of times
    prompt = [
            "Testing QueryInterface without long-lived local-server #%d of %d...",
            "Testing QueryInterface with long-lived local-server #%d of %d..."
    ]

    for i in range(iterations):
        progress(prompt[long_lived_server!=0] % (i+1, iterations))
        tester.TestQueryInterface()

class Tester(win32com.test.util.TestCase):
    def testVTableInProc(self):
        # We used to crash running this the second time - do it a few times
        for i in range(3):
            progress("Testing VTables in-process #%d..." % (i+1))
            TestVTable(pythoncom.CLSCTX_INPROC_SERVER)
    def testVTableLocalServer(self):
        for i in range(3):
            progress("Testing VTables out-of-process #%d..." % (i+1))
            TestVTable(pythoncom.CLSCTX_LOCAL_SERVER)
    def testVTable2(self):
        for i in range(3):
            TestVTable2()
    def testVTableMI(self):
        for i in range(3):
            TestVTableMI()
    def testMultiQueryInterface(self):
        TestQueryInterface(0,6)
        # When we use the custom interface in the presence of a long-lived
        # local server, i.e. a local server that is already running when
        # we request an instance of our COM object, and remains afterwards,
        # then after repeated requests to create an instance of our object
        # the custom interface disappears -- i.e. QueryInterface fails with
        # E_NOINTERFACE. Set the upper range of the following test to 2 to
        # pass this test, i.e. TestQueryInterface(1,2)
        TestQueryInterface(1,6)
    def testDynamic(self):
        TestDynamic()
    def testGenerated(self):
        TestGenerated()

if __name__=='__main__':
    # XXX - todo - Complete hack to crank threading support.
    # Should NOT be necessary
    def NullThreadFunc():
        pass
    import thread
    thread.start_new( NullThreadFunc, () )

    if "-v" in sys.argv: verbose = 1
    
    win32com.test.util.testmain()
