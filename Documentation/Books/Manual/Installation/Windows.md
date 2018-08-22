Installing ArangoDB on Windows
==============================

**TMP**

Introduction
------------

There are two possible methods to install ArangoDB on Windows:

1. Automated, using an _NSIS_ Installer.
1. Manual, using a ZIP archive (XCopy installation).

Both installation methods have their own pros and cons.

**Note:** Please note that ArangoDB will only work on 64bit.

Installing using the Installer
------------------------------

The default installation directory is *C:\Program Files\ArangoDB-3.x.x*. During the
installation process you may change this. In the following description we will assume
that ArangoDB has been installed in the location *&lt;ROOTDIR&gt;*.

You have to be careful when choosing an installation directory. You need either
write permission to this directory or you need to modify the configuration file
for the server process. In the latter case the database directory and the Foxx
directory have to be writable by the user.

### Single- and Multiuser Installation

There are two main modes for the installer of ArangoDB.
The installer lets you select:

- multi user installation (default; admin privileges required)
  Will install ArangoDB as service.
- single user installation
  Allow to install Arangodb as normal user.
  Requires manual starting of the database server.

### Installation Options

The checkboxes allow you to chose weather you want to:

- chose custom install paths
- do an automatic upgrade
- keep an backup of your data
- add executables to path
- create a desktop icon

or not.

#### Custom Install Paths

This checkbox controls if you will be able to override
the default paths for the installation in subsequent steps.

The default installation paths are:

Multi User Default:
- Installation: *C:\Program Files\ArangoDB-3.x.x*
- DataBase:     *C:\ProgramData\ArangoDB*
- Foxx Service: *C:\ProgramData\ArangoDB-apps*

Single User Default:
- Installation: *C:\Users\\\<your user\>\AppData\Local\ArangoDB-3.x.x*
- DataBase:     *C:\Users\\\<your user\>\AppData\Local\ArangoDB*
- Foxx Service: *C:\Users\\\<your user\>\AppData\Local\ArangoDB-apps*

We are not using the roaming part of the user's profile, because doing so
avoids the data being synced to the windows domain controller.

#### Automatic Upgrade

If this checkbox is selected the installer will attempt to perform an automatic
update. For more information please see
[Upgrading from Previous Version](#upgrading-from-previous-version).

#### Keep Backup

Select this to create a backup of your database directory during automatic upgrade.
The backup will be created next to your current database directory suffixed by
a time stamp.

#### Add to Path

Select this to add the binary directory to your system's path (multi user
installation) or user's path (single user installation).

#### Desktop Icon

Select if you want the installer to create Desktop Icons that let you:

- access the web inteface
- start the commandline client (arangosh)
- start the database server (single user installation only)

### Upgrading from Previous Version

If you are upgrading ArangoDB from an earlier version you need to copy your old
database directory [to the new default paths](#custom-install-paths). Upgrading
will keep your old data, password and choice of storage engine as it is.
Switching to the RocksDB storage engine requires an
[export](../Programs/Arangoexport/README.md) and
[reimport](../Programs/Arangoimport/README.md) of your data.

### Starting

If you installed ArangoDB for multiple users (as a service) it is automatically
started. Otherwise you need to use the link that was created on you Desktop if
you chose to let the installer create desktop icons or

the executable *arangod.exe* located in
*&lt;ROOTDIR&gt;\bin*. This will use the configuration file *arangod.conf*
located in *&lt;ROOTDIR&gt;\etc\arangodb*, which you can adjust to your needs
and use the data directory *&lt;ROOTDIR&gt;\var\lib\arangodb*. This is the place
where all your data (databases and collections) will be stored by default.

Please check the output of the *arangod.exe* executable before going on. If the
server started successfully, you should see a line `ArangoDB is ready for
business. Have fun!` at the end of its output.

We now wish to check that the installation is working correctly and to do this
we will be using the administration web interface. Execute *arangod.exe* if you
have not already done so, then open up your web browser and point it to the
page:

    http://127.0.0.1:8529/

### Advanced Starting

If you want to provide our own start scripts, you can set the environment
variable *ARANGODB_CONFIG_PATH*. This variable should point to a directory
containing the configuration files.

### Using the Client

To connect to an already running ArangoDB server instance, there is a shell
*arangosh.exe* located in *&lt;ROOTDIR&gt;\bin*. This starts a shell which can be
used – amongst other things – to administer and query a local or remote
ArangoDB server.

Note that *arangosh.exe* does NOT start a separate server, it only starts the
shell.  To use it you must have a server running somewhere, e.g. by using
the *arangod.exe* executable.

*arangosh.exe* uses configuration from the file *arangosh.conf* located in
*&lt;ROOTDIR&gt;\etc\arangodb\*. Please adjust this to your needs if you want to
use different connection settings etc.

### Uninstalling

To uninstall the Arango server application you can use the windows control panel
(as you would normally uninstall an application). Note however, that any data
files created by the Arango server will remain as well as the *&lt;ROOTDIR&gt;*
directory.  To complete the uninstallation process, remove the data files and
the *&lt;ROOTDIR&gt;* directory manually.


Installing using the ZIP archive (XCopy installation)
-----------------------------------------------------

Even if there is a nice guided _Installer_ for Windows, not all users prefer this
type of installation. In order to have a [portable application](http://en.wikipedia.org/wiki/Portable_application),
easily start different ArangoDB Servers on the same machine, and/or for the maximum flexibility,
you might want to install using the ZIP archive ([XCOPY deployment](http://en.wikipedia.org/wiki/XCOPY_deployment)).

### Unzip the archive

Open an explorer, choose a place where you want ArangoDB to be, and unzip the files there. 
It will create its own toplevel directory with the version number in the string.

### Edit the configuration

This step is optional. If the default configuration of ArangoDB do not suite your needs,
you might edit `etc\arangodb3\arangod.conf` to change some configuration options, like

- [the location of the database files](../Administration/Configuration/Wal.html)
- [ports to bind](../Administration/Configuration/Endpoint.html)
- [storage engine](../Administration/Configuration/GeneralArangod.html#storage-engine)

and so on.

### Start the Server

After installation, you may start ArangoDB in several ways. The exact start-up command
depends on the type of ArangoDB deployment you are interested on (Single Server, Master-Slave,
Active Failover, Cluster, DC2DC).

Please refer to the [_Deployment_](../Deployment/README.md) chapter for further information
on deployment.

A reference on how to start and stop ArangoDB, can be found in the
[_Starting & Stopping_](../Administration/StartingStopping.md) page.
