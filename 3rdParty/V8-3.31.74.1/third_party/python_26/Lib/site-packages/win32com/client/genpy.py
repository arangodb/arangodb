"""genpy.py - The worker for makepy.  See makepy.py for more details

This code was moved simply to speed Python in normal circumstances.  As the makepy.py
is normally run from the command line, it reparses the code each time.  Now makepy
is nothing more than the command line handler and public interface.

The makepy command line etc handling is also getting large enough in its own right!
"""

# NOTE - now supports a "demand" mechanism - the top-level is a package, and
# each class etc can be made individually.
# This should eventually become the default.
# Then the old non-package technique should be removed.
# There should be no b/w compat issues, and will just help clean the code.
# This will be done once the new "demand" mechanism gets a good workout.
import os
import sys
import string
import time
import win32com

import pythoncom
import build

error = "makepy.error"
makepy_version = "0.4.97" # Written to generated file.

GEN_FULL="full"
GEN_DEMAND_BASE = "demand(base)"
GEN_DEMAND_CHILD = "demand(child)"

try:
    TrueRepr = repr(True)
    FalseRepr = repr(False)
except NameError:
    TrueRepr = "1"
    FalseRepr = "0"

# This map is used purely for the users benefit -it shows the
# raw, underlying type of Alias/Enums, etc.  The COM implementation
# does not use this map at runtime - all Alias/Enum have already
# been translated.
mapVTToTypeString = {
    pythoncom.VT_I2: 'types.IntType',
    pythoncom.VT_I4: 'types.IntType',
    pythoncom.VT_R4: 'types.FloatType',
    pythoncom.VT_R8: 'types.FloatType',
    pythoncom.VT_BSTR: 'types.StringType',
    pythoncom.VT_BOOL: 'types.IntType',
    pythoncom.VT_VARIANT: 'types.TypeType',
    pythoncom.VT_I1: 'types.IntType',
    pythoncom.VT_UI1: 'types.IntType',
    pythoncom.VT_UI2: 'types.IntType',
    pythoncom.VT_UI4: 'types.IntType',
    pythoncom.VT_I8: 'types.LongType',
    pythoncom.VT_UI8: 'types.LongType',
    pythoncom.VT_INT: 'types.IntType',
    pythoncom.VT_DATE: 'pythoncom.PyTimeType',
    pythoncom.VT_UINT: 'types.IntType',
}

# Given a propget function's arg desc, return the default parameters for all
# params bar the first.  Eg, then Python does a:
# object.Property = "foo"
# Python can only pass the "foo" value.  If the property has
# multiple args, and the rest have default values, this allows
# Python to correctly pass those defaults.
def MakeDefaultArgsForPropertyPut(argsDesc):
    ret = []
    for desc in argsDesc[1:]:
        default = build.MakeDefaultArgRepr(desc)
        if default is None:
            break
        ret.append(default)
    return tuple(ret)
                            

def MakeMapLineEntry(dispid, wFlags, retType, argTypes, user, resultCLSID):
    # Strip the default value
    argTypes = tuple(map(lambda what: what[:2], argTypes))
    return '(%s, %d, %s, %s, "%s", %s)' % \
        (dispid, wFlags, retType[:2], argTypes, user, resultCLSID)

def MakeEventMethodName(eventName):
    if eventName[:2]=="On":
        return eventName
    else:
        return "On"+eventName

def WriteSinkEventMap(obj, stream):
    print >> stream, '\t_dispid_to_func_ = {'
    for name, entry in obj.propMapGet.items() + obj.propMapPut.items() + obj.mapFuncs.items():
        fdesc = entry.desc
        print >> stream, '\t\t%9d : "%s",' % (entry.desc[0], MakeEventMethodName(entry.names[0]))
    print >> stream, '\t\t}'
    

# MI is used to join my writable helpers, and the OLE
# classes.
class WritableItem:
    def __cmp__(self, other):
        "Compare for sorting"   
        ret = cmp(self.order, other.order)
        if ret==0 and self.doc: ret = cmp(self.doc[0], other.doc[0])
        return ret
    def __repr__(self):
        return "OleItem: doc=%s, order=%d" % (`self.doc`, self.order)


class RecordItem(build.OleItem, WritableItem):
  order = 9
  typename = "RECORD"

  def __init__(self, typeInfo, typeAttr, doc=None, bForUser=1):
##    sys.stderr.write("Record %s: size %s\n" % (doc,typeAttr.cbSizeInstance))
##    sys.stderr.write(" cVars = %s\n" % (typeAttr.cVars,))
##    for i in range(typeAttr.cVars):
##        vdesc = typeInfo.GetVarDesc(i)
##        sys.stderr.write(" Var %d has value %s, type %d, desc=%s\n" % (i, vdesc.value, vdesc.varkind, vdesc.elemdescVar))
##        sys.stderr.write(" Doc is %s\n" % (typeInfo.GetDocumentation(vdesc.memid),))

    build.OleItem.__init__(self, doc)
    self.clsid = typeAttr[0]

  def WriteClass(self, generator):
    pass

# Given an enum, write all aliases for it.
# (no longer necessary for new style code, but still used for old code.
def WriteAliasesForItem(item, aliasItems, stream):
  for alias in aliasItems.values():
    if item.doc and alias.aliasDoc and (alias.aliasDoc[0]==item.doc[0]):
      alias.WriteAliasItem(aliasItems, stream)
      
