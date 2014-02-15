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

Follow the instructions on the
@EXTREF_S{http://www.arangodb.org/download,Downloads} 
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

If you are using @EXTREF_S{http://brew.sh/,homebrew},
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

Choices{#InstallingWindowsChoices}
----------------------------------

The default installation directory is `c:\Program Files\ArangoDB-1.x.y`. During the
installation process you may change this. In the following description we will assume
that ArangoDB has been installed in the location `<ROOTDIR>`.

You have to be careful when choosing an installation directory. You need either
write permission to this directoy or you need to modify the config file for the
server process. In the latter case the database directory and the Foxx directory
should must be writable by the user.

Installating for a single user: Select a different directory during
installation. For example `C:/Users/<username>/arangodb` or `C:/ArangoDB`.

Installating for multiple users: Keep the default directory. After the
installation edit the file `<ROOTDIR>/etc/arangodb/arangod.conf`. Adjust the
`directory` and `app-path` so that these paths point into your home directory.

       [database]
       directory = @HOMEDRIVE@/@HOMEPATH@/arangodb/databases

       [javascript]
       app-path = @HOMEDRIVE@/@HOMEPATH@/arangodb/apps

       Create the directories for each user that wants to use ArangoDB.

Installating as Service: Keep the default directory. After the installation open
a command line as administrator (search for `cmd` and right click `run as
administrator`).

       cmd> arangod --install-service
       INFO: adding service 'ArangoDB - the multi-purpose database' (internal 'ArangoDB')
       INFO: added service with command line '"C:\Program Files (x86)\ArangoDB 1.4.4\bin\arangod.exe" --start-service'

       Open the service manager and start ArangoDB. In order to enable logging
       edit the file "<ROOTDIR>/etc/arangodb/arangod.conf" and uncomment the file
       option.

       [log]
       file = @ROOTDIR@/var/log/arangodb/arangod.log

Client, Server and Lock-Files{#InstallingWindowsFiles}
------------------------------------------------------

Please note that ArangoDB consists of a database server and client tools. If you
start the server, it will place a (read-only) lock file to prevent accidental
access to the data. The server will attempt to remove this lock file when it is
started to see if the lock is still valid - this is in case the installation did
not proceed correctly or if the server terminated unexpectedly.

Starting{#InstallingWindowsStarting}
------------------------------------

To start an ArangoDB server instance with networking enabled, use the executable
`arangod.exe` located in `<ROOTDIR>/bin`. This will use the configuration
file `arangod.conf` located in `<ROOTDIR>/etc/arangodb`, which you can adjust
to your needs and use the data directory "<ROOTDIR>/var/lib/arangodb". This
is the place where all your data (databases and collections) will be stored
by default.

Please check the output of the `arangod.exe` executable before going on. If the
server started successully, you should see a line `ArangoDB is ready for
business. Have fun!` at the end of its output.

We now wish to check that the installation is working correctly and to do this
we will be using the administration web interface. Execute `arangod.exe` if you
have not already done so, then open up your web browser and point it to the
page: 

    http://127.0.0.1:8529/

To check if your installation was successful, click the `Collection` tab and
open the configutation. Select the `System` type. If the installation was
successful, then the page should display a few system collections.

Try to add a new collection and then add some documents to this new collection.
If you have succeeded in creating a new collection and inserting one or more
documents, then your installation is working correctly.

Advanced Starting{#InstallingWindowsAdvanced}
---------------------------------------------

If you want to provide our own start scripts, you can set the environment
variable `ARANGODB_CONFIG_PATH`. This variable should point to a directory
containing the configuration files.

Using the Client{#InstallingWindowsClient}
------------------------------------------

To connect to an already running ArangoDB server instance, there is a shell
`arangosh.exe` located in `<ROOTDIR>/bin`. This starts a shell which can be
used (amongst other things) to administer and query a local or remote
ArangoDB server.

Note that `arangosh.exe` does NOT start a separate server, it only starts the
shell.  To use it, you must have a server running somewhere, e.g. by using
the `arangod.exe` executable.

`arangosh.exe` uses configuration from the file `arangosh.conf` located in
`<ROOTDIR>/etc/arangodb/`. Please adjust this to your needs if you want to
use different connection settings etc.

32bit{#InstallingWindows32Bit}
------------------------------

If you have an EXISTING database, then please note that currently a 32 bit
version of ArangoDB is NOT compatible with a 64 bit version. This means that
if you have a database created with a 32 bit version of ArangoDB it may
become corrupted if you execute a 64 bit version of ArangoDB against the same
database, and vice versa.

Upgrading{#InstallingWindowsUpgrading}
--------------------------------------

To upgrade an EXISTING database created with a previous version of ArangoDB,
please execute the server `arangod.exe` with the option
`--upgrade`. Otherwise starting ArangoDB may fail with errors.

Note that there is no harm in running the upgrade. So you should run this
batch file if you are unsure of the database version you are using.

You should always check the output for errors to see if the upgrade was
completed successfully.

Uninstalling{#InstallingWindowsUninstalling}
--------------------------------------------

To uninstall the Arango server application you can use the windows control panel
(as you would normally uninstall an application). Note however, that any data
files created by the Arango server will remain as well as the `<ROOTDIR>`
directory.  To complete the uninstallation process, remove the data files and
the `<ROOTDIR>` directory manually.

Limitations for Cygwin{#InstallingWindowsCygwin}
------------------------------------------------

Please note some important limitations when running ArangoDB under Cygwin:
Starting ArangoDB can be started from out of a Cygwin terminal, but pressing
CTRL-C will forcefully kill the server process, without giving it a chance to
handle the kill signal. In this case, a regular server shutdown is not possible,
which may leave a file `LOCK` around in the server's data directory.  This file
needs to be removed manually to make ArangoDB start again.  Additionally, as
ArangoDB does not have a chance to handle the kill signal, the server cannot
forcefully flush any data to disk on shutdown, leading to potential data loss.
When starting ArangoDB from a Cygwin terminal it might also happen that no
errors are printed in the terminal output.  Starting ArangoDB from an MS-DOS
command prompt does not impose these limitations and is thus the preferred
method.

Please note that ArangoDB uses UTF-8 as its internal encoding and that the
system console must support a UTF-8 codepage (65001) and font. It may be
necessary to manually switch the console font to a font that supports UTF-8.

@BNAVIGATE_Installing
