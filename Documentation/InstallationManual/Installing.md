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

Follow the instructions on the @EXTREF_S{http://www.arangodb.org/download,Downloads} 
page to use your favorite package manager for the major distributions. After setting 
up the ArangoDB repository you can then easily install ArangoDB using yum, aptitude, 
urpmi, or zypper.

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

The preferred method for installing ArangoDB under Mac OS X is
homebrew. However, in case you are not using homebrew, we provide a command-line
app which contains all the executables.

There is also a version available in the AppStore, which comes with a nice
graphical user interface to start and stop the server. 

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

If you want to install the latest (unstable) version use:

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

ArangoDB is available in Apple's App-Store. Please note, that it sometimes takes 
days or weeks until the latest versions are available. 

Command-Line App {#InstallingMacOSXBundle}
------------------------------------------

In case you are not using homebrew, we also provide a command-line app. You can
download it from

    http://www.arangodb.org/download

Choose `Mac OS X` and go to `Grab binary packages directly`. This allows you to
install the application `ArangoDB-CLI` in your application folder.

Starting the application will start the server and open a terminal window
showing you the log-file.

    ArangoDB server has been started

    The database directory is located at
       '/Applications/ArangoDB-CLI.app/Contents/MacOS/opt/arangodb/var/lib/arangodb'

    The log file is located at
       '/Applications/ArangoDB-CLI.app/Contents/MacOS/opt/arangodb/var/log/arangodb/arangod.log'

    You can access the server using a browser at 'http://127.0.0.1:8529/'
    or start the ArangoDB shell
       '/Applications/ArangoDB-CLI.app/Contents/MacOS/arangosh'

    Switching to log-file now, killing this windows will NOT stop the server.


    2013-10-27T19:42:04Z [23840] INFO ArangoDB (version 1.4.devel [darwin]) is ready for business. Have fun!

Note that it is possible to install the homebrew version and the command-line
app. You should, however, edit the configuration files of one version and change
the port used.

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
that you need to choose the correct package for your platform.

The msi installer will install the ArangoDB server (arangod.exe), the
ArangoDB shell (arangosh.exe) and the ArangoDB import tool (arangoimp.exe) 
in a directory of the user's choice (defaulting to `c:\triAGENS`).

Included Files {#InstallingWindowsFiles}
----------------------------------------

Included in the distribution are some `.bat` files that can be used 
to easily start the ArangoDB server and shell. The `.bat` files will be
installed in the same directory as ArangoDB so they should be easy to find.

Along with ArangoDB some example configuration files (with `.conf` file
ending) will be installed. These configuration files are used by the 
batch files with the same name, and you might adjust them for your own
needs.

The following executables are provided:
- `arangod.exe`: the ArangoDB server binary
- `arangosh.exe`: the ArangoShell (arangosh) binary
- `arangoimp.exe`: an import tool for ArangoDB
- `arangodump.exe`: a dump tool for ArangoDB
- `arangorestore.exe`: a restore tool for ArangoDB

You can invoke any of the above executables on the command-line directly,
however, to properly run each executable needs some configuration. The
configuration can be provided as command-line arguments when invoking the
executable, or be specified in configuration files.

ArangoDB is shipped with a few example configuration files and example 
batch files that can be used to easily invoke some of the executables with
the default configuration.

The following configuration files are provided:
- `arangod.conf`: configuration for the ArangoDB server, used by 
  `arangod.bat`, `console.bat`, and `upgrade.bat`
- `arangosh.conf`: configuration for the ArangoDB shell, used by
  `arangosh.bat` and `foxx-manager.bat`
- `arangoimp.conf`: configuration for the ArangoDB import tool
- `arangodump.conf`: configuration for the ArangoDB dump tool
- `arangorestore.conf`: configuration for the ArangoDB restore tool

The following batch files are provided:
- `arangod.bat`: starts the ArangoDB server, with networking enabled
- `console.bat`: starts the ArangoDB server in emergency console mode, 
  with networking disabled
- `upgrade.bat`: upgrades an existing ArangoDB server
- `arangosh.bat`: starts the ArangoDB shell (requires an already
  running ArangoDB server instance)
- `foxx-manager.bat`: installation utility for Foxx applications

Starting ArangoDB {#InstallingWindowsStarting}
----------------------------------------------

To start the ArangoDB server, use the file `arangod.bat`. This wil start
the ArangoDB server with networking enabled. It will use the configuration
specified in file `arangod.conf`. Once started, the ArangoDB server will
run until you terminate it pressing CTRL-C. Starting ArangoDB for the first 
time will automatically create a database sub-directory in the directory 
ArangoDB was installed in.

Once the ArangoDB server is running, you can use your browser to check 
whether you can connect. Please navigate to confirm:

http://127.0.0.1:8529/

Please note that when using ArangoDB's web interface with Internet Explorer
(IE), you will need IE version 9 or higher to use all features. The web 
interface partly relies on SVG, which is not available in previous versions 
of IE.

To start the ArangoDB shell (_arangosh_), use the batch file `arangosh.bat`
while the ArangoDB server is already running.

If you already have a previous version of ArangoDB installed and want to
upgrade to a newer version, use the batch file `upgrade.bat` This will
start ArangoDB with the `--upgrade` option and perform a migration of an
existing database. Please note that you need to stop a running ArangoDB
server instance before you upgrade. 
Please also check the output of the `upgrade.bat` run for any potential
errors. If the upgrade completes successfully, you can restart the server
regularly using the `arangod.bat` script.

To run any of the ArangoDB executables in your own environment, you will
probably need to adjust the configuration. It is advised that you use a
separate configuration file, and specify the configuration filename on the
command-line when invoking the executable as follows (example for _arangod_):

    > arangod.exe -c path\to\arangod.conf

Limitations for Cygwin {#InstallingWindowsCygwin}
-------------------------------------------------

Please note some important limitations when running ArangoDB under Cygwin:
Starting ArangoDB can be started from out of a Cygwin terminal, but pressing
CTRL-C will forcefully kill the server process, without giving it a chance to 
handle the kill signal. In this case, a regular server shutdown is not
possible, which may leave a file `LOCK` around in the server's data directory.
This file needs to be removed manually to make ArangoDB start again. 
Additionally, as ArangoDB does not have a chance to handle the kill signal,
the server cannot forcefully flush any data to disk on shutdown, leading to
potential data loss.
When starting ArangoDB from a Cygwin terminal it might also happen that no
errors are printed in the terminal output.
Starting ArangoDB from an MS-DOS command prompt does not impose these 
limitations and is thus the preferred method.

Please note that ArangoDB uses UTF-8 as its internal encoding and that the
system console must support a UTF-8 codepage (65001) and font. It may be
necessary to manually switch the console font to a font that supports UTF-8.
