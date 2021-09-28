VelocyPack (VPack)
==================

    Version 1

VelocyPack (VPack) is a fast and compact serialization format


## Generalities

VPack is (unsigned) byte oriented, so VPack values are simply sequences
of bytes and are platform independent. Values are not necessarily
aligned, so all access to larger subvalues must be properly organised to
avoid alignment assumptions of the CPU.


## Value types

We describe a single VPack value, which is recursive in nature, but
resides (with two exceptions, see below) in a single contiguous block of
memory. Assume that the value starts at address A, the first byte V
indicates the type (and often the length) of the VPack value at hand:

We first give an overview with a brief but accurate description for
reference, for arrays and objects see below for details:

  - `0x00`        : none - this indicates absence of any type and value,
                    this is not allowed in VPack values
  - `0x01`        : empty array
  - `0x02`        : array without index table (all subitems have the same
                    byte length), 1-byte byte length
  - `0x03`        : array without index table (all subitems have the same
                    byte length), 2-byte byte length
  - `0x04`        : array without index table (all subitems have the same
                    byte length), 4-byte byte length
  - `0x05`        : array without index table (all subitems have the same
                    byte length), 8-byte byte length
  - `0x06`        : array with 1-byte index table offsets, bytelen and # subvals
  - `0x07`        : array with 2-byte index table offsets, bytelen and # subvals
  - `0x08`        : array with 4-byte index table offsets, bytelen and # subvals
  - `0x09`        : array with 8-byte index table offsets, bytelen and # subvals
  - `0x0a`        : empty object
  - `0x0b`        : object with 1-byte index table offsets, sorted by
                    attribute name, 1-byte bytelen and # subvals
  - `0x0c`        : object with 2-byte index table offsets, sorted by
                    attribute name, 2-byte bytelen and # subvals
  - `0x0d`        : object with 4-byte index table offsets, sorted by
                    attribute name, 4-byte bytelen and # subvals
  - `0x0e`        : object with 8-byte index table offsets, sorted by
                    attribute name, 8-byte bytelen and # subvals
  - `0x0f`        : object with 1-byte index table offsets, not sorted by
                    attribute name, 1-byte bytelen and # subvals
  - `0x10`        : object with 2-byte index table offsets, not sorted by
                    attribute name, 2-byte bytelen and # subvals
  - `0x11`        : object with 4-byte index table offsets, not sorted by
                    attribute name, 4-byte bytelen and # subvals
  - `0x12`        : object with 8-byte index table offsets, not sorted by
                    attribute name, 8-byte bytelen and # subvals
  - `0x13`        : compact array, no index table
  - `0x14`        : compact object, no index table
  - `0x15`-`0x16` : reserved
  - `0x17`        : illegal - this type can be used to indicate a value that
                    is illegal in the embedding application
  - `0x18`        : null
  - `0x19`        : false
  - `0x1a`        : true
  - `0x1b`        : double IEEE-754, 8 bytes follow, stored as little
                    endian uint64 equivalent
  - `0x1c`        : UTC-date in milliseconds since the epoch, stored as 8 byte
                    signed int, little endian, two's complement
  - `0x1d`        : external (only in memory): a char* pointing to the actual
                    place in memory, where another VPack item resides, not
                    allowed in VPack values on disk or on the network
  - `0x1e`        : minKey, nonsensical value that compares < than all other values
  - `0x1f`        : maxKey, nonsensical value that compares > than all other values
  - `0x20`-`0x27` : signed int, little endian, 1 to 8 bytes, number is V - `0x1f`,
                    two's complement
  - `0x28`-`0x2f` : uint, little endian, 1 to 8 bytes, number is V - `0x27`
  - `0x30`-`0x39` : small integers 0, 1, ... 9
  - `0x3a`-`0x3f` : small negative integers -6, -5, ..., -1
  - `0x40`-`0xbe` : UTF-8-string, using V - `0x40` bytes (not Unicode characters!),
                    length 0 is possible, so `0x40` is the empty string,
                    maximal length is 126, note that strings here are not
                    zero-terminated and may contain NUL bytes
  - `0xbf`        : long UTF-8-string, next 8 bytes are length of string in
                    bytes (not Unicode characters) as little endian unsigned
                    integer, note that long strings are not zero-terminated
                    and may contain NUL bytes
  - `0xc0`-`0xc7` : binary blob, next V - `0xbf` bytes are the length of blob in
                    bytes, note that binary blobs are not zero-terminated
  - `0xc8`-`0xcf` : positive long packed BCD-encoded float, V - `0xc7` bytes follow
                    that encode in a little endian way the length of the
                    mantissa in bytes. Directly after that follow 4 bytes
                    encoding the (power of 10) exponent, by which the mantissa
                    is to be multiplied, stored as little endian two's
                    complement signed 32-bit integer. After that, as many
                    bytes follow as the length information at the beginning
                    has specified, each byte encodes two digits in
                    big-endian packed BCD.
                    Example: 12345 decimal can be encoded as
                    `c8 03 00 00 00 00 01 23 45` or
                    `c8 03 ff ff ff ff 12 34 50`
  - `0xd0`-`0xd7` : negative long packed BCD-encoded float, V - `0xcf` bytes
                    follow that encode in a little endian way the length of
                    the mantissa in bytes. After that, same as positive long
                    packed BCD-encoded float above.
  - `0xd8`-`0xed` : reserved
  - `0xee`-`0xef` : value tagging for logical types
  - `0xf0`-`0xff` : custom types


