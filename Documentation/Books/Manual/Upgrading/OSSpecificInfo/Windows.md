**TMP**

Upgrading on Windows
====================

As there are different ways to install ArangoDB on Windows, the upgrade
method depends on the installation method that was used.

In general, it will be needed to:

- install the new ArangoDB binaries on the system
- upgrade the database
- optinal (but suggested) to keep the system clean (unless there are specific
  reasons to not do so): remove the old binaries from the system

Some of the above steps may be done automatically, depending on your
specifc situation.

Upgrading via the Installer
---------------------------

If you have installed via the _Installer_, to upgrade:

- Download the new _Installer_ and run it.
- The _Installer_ will ask if you want to update your current database: select
  the option "_Automatically update existing ArangoDB database_" so that the database
  files will be upgraded.

![Update Option](installer_upgrade.png)

{% hint 'info' %} 
Upgrading via the Installer, when the old data is kept, will keep your 
password and choice of storage engine as it is.
{% endhint %}

- After installing the new package, you will have both packages installed.
You can uninstall the old one manually (make a copy of your old configuration
file first).

![Uninstall old version](both_installations.png)

{% hint 'danger' %} 
When uninstalling the old package, please make sure the option
"_Delete databases with unistallation_" is **not** checked.
{% endhint %}

{% hint 'danger' %} 
When upgrading, the Windows Installer does not use the old configuration file
for the installed _Single Instance_ but a new (default) one ([Issue #3773](https://github.com/arangodb/arangodb/issues/3773)).
To use the old configuration, it is currently needed to:
- stop the server
- replace the new with the old confituration file
- restart the server
{% endhint %}

![Delete Option](installer_delete.png)


Manual upgrade of 'ZIP archive' installation
--------------------------------------------

There are two ways to upgrade a _Single Instance_ that has been started
from a _ZIP_ package:

- In-Place upgrade
- Logical upgrade

### In-Place upgrade


### Logical upgrade

1. Download the new package and extract it on a different location than the
   previous one
2. Stop writes to the old server (e.g. block incoming connections)
3. Take a backup of the data using _arangodump_
4. Stop the old server
5. Optional (depending on whether or not you modfied default confiugaration),
   copy old ArangoDB configuration file to the new server (or just edit
   the new configuration file)
6. Start the new server (with a fresh data directory, by default it will be
   inside the directory created when extracting the _ZIP_ archive)
7. Restore the backup into the new server using _arangorestore_
8. Re-enable the writes (e.g. allow again incoming connections)
9. Optionally remove the old server package when you are confident enough
   that all is working fine (a copy of the directory package, 
