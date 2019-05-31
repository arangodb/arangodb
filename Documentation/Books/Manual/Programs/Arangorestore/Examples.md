Arangorestore Examples
======================

To restore data from a dump previously created with [_Arangodump_](../Arangodump/README.md),
ArangoDB provides the _arangorestore_ tool.

{% hint 'danger' %}
In versions older than 3.3, _Arangorestore_
**must not be used to create several similar database instances in one installation**.

This means that if you have an _Arangodump_ output of database ***A***, create a second database ***B***
on the same instance of ArangoDB, and restore the dump of ***A*** into ***B*** - data integrity can not
be guaranteed. This limitation was solved starting from ArangoDB v3.3.0.
{% endhint %}

Invoking Arangorestore
----------------------

_arangorestore_ can be invoked from the command-line as follows:

    arangorestore --input-directory "dump"

This will connect to an ArangoDB server (tcp://127.0.0.1:8529 by default), then restore the
collection structure and the documents from the files found in the input directory *dump*.
Note that the input directory must have been created by running *arangodump* before.

_arangorestore_ will by default connect to the *_system* database using the default
endpoint. To override the endpoint, or specify a different user, use one of the
following startup options:

- `--server.endpoint <string>`: endpoint to connect to
- `--server.username <string>`: username
- `--server.password <string>`: password to use
  (omit this and you'll be prompted for the password)
- `--server.authentication <bool>`: whether or not to use authentication

If you want to connect to a different database or dump all databases you can additionally
use the following startup options:

- `--server.database <string>`: name of the database to connect to.
  Defaults to the `_system` database.
- `--all-databases true`: restore multiple databases from a dump which used the same option.
  Introduced in v3.5.0.

Note that the specified user must have access to the database(s).
 
Since version 2.6 _arangorestore_ provides the option *--create-database*. Setting this
option to *true* will create the target database if it does not exist. When creating the
target database, the username and passwords passed to _arangorestore_ (in options
*--server.username* and *--server.password*) will be used to create an initial user for the
new database.

The option `--force-same-database` allows restricting arangorestore operations to a
database with the same name as in the source dump's `dump.json` file. It can thus be used
to prevent restoring data into a "wrong" database by accident.

For example, if a dump was taken from database ***A***, and the restore is attempted into 
database ***B***, then with the `--force-same-database` option set to `true`, arangorestore
will abort instantly.

The `--force-same-database` option is set to `false` by default to ensure backwards-compatibility.

Here's an example of reloading data to a non-standard endpoint, using a dedicated
[database name](../../Appendix/Glossary.md#database-name):

    arangorestore --server.endpoint tcp://192.168.173.13:8531 --server.username backup --server.database mydb --input-directory "dump"

To create the target database whe restoring, use a command like this:

    arangorestore --server.username backup --server.database newdb --create-database true --input-directory "dump"

In contrast to the above calls, when working with multiple databases using `--all-databases true`
the parameter `--server.database mydb` must not be specified:

    arangorestore --server.username backup --all-databases true --create-database true --input-directory "dump-multiple"
    
_arangorestore_ will print out its progress while running, and will end with a line
showing some aggregate statistics:

    Processed 2 collection(s), read 2256 byte(s) from datafiles, sent 2 batch(es)


By default, _arangorestore_ will re-create all non-system collections found in the input
directory and load data into them. If the target database already contains collections
which are also present in the input directory, the existing collections in the database
will be dropped and re-created with the data found in the input directory.

The following parameters are available to adjust this behavior:

- `--create-collection <bool>`: set to *true* to create collections in the target
  database. If the target database already contains a collection with the same name,
  it will be dropped first and then re-created with the properties found in the input
  directory. Set to *false* to keep existing collections in the target database. If
  set to *false* and _arangorestore_ encounters a collection that is present in the
  input directory but not in the target database, it will abort. The default value is *true*.
- `--import-data <bool>`: set to *true* to load document data into the collections in
  the target database. Set to *false* to not load any document data. The default value 
  is *true*.
- `--include-system-collections <bool>`: whether or not to include system collections
  when re-creating collections or reloading data. The default value is *false*.

For example, to (re-)create all non-system collections and load document data into them, use:

    arangorestore --create-collection true --import-data true --input-directory "dump"

This will drop potentially existing collections in the target database that are also present
in the input directory.

To include system collections too, use `--include-system-collections true`:
    
    arangorestore --create-collection true --import-data true --include-system-collections true --input-directory "dump"

To (re-)create all non-system collections without loading document data, use:

    arangorestore --create-collection true --import-data false --input-directory "dump"

This will also drop existing collections in the target database that are also present in the
input directory.

To just load document data into all non-system collections, use:

    arangorestore --create-collection false --import-data true --input-directory "dump"

To restrict reloading to just specific collections, there is is the `--collection` option.
It can be specified multiple times if required:

    arangorestore --collection myusers --collection myvalues --input-directory "dump"

Collections will be processed by in alphabetical order by _arangorestore_, with all document
collections being processed before all [edge collections](../../Appendix/Glossary.md#edge-collection).
This remains valid also when multiple threads are in use (from v3.4.0 on).

Note however that when restoring an edge collection no internal checks are made in order to validate that
the documents that the edges connect exist or not. As a consequence, when restoring individual collections
which are part of a graph you are not required to restore in a specific order. 

{% hint 'warning' %}
When restoring only a subset of collections of your database, and graphs are in use, you will need
to make sure you are restoring all the needed collections (the ones that are part of the graph) as 
otherwise you might have edges pointing to non existing documents.
{% endhint %}

To restrict reloading to specific views, there is the `--view` option.
Should you specify the `--collection` parameter views will not be restored _unless_ you explicitly
specify them via the `--view` option.

    arangorestore --collection myusers --view myview --input-directory "dump"

In the case of an arangosearch view you must make sure that the linked collections are either
also restored or already present on the server.

Encryption
----------

See [Arangodump](../Arangodump/Examples.md#encryption) for details.

Reloading Data into a different Collection
------------------------------------------

_arangorestore_ will restore document and edges data with the exact same *_key*, *_rev*, *_from*
and *_to* values as found in the input directory.

With some creativity you can also use _arangodump_ and _arangorestore_ to transfer data from one
collection into another (either on the same server or not). For example, to copy data from
a collection *myvalues* in database *mydb* into a collection *mycopyvalues* in database *mycopy*,
you can start with the following command:

    arangodump --collection myvalues --server.database mydb --output-directory "dump"

This will create two files, `myvalues.structure.json` and `myvalues.data.json`, in the output 
directory. To load data from the datafile into an existing collection *mycopyvalues* in database 
*mycopy*, rename the files to `mycopyvalues.structure.json` and `mycopyvalues.data.json`.

After that, run the following command:

    arangorestore --collection mycopyvalues --server.database mycopy --input-directory "dump"

Restoring in a Cluster
----------------------

From v2.1 on, the *arangorestore* tool supports sharding and can be
used to restore data into a Cluster. Simply point it to one of the
_Coordinators_ in your Cluster and it will work as usual but on sharded
collections in the Cluster.

If *arangorestore* is asked to restore a collection, it will use the same
number of shards, replication factor and shard keys as when the collection
was dumped. The distribution of the shards to the servers will also be the
same as at the time of the dump, provided that the number of _DBServers_ in
the cluster dumped from is identical to the number of DBServers in the
to-be-restored-to cluster.

To modify the number of _shards_ or the _replication factor_ for all or just
some collections, *arangorestore* provides the options `--number-of-shards`
and `--replication-factor` (starting from v3.3.22 and v3.4.2). These options
can be specified multiple times as well, in order to override the settings
for dedicated collections, e.g.

    arangorestore --number-of-shards 2 --number-of-shards mycollection=3 --number-of-shards test=4

The above will restore all collections except "mycollection" and "test" with
2 shards. "mycollection" will have 3 shards when restored, and "test" will
have 4. It is possible to omit the default value and only use
collection-specific overrides. In this case, the number of shards for any
collections not overridden will be determined by looking into the
"numberOfShards" values contained in the dump.

The `--replication-factor` options works in the same way, e.g.

    arangorestore --replication-factor 2 --replication-factor mycollection=1

will set the replication factor to 2 for all collections but "mycollection", which will get a
replication factor of just 1.

{% hint 'info' %}
The options `--number-of-shards` and `replication-factor`, as well as the deprecated
options `--default-number-of-shards` and `--default-replication-factor`, are
**not applicable to system collections**. They are managed by the server.
{% endhint %}

If a collection was dumped from a single instance and is then restored into
a cluster, the sharding will be done by the `_key` attribute by default. One can
manually edit the structural description for the shard keys in the dump files if
required (`*.structure.json`).

If you restore a collection that was dumped from a cluster into a single
ArangoDB instance, the number of shards, replication factor and shard keys will silently
be ignored.

### Factors affecting speed of arangorestore in a Cluster

The following factors affect speed of _arangorestore_ in a Cluster:

- **Replication Factor**: the higher the _replication factor_, the more
  time the restore will take. To speed up the restore you can restore
  using a _replication factor_ of 1 and then increase it again
  after the restore. This will reduce the number of network hops needed
  during the restore.
- **Restore Parallelization**: if the collections are not restored in
  parallel, the restore speed is highly affected. A parallel restore can
  be done from v3.4.0 by using the `--threads` option of _arangorestore_.
  Before v3.4.0 it is possible to achieve parallelization by restoring
  on multiple _Coordinators_ at the same time. Depending on your specific
  case, parallelizing on multiple _Coordinators_ can still be useful even
  when the `--threads` option is in use (from v.3.4.0).

{% hint 'tip' %}
Please refer to the [Fast Cluster Restore](FastClusterRestore.md) page
for further operative details on how to take into account, when restoring
using _arangorestore_, the two factors described above.
{% endhint %}

### Restoring collections with sharding prototypes

*arangorestore* will yield an error when trying to restore a
collection whose shard distribution follows a collection which does
not exist in the cluster and which was not dumped along:

    arangorestore --collection clonedCollection --server.database mydb --input-directory "dump"

    ERROR got error from server: HTTP 500 (Internal Server Error): ArangoError 1486: must not have a distributeShardsLike attribute pointing to an unknown collection
    Processed 0 collection(s), read 0 byte(s) from datafiles, sent 0 batch(es)

The collection can be restored by overriding the error message as
follows:

    arangorestore --collection clonedCollection --server.database mydb --input-directory "dump" --ignore-distribute-shards-like-errors

Restore into an authentication-enabled ArangoDB
-----------------------------------------------

Of course you can restore data into a password-protected ArangoDB as well.
However this requires certain user rights for the user used in the restore process.
The rights are described in detail in the [Managing Users](../../Administration/ManagingUsers/README.md) chapter.
For restore this short overview is sufficient:

- When importing into an existing database, the given user needs `Administrate`
  access on this database.
- When creating a new database during restore, the given user needs `Administrate`
  access on `_system`. The user will be promoted with `Administrate` access on the
  newly created database.