## Arrays

Empty arrays are simply a single byte `0x01`.

We next describe the type cases `0x02` to `0x09`, see below for the
special compact type `0x13`.

Non-empty arrays look like one of the following:

    one of 0x02 to 0x05
    BYTELENGTH
    OPTIONAL UNUSED: padding
    sub VPack values

or

    0x06
    BYTELENGTH in 1 byte
    NRITEMS in 1 byte
    OPTIONAL UNUSED: 6 bytes of padding
    sub VPack values
    INDEXTABLE with 1 byte per entry

or

    0x07
    BYTELENGTH in 2 bytes
    NRITEMS in 2 bytes
    OPTIONAL UNUSED: 4 bytes of padding
    sub VPack values
    INDEXTABLE with 4 byte per entry

or

    0x08
    BYTELENGTH in 4 bytes
    NRITEMS in 4 bytes
    sub VPack values
    INDEXTABLE with 4 byte per entry

or

    0x09
    BYTELENGTH in 8 bytes
    sub VPack values
    INDEXTABLE with 8 byte per entry
    NRITEMS in 8 bytes

If any optional padding is allowed for a type, the padding must consist
of exactly that many bytes that the length of the padding, the length of
BYTELENGTH and the length of NRITEMS (if present) sums up to 8. If the
length of BYTELENGTH is already 8, there is no padding allowed. The
entire padding must consist of zero bytes (ASCII NUL). 

Numbers (for byte length, number of subvalues and offsets in the
INDEXTABLE) are little endian unsigned integers, using 1 byte for
types `0x02` and `0x06`, 2 bytes for types `0x03` and `0x07`, 4 bytes for types
`0x04` and `0x08`, and 8 bytes for types `0x05` and `0x09`.

NRITEMS is a single number as described above.

The INDEXTABLE consists of:
  - for types `0x06`-`0x09` an array of offsets (unaligned, in the number
    format described above) earlier offsets reside at lower addresses.
    Offsets are measured from the start of the VPack value.


Non-empty arrays of types `0x06` to `0x09` have a small header including
their byte length, the number of subvalues, then all the subvalues and
finally an index table containing offsets to the subvalues. To find the
index table, find the number of subvalues, then the end, and from that
the base of the index table, considering how wide its entries are.

