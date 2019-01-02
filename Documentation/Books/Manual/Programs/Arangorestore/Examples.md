Arangorestore Examples
======================

To restore data from a dump previously created with [_Arangodump_](../Arangodump/README.md),
ArangoDB provides the _arangorestore_ tool.

Please note that in versions older than 3.3, _Arangorestore_
**must not be used to create several similar database instances in one installation**.

This means that if you have an _Arangodump_ output of database `a`, create a second database `b`
on the same instance of ArangoDB, and restore the dump of `a` into `b` - data integrity can not
be guaranteed. This limitation was solved starting from ArangoDB version 3.3

Invoking Arangorestore
----------------------

_arangorestore_ can be invoked from the command-line as follows:

    arangorestore --input-directory "dump"

This will connect to an ArangoDB server and reload structural information and 
documents found in the input directory *dump*. Please note that the input directory
must have been created by running *arangodump* before.

_arangorestore_ will by default connect to the *_system* database using the default
endpoint. If you want to connect to a different database or a different endpoint, 
or use authentication, you can use the following command-line options:

- *--server.database <string>*: name of the database to connect to
- *--server.endpoint <string>*: endpoint to connect to
- *--server.username <string>*: username
- *--server.password <string>*: password to use (omit this and you'll be prompted for the
  password)
- *--server.authentication <bool>*: whether or not to use authentication
  
Since version 2.6 _arangorestore_ provides the option *--create-database*. Setting this 
option to *true* will create the target database if it does not exist. When creating the
target database, the username and passwords passed to _arangorestore_ (in options 
*--server.username* and *--server.password*) will be used to create an initial user for the 
new database.

The option `--force-same-database` allows restricting arangorestore operations to a
database with the same name as in the source dump's "dump.json" file. It can thus be used
to prevent restoring data into a "wrong" database by accident.

For example, if a dump was taken from database `a`, and the restore is attempted into 
database `b`, then with the `--force-same-database` option set to `true`, arangorestore
will abort instantly.

The `--force-same-database` option is set to `false` by default to ensure backwards-compatibility.

