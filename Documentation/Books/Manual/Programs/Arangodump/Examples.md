Arangodump Examples
===================

_arangodump_ can be invoked in a command line by executing the following command:

    arangodump --output-directory "dump"

This will connect to an ArangoDB server and dump all non-system collections from
the default database (*_system*) into an output directory named *dump*.
Invoking _arangodump_ will fail if the output directory already exists. This is
an intentional security measure to prevent you from accidentally overwriting already
dumped data. If you are positive that you want to overwrite data in the output
directory, you can use the parameter *--overwrite true* to confirm this:

    arangodump --output-directory "dump" --overwrite true

_arangodump_ will by default connect to the *_system* database using the default
endpoint. To override the endpoint, or specify a different user, use one of the
following startup options:

- `--server.endpoint <string>`: endpoint to connect to
- `--server.username <string>`: username
- `--server.password <string>`: password to use (omit this and you'll be prompted for the
  password)
- `--server.authentication <bool>`: whether or not to use authentication

If you want to connect to a different database or dump all databases you can additionaly
use the following startup options:

- `--all-databases true`: must have access to all databases, and not specify a database.
- `--server.database <string>`: name of the database to connect to

Note that the specified user must have access to the databases.

Here's an example of dumping data from a non-standard endpoint, using a dedicated
[database name](../../Appendix/Glossary.md#database-name):

    arangodump --server.endpoint tcp://192.168.173.13:8531 --server.username backup --server.database mydb --output-directory "dump"

In contrast to the above call `--server.database` must not be specified when dumping
all databases using `--all-databases true`:

    arangodump --server.endpoint tcp://192.168.173.13:8531 --server.username backup --all-databases true --output-directory "dump-multiple"

When finished, _arangodump_ will print out a summary line with some aggregate
statistics about what it did, e.g.:

    Processed 43 collection(s), wrote 408173500 byte(s) into datafiles, sent 88 batch(es)

By default, _arangodump_ will dump both structural information and documents from all
non-system collections. To adjust this, there are the following command-line
arguments:

- `--dump-data <bool>`: set to *true* to include documents in the dump. Set to *false*
  to exclude documents. The default value is *true*.
- `--include-system-collections <bool>`: whether or not to include system collections
  in the dump. The default value is *false*. **Set to _true_ if you are using named
  graphs that you are interested in restoring.**

For example, to only dump structural information of all collections (including system
collections), use:

    arangodump --dump-data false --include-system-collections true --output-directory "dump"

To restrict the dump to just specific collections, there is is the *--collection* option.
It can be specified multiple times if required:

    arangodump --collection myusers --collection myvalues --output-directory "dump"

Structural information for a collection will be saved in files with name pattern
*<collection-name>.structure.json*. Each structure file will contains a JSON object
with these attributes:
- *parameters*: contains the collection properties
- *indexes*: contains the collection indexes

Document data for a collection will be saved in files with name pattern
*<collection-name>.data.json*. Each line in a data file is a document insertion/update or
deletion marker, alongside with some meta data.

Cluster Backup
--------------

Starting with Version 2.1 of ArangoDB, the *arangodump* tool also
supports sharding and can be used to backup data from a Cluster. 
Simply point it to one of the _Coordinators_ and it
will behave exactly as described above, working on sharded collections
in the Cluster.

Please see the [Limitations](Limitations.md).

As above, the output will be one structure description file and one data
file per sharded collection. Note that the data in the data file is
sorted first by shards and within each shard by ascending timestamp. The
structural information of the collection contains the number of shards
and the shard keys.

Note that the version of the arangodump client tool needs to match the
version of the ArangoDB server it connects to.

### Advanced Cluster Options

Starting with version 3.1.17, collections may be [created with shard
distribution](../../DataModeling/Collections/DatabaseMethods.md#create)
identical to an existing prototypical collection; i.e. shards are distributed in
the very same pattern as in the prototype collection. Such collections cannot be
dumped without the referenced collection or arangodump yields an error.

    arangodump --collection clonedCollection --output-directory "dump"

    ERROR Collection clonedCollection's shard distribution is based on a that of collection prototypeCollection, which is not dumped along. You may dump the collection regardless of the missing prototype collection by using the --ignore-distribute-shards-like-errors parameter.

There are two ways to approach that problem.
Dump the prototype collection as well:

    arangodump --collection clonedCollection --collection prototypeCollection --output-directory "dump"

    Processed 2 collection(s), wrote 81920 byte(s) into datafiles, sent 1 batch(es)

Or override that behavior to be able to dump the collection in isolation
individually:

    arangodump --collection clonedCollection --output-directory "dump" --ignore-distribute-shards-like-errors

    Processed 1 collection(s), wrote 34217 byte(s) into datafiles, sent 1 batch(es)

Note that in consequence, restoring such a collection without its prototype is
affected. See documentation on [arangorestore](../Arangorestore/README.md) for
more details about restoring the collection.

Encryption
----------

{% hint 'info' %}
This feature is only available in the
[**Enterprise Edition**](https://www.arangodb.com/why-arangodb/arangodb-enterprise/)
{% endhint %}

Starting from version 3.3 encryption of the dump is supported.

The dump is encrypted using an encryption keyfile, which must contain exactly 32
bytes of data (required by the AES block cipher).

The keyfile can be created by an external program, or, on Linux, by using a command
like the following:

```
dd if=/dev/random bs=1 count=32 of=yourSecretKeyFile
```

For security reasons, it is best to create these keys offline (away from your
database servers) and directly store them in your secret management
tool.


In order to create an encrypted backup, add the `--encryption.keyfile`
option when invoking _arangodump_, in addition to any other option you
are already using. The following example assumes that your secret key
is stored in ~/SECRET-KEY:

```
arangodump --collection "secret-collection" dump --encryption.keyfile ~/SECRET-KEY
```

Note that _arangodump_ will not store the key anywhere. It is the responsibility
of the user to find a safe place for the key. However, _arangodump_ will store
the used encryption method in a file named `ENCRYPTION` in the dump directory.
That way _arangorestore_ can later find out whether it is dealing with an
encrypted dump or not.

Trying to restore the encrypted dump without specifying the key will fail:

```
arangorestore --collection "secret-collection" dump --create-collection true
```

and _arangorestore_ will report the following error:

```
the dump data seems to be encrypted with aes-256-ctr, but no key information was specified to decrypt the dump
it is recommended to specify either `--encryption.keyfile` or `--encryption.key-generator` when invoking arangorestore with an encrypted dump
```

It is required to use the exact same key when restoring the data. Again this is
done by providing the `--encryption.keyfile` parameter:

```
arangorestore --collection "secret-collection" dump --create-collection true --encryption.keyfile ~/SECRET-KEY
```

Using a different key will lead to the backup being non-recoverable.

Note that encrypted backups can be used together with the already existing 
RocksDB encryption-at-rest feature, but they can also be used for the MMFiles
engine, which does not have encryption-at-rest.