For types `0x02` to `0x05` there is no offset table and no number of items.
The first item begins at address A+2, A+3, A+5 or respectively A+9,
depending on the type and thus the width of the byte length field. Note
the following special rule: The actual position of the first subvalue
is allowed to be further back, after some run of padding zero bytes.

For example, if 2 bytes are used for both the byte length (BYTELENGTH),
then an optional padding of 4 zero bytes is then allowed to follow, and
the actual VPack subvalues can start at A+9. 
This is to give a program that builds a VPack value the opportunity to
reserve 8 bytes in the beginning and only later find out that fewer bytes
suffice to write the byte length. One can determine the number of
subvalues by finding the first subvalue, its byte length, and
dividing the amount of available space by it.

For types `0x06` to `0x09` the offset table describes where the subvalues
reside. It is not necessary for the subvalues to start immediately after
the number of subvalues field. 

As above, it is allowed to include optional padding. Again here, any
padding must consist of a run of consecutive zero bytes (ASCII NUL) and
must be as long that it fills up the length of BYTELENGTH and the length 
of NRITEMS to 8.

For example, if both BYTELENGTH and NRITEMS can be expressed using 2 bytes
each, the sum of their lengths is 4. It is therefore allowed to add 4
bytes of padding here, so that the first subvalue could be at address A+9.

There is one exception for the 8-byte numbers case (type `0x05`): 
In this case the number of elements is moved behind the index table. 
This is to get away without moving memory when one has reserved 8 bytes 
in the beginning and later noticed that all 8 bytes are needed for the
byte length. For this case it is not allowed to include any padding.

All offsets are measured from base A.


*Example*:

`[1,2,3]` has the hex dump

    02 05 31 32 33

in the most compact representation, but the following are equally
possible, though not necessarily advised to use:

*Examples*:

    03 06 00 31 32 33

    04 08 00 00 00 31 32 33

    05 0c 00 00 00 00 00 00 00 31 32 33

    06 09 03 31 32 33 03 04 05

    07 0e 00 03 00 31 32 33 05 00 06 00 07 00

    08 18 00 00 00 03 00 00 00 31 32 33 09 00 00 00 0a 00 00 00 0b 00 00 00

    09
    2c 00 00 00 00 00 00 00
    31 32 33
    09 00 00 00 00 00 00 00
    0a 00 00 00 00 00 00 00
    0b 00 00 00 00 00 00 00
    03 00 00 00 00 00 00 00

Note that it is not recommended to encode short arrays in too long a
format.

We now describe the special type `0x13`, which is useful for a
particularly compact array representation. Note that to some extent this
goes against the principles of the VelocyPack format, since quick access
to subvalues is no longer possible, all items in the array must be
scanned to find a particular one. However, there are certain use cases
for VelocyPack which only require sequential access (for example JSON
dumping) and have a particular need for compactness.

The overall format of this array type is

    0x13 as type byte
    BYTELENGTH
    sub VPack values
    NRITEMS

There is no index table at all, although the sub VelocyPack values can
have different byte sizes. The BYTELENGTH and NRITEMS are encoded in a
special format, which we describe now.

The BYTELENGTH consists of 1 to 8 bytes, of which all but the last one
have their high bit set. Thus, the high bits determine, how many bytes
are actually used. The lower 7 bits of all these bits together comprise
the actual byte length in a little endian fashion. That is, the byte at
address A+1 contains the least significant 7 bits (0 to 6) of the byte length,
the following byte at address A+2 contains the bits 7 to 13, and so on.
Since the total number of bytes is limited to 8, this encodes unsigned
integers of up to 56 bits, which is the overall limit for the size of
such a compact array representation.

The NRITEMS entry is encoded essentially the same, except that it is
laid out in reverse order in memory. That is, one has to use the
BYTELENGTH to find the end of the array value and go back bytes until
one finds a byte with high bit reset. The last byte (at the highest
memory address) contains the least significant 7 bits of the NRITEMS
value, the second one bits 7 to 13 and so on.

Here is an example, the array [1, 16] can be encoded as follows:

    13 06
    31 28 10
    02

