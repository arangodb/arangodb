#
# Copy this to some place in your homedirectory, for example
#   ~/.gdb/GdbSlicePrettyPrint.py
# and add the line
#   source ~/.gdb/GdbSlicePrettyPrint.py
# to your ~/.gdbinit file. Also make sure the arangovpack (built with
# ArangoDB is in a directory contained in your path.
# Then you can use
#   p <Slice>
# in gdb sessions.
#

import subprocess

class SlicePrinter (object):
  "Print a VelocyPack Slice prettily."

  def __init__ (self, val):
    self.val = val

  def readUInt(self, raw, size):
    x = 0
    shift = 0
    i = 0
    if size > 0:
      while i < size:
        x += int(raw[i]) << shift
        shift += 8
        i += 1
      return x
    else:
      size = -size
      while i < size:
        x += int(raw[size-i]) << shift
        shift += 8
        i += 1
      return x

  def readVarUInt(self, raw, forwards):
    r = 0
    s = 0
    while True:
      i = int(raw[0])
      r |= ((i & 0x7f) << s)
      s += 7
      if forwards:
        raw += 1
      else:
        raw -= 1
      if i & 0x80 == 0:
        return r

  def findByteLength(self, raw):
    # Raw must be a Value of type uint8_t* pointing to a vpack value
    # This finds the length of the vpack value
    typeByte = int(raw[0])
    if typeByte == 0x00:     # None
      return 0
    if typeByte == 0x01:     # empty array
      return 1
    if typeByte == 0x0a:     # empty object
      return 1
    if typeByte >= 0x02 and typeByte <= 0x05:  # array equal length subobjs
      return self.readUInt(raw+1, typeByte - 0x01)
    if typeByte >= 0x06 and typeByte <= 0x09:  # array
      return self.readUInt(raw+1, typeByte - 0x05)
    if typeByte >= 0x0b and typeByte <= 0x0e:  # object
      return self.readUInt(raw+1, typeByte - 0x0a)
    if typeByte >= 0x13 and typeByte <= 0x14:  # compact array or object
      return self.readVarUInt(raw+1, True)
    if typeByte >= 0x0f and typeByte <= 0x12:  # unused
      return 1
    if typeByte == 0x17:   # illegal
      return 1
    if typeByte >= 0x18 and typeByte <= 0x1a:
      return 1
    if typeByte == 0x1b:   # double
      return 9
    if typeByte == 0x1c:   # UTC-date
      return 9
    if typeByte == 0x1d:   # external
      return 0
    if typeByte >= 0x1e and typeByte <= 0x1f:  # minKey, maxKey
      return 1
    if typeByte >= 0x20 and typeByte <= 0x27:  # int
      return 1 + typeByte - 0x1f
    if typeByte >= 0x28 and typeByte <= 0x2f:  # uint
      return 1 + typeByte - 0x27
    if typeByte >= 0x30 and typeByte <= 0x3f:  # small int
      return 1
    if typeByte >= 0x40 and typeByte <= 0xbe:  # short string
      return 1 + typeByte - 0x40
    if typeByte == 0xbf:                       # long string
      return 9 + self.readUInt(raw+1, 8)
    if typeByte >= 0xc0 and typeByte <= 0xc7:  # binary blob
      return 1 + typeByte - 0xbf + self.readUInt(raw+1, typeByte - 0xbf)
    if typeByte >= 0xc8 and typeByte <= 0xcf:  # positive BCD integer
      return 1 + typeByte - 0xc7 + 4 + self.readUInt(raw+1, typeByte - 0xc7)
    if typeByte >= 0xd0 and typeByte <= 0xd7:  # negative BCD integer
      return 1 + typeByte - 0xcf + 4 + self.readUInt(raw+1, typeByte - 0xcf)
    if typeByte >= 0xd8 and typeByte <= 0xef:  # reserved
      return 1
    if typeByte == 0xf0:   # Custom types
      return 1
    if typeByte == 0xf1:
      return 2
    if typeByte == 0xf2:
      return 4
    if typeByte == 0xf3:
      return 8
    if typeByte >= 0xf4 and typeByte <= 0xf6:
      return 2 + self.readUInt(raw + 1, 1)
    if typeByte >= 0xf7 and typeByte <= 0xf9:
      return 2 + self.readUInt(raw + 1, 2)
    if typeByte >= 0xfa and typeByte <= 0xfc:
      return 2 + self.readUInt(raw + 1, 4)
    if typeByte >= 0xfd and typeByte <= 0xff:
      return 2 + self.readUInt(raw + 1, 8)
    return 4712

  def to_string(self):
    vpack = self.val["_start"]    # uint8_t*
    length = self.findByteLength(vpack)
    if length == 0:
      return "External" if int(vpack[0]) == 0x1d else "NoneSlice"
    b = bytearray(length)
    for i in range(0, length):
      b[i] = int(vpack[i])
    p = subprocess.Popen(["arangovpack", "--print-non-json"], 
                                         stdin=subprocess.PIPE,
                                         stdout=subprocess.PIPE)
    s, e = p.communicate(b)
    return s.decode("utf-8")
     
def str_lookup_function(val):
  lookup_type = val.type
  if lookup_type.tag == "arangodb::velocypack::Slice" or \
     lookup_type == "arangodb::velocypack::Slice":
    return SlicePrinter(val)
  if str(lookup_type).find("velocypack::Slice") >= 0 or \
     str(lookup_type).find("VPackSlice") >= 0:
    return SlicePrinter(val)
  return None

gdb.pretty_printers.append(str_lookup_function)
