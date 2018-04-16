Installation
============

Head to [arangodb.com/download](https://www.arangodb.com/download/),
select your operating system and download ArangoDB. You may also follow
the instructions on how to install with a package manager, if available.

If you installed a binary package under Linux, the server is
automatically started.

If you installed ArangoDB using homebrew under MacOS X, start the
server by running `/usr/local/sbin/arangod`.

If you installed ArangoDB under Windows as a service, the server is
automatically started. Otherwise, run the `arangod.exe` located in the
installation folder's `bin` directory. You may have to run it as administrator
to grant it write permissions to `C:\Program Files`.

For more in-depth information on how to install ArangoDB, as well as available
startup parameters, installation in a cluster and so on, see
[Installation](../Installation/README.md) and
[Deployment](../Deployment/README.md).

{% hint 'info' %}
ArangoDB offers two [**storage engines**](../Architecture/StorageEngines.md):
MMFiles and RocksDB. Choose the one which suits your needs best in the
installation process or on first startup.
{% endhint %}


Securing the installation
-------------------------

The default installation contains one database *_system* and a user
named *root*.

Debian based packages and the Windows installer will ask for a
password during the installation process. Red-Hat based packages will
set a random password. For all other installation packages you need to
execute

```
shell> arango-secure-installation
```

This will ask for a root password and sets this password.