## Objects

Empty objects are simply a single byte `0x0a`.

We next describe the type cases `0x0b` to `0x12`, see below for the
special compact type `0x14`.

Non-empty objects look like this:

    one of 0x0b - 0x12
    BYTELENGTH
    optional NRITEMS
    sub VPack values as pairs of attribute and value
    optional INDEXTABLE
    NRITEMS for the 8-byte case

Numbers (for byte length, number of subvalues and offsets in the
INDEXTABLE) are little endian unsigned integers, using 1 byte for
types `0x0b` and `0x0f`, 2 bytes for types `0x0c` and `0x10`, 4 bytes for types
`0x0d` and `0x11`, and 8 bytes for types `0x0e` and `0x12`.

NRITEMS is a single number as described above.

The INDEXTABLE consists of:
  - an array of offsets (unaligned, in the number format described
    above) earlier offsets reside at lower addresses.
    Offsets are measured from the beginning of the VPack value.

Non-empty objects have a small header including their byte length, the
number of subvalues, then all the subvalues and finally an index table
containing offsets to the subvalues. To find the index table, find
number of subvalues, then the end, and from that the base of the index
table, considering how wide its entries are.

For all types the offset table describes where the subvalues reside. It
is not necessary for the subvalues to start immediately after the number
of subvalues field. For performance reasons when building the value, it
could be desirable to reserve 8 bytes for the byte length and the number
of subvalues and not fill the gap, even though it turns out later that
offsets and thus the byte length only uses 2 bytes, say.

There is one special case: the empty object is simply stored as the
single byte `0x0a`.

There is another exception: For 8-byte numbers (`0x12`) the number of
subvalues is stored behind the INDEXTABLE. This is to get away without
moving memory when one has reserved 8 bytes in the beginning and later
noticed that all 8 bytes are needed for the byte length.

All offsets are measured from base A.

Each entry consists of two parts, the key and the value, they are
encoded as normal VPack values as above, the first is always a short or
long UTF-8 string starting with a byte `0x40`-`0xbf` as described below. The
second is any other VPack value.

There is one extension: For the key it is possible to use the positive
small integer values `0x30`-`0x39` or an unsigned integer starting with a
type byte of `0x28`-`0x2f`. Any such integer value is an index into an
outside-given table of attribute names. These are convenient when only
very few attribute names occur or some are repeated very often. The
standard way to encode such an attribute name table is as a VPack array
of strings as specified here.

Objects are always stored with sorted key/value pairs, sorted by bytewise 
comparisons of the keys on each nesting level. Sorting has some overhead 
but will allow looking up keys in logarithmic time later. Note that only the 
index table needs to be sorted, it is not required that the offsets in 
these tables are increasing. Since the index table resides after the actual 
subvalues, one can build up a complex VPack value by writing linearly.

Example: the object `{"a": 12, "b": true, "c": "xyz"}` can have the hexdump:

    0b
    13 03
    41 62 1a
    41 61 28 0c
    41 63 43 78 79 7a
    06 03 0a

The same object could have been done with an index table with longer
entries, as in this example:

    0d
    22 00 00 00
    03 00 00 00
    41 62 1a
    41 61 28 0c
    41 63 43 78 79 7a
    0c 00 00 00 09 00 00 00 10 00 00 00

Similarly with type `0x0c` and 2-byte offsets, byte length and number of
subvalues, or with type `0x0e` and 8-byte numbers.

Note that it is not recommended to encode short objects with too long
index tables.

### Special compact objects

We now describe the special type `0x14`, which is useful for a
particularly compact object representation. Note that to some extent
this goes against the principles of the VelocyPack format, since quick
access to subvalues is no longer possible, all key/value pairs in the
object must be scanned to find a particular one. However, there are
certain use cases for VelocyPack which only require sequential access
(for example JSON dumping) and have a particular need for compactness.

The overall format of this object type is

    0x14 as type byte
    BYTELENGTH
    sub VPack key/value pairs
    NRPAIRS

