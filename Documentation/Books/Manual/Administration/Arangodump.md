Dumping Data from an ArangoDB database
======================================

To dump data from an ArangoDB server instance, you will need to invoke _arangodump_.
Dumps can be re-imported with _arangorestore_. _arangodump_ can be invoked by executing
the following command:

    unix> arangodump --output-directory "dump"

This will connect to an ArangoDB server and dump all non-system collections from
the default database (*_system*) into an output directory named *dump*.
Invoking _arangodump_ will fail if the output directory already exists. This is
an intentional security measure to prevent you from accidentally overwriting already
dumped data. If you are positive that you want to overwrite data in the output 
directory, you can use the parameter *--overwrite true* to confirm this:

    unix> arangodump --output-directory "dump" --overwrite true

_arangodump_ will by default connect to the *_system* database using the default
endpoint. If you want to connect to a different database or a different endpoint, 
or use authentication, you can use the following command-line options:

* *--server.database <string>*: name of the database to connect to
* *--server.endpoint <string>*: endpoint to connect to
* *--server.username <string>*: username
* *--server.password <string>*: password to use (omit this and you'll be prompted for the
  password)
* *--server.authentication <bool>*: whether or not to use authentication

Here's an example of dumping data from a non-standard endpoint, using a dedicated
[database name](../Appendix/Glossary.md#database-name):

    unix> arangodump --server.endpoint tcp://192.168.173.13:8531 --server.username backup --server.database mydb --output-directory "dump"

When finished, _arangodump_ will print out a summary line with some aggregate 
statistics about what it did, e.g.:

    Processed 43 collection(s), wrote 408173500 byte(s) into datafiles, sent 88 batch(es)

By default, _arangodump_ will dump both structural information and documents from all
non-system collections. To adjust this, there are the following command-line 
arguments:

* *--dump-data <bool>*: set to *true* to include documents in the dump. Set to *false* 
  to exclude documents. The default value is *true*.
* *--include-system-collections <bool>*: whether or not to include system collections
  in the dump. The default value is *false*.
  
For example, to only dump structural information of all collections (including system
collections), use:

    unix> arangodump --dump-data false --include-system-collections true --output-directory "dump"

To restrict the dump to just specific collections, there is is the *--collection* option.
It can be specified multiple times if required:
    
    unix> arangodump --collection myusers --collection myvalues --output-directory "dump"

Structural information for a collection will be saved in files with name pattern 
*<collection-name>.structure.json*. Each structure file will contains a JSON object 
with these attributes:
- *parameters*: contains the collection properties
- *indexes*: contains the collection indexes

Document data for a collection will be saved in files with name pattern 
*<collection-name>.data.json*. Each line in a data file is a document insertion/update or
deletion marker, alongside with some meta data.

Starting with Version 2.1 of ArangoDB, the *arangodump* tool also
supports sharding. Simply point it to one of the coordinators and it
will behave exactly as described above, working on sharded collections
in the cluster.

However, as opposed to the single instance situation, this operation 
does not guarantee to dump a consistent snapshot if write operations 
happen during the dump operation. It is therefore recommended not to 
perform any data-modifcation operations on the cluster whilst *arangodump* 
is running.

As above, the output will be one structure description file and one data
file per sharded collection. Note that the data in the data file is
sorted first by shards and within each shard by ascending timestamp. The
structural information of the collection contains the number of shards
and the shard keys.

Note that the version of the arangodump client tool needs to match the 
version of the ArangoDB server it connects to.
    

Advanced cluster options
------------------------

Starting with version 3.1.17, collections may be created with shard
distribution identical to an existing prototypical collection;
i.e. shards are distributed in the very same pattern as in the
prototype collection. Such collections cannot be dumped without the
reference collection or arangodump with yield an error.

    unix> arangodump --collection clonedCollection --output-directory "dump"

    ERROR Collection clonedCollection's shard distribution is based on a that of collection prototypeCollection, which is not dumped along. You may dump the collection regardless of the missing prototype collection by using the --ignore-distribute-shards-like-errors parameter.

There are two ways to approach that problem: Solve it, i.e. dump the
prototype collection along:

    unix> arangodump --collection clonedCollection --collection prototypeCollection --output-directory "dump"
    
    Processed 2 collection(s), wrote 81920 byte(s) into datafiles, sent 1 batch(es)

Or override that behaviour to be able to dump the collection
individually.

    unix> arangodump --collection B clonedCollection --output-directory "dump" --ignore-distribute-shards-like-errors
    
    Processed 1 collection(s), wrote 34217 byte(s) into datafiles, sent 1 batch(es)

No that in consequence, restoring such a collection without its
prototype is affected. [arangorestore](Arangorestore.md)


Encryption
----------

In the ArangoDB Enterprise Edition there are the additional parameters:

### Encryption key stored in file

*--encryption.keyfile path-of-keyfile*

The file `path-to-keyfile` must contain the encryption key. This
file must be secured, so that only `arangod` can access it. You should
also ensure that in case some-one steals the hardware, he will not be
able to read the file. For example, by encryption `/mytmpfs` or
creating a in-memory file-system under `/mytmpfs`.

### Encryption key generated by a program

*--encryption.key-generator path-to-my-generator*

The program `path-to-my-generator` must output the encryption on
standard output and exit.

### Creating keys

The encryption keyfile must contain 32 bytes of random data.

You can create it with a command line this.

```
dd if=/dev/random bs=1 count=32 of=yourSecretKeyFile
```

For security, it is best to create these keys offline (away from your
database servers) and directly store them in you secret management
tool.

