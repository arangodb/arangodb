Upgrading to ArangoDB 3.0
=========================

Please read the following sections if you upgrade from a previous
version to ArangoDB 3.0. Please be sure that you have checked the list
of [changes in 3.0](../../ReleaseNotes/UpgradingChanges30.md) before
upgrading.

Migrating databases and collections from ArangoDB 2.8 to 3.0
------------------------------------------------------------

ArangoDB 3.0 does not provide an automatic update mechanism for database
directories created with the 2.x branches of ArangoDB.

In order to migrate data from ArangoDB 2.8 (or an older 2.x version) into
ArangoDB 3.0, it is necessary to export the data from 2.8 using `arangodump`, 
and then import the dump into a fresh ArangoDB 3.0 with `arangorestore`.

To do this, first run the 2.8 version of `arangodump` to export the database
data into a directory. `arangodump` will dump the `_system` database by default. 
In order to make it dump multiple databases, it needs to be invoked once per
source database, e.g.

```
# in 2.8
arangodump --server.database _system --output-directory dump-system
arangodump --server.database mydb --output-directory dump-mydb
...
```

That will produce a dump directory for each database that `arangodump` is
called for. If the server has authentication turned on, it may be necessary to
provide the required credentials when invoking `arangodump`, e.g.

```
arangodump --server.database _system --server.username myuser --server.password mypasswd --output-directory dump-system
```

The dumps produced by `arangodump` can now be imported into ArangoDB 3.0 using 
the 3.0 version of `arangodump`:

```
# in 3.0
arangorestore --server.database _system --input-directory dump-system
arangorestore --server.database mydb --input-directory dump-mydb
...
```

arangorestore will by default fail if the target database does not exist. It can
be told to create it automatically using the option `--create-database true`:

```
arangorestore --server.database mydb --create-database true --input-directory dump-mydb
```
And again it may be required to provide access credentials when invoking 
`arangorestore`:

```
arangorestore --server.database mydb --create-database true --server.username myuser --server.password mypasswd --input-directory dump-system
```

Please note that the version of dump/restore should match the server version, i.e.
it is required to dump the original data with the 2.8 version of `arangodump` 
and restore it with the 3.0 version of `arangorestore`. 

After that the 3.0 instance of ArangoDB will contain the databases and collections 
that were present in the 2.8 instance.

Adjusting authentication info
-----------------------------

Authentication information was stored per database in ArangoDB 2.8, meaning there 
could be different users and access credentials per database. In 3.0, the users are
stored in a central location in the `_system` database. To use the same user setup
as in 2.8, it may be required to create extra users and/or adjust their permissions.

In order to do that, please connect to the 3.0 instance with an ArangoShell (this
will connect to the `_system` database by default):

```
arangosh --server.username myuser --server.password mypasswd
```

Use the following commands to create a new user with some password and grant them
access to a specific database

```
require("@arangodb/users").save(username, password, true);
require("@arangodb/users").grantDatabase(username, databaseName, "rw");
```

For example, to create a user `myuser` with password `mypasswd` and give them 
access to databases `mydb1` and `mydb2`, the commands would look as follows:

```
require("@arangodb/users").save("myuser", "mypasswd", true);
require("@arangodb/users").grantDatabase("myuser", "mydb1", "rw");
require("@arangodb/users").grantDatabase("myuser", "mydb2", "rw");
```

Existing users can also be updated, removed or listed using the following
commands:

```
/* update user myuser with password mypasswd */
require("@arangodb/users").update("myuser", "mypasswd", true); 

/* remove user myuser */
require("@arangodb/users").remove("myuser");

/* list all users */
require("@arangodb/users").all();
```

Foxx applications
-----------------

The dump/restore procedure described above will not export and re-import Foxx applications.
In order to move these from 2.8 to 3.0, Foxx applications should be exported as zip files 
via the 2.8 web interface.

The zip files can then be uploaded in the "Services" section in the ArangoDB 3.0 web interface.
Applications may need to be adjusted manually to run in 3.0. Please consult the 
[migration guide for Foxx apps](../../Foxx/Migrating2x/README.md).

An alternative way of moving Foxx apps into 3.0 is to copy the source directory of a 2.8 Foxx
application manually into the 3.0 Foxx apps directory for the target database (which is normally
`/var/lib/arangodb3-apps/_db/<dbname>/` but the exact location is platform-specific).
