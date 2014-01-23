Reloading Data into an ArangoDB database {#RestoreManual}
=========================================================

@NAVIGATE_RestoreManual
@EMBEDTOC{RestoreManualTOC}

To reload data from a dump previously created with @ref DumpManual "arangodump",
ArangoDB provides the _arangorestore_ tool.

Invoking arangorestore {#RestoreManualInvoking}
===============================================

_arangorestore_ can be invoked from the command-line as follows:

    unix> arangorestore --input-directory "dump"

This will connect to an ArangoDB server and reload structural information and 
documents found in the input directory `dump`. Please note that the input directory
must have been created by running `arangodump` before.

_arangorestore_ will by default connect to the `_system` database using the default
endpoint. If you want to connect to a different database or a different endpoint, 
or use authentication, you can use the following command-line options:

- `--server.database <string>`: name of the database to connect to
- `--server.endpoint <string>`: endpoint to connect to
- `--server.username <string>`: username
- `--server.password <string>`: password to use (omit this and you'll be prompted for the
  password)
- `--server.disable-authentication <bool>`: whether or not to use authentication

Here's an example of reloading data to a non-standard endpoint, using a dedicated
database name:

    unix> arangorestore --server.endpoint tcp://192.168.173.13:8531 --server.username backup --server.database mydb --input-directory "dump"

_arangorestore_ will print out its progress while running, and will end with a line
showing some aggregate statistics:

    Processed 2 collection(s), read 2256 byte(s) from datafiles, sent 2 batch(es)


By default, _arangorestore_ will re-create all non-system collections found in the input 
directory and load data into them. If the target database already contains collections 
which are also present in the input directory, the existing collections in the database 
will be dropped and re-created with the data found in the input directory.

The following parameters are available to adjust this behavior:

- `--create-collection <bool>`: set to `true` to create collections in the target
  database. If the target database already contains a collection with the same name,
  it will be dropped first and then re-created with the properties found in the input
  directory. Set to `false` to keep existing collections in the target database. If 
  set to `false` and _arangorestore_ encounters a collection that is present in both 
  the target database and the input directory, it will abort. The default value is `true`.
- `--import-data <bool>`: set to `true` to load document data into the collections in
  the target database. Set to `false` to not load any document data. The default value 
  is `true`.
- `--include-system-collections <bool>`: whether or not to include system collections
  when re-creating collections or reloading data. The default value is `false`.
  
For example, to (re-)create all non-system collections and load document data into them, use:

    unix> arangorestore --create-collection true --import-data true --input-directory "dump"

This will drop potentially existing collections in the target database that are also present
in the input directory.

To include system collections too, use `--include-system-collections true`:
    
    unix> arangorestore --create-collection true --import-data true --include-system-collections true --input-directory "dump"

To (re-)create all non-system collections without loading document data, use:

    unix> arangorestore --create-collection true --import-data false --input-directory "dump"

This will also drop existing collections in the target database that are also present in the
input directory.

To just load document data into all non-system collections, use:

    unix> arangorestore --create-collection false --import-data true --input-directory "dump"

To restrict reloading to just specific collections, there is is the `--collection` option.
It can be specified multiple times if required:
    
    unix> arangorestore --collection myusers --collection myvalues --input-directory "dump"

Collections will be processed by in alphabetical order by _arangorestore_, with all document
collections being processed before all edge collections. This is to ensure that reloading
data into edge collections will have the document collections linked in edges (`_from` and
`_to` attributes) loaded.

Restoring Revision Ids and Collection Ids {#RestoreManualIds}
=============================================================
 
_arangorestore_ will reload document and edges data with the exact same `_key`, `_from` and 
`_to` values found in the input directory. However, when loading document data, it will assign
its own values for the `_rev` attribute of the reloaded documents. Though this difference is 
intentional (normally, every server should create its own `_rev` values) there might be 
situations when it is required to re-use the exact same `_rev` values for the reloaded data.
This can be achieved by setting the `--recycle-ids` parameter to `true`:

    unix> arangorestore --collection myusers --collection myvalues --recycle-ids true --input-directory "dump"

Note that setting `--recycle-ids` to `true` will also cause collections to be (re-)created in
the target database with the exact same collection id as in the input directory. Any potentially
existing collection in the target database with the same collection id will then be dropped.

Setting `--recycle-ids` to `false` or omitting it will only use the collection name from the
input directory and allow the target database to create the collection with a different id
(though with the same name) than in the input directory.

Reloading Data into a different Collection {#RestoreManualDifferent}
====================================================================

With some creativity you can use _arangodump_ and _arangorestore_ to transfer data from one
collection into another (either on the same server or not). For example, to copy data from
a collection `myvalues` in database `mydb` into a collection `mycopyvalues` in database `mycopy`,
you can start with the following command:

    unix> arangodump --collection myvalues --server.database mydb --output-directory "dump"

This will create two files, `myvalues.structure.json` and `myvalues.data.json`, in the output 
directory. To load data from the datafile into an existing collection `mycopyvalues` in database 
`mycopy`, rename the files to `mycopyvalues.structure.json` and `mycopyvalues.data.json`.
After that, run the following command:
    
    unix> arangorestore --collection mycopyvalues --server.database mycopy --input-directory "dump"

@BNAVIGATE_RestoreManual
