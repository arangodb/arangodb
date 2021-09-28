VelocyPack (VPack) - a fast and compact format for serialization and storage
============================================================================

TravisCI: [![Build Status](https://api.travis-ci.com/arangodb/velocypack.svg?branch=main)](https://travis-ci.com/arangodb/velocypack)   AppVeyor: [![Build status](https://ci.appveyor.com/api/projects/status/pkbl4t7vey88bqud?svg=true)](https://ci.appveyor.com/project/jsteemann/velocypack)   Coveralls: [![Coverage Status](https://coveralls.io/repos/arangodb/velocypack/badge.svg?branch=main&service=github)](https://coveralls.io/github/arangodb/velocypack?branch=main)

Motivation
----------

These days, JSON (JavaScript Object Notation, see
[ECMA-404](http://www.ecma-international.org/publications/files/ECMA-ST/ECMA-404.pdf))
is used in many cases where data has to be exchanged.
Lots of protocols between different services use it, databases store
JSON (document stores naturally, but others increasingly as well). It
is popular, because it is simple, human-readable, and yet surprisingly
versatile, despite its limitations.

At the same time there is a plethora of alternatives ranging from XML
over Universal Binary JSON, MongoDB's BSON, MessagePack, BJSON (binary
JSON), Apache Thrift till Google's protocol buffers and ArangoDB's
shaped JSON.

When looking into this, we were surprised to find that none of these
formats manages to combine compactness, platform independence, fast
access to sub-objects and rapid conversion from and to JSON.

We have invented VPack because we need a binary format that

  - is self-contained, schemaless and platform independent
  - is compact
  - covers all of JSON plus dates, integers, binary data and arbitrary
    precision numbers
  - can be used in a database kernel to access sub-documents for
    example for indexes, so it must be possible to access sub-documents
    (array and object members) efficiently
  - can be transferred to JSON and from JSON rapidly
  - avoids too many memory allocations
  - gives flexibility to assemble objects, such that sub-objects reside
    in the database in an unchanged way
  - allows to use an external table for frequently used attribute names
  - quickly allows to read off the type and length of a given object
    from its first byte(s)

All this gives us the possibility to use *the same byte sequence of
data* for **transport**, **storage** and (read-only) **work**. Using a
single data format not only eliminates a lot of conversions but can 
also reduce runtime memory usage, as data does only need a single 
in-memory representation.

The other popular formats we looked at have all some deficiency with
respect to the above list. To name but a few:

  - JSON itself lacks some data types (dates and binary data) and does
    not provide quick sub-value access without parsing. Parsing JSON is
    also quite a challenge performance-wise
  - XML is not compact and is not good with binary data, it also lacks
    quick sub-value access
  - BSON gets quite a lot right with respect to data types, but is 
    seriously lacking w.r.t. sub-value access. Furthermore, it is not
    very compact and quite wasteful space-wise when storing array values
  - Apache Thrift and Google's Protocol Buffers are not schemaless and 
    self-contained. Their transport format is a serialization that is
    not good for rapid sub-value access
  - MessagePack is probably the closest to our shopping list. It has
    has decent data types and is quite compact. However, we found that 
    one can do better in terms of compactness for some cases. More
    important for us, MessagePack provides no quick sub-value access
  - Our own shaped JSON (used in ArangoDB as internal storage format)
    has very quick sub-value access, but the shape data is kept outside
    the actual data, so the shaped values are not self-contained.
    Furthermore, we have run into scalability issues on multi-core
    because of the shared data structures used for interpretation of
    the values

Any new data format must be backed by good C++ classes to allow

  - easy and fast parsing from JSON
  - easy and convenient buildup without too many memory allocations
  - fast access to data and its sub-objects (for arrays and objects)
  - flexible memory management
  - fast dumping to JSON

The VelocyPack format is an attempt to achieve all this, and our first
experiments and usage attempts are very encouraging.

This repository contains a C++ library for building, manipulating and
serializing VPack data. It is the *reference implementation for the 
VelocyPack format*. The library is written in C++17 so it should compile 
on many up-to-date systems.


Specification
-------------

See the file [VelocyPack.md](VelocyPack.md) for a detailed description of
the VPack format.


Performance
-----------

See the file [Performance.md](Performance.md) for a thorough comparison
to other formats like JSON itself, MessagePack and BSON. We look at file
sizes as well as parsing and conversion performance.


Building the VPack library
--------------------------

The VPack library can be built on Linux, MacOS and Windows. It will likely
compile and work on other platforms for which a recent version of *cmake* and
a working C++17-enabled compiler are available.

See the file [Install.md](Install.md) for compilation and installation
instructions.


Using the VPack library
-----------------------

Please consult the file [examples/API.md](examples/API.md) for usage examples, 
and the file [examples/Embedding.md](examples/Embedding.md) for information
about how to embed the library into client applications.


Contributing
------------

We welcome bug fixes and patches from 3rd party contributors!

Please follow the guidelines in [CONTRIBUTING.md](.github/CONTRIBUTING.md)
if you want to contribute to VelocyPack. Have a look for the tag _help wanted_
in the issue tracker!

We also provide a golang version of VPack in the 
[go-velocypack repository](https://github.com/arangodb/go-velocypack) and a
Java version in the [java-velocypack](https://github.com/arangodb/java-velocypack).

Additionally, there is a third party VPack implementation for 
[PHP](https://github.com/martin-schilling/php-velocypack).
