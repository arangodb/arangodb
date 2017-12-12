# Compiling ArangoDB under Windows

## Problem

I want to compile ArangoDB under Windows.

**Note:** For this recipe you need ArangoDB 2.3. or a version below. For ArangoDB since 2.4 look at the [new Compiling ArangoDB under Windows](Windows.md).

## Solution

While compiling ArangoDB under Linux is straight forward - simply execute `configure` and `make` - compiling under Windows 
is a bit more demanding.

### Ingredients

For this recipe, you need to install the following programs under Windows:

* [cygwin](https://www.cygwin.com/)

  You need at least `make` from cygwin. Cygwin also offers a `cmake` do not install this version. 

* [cmake](https://cmake.org/)

  Either version 2.8.12 or 3.0.2 should work. Make sure to download the 64bit version.

* [Visual Studio Express 2013 for Windows Desktop](https://www.microsoft.com/en-us/download/details.aspx?id=44914)

  Please note that there are different versions of Visual Studio. The `Visual Studio for Windows` will not work.
  You need to install `Visual Studio (Express) for Windows Desktop`. You must configure your path in such a way
  that the compiler can be found. One way is to execute the `vcvarsall.bat` script from the `VC` folder.

* [Nullsoft Scriptable Install System](http://nsis.sourceforge.net/Download)

### Building the required libraries

First of all, start a `bash` from cygwin and clone the repository

    https://github.com/arangodb/arangodb-windows-libraries.git

and switch into the directory `arangodb-windows-libraries`. This repository contains the open-source libraries which
are needed by ArangoDB.

* Google's V8
* etcd from CoreOS
* getopt
* icu
* libev
* linenoise
* openssl
* regex
* zlib

In order to build the corresponding 32bit and 64bit versions of the library, execute

    make
    make install
    make 3rdParty

This will create a folder `3rdParty-Windows` containing the headers and libraries.

### Building ArangoDB itself

Clone the repository

    https://github.com/arangodb/arangodb.git

and copy the `3rdParty-Windows` folder into this directory `ArangoDB`.

Now switch into the `ArangoDB` folder and execute

    make pack-win32

or

    make pack-win64

in order to build the installer file for either 32bit or 64bit.

### Executables only

If you do not need the installer file, you can use the `cmake` to build the executables. Instead of `make pack-win32`
use

    mkdir Build32
    cd Build32
    cmake -G "Visual Studio 12" ..

This will create a solution file in the `Build32` folder. You can now start Visual Studio and open this
solution file.

Alternatively use `cmake` to build the executables.

    cmake --build . --config Release

**Author**: [Frank Celler](https://github.com/fceller)

**Tags**: #windows #build
