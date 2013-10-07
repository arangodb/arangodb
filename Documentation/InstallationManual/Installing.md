Installing ArangoDB {#Installing}
=================================

@NAVIGATE_Installing
@EMBEDTOC{InstallingTOC}

Linux {#InstallingLinux}
========================

You can find binary packages for various Linux distributions
@EXTREF{http://www.arangodb.org/download/,here}.

We provide packages for

- Centos
- Debian
- Fedora
- Mandriva
- OpenSUSE
- RedHat RHEL
- SUSE SLE
- Ubuntu

Using a Package Manager to install ArangoDB {#InstallingLinuxPackageManager}
----------------------------------------------------------------------------

Follow the instructions on the download page to use your favorite package manager
for the major distributions. After setting up the ArangoDB repository you can then
easily install ArangoDB using yum, aptitude, urpmi, or zypper.

### Gentoo

Please use the 
@EXTREF_S{https://github.com/mgiken/portage-overlay/tree/master/dev-db/ArangoDB,portage}
provided by @@mgiken.

### Linux-Mint {#InstallingDebian}

Download and import GPG-PublicKey

    wget -O RPM-GPG-KEY-www.arangodb.org http://www.arangodb.org/repositories/PublicKey
    apt-key add RPM-GPG-KEY-www.arangodb.org

Add the corresponding repository in file  `/etc/apt/sources.list`:

    deb http://www.arangodb.org/repositories LinuxMint-13 main

Update the repository data:

    aptitude update

Now you should be able to search for arangodb:

    aptitude search arangodb

In order to install arangodb:

    aptitude install arangodb

Using Vagrant and Chef
----------------------

A Chef recipe is available from jbianquetti at

    https://github.com/jbianquetti/chef-arangodb

Mac OS X {#InstallingMacOSX}
============================

You can find the Mac OS X packages here:

    http://www.arangodb.org/repositories/MacOSX

Homebrew {#InstallingMacOSXHomebrew}
------------------------------------

If you are using @S_EXTREF{http://mxcl.github.com/homebrew/,homebrew},
then you can install the ArangoDB using `brew` as follows:

    brew install arangodb

This will install the current stable version of ArangoDB and all
dependencies within your Homebrew tree. Note that the server will be
installed as

    /usr/local/sbin/arangod

The ArangoDB shell will be install as

    /usr/local/bin/arangosh

If you want to install the latest version use:

    brew install --HEAD arangodb

You can unstall ArangoDB using

    brew uninstall arangodb

However, in case you started ArangoDB using the launchctl, then you
need to unload it before uninstalling the server.

    launchctl unload ~/Library/LaunchAgents/homebrew.mxcl.arangodb.plist

Then remove the LaunchAgent

    rm ~/Library/LaunchAgents/homebrew.mxcl.arangodb.plist


Apple's App Store {#InstallingMacOSXAppStore}
---------------------------------------------

ArangoDB is available in Apple's App-Store. Please note, that it
sometimes take a few days or weeks until the latest versions will be
available.

Windows {#InstallingWindows}
============================

We provide precompiled Windows binaries for ArangoDB. The binaries
are statically linked with the required libraries such as V8, but 
they may still require some Windows platform libraries to be present.
These libraries should be present on a Windows Vista, Windows 7, and
Windows 8 by default, but there may be issues with other platforms.

The Windows builds are available as `.msi` packages 
@EXTREF{http://www.arangodb.org/download/,here}.
Please note that we provide binaries for 32 and 64 bit Windows, and 
that you need the package that matches your platform.

The msi installer will install the ArangoDB server, shell (arangosh) and 
the ArangoDB import tool (arangoimp) in a directory of the user's choice.

Included in the distribution are some `.bat` files that can be used 
to easily start the ArangoDB server and shell. The `.bat` files will be
installed in the same directory as ArangoDB so they should be easy to find.
The batch files contain a lot of configuration settings, and you might want 
to eventually adjust these parameters to match your own environment.

To start the ArangoDB server, use the batch file `serverExample.bat`.
It will start the ArangoDB server and will wait until you terminate it
by pressing CTRL-C. Starting ArangoDB for the first time will automatically
create a database sub-directory in the directory ArangoDB was installed in.

If you already have a previous version of ArangoDB installed and want to
upgrade to a newer version, use the `upgradeExample.bat` file. This will
start ArangoDB with the `--upgrade` option and perform a migration of an
existing database.

To start _arangosh_, use the batch file `shellExample.bat`.

Please note an important limitation when running ArangoDB under Cygwin:
Starting ArangoDB can be started from out of a Cygwin terminal, but pressing
CTRL-C will forcefully kill the server process, without giving it a chance to 
handle the kill signal. In this case, a regular server shutdown is not
possible, which may leave a file `LOCK` around in the server's data directory.
This file needs to be removed manually to make ArangoDB start again. 
Additionally, as ArangoDB does not have a chance to handle the kill signal,
the server cannot forcefully flush any data to disk on shutdown, leading to
potential data loss.

Starting ArangoDB from an MS-DOS command prompt does not impose the 
limitations, and the kill signal will be handled normally by the server, 
allowing it to shut down normally.