class AliasItem(build.OleItem, WritableItem):
  order = 2
  typename = "ALIAS"

  def __init__(self, typeinfo, attr, doc=None, bForUser = 1):
    build.OleItem.__init__(self, doc)

    ai = attr[14]
    self.attr = attr
    if type(ai) == type(()) and \
      type(ai[1])==type(0): # XXX - This is a hack - why tuples?  Need to resolve?
      href = ai[1]
      alinfo = typeinfo.GetRefTypeInfo(href)
      self.aliasDoc = alinfo.GetDocumentation(-1)
      self.aliasAttr = alinfo.GetTypeAttr()
    else:
      self.aliasDoc = None
      self.aliasAttr = None

  def WriteAliasItem(self, aliasDict, stream):
    # we could have been written as part of an alias dependency
    if self.bWritten:
      return

    if self.aliasDoc:
      depName = self.aliasDoc[0]
      if aliasDict.has_key(depName):
        aliasDict[depName].WriteAliasItem(aliasDict, stream)
      print >> stream, self.doc[0] + " = " + depName
    else:
      ai = self.attr[14]
      if type(ai) == type(0):
        try:
          typeStr = mapVTToTypeString[ai]
          print >> stream, "# %s=%s" % (self.doc[0], typeStr)
        except KeyError:
          print >> stream, self.doc[0] + " = None # Can't convert alias info " + str(ai)
    print >> stream
    self.bWritten = 1

class EnumerationItem(build.OleItem, WritableItem):
  order = 1
  typename = "ENUMERATION"

  def __init__(self, typeinfo, attr, doc=None, bForUser=1):
    build.OleItem.__init__(self, doc)

    self.clsid = attr[0]
    self.mapVars = {}
    typeFlags = attr[11]
    self.hidden = typeFlags & pythoncom.TYPEFLAG_FHIDDEN or \
                  typeFlags & pythoncom.TYPEFLAG_FRESTRICTED

    for j in range(attr[7]):
      vdesc = typeinfo.GetVarDesc(j)
      name = typeinfo.GetNames(vdesc[0])[0]
      self.mapVars[name] = build.MapEntry(vdesc)

##  def WriteEnumerationHeaders(self, aliasItems, stream):
##    enumName = self.doc[0]
##    print >> stream "%s=constants # Compatibility with previous versions." % (enumName)
##    WriteAliasesForItem(self, aliasItems)
    
  def WriteEnumerationItems(self, stream):
    enumName = self.doc[0]
    # Write in name alpha order
    names = self.mapVars.keys()
    names.sort()
    for name in names:
      entry = self.mapVars[name]
      vdesc = entry.desc
      if vdesc[4] == pythoncom.VAR_CONST:
        val = vdesc[1]
        if type(val) in (type(0), type(0L)):
          if val==0x80000000L: # special case
            use = "0x80000000L" # 'L' for future warning
          elif val > 0x80000000L or val < 0: # avoid a FutureWarning
            use = long(val)
          else:
            use = hex(val)
        else:
          use = repr(str(val))
        print >> stream, "\t%-30s=%-10s # from enum %s" % \
                      (build.MakePublicAttributeName(name, True), use, enumName)

class VTableItem(build.VTableItem, WritableItem):
    order = 4

    def WriteClass(self, generator):
        self.WriteVTableMap(generator)
        self.bWritten = 1

    def WriteVTableMap(self, generator):
        stream = generator.file
        print >> stream, "%s_vtables_dispatch_ = %d" % (self.python_name, self.bIsDispatch)
        print >> stream, "%s_vtables_ = [" % (self.python_name, ) 
        for v in self.vtableFuncs:
            names, dispid, desc = v
            arg_desc = desc[2]

            arg_reprs = []
            # more hoops so we don't generate huge lines.
            item_num = 0
            print >> stream, "\t((",
            for name in names:
                print >> stream, repr(name), ",",
                item_num = item_num + 1
                if item_num % 5 == 0:
                    print >> stream, "\n\t\t\t",
            print >> stream, "), %d, (%r, %r, [" % (dispid, desc[0], desc[1]),
            for arg in arg_desc:
                item_num = item_num + 1
                if item_num % 5 == 0:
                    print >> stream, "\n\t\t\t",
                defval = build.MakeDefaultArgRepr(arg)
                if arg[3] is None:
                    arg3_repr = None
                else:
                    arg3_repr = repr(arg[3])
                print >> stream, repr((arg[0], arg[1], defval, arg3_repr)), ",",
            print >> stream, "],",
            for d in desc[3:]:
                print >> stream, repr(d), ",", 
            print >> stream, ")),"
        print >> stream, "]"
        print >> stream

