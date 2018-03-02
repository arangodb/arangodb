Windows
=======

The default installation directory is *c:\Program Files\ArangoDB-1.x.y*. During the
installation process you may change this. In the following description we will assume
that ArangoDB has been installed in the location *&lt;ROOTDIR&gt;*.

You have to be careful when choosing an installation directory. You need either
write permission to this directory or you need to modify the config file for the
server process. In the latter case the database directory and the Foxx directory
has to be writable by the user.

Installing for a single user: Select a different directory during
installation. For example *c:\Users\&lt;Username&gt;\ArangoDB* or *c:\ArangoDB*.

Installing for multiple users: Keep the default directory. After the
installation edit the file *&lt;ROOTDIR&gt;\etc\ArangoDB\arangod.conf*. Adjust the
*directory* and *app-path* so that these paths point into your home directory.

       [database]
       directory = @HOMEDRIVE@\@HOMEPATH@\arangodb\databases

       [javascript]
       app-path = @HOMEDRIVE@\@HOMEPATH@\arangodb\apps

       Create the directories for each user that wants to use ArangoDB.

Installing as Service: Keep the default directory. After the installation open
a command line as administrator (search for *cmd* and right click *run as
administrator*).

       cmd> arangod --install-service
       INFO: adding service 'ArangoDB - the multi-model database' (internal 'ArangoDB')
       INFO: added service with command line '"C:\Program Files (x86)\ArangoDB 1.4.4\bin\arangod.exe" --start-service'

       Open the service manager and start ArangoDB. In order to enable logging
       edit the file "<ROOTDIR>\etc\arangodb\arangod.conf" and uncomment the file
       option.

       [log]
       file = @ROOTDIR@\var\log\arangodb\arangod.log


### Client, Server and Lock-Files

Please note that ArangoDB consists of a database server and client tools. If you
start the server, it will place a (read-only) lock file to prevent accidental
access to the data. The server will attempt to remove this lock file when it is
started to see if the lock is still valid - this is in case the installation did
not proceed correctly or if the server terminated unexpectedly.

### Starting

To start an ArangoDB server instance with networking enabled, use the executable
*arangod.exe* located in *&lt;ROOTDIR&gt;\bin*. This will use the configuration
file *arangod.conf* located in *&lt;ROOTDIR&gt;\etc\arangodb*, which you can adjust
to your needs and use the data directory *&lt;ROOTDIR&gt;\var\lib\arangodb*. This
is the place where all your data (databases and collections) will be stored
by default.

Please check the output of the *arangod.exe* executable before going on. If the
server started successfully, you should see a line `ArangoDB is ready for
business. Have fun!` at the end of its output.

We now wish to check that the installation is working correctly and to do this
we will be using the administration web interface. Execute *arangod.exe* if you
have not already done so, then open up your web browser and point it to the
page: 

    http://127.0.0.1:8529/

To check if your installation was successful, click the *Collection* tab and
open the configuration. Select the *System* type. If the installation was
successful, then the page should display a few system collections.

Try to add a new collection and then add some documents to this new collection.
If you have succeeded in creating a new collection and inserting one or more
documents, then your installation is working correctly.

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

### 32bit

If you have an EXISTING database, then please note that currently a 32 bit
version of ArangoDB is NOT compatible with a 64 bit version. This means that
if you have a database created with a 32 bit version of ArangoDB it may
become corrupted if you execute a 64 bit version of ArangoDB against the same
database, and vice versa.

### Upgrading

To upgrade an EXISTING database created with a previous version of ArangoDB,
please execute the server *arangod.exe* with the option
*--upgrade*. Otherwise starting ArangoDB may fail with errors.

Note that there is no harm in running the upgrade. So you should run this
batch file if you are unsure of the database version you are using.

You should always check the output for errors to see if the upgrade was
completed successfully.

### Uninstalling

To uninstall the Arango server application you can use the windows control panel
(as you would normally uninstall an application). Note however, that any data
files created by the Arango server will remain as well as the *&lt;ROOTDIR&gt;*
directory.  To complete the uninstallation process, remove the data files and
the *&lt;ROOTDIR&gt;* directory manually.

### Limitations for Cygwin

Please note some important limitations when running ArangoDB under Cygwin:
Starting ArangoDB can be started from out of a Cygwin terminal, but pressing
*CTRL-C* will forcefully kill the server process without giving it a chance to
handle the kill signal. In this case, a regular server shutdown is not possible,
which may leave a file *LOCK* around in the server's data directory.  This file
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
