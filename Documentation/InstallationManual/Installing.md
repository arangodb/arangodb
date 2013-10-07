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


Apples App Store {#InstallingMacOSXAppStore}
--------------------------------------------

ArangoDB is available in Apple's App-Store. Please note, that it
sometimes take a few days or weeks until the latest versions will be
available.