class DispatchItem(build.DispatchItem, WritableItem):
    order = 3

    def __init__(self, typeinfo, attr, doc=None):
        build.DispatchItem.__init__(self, typeinfo, attr, doc)
        self.type_attr = attr
        self.coclass_clsid = None

    def WriteClass(self, generator):
      if not self.bIsDispatch and not self.type_attr.typekind == pythoncom.TKIND_DISPATCH:
          return
      # This is pretty screwey - now we have vtable support we
      # should probably rethink this (ie, maybe write both sides for sinks, etc)
      if self.bIsSink:
          self.WriteEventSinkClassHeader(generator)
          self.WriteCallbackClassBody(generator)
      else:
          self.WriteClassHeader(generator)
          self.WriteClassBody(generator)
      print >> generator.file
      self.bWritten = 1

    def WriteClassHeader(self, generator):
        generator.checkWriteDispatchBaseClass()
        doc = self.doc
        stream = generator.file
        print >> stream, 'class ' + self.python_name + '(DispatchBaseClass):'
        if doc[1]: print >> stream, '\t' + build._safeQuotedString(doc[1])
        try:
            progId = pythoncom.ProgIDFromCLSID(self.clsid)
            print >> stream, "\t# This class is creatable by the name '%s'" % (progId)
        except pythoncom.com_error:
            pass
        print >> stream, "\tCLSID = " + repr(self.clsid)
        if self.coclass_clsid is None:
            print >> stream, "\tcoclass_clsid = None"
        else:
            print >> stream, "\tcoclass_clsid = " + repr(self.coclass_clsid)
        print >> stream
        self.bWritten = 1

    def WriteEventSinkClassHeader(self, generator):
        generator.checkWriteEventBaseClass()
        doc = self.doc
        stream = generator.file
        print >> stream, 'class ' + self.python_name + ':'
        if doc[1]: print >> stream, '\t' + build._safeQuotedString(doc[1])
        try:
            progId = pythoncom.ProgIDFromCLSID(self.clsid)
            print >> stream, "\t# This class is creatable by the name '%s'" % (progId)
        except pythoncom.com_error:
            pass
        print >> stream, '\tCLSID = CLSID_Sink = ' + repr(self.clsid)
        if self.coclass_clsid is None:
            print >> stream, "\tcoclass_clsid = None"
        else:
            print >> stream, "\tcoclass_clsid = " + repr(self.coclass_clsid)
        print >> stream, '\t_public_methods_ = [] # For COM Server support'
        WriteSinkEventMap(self, stream)
        print >> stream
        print >> stream, '\tdef __init__(self, oobj = None):'
        print >> stream, "\t\tif oobj is None:"
        print >> stream, "\t\t\tself._olecp = None"
        print >> stream, "\t\telse:"
        print >> stream, '\t\t\timport win32com.server.util'
        print >> stream, '\t\t\tfrom win32com.server.policy import EventHandlerPolicy'
        print >> stream, '\t\t\tcpc=oobj._oleobj_.QueryInterface(pythoncom.IID_IConnectionPointContainer)'
        print >> stream, '\t\t\tcp=cpc.FindConnectionPoint(self.CLSID_Sink)'
        print >> stream, '\t\t\tcookie=cp.Advise(win32com.server.util.wrap(self, usePolicy=EventHandlerPolicy))'
        print >> stream, '\t\t\tself._olecp,self._olecp_cookie = cp,cookie'
        print >> stream, '\tdef __del__(self):'
        print >> stream, '\t\ttry:'
        print >> stream, '\t\t\tself.close()'
        print >> stream, '\t\texcept pythoncom.com_error:'
        print >> stream, '\t\t\tpass'
        print >> stream, '\tdef close(self):'
        print >> stream, '\t\tif self._olecp is not None:'
        print >> stream, '\t\t\tcp,cookie,self._olecp,self._olecp_cookie = self._olecp,self._olecp_cookie,None,None'
        print >> stream, '\t\t\tcp.Unadvise(cookie)'
        print >> stream, '\tdef _query_interface_(self, iid):'
        print >> stream, '\t\timport win32com.server.util'
        print >> stream, '\t\tif iid==self.CLSID_Sink: return win32com.server.util.wrap(self)'
        print >> stream
        self.bWritten = 1

    def WriteCallbackClassBody(self, generator):
        stream = generator.file
        print >> stream, "\t# Event Handlers"
        print >> stream, "\t# If you create handlers, they should have the following prototypes:"
        for name, entry in self.propMapGet.items() + self.propMapPut.items() + self.mapFuncs.items():
            fdesc = entry.desc
            methName = MakeEventMethodName(entry.names[0])
            print >> stream, '#\tdef ' + methName + '(self' + build.BuildCallList(fdesc, entry.names, "defaultNamedOptArg", "defaultNamedNotOptArg","defaultUnnamedArg", "pythoncom.Missing", is_comment = True) + '):'
            if entry.doc and entry.doc[1]:
                print >> stream, '#\t\t' + build._safeQuotedString(entry.doc[1])
        print >> stream
        self.bWritten = 1

    def WriteClassBody(self, generator):
        stream = generator.file
        # Write in alpha order.
        names = self.mapFuncs.keys()
        names.sort()
        specialItems = {"count":None, "item":None,"value":None,"_newenum":None} # If found, will end up with (entry, invoke_tupe)
        itemCount = None
        for name in names:
            entry=self.mapFuncs[name]
            # skip [restricted] methods, unless it is the
            # enumerator (which, being part of the "system",
            # we know about and can use)
            dispid = entry.desc[0]
            if entry.desc[9] & pythoncom.FUNCFLAG_FRESTRICTED and \
                dispid != pythoncom.DISPID_NEWENUM:
                continue
            # If not accessible via IDispatch, then we can't use it here.
            if entry.desc[3] != pythoncom.FUNC_DISPATCH:
                continue
            if dispid==pythoncom.DISPID_VALUE:
                lkey = "value"
            elif dispid==pythoncom.DISPID_NEWENUM:
                specialItems["_newenum"] = (entry, entry.desc[4], None)
                continue # Dont build this one now!
            else:
                lkey = string.lower(name)
            if specialItems.has_key(lkey) and specialItems[lkey] is None: # remember if a special one.
                specialItems[lkey] = (entry, entry.desc[4], None)
            if generator.bBuildHidden or not entry.hidden:
                if entry.GetResultName():
                    print >> stream, '\t# Result is of type ' + entry.GetResultName()
                if entry.wasProperty:
                    print >> stream, '\t# The method %s is actually a property, but must be used as a method to correctly pass the arguments' % name
                ret = self.MakeFuncMethod(entry,build.MakePublicAttributeName(name))
                for line in ret:
                    print >> stream, line
        print >> stream, "\t_prop_map_get_ = {"
        names = self.propMap.keys(); names.sort()
        for key in names:
            entry = self.propMap[key]
            if generator.bBuildHidden or not entry.hidden:
                resultName = entry.GetResultName()
                if resultName:
                    print >> stream, "\t\t# Property '%s' is an object of type '%s'" % (key, resultName)
                lkey = string.lower(key)
                details = entry.desc
                resultDesc = details[2]
                argDesc = ()
                mapEntry = MakeMapLineEntry(details[0], pythoncom.DISPATCH_PROPERTYGET, resultDesc, argDesc, key, entry.GetResultCLSIDStr())
            
                if entry.desc[0]==pythoncom.DISPID_VALUE:
                    lkey = "value"
                elif entry.desc[0]==pythoncom.DISPID_NEWENUM:
                    lkey = "_newenum"
                else:
                    lkey = string.lower(key)
                if specialItems.has_key(lkey) and specialItems[lkey] is None: # remember if a special one.
                    specialItems[lkey] = (entry, pythoncom.DISPATCH_PROPERTYGET, mapEntry)
                    # All special methods, except _newenum, are written
                    # "normally".  This is a mess!
                    if entry.desc[0]==pythoncom.DISPID_NEWENUM:
                        continue 

                print >> stream, '\t\t"%s": %s,' % (build.MakePublicAttributeName(key), mapEntry)
        names = self.propMapGet.keys(); names.sort()
        for key in names:
            entry = self.propMapGet[key]
            if generator.bBuildHidden or not entry.hidden:
                if entry.GetResultName():
                    print >> stream, "\t\t# Method '%s' returns object of type '%s'" % (key, entry.GetResultName())
                details = entry.desc
                lkey = string.lower(key)
                argDesc = details[2]
                resultDesc = details[8]
                mapEntry = MakeMapLineEntry(details[0], pythoncom.DISPATCH_PROPERTYGET, resultDesc, argDesc, key, entry.GetResultCLSIDStr())
                if entry.desc[0]==pythoncom.DISPID_VALUE:
                    lkey = "value"
                elif entry.desc[0]==pythoncom.DISPID_NEWENUM:
                    lkey = "_newenum"
                else:
                    lkey = string.lower(key)
                if specialItems.has_key(lkey) and specialItems[lkey] is None: # remember if a special one.
                    specialItems[lkey]=(entry, pythoncom.DISPATCH_PROPERTYGET, mapEntry)
                    # All special methods, except _newenum, are written
                    # "normally".  This is a mess!
                    if entry.desc[0]==pythoncom.DISPID_NEWENUM:
                        continue 
                print >> stream, '\t\t"%s": %s,' % (build.MakePublicAttributeName(key), mapEntry)

        print >> stream, "\t}"

        print >> stream, "\t_prop_map_put_ = {"
        # These are "Invoke" args
        names = self.propMap.keys(); names.sort()
        for key in names:
            entry = self.propMap[key]
            if generator.bBuildHidden or not entry.hidden:
                lkey=string.lower(key)
                details = entry.desc
                # If default arg is None, write an empty tuple
                defArgDesc = build.MakeDefaultArgRepr(details[2])
                if defArgDesc is None:
                    defArgDesc = ""
                else:
                    defArgDesc = defArgDesc + ","
                print >> stream, '\t\t"%s" : ((%s, LCID, %d, 0),(%s)),' % (build.MakePublicAttributeName(key), details[0], pythoncom.DISPATCH_PROPERTYPUT, defArgDesc)

        names = self.propMapPut.keys(); names.sort()
        for key in names:
            entry = self.propMapPut[key]
            if generator.bBuildHidden or not entry.hidden:
                details = entry.desc
                defArgDesc = MakeDefaultArgsForPropertyPut(details[2])
                print >> stream, '\t\t"%s": ((%s, LCID, %d, 0),%s),' % (build.MakePublicAttributeName(key), details[0], details[4], defArgDesc)
        print >> stream, "\t}"
        
        if specialItems["value"]:
            entry, invoketype, propArgs = specialItems["value"]
            if propArgs is None:
                typename = "method"
                ret = self.MakeFuncMethod(entry,'__call__')
            else:
                typename = "property"
                ret = [ "\tdef __call__(self):\n\t\treturn self._ApplyTypes_(*%s)" % propArgs]
            print >> stream, "\t# Default %s for this class is '%s'" % (typename, entry.names[0])
            for line in ret:
                print >> stream, line
            print >> stream, "\t# str(ob) and int(ob) will use __call__"
            print >> stream, "\tdef __unicode__(self, *args):"
            print >> stream, "\t\ttry:"
            print >> stream, "\t\t\treturn unicode(self.__call__(*args))"
            print >> stream, "\t\texcept pythoncom.com_error:"
            print >> stream, "\t\t\treturn repr(self)"
            print >> stream, "\tdef __str__(self, *args):"
            print >> stream, "\t\treturn str(self.__unicode__(*args))"
            print >> stream, "\tdef __int__(self, *args):"
            print >> stream, "\t\treturn int(self.__call__(*args))"
            

        if specialItems["_newenum"]:
            enumEntry, invoketype, propArgs = specialItems["_newenum"]
            resultCLSID = enumEntry.GetResultCLSIDStr()
            # If we dont have a good CLSID for the enum result, assume it is the same as the Item() method.
            if resultCLSID == "None" and self.mapFuncs.has_key("Item"):
                resultCLSID = self.mapFuncs["Item"].GetResultCLSIDStr()
            # "Native" Python iterator support
            print >> stream, '\tdef __iter__(self):'
            print >> stream, '\t\t"Return a Python iterator for this object"'
            print >> stream, '\t\tob = self._oleobj_.InvokeTypes(%d,LCID,%d,(13, 10),())' % (pythoncom.DISPID_NEWENUM, enumEntry.desc[4])
            print >> stream, '\t\treturn win32com.client.util.Iterator(ob, %s)' % resultCLSID
            # And 'old style' iterator support - magically used to simulate iterators
            # before Python grew them
            print >> stream, '\tdef _NewEnum(self):'
            print >> stream, '\t\t"Create an enumerator from this object"'
            print >> stream, '\t\treturn win32com.client.util.WrapEnum(self._oleobj_.InvokeTypes(%d,LCID,%d,(13, 10),()),%s)' % (pythoncom.DISPID_NEWENUM, enumEntry.desc[4], resultCLSID)
            print >> stream, '\tdef __getitem__(self, index):'
            print >> stream, '\t\t"Allow this class to be accessed as a collection"'
            print >> stream, "\t\tif not self.__dict__.has_key('_enum_'):"
            print >> stream, "\t\t\tself.__dict__['_enum_'] = self._NewEnum()"
            print >> stream, "\t\treturn self._enum_.__getitem__(index)"
        else: # Not an Enumerator, but may be an "Item/Count" based collection
            if specialItems["item"]:
                entry, invoketype, propArgs = specialItems["item"]
                print >> stream, '\t#This class has Item property/method which may take args - allow indexed access'
                print >> stream, '\tdef __getitem__(self, item):'
                print >> stream, '\t\treturn self._get_good_object_(self._oleobj_.Invoke(*(%d, LCID, %d, 1, item)), "Item")' % (entry.desc[0], invoketype)
        if specialItems["count"]:
            entry, invoketype, propArgs = specialItems["count"]
            if propArgs is None:
                typename = "method"
                ret = self.MakeFuncMethod(entry,'__len__')
            else:
                typename = "property"
                ret = [ "\tdef __len__(self):\n\t\treturn self._ApplyTypes_(*%s)" % propArgs]
            print >> stream, "\t#This class has Count() %s - allow len(ob) to provide this" % (typename)
            for line in ret:
                print >> stream, line
            # Also include a __nonzero__
            print >> stream, "\t#This class has a __len__ - this is needed so 'if object:' always returns TRUE."
            print >> stream, "\tdef __nonzero__(self):"
            print >> stream, "\t\treturn %s" % (TrueRepr,)

