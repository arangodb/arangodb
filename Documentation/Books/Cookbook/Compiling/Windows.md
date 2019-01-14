Compiling ArangoDB under Windows
================================

Problem
-------

I want to compile ArangoDB 3.4 and onwards under Windows.

**Note:** If you want to compile version 3.3 or earlier, then look at the
[Compiling ArangoDB under Windows](https://docs.arangodb.com/3.3/Cookbook/Compiling/Windows.html)
recipe in the 3.3 documentation.

Solution
--------

With ArangoDB 3.0 a complete cmake environment was introduced. This also streamlines the dependencies on Windows.
We suggest to use [chocolatey.org](https://chocolatey.org/) to install most of the dependencies. For sure
most projects offer their own setup & install packages, chocolatey offers a simplified way to install them
with less user interactions. You can even use chocolatey via
[ansibles 2.7 winrm facility](https://docs.ansible.com/ansible/latest/user_guide/windows.html)
to do unattended installations of some software on Windows.

### Ingredients

First install the choco package manager by pasting this tiny cmdlet into a command window
*(needs to be run with Administrator privileges; Right click start menu, **Command Prompt (Admin)**)*:

    @powershell -NoProfile -ExecutionPolicy Bypass -Command "iex ((new-object net.webclient).DownloadString('https://chocolatey.org/install.ps1'))" && SET PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin

#### Visual Studio and its Compiler

Since choco currently fails to alter the environment for
[Microsoft Visual Studio](https://www.visualstudio.com/en-us/products/visual-studio-community-vs.aspx),
we suggest to download and install Visual Studio by hand.
Currently Visual Studio 2017 is the only supported option.

{% hint 'warning' %}
You need to make sure that it installs the **Desktop development with C++** preset,
else cmake will fail to detect it later on. Furthermore, the **Windows 8.1 SDK and UCRT SDK**
optional component is required to be selected during Visual Studio installation, else V8
will fail to compile later on.
{% endhint %}

After it successfully installed, start it once, so it can finish its setup.

#### More Dependencies

Now you can invoke the choco package manager for an unattended install of the dependencies
*(needs to be run with Administrator privileges again)*:

    choco install -y cmake.portable nsis python2 procdump windbg wget 

Then we need to install the [OpenSSL](https://openssl.org) library from its sources or using precompiled
[Third Party OpenSSL Related Binary Distributions](https://wiki.openssl.org/index.php/Binaries).

#### Optional Dependencies

If you intend to run the unit tests or compile from git, you also need
*(needs to be run with Administrator privileges again)*:

    choco install -y git winflexbison ruby

Close and reopen the Administrator command window in order to continue with the ruby devkit:

    choco install -y ruby2.devkit

And manually install the requirements via the `Gemfile` fetched from the ArangoDB Git repository
*(needs to be run with Administrator privileges)*:

    wget https://raw.githubusercontent.com/arangodb/arangodb/devel/tests/rb/HttpInterface/Gemfile
    setx PATH %PATH%;C:\tools\DevKit2\bin;C:\tools\DevKit2\mingw\bin
    gem install bundler
    bundler

Note that the V8 build scripts and gyp aren't compatible with Python 3.x hence you need python2!

### Building ArangoDB

Download and extract the release tarball from https://www.arangodb.com/download/

Or clone the GitHub repository and checkout the branch or tag you need (e.g. `devel`)

    git clone https://github.com/arangodb/arangodb.git -b devel
    cd arangodb

Generate the Visual studio project files, and check back that cmake discovered all components on your system:

    mkdir Build64
    cd Build64
    cmake -G "Visual Studio 15 2017 Win64" ..

Note that in some cases cmake struggles to find the proper python interpreter
(i.e. the cygwin one won't work). You can force overrule it by appending:

    -DPYTHON_EXECUTABLE:FILEPATH=C:/Python27/python.exe

You can now load these in the Visual Studio IDE or use cmake to start the build:

    cmake --build . --config RelWithDebInfo

The binaries need the ICU datafile `icudt54l.dat`, which is automatically copied into the directory containing the
executable.

### Unit tests (Optional)

The unit tests require a [cygwin](https://www.cygwin.com/) environment.

#### Cygwin Installation Hints

You need at least `make` from cygwin. Cygwin also offers a `cmake`. Do **not** install the cygwin cmake.

You should also issue these commands to generate user information for the cygwin commands:

    mkpasswd > /etc/passwd
    mkgroup > /etc/group

Turning ACL off (noacl) for all mounts in cygwin fixes permissions troubles that may appear in the build:

    # /etc/fstab
    #
    #    This file is read once by the first process in a Cygwin process tree.
    #    To pick up changes, restart all Cygwin processes.  For a description
    #    see https://cygwin.com/cygwin-ug-net/using.html#mount-table
    
    # noacl = Ignore Access Control List and let Windows handle permissions
    C:/cygwin64/bin  /usr/bin   ntfs      binary,auto,noacl           0  0
    C:/cygwin64/lib  /usr/lib   ntfs      binary,auto,noacl           0  0
    C:/cygwin64      /          ntfs      override,binary,auto,noacl  0  0
    none             /cygdrive  cygdrive  binary,posix=0,user,noacl   0  0

#### Enable native symlinks for Cygwin and git

Cygwin will create proprietary files as placeholders by default instead of
actually symlinking files. The placeholders later tell Cygwin where to resolve
paths to. It does not intercept every access to the placeholders however, so
that 3rd party scripts break. Windows Vista and above support real symlinks,
and Cygwin can be configured to make use of it:

    # use actual symlinks to prevent documentation build errors
    # (requires elevated rights!)
    export CYGWIN="winsymlinks:native"

Note that you must run Cygwin as administrator or change the Windows group
policies to allow user accounts to create symlinks (`gpedit.msc` if available).

BTW: You can create symlinks manually on Windows like:

    mklink /H target/file.ext source/file.ext
    mklink /D target/path source/path
    mklink /J target/path source/path/for/junction

And in Cygwin:

    ln -s source target

#### Making the ICU database publically available

If you intend to use the machine for development purposes, it may be more practical to copy it to a common place:

    cd 3rdParty/V8/v*/third_party/icu/source/data/in && cp icudt*.dat /cygdrive/c/Windows/

And configure your environment (yes this instruction remembers to the hitchhikers guide to the galaxy...) so that
`ICU_DATA` points to `c:\\Windows`. You do that by opening the explorer,
right click on `This PC` in the tree on the left, choose `Properties` in the opening window `Advanced system settings`,
in the Popup `Environment Variables`, another popup opens, in the `System Variables` part you click `New`, 
And variable name: `ICU_DATA` to the value: `c:\\Windows`

![HowtoSetEnv](../assets/CompilingUnderWindows/SetEnvironmentVar.png)

#### Running Unit tests

You can then run the integration tests in the cygwin shell like that:

    Build64/bin/RelWithDebInfo/arangosh.exe \
    -c etc/relative/arangosh.conf \
    --log.level warning \
    --server.endpoint tcp://127.0.0.1:1024 \
    --javascript.execute UnitTests/unittest.js \
      -- \
      all \
      --build Build64 \
      --buildType RelWithDebInfo \
      --skipNondeterministic true \
      --skipTimeCritical true \
      --skipBoost true \
      --skipGeo true

Additional options `--ruby c:/tools/ruby25/bin/ruby` and `--rspec c:/tools/ruby25/bin/rspec`
should be used only if Ruby is not in the *PATH*.

**Authors**:
[Frank Celler](https://github.com/fceller),
[Wilfried Goesgens](https://github.com/dothebart),
[Simran Brucherseifer](https://github.com/Simran-B) and
[Vadim Kondratyev](https://github.com/KVS85).

**Tags**: #windows
