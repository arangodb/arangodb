# Compiling on Debian

## Problem

You want to compile and run the devel branch, for example to test a bug fix. In this example the system is Debian based.

## Solution

This solution was made using a fresh Debian Testing machine on Amazon EC2. For completeness, the steps pertaining to AWS are also included in this recipe.

### Launch the VM

*Optional*

Login to your AWS account and launch an instance of Debian Testing. I used an 'm3.xlarge' since that has a bunch of cores, more than enough memory, optimized network and the instance store is on SSDs which can be switched to provisioned IOPs.

The Current AMI ID's can be found in the Debian Wiki: https://wiki.debian.org/Cloud/AmazonEC2Image/Wheezy

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
aptitude update
aptitude safe-upgrade
aptitude full-upgrade
reboot
```

### Install Build dependencies

*Mandatory*

Before you can build ArangoDB, you need a few packages pre-installed on your system. ArangoDB also bundles a few libraries, which can be enabled/used by using the "all-in-one" switches during the configuration state.

More information on the dependencies is available in the official documentation: https://docs.arangodb.com/Installing/Compiling.html

Login again and install them.

```bash
sudo aptitude install git-core \
    build-essential \
    autoconf automake \
    libssl-dev libreadline-dev libboost-test-dev \
    cmake \
    python2.7 \
    golang-go
```

### Build ArangoDB - autoconf way

*Mandatory*

To build ArangoDB we *don't* need to be root. It's perfectly fine to build as admin.

Depending on what you want to do with your build, it may make sense to set a few compiler flags. Below we include debug symbols and disable all optimization.

```bash
export CFLAGS="-g -O0"
export CXXFLAGS="-g -O0"
```

Now, as normal user, we will clone and build ArangoDB

```bash
git clone -b devel git://github.com/arangodb/arangodb.git
cd ArangoDB
make setup
./configure
```

The configure summary should include a version and details for: readline, openssl, zlib, libev, v8, icu.

Now we're ready to build, and let's make sure we max out the machine:

```bash
make -j $(nproc)
```

### Next steps?

*Optional*

Now that you have a custom build, you probably want to run it. Have a look at [Running Custom Build](RunningCustomBuild.md)

## Build ArangoDB - cmake way for ARM-targets
The cmake way will work on ARM systems, plus you will end up with `.deb` packages.

You should configure your system to use the gold-ld, since its using far less resources:

```bash
cd /usr/bin
rm ld
ln -s ld.gold ld
```

First we need to compile the dependencies (V8, libev...) via the traditional configure / make style:

```bash
./configure
make
```

Once we got all the dependencies compiled we can continue using them for subsequent builds:

```bash
cd 3rdParty; make -f Makefile.v8-arm install
```

then continue with the main package building:

```bash
make pack-arm
```

If you want to fiddle with the process, visit `GNUmakefile` and search for `pack-arm` to find its invocation of cmake.


**Author:** [Patrick Huber](https://github.com/stackmagic)

**Tags:** #debian #driver