class CoClassItem(build.OleItem, WritableItem):
  order = 5
  typename = "COCLASS"

  def __init__(self, typeinfo, attr, doc=None, sources = [], interfaces = [], bForUser=1):
    build.OleItem.__init__(self, doc)
    self.clsid = attr[0]
    self.sources = sources
    self.interfaces = interfaces
    self.bIsDispatch = 1 # Pretend it is so it is written to the class map.

  def WriteClass(self, generator):
    generator.checkWriteCoClassBaseClass()
    doc = self.doc
    stream = generator.file
    if generator.generate_type == GEN_DEMAND_CHILD:
      # Some special imports we must setup.
      referenced_items = []
      for ref, flag in self.sources:
        referenced_items.append(ref)
      for ref, flag in self.interfaces:
        referenced_items.append(ref)
      print >> stream, "import sys"
      for ref in referenced_items:
        print >> stream, "__import__('%s.%s')" % (generator.base_mod_name, ref.python_name)
        print >> stream, "%s = sys.modules['%s.%s'].%s" % (ref.python_name, generator.base_mod_name, ref.python_name, ref.python_name)
        # And pretend we have written it - the name is now available as if we had!
        ref.bWritten = 1
    try:
      progId = pythoncom.ProgIDFromCLSID(self.clsid)
      print >> stream, "# This CoClass is known by the name '%s'" % (progId)
    except pythoncom.com_error:
      pass
    print >> stream, 'class %s(CoClassBaseClass): # A CoClass' % (self.python_name)
    if doc and doc[1]: print >> stream, '\t# ' + doc[1]
    print >> stream, '\tCLSID = %r' % (self.clsid,)
    print >> stream, '\tcoclass_sources = ['
    defItem = None
    for item, flag in self.sources:
      if flag & pythoncom.IMPLTYPEFLAG_FDEFAULT:
        defItem = item
      # If we have written a Python class, reference the name - 
      # otherwise just the IID.
      if item.bWritten: key = item.python_name
      else: key = repr(str(item.clsid)) # really the iid.
      print >> stream, '\t\t%s,' % (key)
    print >> stream, '\t]'
    if defItem:
      if defItem.bWritten: defName = defItem.python_name
      else: defName = repr(str(defItem.clsid)) # really the iid.
      print >> stream, '\tdefault_source = %s' % (defName,)
    print >> stream, '\tcoclass_interfaces = ['
    defItem = None
    for item, flag in self.interfaces:
      if flag & pythoncom.IMPLTYPEFLAG_FDEFAULT: # and dual:
        defItem = item
      # If we have written a class, refeence its name, otherwise the IID
      if item.bWritten: key = item.python_name
      else: key = repr(str(item.clsid)) # really the iid.
      print >> stream, '\t\t%s,' % (key,)
    print >> stream, '\t]'
    if defItem:
      if defItem.bWritten: defName = defItem.python_name
      else: defName = repr(str(defItem.clsid)) # really the iid.
      print >> stream, '\tdefault_interface = %s' % (defName,)
    self.bWritten = 1
    print >> stream

