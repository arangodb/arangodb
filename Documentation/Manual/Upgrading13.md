Upgrading to ArangoDB 1.3 {#Upgrading13}
========================================

@NAVIGATE_Upgrading13
@EMBEDTOC{Upgrading13TOC}

Upgrading {#Upgrading13Introduction}
====================================

ArangoDB 1.3 provides a lot of new features and APIs when compared to ArangoDB
1.2. The most important one being true multi-collection transactions support.

The following list contains changes in ArangoDB 1.3 that are not 100%
downwards-compatible to ArangoDB 1.2.

Existing users of ArangoDB 1.2 should read the list carefully and make sure they
have undertaken all necessary steps and precautions before upgrading from
ArangoDB 1.2 to ArangoDB 1.3. Please also check @ref Upgrading13Troubleshooting.


Database Directory Version Check and Upgrade {#Upgrading13VersionCheck}
-----------------------------------------------------------------------

Starting with ArangoDB 1.1, _arangod_ will perform a database version check at
startup. This has not changed in ArangoDB 1.3.

The version check will look for a file named _VERSION_ in its database
directory. If the file is not present, _arangod_ in version 1.3 will perform an
auto-upgrade (can also be considered a database "initialisation"). This
auto-upgrade will create the system collections necessary to run ArangoDB, and
it will also create the VERSION file with a version number like `1.3` inside.

If the _VERSION_ file is present but is from a non-matching version of ArangoDB
(e.g. ArangoDB 1.2 if you upgrade), _arangod_ will refuse to start. Instead, it
will ask you start the server with the option `--upgrade`.  Using the
`--upgrade` option will make the server trigger any required upgrade tasks to
migrate data from ArangoDB 1.2 to ArangoDB 1.3.

This manual invocation of an upgrade shall ensure that users have full control
over when they perform any updates/upgrades of their data, and do not risk
running an incompatible tandem of server and database versions.

If you try starting an ArangoDB 1.3 server with a database created by an earlier
version of ArangoDB, and did not invoke the upgrade procedure, the output of
ArangoDB will look like this:

    > bin/arangod --server.endpoint tcp://127.0.0.1:8529 --database.directory /tmp/12

    ...
    2013-05-10T08:35:59Z [9017] ERROR Database directory version (1.2) is lower than server version (1.3).
    2013-05-10T08:35:59Z [9017] ERROR It seems like you have upgraded the ArangoDB binary. If this is what you wanted to do, please restart with the --upgrade option to upgrade the data in the database directory.
    2013-05-10T08:35:59Z [9017] FATAL Database version check failed. Please start the server with the --upgrade option
    ...

So it is really necessary to forcefully invoke the upgrade procedure. Please
create a backup of your database directory before upgrading. Please also make
sure that the database directory and all subdirectories and files in it are
writeable for the user you run ArangoDB with.

As mentioned, invoking the upgrade procedure can be done by specifying the
additional command line option `--upgrade` as follows:

    > bin/arangod --server.endpoint tcp://127.0.0.1:8529 --database.directory /tmp/12 --upgrade

    ...
    2013-05-10T08:38:41Z [9039] INFO Starting upgrade from version 1.2 to 1.3
    2013-05-10T08:38:41Z [9039] INFO Found 20 defined task(s), 8 task(s) to run
    ...
    2013-05-10T08:38:43Z [9039] INFO Upgrade successfully finished
    2013-05-10T08:38:43Z [9039] INFO database version check passed
    ...

The upgrade procecure will execute the defined tasks to run _arangod_ with all
new features and data formats. It should normally run without problems and
indicate success at its end. If it detects a problem that it cannot fix, it will
halt on the first error and warn you.

Re-starting arangod with the `--upgrade` option will execute only the previously
failed and not yet executed tasks.

Upgrading from ArangoDB 1.2 to ArangoDB 1.3 will rewrite data in the collections'
datafiles. The upgrade procedure will create a backup of all files it processes, 
in case something goes wrong.

In case the upgrade did not produce any problems and ArangoDB works well for you
after the upgrade, you may want to remove these backup files manually.

All collection datafiles will be backed up in the original collection
directories in the database directory. You can easily detect the backup files
because they have a filename ending in `.old`.


Upgrade a binary package {#Upgrading13BinaryPackage}
---------------------------------------------------

Linux:
- Upgrade ArangoDB by package manager (Example `zypper update arangodb`)
- check configuration file: `/etc/arangodb/arangod.conf`
- Upgrade database files with `/etc/init.d/arangodb upgrade`

Mac OS X binary package
- You can find the new Mac OS X packages here: `http://www.arangodb.org/repositories/MacOSX`
- check configuration file: `/etc/arangodb/arangod.conf`
- Upgrade database files `/usr/sbin/arangod --upgrade'

Mac OS X with homebrew
- Upgrade ArangoDB by `brew upgrade arangodb'
- check configuration file: `/usr/local/Cellar/1.X.Y/etc/arangodb/arangod.conf`
- Upgrade database files `/usr/local/sbin/arangod --upgrade`

In case you upgrade from a previous version of ArangoDB, please make sure you
perform the changes to the configuration file as described in @ref Upgrading13Options.
Otherwise ArangoDB 1.3 will not start properly.


New and Changed Command-Line Options{#Upgrading13Options}
---------------------------------------------------------

In order to support node modules and packages, a new command-line option was
introduced:

    --javascript.package-path <directory>

must be used to specify the directory containing the NPM packages. This is option
is presented in the pre-defined configuration files. In case a created your own
configuration, you need to add this option and also make sure that

    --javascript.modules-path <directories>

contains the new `node` directory.

Example values for `--javascript.modules-path` and `--javascript.package-path` are:

    --javascript.modules-path = DIR/js/server/modules;DIR/js/common/modules;DIR/js/node

    --javascript.package-path = DIR/js/npm

where `DIR` is the directory that contains the shared data installed by ArangoDB
during installation. It might be `/usr/local/share/arangodb`, but the actual value is
system-dependent.

Not adding the `node` directory to `--javascript.modules-path` or not setting 
`--javascript.package-path` will result in server startup errors.


The configuration options `--scheduler.report-intervall` and `--dispatcher.report-intervall`
have been renamed to `--scheduler.report-interval` and `--dispatcher.report-interval`.

These are rarely used debugging options that are not contained in any of the configuration
files shipped with ArangoDB, so the changed name should not have an effect for end users.


Removed Features{#Upgrading13RemovedFeatures}
---------------------------------------------

The configure options `--enable-zone-debug` and `--enable-arangob` have been removed.
These should not have been used by end users anyway, so this change should not have
an effect.


Troubleshooting{#Upgrading13Troubleshooting}
============================================

If the server does not start after an upgrade, please check that your configuration
contains the `node` directory in the `--javascript.modules-path`, and that the
parameter `--javascript.package-path` is set.

On systems with rlimit, ArangoDB 1.3 will also require the minimum number of file 
descriptors it can use to be 256. If the limit is set to a lower value, ArangoDB will
try to increase the limit to 256. If raising the limit does fail, ArangoDB will refuse 
to start, and fail with an error message like

    2013-05-10T09:00:40Z [11492] FATAL cannot raise the file descriptor limit to 256, got 'Operation not permitted'

In this case the number of file descriptors should be increased. Please note that 256 
is a minimum value and that you should allow ArangoDB to use much more file descriptors
if you afford it.
  
To avoid the file descriptor check on startup in an emergency case, you can use the 
startup option 

    --server.descriptors-minimum 0


Please also check the logfile written by ArangoDB for further errors messages.

