##
##	   Copyright (c) 2000, 2001, 2002 Thomas Heller
##
## Permission is hereby granted, free of charge, to any person obtaining
## a copy of this software and associated documentation files (the
## "Software"), to deal in the Software without restriction, including
## without limitation the rights to use, copy, modify, merge, publish,
## distribute, sublicense, and/or sell copies of the Software, and to
## permit persons to whom the Software is furnished to do so, subject to
## the following conditions:
##
## The above copyright notice and this permission notice shall be
## included in all copies or substantial portions of the Software.
##
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
## EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
## MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
## NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
## LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
## OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
## WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
##

# String Tables.
# See Knowledge Base, Q200893
#
#
# $Id: StringTables.py 380 2004-01-16 10:47:02Z theller $
#
# $Log$
# Revision 1.1  2003/08/29 12:30:52  mhammond
# New py2exe now uses the old resource functions :)
#
# Revision 1.1  2002/01/29 09:30:55  theller
# version 0.3.0
#
#
#

RT_STRING = 6 # win32 resource type

_use_unicode = 0

try:
    _use_unicode = unicode
except NameError:
    try:
        import pywintypes
    except ImportError:
        raise ImportError, "Could not import StringTables, no unicode available"
        
if _use_unicode:

    def w32_uc(text):
        """convert a string into unicode, then encode it into UTF-16
        little endian, ready to use for win32 apis"""
        return unicode(text, "unicode-escape").encode("utf-16-le")

else:
    def w32_uc(text):
        return pywintypes.Unicode(text).raw

class StringTable:
    
    """Collects (id, string) pairs and allows to build Win32
    StringTable resources from them."""

    def __init__(self):
        self.strings = {}
        
    def add_string(self, id, text):
        self.strings[id] = text

    def sections(self):
        ids = self.strings.keys()
        ids.sort()
        sections = {}
        
        for id in ids:
            sectnum = id / 16 + 1
## Sigh. 1.5 doesn't have setdefault!
##            table = sections.setdefault(sectnum, {})
            table = sections.get(sectnum)
            if table is None:
                table = sections[sectnum] = {}
            table[id % 16] = self.strings[id]

        return sections

    def binary(self):
        import struct
        sections = []
        for key, sect in self.sections().items():
            data = ""
            for i in range(16):
                ustr = w32_uc(sect.get(i, ""))
                fmt = "h%ds" % len(ustr)
                data = data + struct.pack(fmt, len(ustr)/2, ustr)
            sections.append((key, data))
        return sections
            

if __name__ == '__main__':
    st = StringTable()
    st.add_string(32, "Hallo")
    st.add_string(33, "Hallo1")
    st.add_string(34, "Hallo2")
    st.add_string(35, "Hallo3")
    st.add_string(1023, "__service__.VCULogService")
    st.add_string(1024, "__service__.VCULogService")
    st.add_string(1025, "__service__.VCULogService")
    st.add_string(1026, "__service__.VCULogService")

    import sys
    sys.path.append("c:/tmp")
    from hexdump import hexdump

    for sectnum, data in st.binary():
        print "ID", sectnum
        hexdump(data)