Here's an example of reloading data to a non-standard endpoint, using a dedicated
[database name](../../Appendix/Glossary.md#database-name):

    arangorestore --server.endpoint tcp://192.168.173.13:8531 --server.username backup --server.database mydb --input-directory "dump"

To create the target database whe restoring, use a command like this:
    
    arangorestore --server.username backup --server.database newdb --create-database true --input-directory "dump"

_arangorestore_ will print out its progress while running, and will end with a line
showing some aggregate statistics:

    Processed 2 collection(s), read 2256 byte(s) from datafiles, sent 2 batch(es)


By default, _arangorestore_ will re-create all non-system collections found in the input 
directory and load data into them. If the target database already contains collections 
which are also present in the input directory, the existing collections in the database 
will be dropped and re-created with the data found in the input directory.

The following parameters are available to adjust this behavior:

- *--create-collection <bool>*: set to *true* to create collections in the target
  database. If the target database already contains a collection with the same name,
  it will be dropped first and then re-created with the properties found in the input
  directory. Set to *false* to keep existing collections in the target database. If 
  set to *false* and _arangorestore_ encounters a collection that is present in the
  input directory but not in the target database, it will abort. The default value is *true*.
- *--import-data <bool>*: set to *true* to load document data into the collections in
  the target database. Set to *false* to not load any document data. The default value 
  is *true*.
- *--include-system-collections <bool>*: whether or not to include system collections
  when re-creating collections or reloading data. The default value is *false*.
  
For example, to (re-)create all non-system collections and load document data into them, use:

    arangorestore --create-collection true --import-data true --input-directory "dump"

This will drop potentially existing collections in the target database that are also present
in the input directory.

To include system collections too, use *--include-system-collections true*:
    
    arangorestore --create-collection true --import-data true --include-system-collections true --input-directory "dump"

To (re-)create all non-system collections without loading document data, use:

    arangorestore --create-collection true --import-data false --input-directory "dump"

This will also drop existing collections in the target database that are also present in the
input directory.

To just load document data into all non-system collections, use:

    arangorestore --create-collection false --import-data true --input-directory "dump"

To restrict reloading to just specific collections, there is is the *--collection* option.
It can be specified multiple times if required:
    
    arangorestore --collection myusers --collection myvalues --input-directory "dump"

Collections will be processed by in alphabetical order by _arangorestore_, with all document
collections being processed before all [edge collection](../../Appendix/Glossary.md#edge-collection)s. This is to ensure that reloading
data into edge collections will have the document collections linked in edges (*_from* and
*_to* attributes) loaded.

To restrict reloading to specific views, there is the *--view* option.
Should you specify the *--collection* parameter views will not be restored _unless_ you explicitly
specify them via the *--view* option.
    
    arangorestore --collection myusers --view myview --input-directory "dump"
    
In the case of an arangosearch view you must make sure that the linked collections are either
also restored or already present on the server.

Encryption
----------

See [Arangodump](../Arangodump/Examples.md#encryption) for details.

Restoring Revision IDs and Collection IDs
-----------------------------------------
 
_arangorestore_ will reload document and edges data with the exact same *_key*, *_from* and 
*_to* values found in the input directory. However, when loading document data, it will assign
its own values for the *_rev* attribute of the reloaded documents. Though this difference is 
intentional (normally, every server should create its own *_rev* values) there might be 
situations when it is required to re-use the exact same *_rev* values for the reloaded data.
This can be achieved by setting the *--recycle-ids* parameter to *true*:

    arangorestore --collection myusers --collection myvalues --input-directory "dump"

Note that setting *--recycle-ids* to *true* will also cause collections to be (re-)created in
the target database with the exact same collection id as in the input directory. Any potentially
existing collection in the target database with the same collection id will then be dropped.

Reloading Data into a different Collection
------------------------------------------

With some creativity you can use _arangodump_ and _arangorestore_ to transfer data from one
collection into another (either on the same server or not). For example, to copy data from
a collection *myvalues* in database *mydb* into a collection *mycopyvalues* in database *mycopy*,
you can start with the following command:

    arangodump --collection myvalues --server.database mydb --output-directory "dump"

This will create two files, *myvalues.structure.json* and *myvalues.data.json*, in the output 
directory. To load data from the datafile into an existing collection *mycopyvalues* in database 
*mycopy*, rename the files to *mycopyvalues.structure.json* and *mycopyvalues.data.json*.
After that, run the following command:
    
    arangorestore --collection mycopyvalues --server.database mycopy --input-directory "dump"

Using arangorestore with sharding
---------------------------------

As of Version 2.1 the *arangorestore* tool supports sharding. Simply
point it to one of the coordinators in your cluster and it will
work as usual but on sharded collections in the cluster.

If *arangorestore* is asked to drop and re-create a collection, it
will use the same number of shards and the same shard keys as when
the collection was dumped. The distribution of the shards to the
servers will also be the same as at the time of the dump. This means 
in particular that DBservers with the same IDs as before must be present in the
cluster at time of the restore. 

If a collection was dumped from a single instance, one can manually
add the structural description for the shard keys and the number and
distribution of the shards and then the restore into a cluster will
work.

If you restore a collection that was dumped from a cluster into a single
ArangoDB instance, the number of shards and the shard keys will silently
be ignored.

Note that in a cluster, every newly created collection will have a new
ID, it is not possible to reuse the ID from the originally dumped
collection. This is for safety reasons to ensure consistency of IDs.

### Restoring collections with sharding prototypes

*arangorestore* will yield an error, while trying to restore a
collection, whose shard distribution follows a collection, which does
not exist in the cluster and which was not dumped along:

    arangorestore --collection clonedCollection --server.database mydb --input-directory "dump"

    ERROR got error from server: HTTP 500 (Internal Server Error): ArangoError 1486: must not have a distributeShardsLike attribute pointing to an unknown collection
    Processed 0 collection(s), read 0 byte(s) from datafiles, sent 0 batch(es)

The collection can be restored by overriding the error message as
follows:

    arangorestore --collection clonedCollection --server.database mydb --input-directory "dump" --ignore-distribute-shards-like-errors

Restore into an authentication enabled ArangoDB
-----------------------------------------------

Of course you can restore data into a password protected ArangoDB as well.
However this requires certain user rights for the user used in the restore process.
The rights are described in detail in the [Managing Users](../../Administration/ManagingUsers/README.md) chapter.
For restore this short overview is sufficient:

- When importing into an existing database, the given user needs `Administrate`
  access on this database.
- When creating a new Database during restore, the given user needs `Administrate`
  access on `_system`. The user will be promoted with `Administrate` access on the
  newly created database.