class GeneratorProgress:
    def __init__(self):
        pass
    def Starting(self, tlb_desc):
        """Called when the process starts.
        """
        self.tlb_desc = tlb_desc
    def Finished(self):
        """Called when the process is complete.
        """
    def SetDescription(self, desc, maxticks = None):
        """We are entering a major step.  If maxticks, then this
        is how many ticks we expect to make until finished
        """
    def Tick(self, desc = None):
        """Minor progress step.  Can provide new description if necessary
        """
    def VerboseProgress(self, desc):
        """Verbose/Debugging output.
        """
    def LogWarning(self, desc):
        """If a warning is generated
        """
    def LogBeginGenerate(self, filename):
        pass
    def Close(self):
        pass

class Generator:
  def __init__(self, typelib, sourceFilename, progressObject, bBuildHidden=1, bUnicodeToString=0):
    self.bHaveWrittenDispatchBaseClass = 0
    self.bHaveWrittenCoClassBaseClass = 0
    self.bHaveWrittenEventBaseClass = 0
    self.typelib = typelib
    self.sourceFilename = sourceFilename
    self.bBuildHidden = bBuildHidden
    self.bUnicodeToString = bUnicodeToString
    self.progress = progressObject
    # These 2 are later additions and most of the code still 'print's...
    self.file = None

  def CollectOleItemInfosFromType(self):
    ret = []
    for i in xrange(self.typelib.GetTypeInfoCount()):
      info = self.typelib.GetTypeInfo(i)
      infotype = self.typelib.GetTypeInfoType(i)
      doc = self.typelib.GetDocumentation(i)
      attr = info.GetTypeAttr()
      ret.append((info, infotype, doc, attr))
    return ret

  def _Build_CoClass(self, type_info_tuple):
    info, infotype, doc, attr = type_info_tuple
    # find the source and dispinterfaces for the coclass
    child_infos = []
    for j in range(attr[8]):
      flags = info.GetImplTypeFlags(j)
      try:
        refType = info.GetRefTypeInfo(info.GetRefTypeOfImplType(j))
      except pythoncom.com_error:
        # Can't load a dependent typelib?
        continue
      refAttr = refType.GetTypeAttr()
      child_infos.append( (info, refAttr.typekind, refType, refType.GetDocumentation(-1), refAttr, flags) )
      
    # Done generating children - now the CoClass itself.
    newItem = CoClassItem(info, attr, doc)
    return newItem, child_infos

  def _Build_CoClassChildren(self, coclass, coclass_info, oleItems, vtableItems):
    sources = {}
    interfaces = {}
    for info, info_type, refType, doc, refAttr, flags in coclass_info:
