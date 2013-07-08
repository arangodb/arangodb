Compiling ArangoDB from scratch{#Compiling}
===========================================

@NAVIGATE_Compiling
@EMBEDTOC{CompilingTOC}

Compiling ArangoDB{#CompilingIntro}
===================================

The following sections describe how to compile and build the ArangoDB from
scratch. The ArangoDB will compile on most Linux and Mac OS X systems. It
assumes that you use the GNU C++ compiler to compile the source. The ArangoDB
has been tested with the GNU C++ compiler, but should compile with any Posix
compliant compiler. Please let us know, whether you successfully compiled it
with another C++ compiler.

There are possibilities:

- all-in-one: this version contains the source code of the ArangoDB, all
              generated files from the autotools, FLEX, and BISON as well
              as a version of V8, libev, and ICU.

- devel: this version contains the development version of the ArangoDB.
         Use this branch, if you want to make changes to ArangoDB 
         source.

The devel version requires a complete development environment, while the
all-in-one version allows you to compile the ArangoDB without installing all the
prerequisites. The disadvantage is that it takes longer to compile and you
cannot make changes to the flex or bison files.

Amazon Micro Instance{#CompilingAmazonMicroInstance}
----------------------------------------------------

@@sohgoh has reported that it is very easy to install ArangoDB on an Amazon
Micro Instance:

    amazon> sudo yum install readline-devel
    amazon> ./configure
    amazon> make
    amazon> make install

For detailed instructions the following section.

All-In-One Version{#CompilingAIO}
=================================

Note: there are separate instructions for the **devel** version in the next section.

Basic System Requirements{#CompilingAIOPrerequisites}
-----------------------------------------------------

Verify that your system contains:

- the GNU C++ compiler "g++" and standard C++ libraries
- the GNU make

In addition you will need the following library:

- the GNU readline library
- the OpenSSL library

Under Mac OS X you also need to install:

- Xcode
- scons

Download the Source{#DownloadSourceAIO}
---------------------------------------

Download the latest source using GIT:

    git clone git://github.com/triAGENS/ArangoDB.git

Configure{#CompilingAIOConfigure}
---------------------------------

Switch into the ArangoDB directory

    cd ArangoDB

In order to configure the build environment execute

    ./configure --enable-all-in-one-v8 --enable-all-in-one-libev --enable-all-in-one-icu

to setup the makefiles. This will check the various system characteristics and
installed libraries.

Compile{#CompilingAIOCompile}
-----------------------------

Compile the program by executing

    make

This will compile the ArangoDB and create a binary of the server in

    ./bin/arangod

Test{#CompilingAIOTest}
-----------------------

Create an empty directory

    unix> mkdir /tmp/database-dir

Check the binary by starting it using the command line.

    unix> ./bin/arangod -c etc/relative/arangod.conf --server.endpoint tcp://127.0.0.1:12345 --server.disable-authentication true /tmp/database-dir

This will start up the ArangoDB and listen for HTTP requests on port 12345 bound
to IP address 127.0.0.1. You should see the startup messages

    2012-02-05T13:23:52Z  [455] INFO ArangoDB (version 1.x.y) is ready for business
    2012-02-05T13:23:52Z  [455] INFO HTTP client port: 12345
    2012-02-05T13:23:52Z  [455] INFO Have Fun!

If it fails with a message about the database directory, please make sure the
database directory you specified exists and can be written into.

Use your favorite browser to access the URL

    http://127.0.0.1:12345/version

This should produce a JSON object like

    {"server" : "arango", "version" : "1.x.y"}

as result.

Install{#CompilingAIOInstall}
-----------------------------

Install everything by executing

    make install

You must be root to do this or at least have write permission to the
corresponding directories.

The server will by default be installed in

    /usr/sbin/arangod

The configuration file will be installed in

    /etc/arangodb/arangod.conf

The database will be installed in

    /var/lib/arangodb

The arango shell will be installed in

    /usr/bin/arangosh

Devel Version{#CompilingDevel}
==============================

Basic System Requirements{#CompilingDevelPrerequisites}
-------------------------------------------------------

Verify that your system contains

- the GNU C++ compiler "g++" and standard C++ libraries
- the GNU autotools (autoconf, automake)
- the GNU make
- the GNU scanner generator FLEX, at least version 2.3.35
- the GNU parser generator BISON, at least version 2.4
- Python, version 2 or 3

In addition you will need the following libraries

- libev in version 3 or 4
- Google's V8 engine
- the ICU library
- the GNU readline library
- the OpenSSL library
- the Boost test framework library (boost_unit_test_framework)

To compile Google V8 yourself, you will also need Python 2 and SCons.

Some distributions, for example Centos 5, provide only very out-dated versions
of FLEX, BISON, and the V8 engine. In that case you need to compile newer
versions of the programs and/or libraries.

Install or download the prerequisites

- Google's V8 engine (see http://code.google.com/p/v8)
- SCons for compiling V8 (see http://www.scons.org)
- libev (see http://software.schmorp.de/pkg/libev.html) 
- OpenSSL (http://openssl.org/)

if neccessary.  Most linux systems already supply RPM or DEP for these
packages. Please note that you have to install the development packages.

Download the Source{#DownloadSourceDevel}
-----------------------------------------

Download the latest source using GIT:

    git clone git://github.com/triAGENS/ArangoDB.git

Setup{#CompilingDevelSetup}
---------------------------

Switch into the ArangoDB directory

    cd ArangoDB

The source tarball contains a pre-generated "configure" script. You can
regenerate this script by using the GNU auto tools. In order to do so, execute

    make setup

This will call aclocal, autoheader, automake, and autoconf in the correct order.

Configure{#CompilingDevelConfigure}
-----------------------------------

In order to configure the build environment execute

    unix> ./configure --disable-all-in-one-v8 --disable-all-in-one-libev --disable-all-in-one-icu --enable-maintainer-mode

to setup the makefiles. This will check for the various system characteristics
and installed libraries.

Now continue with @ref CompilingAIOCompile.

The following configuration options exists:

`--enable-all-in-one-libev` tells the build system to use the bundled version
of LIBEV instead of using the system version.

`--disable-all-in-one-libev` tells the build system to use the installed
system version of LIBEV instead of compiling the supplied version from the
3rdParty directory in the make run.

`--enable-all-in-one-v8` tells the build system to use the bundled version of
V8 instead of using the system version.

`--disable-all-in-one-v8` tells the build system to use the installed system
version of V8 instead of compiling the supplied version from the 3rdParty
directory in the make run.

`--enable-all-in-one-icu` tells the build system to use the bundled version of
ICU instead of using the system version.

`--disable-all-in-one-icu` tells the build system to use the installed system
version of ICU instead of compiling the supplied version from the 3rdParty
directory in the make run.

`--enable-maintainer-mode` tells the build system to use BISON and FLEX to
regenerate the parser and scanner files. If disabled, the supplied files will be
used so you cannot make changes to the parser and scanner files.  You need at
least BISON 2.4.1 and FLEX 2.5.35.  This option also allows you to make changes
to the error messages file, which is converted to js and C header files using
Python. You will need Python 2 or 3 for this.  Furthermore, this option enables
additional test cases to be executed in a `make unittests` run. You also need to
install the Boost test framework for this.