There is no index table at all, although the sub VelocyPack values can
have different byte sizes. The BYTELENGTH and NRPAIRS are encoded in a
special format, which we describe now. It is the same as for the special
compact array type `0x13`, which we repeat here for the sake of
completeness.

The BYTELENGTH consists of 1 to 8 bytes, of which all but the last one
have their high bit set. Thus, the high bits determine, how many bytes
are actually used. The lower 7 bits of all these bits together comprise
the actual byte length in a little endian fashion. That is, the byte at
address A+1 contains the least significant 7 bits (0 to 6) of the byte
length, the following byte at address A+2 contains the bits 7 to 13, and
so on. Since the total number of bytes is limited to 8, this encodes
unsigned integers of up to 56 bits, which is the overall limit for the
size of such a compact array representation.

The NRPAIRS entry is encoded essentially the same, except that it
is laid out in reverse order in memory. That is, one has to use the
BYTELENGTH to find the end of the array value and go back bytes until
one finds a byte with high bit reset. The last byte (at the highest
memory address) contains the least significant 7 bits of the NRPAIRS
value, the second one bits 7 to 13 and so on.

Here is an example, the object `{"a":1, "b":16}` can be encoded as follows:

    14 0a
    41 61 31 42 62 28 10
    02


## Doubles

Type `0x1b` indicates a double IEEE-754 value using the 8 bytes following
the type byte. To guarantee platform-independentness the details of the
byte order are as follows. Encoding is done by using memcpy to copy the
internal double value to an uint64\_t. This 64-bit unsigned integer is
then stored as little endian 8 byte integer in the VPack value. Decoding
works in the opposite direction. This should sort out the undetermined
byte order in IEEE-754 in practice.


## Dates

Type `0x1c` indicates a signed 64-int integer stored in 8 bytes little
endian two's complement notation directly after the type. The value means
a universal UTC-time measured in milliseconds since the epoch, which is
00:00 on 1 January 1970 UTC.


## External VPack values

This type is only for use within memory, not for data exchange over disk
or network. Therefore, we only need to specify that the following k
bytes are the memcpy of a char* on the current architecture. That char*
points to the actual VPack value elsewhere in memory.


## Artificial minimal and maximal keys

These values of types `0x1e` and `0x1f` have no meaning other than comparing
smaller or greater respectively than any other VPack value. The idea is
that these can be used in systems that define a total order on all VPack
values to specify left or right ends of infinite intervals.


## Integer types

There are different ways to specify integers. For small values -6 to 9
inclusively there are specific type bytes in the range `0x30` to `0x3f` to
allow for storage in a single byte. After that there are signed and
unsigned integer types that can code in the type byte the number of
bytes used (ranges `0x20`-`0x27` for signed and `0x28`-`0x2f` for unsigned).


## Null and boolean values

These three values use a single byte to store the corresponding JSON
values.


## Strings

Strings are stored as UTF-8 encoded byte sequences. There are two
variants, a short one and a long one. In the short one, the byte length
(not the number of UTF-8 characters) is directly encoded in the type,
and this works up to and including byte length 126. Types `0x40` to `0xbe`
are used for this and the byte length is V - `0x3f`, if V is the type
byte. For strings longer than 126 bytes, the type byte is `0xbf` and the
byte length of the string is stored in the first 8 bytes after the type
byte, using a little endian unsigned integer representation. The actual
string follows after these 8 bytes. There is no terminating zero byte in
either case and the string may contain zero bytes.


## Binary data

The type bytes `0xc0` to `0xc7` allow to store arbitrary binary byte
sequences as a VPack value. The format is as follows: If V is the type
byte, then V - `0xbf` bytes follow it to make a little endian unsigned
integer representing the length of the binary data, which directly
follows these length bytes. No alignment is guaranteed. The content is
entirely up to the user.


## Packed BCD long floats