#          sys.stderr.write("Attr typeflags for coclass referenced object %s=%d (%d), typekind=%d\n" % (name, refAttr.wTypeFlags, refAttr.wTypeFlags & pythoncom.TYPEFLAG_FDUAL,refAttr.typekind))
        if refAttr.typekind == pythoncom.TKIND_DISPATCH or \
           (refAttr.typekind == pythoncom.TKIND_INTERFACE and refAttr[11] & pythoncom.TYPEFLAG_FDISPATCHABLE):
          clsid = refAttr[0]
          if oleItems.has_key(clsid):
            dispItem = oleItems[clsid]
          else:
            dispItem = DispatchItem(refType, refAttr, doc)
            oleItems[dispItem.clsid] = dispItem
          dispItem.coclass_clsid = coclass.clsid
          if flags & pythoncom.IMPLTYPEFLAG_FSOURCE:
            dispItem.bIsSink = 1
            sources[dispItem.clsid] = (dispItem, flags)
          else:
            interfaces[dispItem.clsid] = (dispItem, flags)
          # If dual interface, make do that too.
          if not vtableItems.has_key(clsid) and refAttr[11] & pythoncom.TYPEFLAG_FDUAL:
            refType = refType.GetRefTypeInfo(refType.GetRefTypeOfImplType(-1))
            refAttr = refType.GetTypeAttr()
            assert refAttr.typekind == pythoncom.TKIND_INTERFACE, "must be interface bynow!"
            vtableItem = VTableItem(refType, refAttr, doc)
            vtableItems[clsid] = vtableItem
    coclass.sources = sources.values()
    coclass.interfaces = interfaces.values()

  def _Build_Interface(self, type_info_tuple):
    info, infotype, doc, attr = type_info_tuple
    oleItem = vtableItem = None
    if infotype == pythoncom.TKIND_DISPATCH or \
       (infotype == pythoncom.TKIND_INTERFACE and attr[11] & pythoncom.TYPEFLAG_FDISPATCHABLE):
        oleItem = DispatchItem(info, attr, doc)
        # If this DISPATCH interface dual, then build that too.
        if (attr.wTypeFlags & pythoncom.TYPEFLAG_FDUAL):
            # Get the vtable interface
            refhtype = info.GetRefTypeOfImplType(-1)
            info = info.GetRefTypeInfo(refhtype)
            attr = info.GetTypeAttr()
            infotype = pythoncom.TKIND_INTERFACE
        else:
            infotype = None
    assert infotype in [None, pythoncom.TKIND_INTERFACE], "Must be a real interface at this point"
    if infotype == pythoncom.TKIND_INTERFACE:
        vtableItem = VTableItem(info, attr, doc)
    return oleItem, vtableItem

  def BuildOleItemsFromType(self):
    assert self.bBuildHidden, "This code doesnt look at the hidden flag - I thought everyone set it true!?!?!"
    oleItems = {}
    enumItems = {}
    recordItems = {}
    vtableItems = {}
    
    for type_info_tuple in self.CollectOleItemInfosFromType():
      info, infotype, doc, attr = type_info_tuple
      clsid = attr[0]
      if infotype == pythoncom.TKIND_ENUM or infotype == pythoncom.TKIND_MODULE:
        newItem = EnumerationItem(info, attr, doc)
        enumItems[newItem.doc[0]] = newItem
      # We never hide interfaces (MSAccess, for example, nominates interfaces as
      # hidden, assuming that you only ever use them via the CoClass)
      elif infotype in [pythoncom.TKIND_DISPATCH, pythoncom.TKIND_INTERFACE]:
        if not oleItems.has_key(clsid):
          oleItem, vtableItem = self._Build_Interface(type_info_tuple)
          oleItems[clsid] = oleItem # Even "None" goes in here.
          if vtableItem is not None:
              vtableItems[clsid] = vtableItem
      elif infotype == pythoncom.TKIND_RECORD or infotype == pythoncom.TKIND_UNION:
        newItem = RecordItem(info, attr, doc)
        recordItems[newItem.clsid] = newItem
      elif infotype == pythoncom.TKIND_ALIAS:
        # We dont care about alias' - handled intrinsicly.
        continue
      elif infotype == pythoncom.TKIND_COCLASS:
        newItem, child_infos = self._Build_CoClass(type_info_tuple)
        self._Build_CoClassChildren(newItem, child_infos, oleItems, vtableItems)
        oleItems[newItem.clsid] = newItem
      else:
        self.progress.LogWarning("Unknown TKIND found: %d" % infotype)
  
    return oleItems, enumItems, recordItems, vtableItems

  def generate(self, file, is_for_demand = 0):
    if is_for_demand:
      self.generate_type = GEN_DEMAND_BASE
    else:
      self.generate_type = GEN_FULL
    self.file = file
    self.do_generate()
    self.file = None
    self.progress.Finished()

  def do_gen_file_header(self):
    la = self.typelib.GetLibAttr()
    moduleDoc = self.typelib.GetDocumentation(-1)
    docDesc = ""
    if moduleDoc[1]:
      docDesc = moduleDoc[1]

    # Reset all the 'per file' state
    self.bHaveWrittenDispatchBaseClass = 0
    self.bHaveWrittenCoClassBaseClass = 0
    self.bHaveWrittenEventBaseClass = 0

    print >> self.file, '# -*- coding: mbcs -*-' # Is this always correct?
    print >> self.file, '# Created by makepy.py version %s' % (makepy_version,)
    print >> self.file, '# By python version %s' % \
                        (sys.version.replace("\n", "-"),)
    if self.sourceFilename:
        print >> self.file, "# From type library '%s'" % (os.path.split(self.sourceFilename)[1],)
    print >> self.file, '# On %s' % time.ctime(time.time())

    print >> self.file, '"""' + docDesc + '"""'

    print >> self.file, 'makepy_version =', `makepy_version`
    try:
        print >> self.file, 'python_version = 0x%x' % (sys.hexversion,)
    except AttributeError:
        print >> self.file, 'python_version = 0x0 # Presumably Python 1.5.2 - 0x0 is not a problem'
    print >> self.file
    print >> self.file, 'import win32com.client.CLSIDToClass, pythoncom'
    print >> self.file, 'import win32com.client.util'
    print >> self.file, 'from pywintypes import IID'
    print >> self.file, 'from win32com.client import Dispatch'
    print >> self.file
    print >> self.file, '# The following 3 lines may need tweaking for the particular server'
    print >> self.file, '# Candidates are pythoncom.Missing, .Empty and .ArgNotFound'
    print >> self.file, 'defaultNamedOptArg=pythoncom.Empty'
    print >> self.file, 'defaultNamedNotOptArg=pythoncom.Empty'
    print >> self.file, 'defaultUnnamedArg=pythoncom.Empty'
    print >> self.file
    print >> self.file, 'CLSID = ' + repr(la[0])
    print >> self.file, 'MajorVersion = ' + str(la[3])
    print >> self.file, 'MinorVersion = ' + str(la[4])
    print >> self.file, 'LibraryFlags = ' + str(la[5])
    print >> self.file, 'LCID = ' + hex(la[1])
    print >> self.file

  def do_generate(self):
    moduleDoc = self.typelib.GetDocumentation(-1)
    stream = self.file
    docDesc = ""
    if moduleDoc[1]:
      docDesc = moduleDoc[1]
    self.progress.Starting(docDesc)
    self.progress.SetDescription("Building definitions from type library...")

    self.do_gen_file_header()

    oleItems, enumItems, recordItems, vtableItems = self.BuildOleItemsFromType()

    self.progress.SetDescription("Generating...", len(oleItems)+len(enumItems)+len(vtableItems))

    # Generate the constants and their support.
    if enumItems:
        print >> stream, "class constants:"
        list = enumItems.values()
        list.sort()
        for oleitem in list:
            oleitem.WriteEnumerationItems(stream)
            self.progress.Tick()
        print >> stream

    if self.generate_type == GEN_FULL:
      list = oleItems.values()
      list = filter(lambda l: l is not None, list)
      list.sort()
      for oleitem in list:
        self.progress.Tick()
        oleitem.WriteClass(self)

      list = vtableItems.values()
      list.sort()
      for oleitem in list:
        self.progress.Tick()
        oleitem.WriteClass(self)
    else:
        self.progress.Tick(len(oleItems)+len(vtableItems))

    print >> stream, 'RecordMap = {'
    list = recordItems.values()
    for record in list:
        if str(record.clsid) == pythoncom.IID_NULL:
            print >> stream, "\t###%s: %s, # Typedef disabled because it doesn't have a non-null GUID" % (`record.doc[0]`, `str(record.clsid)`)
        else:
            print >> stream, "\t%s: %s," % (`record.doc[0]`, `str(record.clsid)`)
    print >> stream, "}"
    print >> stream

    # Write out _all_ my generated CLSID's in the map
    if self.generate_type == GEN_FULL:
      print >> stream, 'CLSIDToClassMap = {'
      for item in oleItems.values():
          if item is not None and item.bWritten:
              print >> stream, "\t'%s' : %s," % (str(item.clsid), item.python_name)
      print >> stream, '}'
      print >> stream, 'CLSIDToPackageMap = {}'
      print >> stream, 'win32com.client.CLSIDToClass.RegisterCLSIDsFromDict( CLSIDToClassMap )'
      print >> stream, "VTablesToPackageMap = {}"
      print >> stream, "VTablesToClassMap = {"
      for item in vtableItems.values():
        print >> stream, "\t'%s' : '%s'," % (item.clsid,item.python_name)
      print >> stream, '}'
      print >> stream

    else:
      print >> stream, 'CLSIDToClassMap = {}'
      print >> stream, 'CLSIDToPackageMap = {'
      for item in oleItems.values():
        if item is not None:
          print >> stream, "\t'%s' : %s," % (str(item.clsid), `item.python_name`)
      print >> stream, '}'
      print >> stream, "VTablesToClassMap = {}"
      print >> stream, "VTablesToPackageMap = {"
      for item in vtableItems.values():
        print >> stream, "\t'%s' : '%s'," % (item.clsid,item.python_name)
      print >> stream, '}'
      print >> stream

    print >> stream
    # Bit of a hack - build a temp map of iteItems + vtableItems - coClasses
    map = {}
    for item in oleItems.values():
        if item is not None and not isinstance(item, CoClassItem):
            map[item.python_name] = item.clsid
    for item in vtableItems.values(): # No nones or CoClasses in this map
        map[item.python_name] = item.clsid
            
    print >> stream, "NamesToIIDMap = {"
    for name, iid in map.items():
        print >> stream, "\t'%s' : '%s'," % (name, iid)
    print >> stream, '}'
    print >> stream

    if enumItems:
      print >> stream, 'win32com.client.constants.__dicts__.append(constants.__dict__)'
    print >> stream

  def generate_child(self, child, dir):
    "Generate a single child.  May force a few children to be built as we generate deps"
    self.generate_type = GEN_DEMAND_CHILD

    la = self.typelib.GetLibAttr()
    lcid = la[1]
    clsid = la[0]
    major=la[3]
    minor=la[4]
    self.base_mod_name = "win32com.gen_py." + str(clsid)[1:-1] + "x%sx%sx%s" % (lcid, major, minor)
    try:
      # Process the type library's CoClass objects, looking for the
      # specified name, or where a child has the specified name.
      # This ensures that all interesting things (including event interfaces)
      # are generated correctly.
      oleItems = {}
      vtableItems = {}
      infos = self.CollectOleItemInfosFromType()
      found = 0
      for type_info_tuple in infos:
        info, infotype, doc, attr = type_info_tuple
        if infotype == pythoncom.TKIND_COCLASS:
            coClassItem, child_infos = self._Build_CoClass(type_info_tuple)
            found = build.MakePublicAttributeName(doc[0])==child
            if not found:
                # OK, check the child interfaces
                for info, info_type, refType, doc, refAttr, flags in child_infos:
                    if build.MakePublicAttributeName(doc[0]) == child:
                        found = 1
                        break
            if found:
                oleItems[coClassItem.clsid] = coClassItem
                self._Build_CoClassChildren(coClassItem, child_infos, oleItems, vtableItems)
                break
      if not found:
        # Doesn't appear in a class defn - look in the interface objects for it
        for type_info_tuple in infos:
          info, infotype, doc, attr = type_info_tuple
          if infotype in [pythoncom.TKIND_INTERFACE, pythoncom.TKIND_DISPATCH]:
            if build.MakePublicAttributeName(doc[0]) == child:
              found = 1
              oleItem, vtableItem = self._Build_Interface(type_info_tuple)
              oleItems[clsid] = oleItem # Even "None" goes in here.
              if vtableItem is not None:
                vtableItems[clsid] = vtableItem
                
      assert found, "Cant find the '%s' interface in the CoClasses, or the interfaces" % (child,)
      # Make a map of iid: dispitem, vtableitem)
      items = {}
      for key, value in oleItems.items():
          items[key] = (value,None)
      for key, value in vtableItems.items():
          existing = items.get(key, None)
          if existing is not None:
              new_val = existing[0], value
          else:
              new_val = None, value
          items[key] = new_val

      self.progress.SetDescription("Generating...", len(items))
      for oleitem, vtableitem in items.values():
        an_item = oleitem or vtableitem
        assert not self.file, "already have a file?"
        self.file = open(os.path.join(dir, an_item.python_name) + ".py", "w")
        try:
          if oleitem is not None:
            self.do_gen_child_item(oleitem)
          if vtableitem is not None:
            self.do_gen_child_item(vtableitem)
          self.progress.Tick()
        finally:
          self.file.close()
          self.file = None
    finally:
      self.progress.Finished()

  def do_gen_child_item(self, oleitem):
    moduleDoc = self.typelib.GetDocumentation(-1)
    docDesc = ""
    if moduleDoc[1]:
      docDesc = moduleDoc[1]
    self.progress.Starting(docDesc)
    self.progress.SetDescription("Building definitions from type library...")
    self.do_gen_file_header()
    oleitem.WriteClass(self)
    if oleitem.bWritten:
        print >> self.file, 'win32com.client.CLSIDToClass.RegisterCLSID( "%s", %s )' % (oleitem.clsid, oleitem.python_name)

  def checkWriteDispatchBaseClass(self):
    if not self.bHaveWrittenDispatchBaseClass:
      print >> self.file, "from win32com.client import DispatchBaseClass"
      self.bHaveWrittenDispatchBaseClass = 1

  def checkWriteCoClassBaseClass(self):
    if not self.bHaveWrittenCoClassBaseClass:
      print >> self.file, "from win32com.client import CoClassBaseClass"
      self.bHaveWrittenCoClassBaseClass = 1

  def checkWriteEventBaseClass(self):
    # Not a base class as such...
      if not self.bHaveWrittenEventBaseClass:
        # Nothing to do any more!
        self.bHaveWrittenEventBaseClass = 1

if __name__=='__main__':
  print "This is a worker module.  Please use makepy to generate Python files."
