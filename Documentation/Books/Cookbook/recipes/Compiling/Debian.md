# Compiling on Debian

## Problem

You want to compile and run the devel branch, for example to test a bug fix. In this example the system is Debian based.

## Solution

This solution was made using a fresh Debian Testing machine on Amazon EC2. For completeness, the steps pertaining to AWS are also included in this recipe.

### Launch the VM

*Optional*

Login to your AWS account and launch an instance of Debian Testing. I used an 'm3.xlarge' since that has a bunch of cores, more than enough memory, optimized network and the instance store is on SSDs which can be switched to provisioned IOPs.

The Current AMI ID's can be found in the Debian Wiki: https://wiki.debian.org/Cloud/AmazonEC2Image/Jessie

### Upgrade to the very latest version

*Optional*

Once your EC2 instance is up, login ad `admin` and `sudo su` to become `root`.

First, we remove the backports and change the primary sources.list

```bash
rm -rf /etc/apt/sources.list.d
echo "deb     http://http.debian.net/debian testing main contrib"  > /etc/apt/sources.list
echo "deb-src http://http.debian.net/debian testing main contrib" >> /etc/apt/sources.list
```

Update and upgrade the system. Make sure you don't have any broken/unconfigured packages. Sometimes you need to run safe/full upgrade more than once. When you're done, reboot.

```bash
apt-get install aptitude
aptitude -y update
aptitude -y safe-upgrade
aptitude -y full-upgrade
reboot
```

### Install Build dependencies

*Mandatory*

Before you can build ArangoDB, you need a few packages pre-installed on your system.

Login again and install them.

```bash
sudo aptitude -y install git-core \
    build-essential \
    libssl-dev \
    libjemalloc-dev \
    cmake \
    python2.7 \
```

### 
Download the Source

Download the latest source using ***git***:
    
    unix> git clone git://github.com/arangodb/arangodb.git

This will automatically clone the **devel** branch.

Note: if you only plan to compile ArangoDB locally and do not want to modify or push
any changes, you can speed up cloning substantially by using the *--single-branch* and 
*--depth* parameters for the clone command as follows:
    
    unix> git clone --single-branch --depth 1 git://github.com/arangodb/arangodb.git

### Setup

Switch into the ArangoDB directory

    unix> cd arangodb
    unix> mkdir build
    unix> cd build

In order to generate the build environment please execute

    unix> cmake .. 

to setup the makefiles. This will check the various system characteristics and
installed libraries. If you installed the compiler in a non standard location, you may need to specify it:

    cmake -DCMAKE_C_COMPILER=/opt/bin/gcc -DCMAKE_CXX_COMPILER=/opt/bin/g++ ..

If you compile on MacOS, you should add the following options to the cmake command:

    cmake .. -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl -DCMAKE_BUILD_TYPE=RelWithDebInfo  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.11

If you also plan to make changes to the source code of ArangoDB, you should compile with the `Debug` target;
The `Debug` target enables additional sanity checks etc. which would slow down production binaries.

Other options valuable for development:

    -DARANGODB_ENABLE_MAINTAINER_MODE

Needed, if you plan to make changes to AQL language (which is implemented using a lexer and parser
files in `arangod/Aql/grammar.y` and `arangod/Aql/tokens.ll`) your system has to contain the tools FLEX and BISON.

    -DARANGODB_ENABLE_BACKTRACE

(requires the maintainer mode) If you want to have c++ stacktraces attached to your exceptions.
This can be usefull to more quick locate the place where an exception or an assertion was thrown.


scripts. It allows to run ArangoDB from the compile directory directly, without the
need for a *make install* command and specifying much configuration parameters. 
When used, you can start ArangoDB using this command:

    bin/arangod /tmp/database-dir

ArangoDB will then automatically use the configuration from file *etc/relative/arangod.conf*.

    -DUSE_FAILURE_TESTS

This option activates additional code in the server that intentionally makes the
server crash or misbehave (e.g. by pretending the system ran out of
memory). This option is useful for writing tests.

By default the libc allocator is chosen. If your system offers the jemalloc it will be
prefered over tcmalloc and the system allocator. 

### shared memory
Gyp is used as makefile generator by V8. Gyp requires shared memory to be available,
which may not if you i.e. compile in a chroot. You can make it available like this:

    none /opt/chroots/ubuntu_precise_x64/dev/shm tmpfs rw,nosuid,nodev,noexec 0 2
    devpts    /opt/chroots/ubuntu_precise_x64/dev/pts    devpts    gid=5,mode=620    0  0


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
2016-06-01T12:47:29Z [29266] INFO ArangoDB xxx ... 
2016-06-10T12:47:29Z [29266] INFO using endpoint 'tcp://127.0.0.1.8529' for non-encrypted requests
2016-06-01T12:47:30Z [29266] INFO Authentication is turned on
2016-60-01T12:47:30Z [29266] INFO ArangoDB (version xxx) is ready for business. Have fun!
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


    unix> rm -f CMakeCache.txt
    unix> cmake .. 
    unix> make

In order to reset everything and also recompile all 3rd party libraries, issue
the following commands:


    unix> git checkout -- .
    unix> cd ..; rm -rf build; mkdir build; cd build

This will clean up ArangoDB and the 3rd party libraries, and rebuild everything.

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

You should add an arangodb user and group (as root), plus make shure it owns these directories:

    useradd -g arangodb arangodb
    chown -R arangodb:arangodb /usr/local/var/lib/arangodb3-apps/
    chown -R arangodb:arangodb /tmp/database-dir/

**Note:** The installation directory will be different if you use one of the
`precompiled` packages. Please check the default locations of your operating
system, e. g. `/etc` and `/var/lib`.

When upgrading from a previous version of ArangoDB, please make sure you inspect
ArangoDB's log file after an upgrade. It may also be necessary to start ArangoDB
with the *--database.upgrade* parameter once to perform required upgrade or
initialization tasks.

**Author:** [Patrick Huber](https://github.com/stackmagic)
**Author:** [Wilfried Goesgens](https://github.com/dothebart)

**Tags:** #debian #driver
