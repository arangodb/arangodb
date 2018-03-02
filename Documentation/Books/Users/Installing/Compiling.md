Compiling ArangoDB from scratch
===============================

The following sections describe how to compile and build the ArangoDB from
scratch. ArangoDB will compile on most Linux and Mac OS X systems. We assume
that you use the GNU C/C++ compiler or clang/clang++ to compile the
source. ArangoDB has been tested with the GNU C/C++ compiler and clang/clang++,
but should be able to compile with any Posix-compliant, C++11-enabled compiler.
Please let us know whether you successfully compiled it with another C/C++
compiler.

By default, cloning the github repository will checkout **devel**. This version
contains the development version of the ArangoDB.  Use this branch if you want
to make changes to the ArangoDB source.

Devel Version
-------------

Note: a separate [blog
article](http://jsteemann.github.io/blog/2014/10/16/how-to-compile-arangodb-from-source/)
is available that describes how to compile ArangoDB from source on Ubuntu.

### Basic System Requirements

Verify that your system contains

* the GNU C/C++ compilers "gcc" and "g++" and the standard C/C++ libraries, with support
  for C++11. You will need version gcc 4.9.0 or higher. For "clang" and "clang++", 
  you will need at least version 3.6.
* the GNU autotools (autoconf, automake)
* GNU make
* the GNU scanner generator FLEX, at least version 2.3.35
* the GNU parser generator BISON, at least version 2.4
* Python, version 2 or 3
* the OpenSSL library, version 1.0.1g or higher (development package)
* the GNU readline library (development package)
* Go, at least version 1.4.1

Most Linux systems already supply RPMs or DPKGs for these packages. 
Some distributions, for example Ubuntu 12.04 or Centos 5, provide only very out-dated 
versions of compilers, FLEX, BISON, and/or the V8 engine. In that case you need to compile 
newer versions of the programs and/or libraries.

When compiling with special configure options, you may need the following extra libraries:

* the Boost test framework library (only when using configure option `--enable-maintainer-mode`) 

### Download the Source

Download the latest source using ***git***:
    
    unix> git clone git://github.com/arangodb/arangodb.git

This will automatically clone the **devel** branch.

Note: if you only plan to compile ArangoDB locally and do not want to modify or push
any changes, you can speed up cloning substantially by using the *--single-branch* and 
*--depth* parameters for the clone command as follows:
    
    unix> git clone --single-branch --depth 1 git://github.com/arangodb/arangodb.git

### Setup

Switch into the ArangoDB directory

    unix> cd ArangoDB

In order to generate the configure script, execute

    unix> make setup

This will call aclocal, autoheader, automake, and autoconf in the correct order.

### Configure

In order to configure the build environment please execute

    unix> ./configure

to setup the makefiles. This will check the various system characteristics and
installed libraries.

Please note that it may be required to set the *--host* and *--target* variables
when running the configure command. For example, if you compile on MacOS, you
should add the following options to the configure command:

    --host=x86_64-apple-darwin --target=x86_64-apple-darwin

The host and target values for other architectures vary. 

If you also plan to make changes to the source code of ArangoDB, add the
following option to the *configure* command: *--enable-maintainer-mode*. Using
this option, you can make changes to the lexer and parser files and some other
source files that will generate other files. Enabling this option will add extra
dependencies to BISON, FLEX, and PYTHON. These external tools then need to be
available in the correct versions on your system.

The following configuration options exist:

`--enable-relative` 

This will make relative paths be used in the compiled binaries and
scripts. It allows to run ArangoDB from the compile directory directly, without the
need for a *make install* command and specifying much configuration parameters. 
When used, you can start ArangoDB using this command:

    bin/arangod /tmp/database-dir

ArangoDB will then automatically use the configuration from file *etc/relative/arangod.conf*.

`--enable-all-in-one-etcd` 

This tells the build system to use the bundled version of ETCD. This is the
default and recommended.

`--enable-internal-go` 

This tells the build system to use Go binaries located in the 3rdParty
directory. Note that ArangoDB does not ship with Go binaries, and that the Go
binaries must be copied into this directory manually.

`--enable-maintainer-mode` 

This tells the build system to use BISON and FLEX to regenerate the parser and
scanner files. If disabled, the supplied files will be used so you cannot make
changes to the parser and scanner files.  You need at least BISON 2.4.1 and FLEX
2.5.35.  This option also allows you to make changes to the error messages file,
which is converted to js and C header files using Python. You will need Python 2
or 3 for this.  Furthermore, this option enables additional test cases to be
executed in a *make unittests* run. You also need to install the Boost test
framework for this.

Additionally, turning on the maintainer mode will turn on a lot of assertions in
the code.

`--enable-failure-tests`

This option activates additional code in the server that intentionally makes the
server crash or misbehave (e.g. by pretending the system ran out of
memory). This option is useful for writing tests.

`--enable-v8-debug`

Builds a debug version of the V8 library. This is useful only when working on
the V8 integration inside ArangoDB.

`--enable-tcmalloc`

Links arangod and the client tools against the tcmalloc library installed on the
system. Note that when this option is set, a tcmalloc library must be present
and exposed under the name `libtcmalloc`, `libtcmalloc_minimal` or
`libtcmalloc_debug`.

Please also make sure that relevant compiler optimizations flags are set. This
can be achieved by setting the `CFLAGS` and `CXXFLAGS` environment variables
when invoking the configure command. For example, to build with optimizations
use:

`CFLAGS="-O3" CXXFLAGS="-O3" ./configure ...`

Please also note that specific compilers may need specific configuration in order
to make ArangoDB compile and run. For example, g++ 6.0 and higher by default 
perform some optimizations that are unsafe for V8 and may make it crash. For
g++ 6 or higher, the following environment variables need to be set:

`CFLAGS="-fno-delete-null-pointer-checks" CXXFLAGS="-fno-delete-null-pointer-checks" ./configure ...`

### Compiling Go

Users F21 and duralog told us that some systems don't provide an update-to-date
version of go. This seems to be the case for at least Ubuntu 12 and 13. To
install go on these system, you may follow the instructions provided
[here](http://blog.labix.org/2013/06/15/in-flight-deb-packages-of-go).  For
other systems, you may follow the instructions
[here](http://golang.org/doc/install).

To make ArangoDB use a specific version of go, you may copy the go binaries into
the 3rdParty/go-32 or 3rdParty/go-64 directories of ArangoDB (depending on your
architecture), and then tell ArangoDB to use this specific go version by using
the *--enable-internal-go* configure option.

User duralog provided some the following script to pull the latest release
version of go into the ArangoDB source directory and build it:

    cd ArangoDB
    hg clone -u release https://code.google.com/p/go 3rdParty/go-64 && \
      cd 3rdParty/go-64/src && \
      ./all.bash

    # now that go is installed, run your configure with --enable-internal-go
    ./configure --enable-internal-go


### Compile

Compile the programs (server, client, utilities) by executing

    make

This will compile ArangoDB and create a binary of the server in

    ./bin/arangod

### Test

Create an empty directory

    unix> mkdir /tmp/database-dir

Check the binary by starting it using the command line.

    unix> ./bin/arangod -c etc/relative/arangod.conf --server.endpoint tcp://127.0.0.1:8529 /tmp/database-dir

This will start up the ArangoDB and listen for HTTP requests on port 8529 bound
to IP address 127.0.0.1. You should see the startup messages similar to the
following:

```
2013-10-14T12:47:29Z [29266] INFO ArangoDB xxx ... 
2013-10-14T12:47:29Z [29266] INFO using endpoint 'tcp://127.0.0.1.8529' for non-encrypted requests
2013-10-14T12:47:30Z [29266] INFO Authentication is turned off
2013-10-14T12:47:30Z [29266] INFO ArangoDB (version xxx) is ready for business. Have fun!
```

If it fails with a message about the database directory, please make sure the
database directory you specified exists and can be written into.

Use your favorite browser to access the URL

    http://127.0.0.1:8529/_api/version

This should produce a JSON object like

    {"server" : "arango", "version" : "..."}

as result.

### Re-building ArangoDB after an update

To stay up-to-date with changes made in the main ArangoDB repository, you will
need to pull the changes from it and re-run `make`.

Normally, this will be as simple as follows:

    unix> git pull
    unix> make

From time to time there will be bigger structural changes in ArangoDB, which may
render the old Makefiles invalid. Should this be the case and `make` complains
about missing files etc., the following commands should fix it:

    unix> rm -rf lib/*/.deps arangod/*/.deps arangosh/*/.deps Makefile
    unix> make setup
    unix> ./configure <your configure options go here>
    unix> make

In order to reset everything and also recompile all 3rd party libraries, issue
the following commands:

    unix> make superclean
    unix> git checkout -- .
    unix> make setup
    unix> ./configure <your configure options go here>
    unix> make

This will clean up ArangoDB and the 3rd party libraries, and rebuild everything.

If you forgot your previous configure options, you can look them up with

    unix> head config.log
   
before issuing `make superclean` (as make `superclean` also removes the file `config.log`). 

Sometimes you can get away with the less intrusive commands.

### Install

Install everything by executing

    make install

You must be root to do this or at least have write permission to the
corresponding directories.

The server will by default be installed in

    /usr/local/sbin/arangod

The configuration file will be installed in

    /usr/local/etc/arangodb/arangod.conf

The database will be installed in

    /usr/local/var/lib/arangodb

The ArangoShell will be installed in

    /usr/local/bin/arangosh

**Note:** The installation directory will be different if you use one of the
`precompiled` packages. Please check the default locations of your operating
system, e. g. `/etc` and `/var/lib`.

When upgrading from a previous version of ArangoDB, please make sure you inspect
ArangoDB's log file after an upgrade. It may also be necessary to start ArangoDB
with the *--upgrade* parameter once to perform required upgrade or
initialization tasks.