These types are used to represent arbitrary precision decimal numbers.
There are different types for positive and negative numbers. The overall
format of these values is:

    one of 0xc8 - 0xcf (positive) or of 0xd0 - 0xd7 (negative)
    LENGTH OF MANTISSA in bytes
    EXPONENT (as 4-byte little endian signed two's complement integer)
    MANTISSA (as packed BCD-encoded integer, big-endian)

The type byte describes the sign of the number as well as the number of
bytes used to specify the byte length of the mantissa. As usual, if V is
the type byte, then V - `0xc7` (in the positive case) or V - `0xcf` (in the
negative case) bytes are used for the length of the mantissa, stored as
little endian unsigned integer directly after the byte length. After
this follow exactly 4 bytes (little endian signed two's complement
integer) to specify the exponent. After the exponent, the actual
mantissa bytes follow.

Packed BCD is used, so that each byte stores exactly 2 decimal digits as
in `0x34` for the decimal digits 34. Therefore, the mantissa always has an
even number of decimal digits. Note that the mantissa is stored in big
endian form, to make parsing and dumping efficient. This leads to the
"unholy nibble problem": When a JSON parser sees the beginning of a
longish number, it does not know whether an even or odd number of digits
follow. However, for efficiency reasons it wants to start writing bytes
to the output as it reads the input. This is, where the exponent comes
to the rescue, which is illustrated by the following example.
12345 decimal can be encoded as:

    c8 03 00 00 00 00 01 23 45
    c8 03 ff ff ff ff 12 34 50

The former encoding puts a leading 0 in the first byte and uses exponent
0, the latter encoding directly starts putting two decimal digits in one
byte and then in the end has to "erase" the trailing 0 by using exponent
-1, encoded by the 4 byte sequence `ff ff ff ff`.

Therefore, the unholy nibble problem is solved and parsing (and indeed
dumping) can be efficient.


## Tagging

Types `0xee`-`0xef` are used for tagging of values to implement logical
types.

For example, if type `0x1c` did not exist, the database driver could
serialize a timestamp object (Date in JavaScript, Instant in Java, etc)
into a Unix timestamp, a 64-bit integer. Assuming the lack of schema,
upon deserialization it would not be possible to tell an integer from
a timestamp and deserialize the value accordingly.

Type tagging resolves this by attaching an integer tag to values that
can then be read when deserializing the value, e.g. that tag=1 is a
timestamp and the relevant timestamp class should be used.

The tag values are specified separately and applications can also
specify their own to have the database driver deserialize their specific
data types into the appropriate classes (including models).

Essentially this is object-relational mapping for parts of documents.

The format of the type is:

    0xee
    TAG number in 1 byte
    sub VPack value

or

    0xef
    TAG number in 8 bytes, little-endian encoding
    sub VPack value


## Custom types

Note that custom types should usually not be used for data exchange but
only internally in systems. Nevertheless, the design of this part of
the specification is made such that it is possible by generic methods
to derive the byte length of each custom data type.

The following user-defined types exist:

  - `0xf0`        : 1 byte payload, directly following the type byte
  - `0xf1`        : 2 bytes payload, directly following the type byte
  - `0xf2`        : 4 bytes payload, directly following the type byte
  - `0xf3`        : 8 bytes payload, directly following the type byte
  - `0xf4`-`0xf6` : length of the payload is described by a single further 
                    unsigned byte directly following the type byte, the
                    payload of that many bytes follows
  - `0xf7`-`0xf9` : length of the payload is described by two bytes (little 
                    endian unsigned integer) directly following the type
                    byte, the payload of that many bytes follows
  - `0xfa`-`0xfc` : length of the payload is described by four bytes (little 
                    endian unsigned integer) directly following the type
                    byte, the payload of that many bytes follows
  - `0xfd`-`0xff` : length of the payload is described by eight bytes (little 
                    endian unsigned integer) directly following the type
                    byte, the payload of that many bytes follows

Note: In types `0xf4` to `0xff` the "payload" refers to the actual data not
including the length specification.
