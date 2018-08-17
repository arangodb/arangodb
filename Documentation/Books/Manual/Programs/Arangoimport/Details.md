Arangoimport Details
====================

The most convenient method to import a lot of data into ArangoDB is to use the
*arangoimport* command-line tool. It allows you to bulk import data records
from a file into a database collection. Multiple files can be imported into
the same or different collections by invoking it multiple times.

Importing into an Edge Collection
---------------------------------

Arangoimport can also be used to import data into an existing edge collection.
The import data must, for each edge to import, contain at least the *_from* and
*_to* attributes. These indicate which other two documents the edge should connect.
It is necessary that these attributes are set for all records, and point to
valid document IDs in existing collections.

*Example*

```js
{ "_from" : "users/1234", "_to" : "users/4321", "desc" : "1234 is connected to 4321" }
```

**Note**: The edge collection must already exist when the import is started. Using
the *--create-collection* flag will not work because arangoimport will always try to
create a regular document collection if the target collection does not exist.

Attribute Naming and Special Attributes
---------------------------------------

Attributes whose names start with an underscore are treated in a special way by
ArangoDB:

- the optional *_key* attribute contains the document's key. If specified, the value
  must be formally valid (e.g. must be a string and conform to the naming conventions).
  Additionally, the key value must be unique within the
  collection the import is run for.
- *_from*: when importing into an edge collection, this attribute contains the id
  of one of the documents connected by the edge. The value of *_from* must be a
  syntactically valid document id and the referred collection must exist.
- *_to*: when importing into an edge collection, this attribute contains the id
  of the other document connected by the edge. The value of *_to* must be a
  syntactically valid document id and the referred collection must exist.
- *_rev*: this attribute contains the revision number of a document. However, the
  revision numbers are managed by ArangoDB and cannot be specified on import. Thus
  any value in this attribute is ignored on import.

If you import values into *_key*, you should make sure they are valid and unique.

When importing data into an edge collection, you should make sure that all import
documents can *_from* and *_to* and that their values point to existing documents.

To avoid specifying complete document ids (consisting of collection names and document
keys) for *_from* and *_to* values, there are the options *--from-collection-prefix* and
*--to-collection-prefix*. If specified, these values will be automatically prepended
to each value in *_from* (or *_to* resp.). This allows specifying only document keys
inside *_from* and/or *_to*.

*Example*

    arangoimport --from-collection-prefix users --to-collection-prefix products ...

Importing the following document will then create an edge between *users/1234* and
*products/4321*:

```js
{ "_from" : "1234", "_to" : "4321", "desc" : "users/1234 is connected to products/4321" }
```

Updating existing documents
---------------------------

By default, arangoimport will try to insert all documents from the import file into the
specified collection. In case the import file contains documents that are already present
in the target collection (matching is done via the *_key* attributes), then a default
arangoimport run will not import these documents and complain about unique key constraint
violations.

However, arangoimport can be used to update or replace existing documents in case they
already exist in the target collection. It provides the command-line option *--on-duplicate*
to control the behavior in case a document is already present in the database.

The default value of *--on-duplicate* is *error*. This means that when the import file
contains a document that is present in the target collection already, then trying to
re-insert a document with the same *_key* value is considered an error, and the document in
the database will not be modified.

Other possible values for *--on-duplicate* are:

- *update*: each document present in the import file that is also present in the target
  collection already will be updated by arangoimport. *update* will perform a partial update
  of the existing document, modifying only the attributes that are present in the import
  file and leaving all other attributes untouched.

  The values of system attributes *_id*, *_key*, *_rev*, *_from* and *_to* cannot be
  updated or replaced in existing documents.

- *replace*: each document present in the import file that is also present in the target
  collection already will be replace by arangoimport. *replace* will replace the existing
  document entirely, resulting in a document with only the attributes specified in the import
  file.

  The values of system attributes *_id*, *_key*, *_rev*, *_from* and *_to* cannot be
  updated or replaced in existing documents.

- *ignore*: each document present in the import file that is also present in the target
  collection already will be ignored and not modified in the target collection.

When *--on-duplicate* is set to either *update* or *replace*, arangoimport will return the
number of documents updated/replaced in the *updated* return value. When set to another
value, the value of *updated* will always be zero. When *--on-duplicate* is set to *ignore*,
arangoimport will return the number of ignored documents in the *ignored* return value.
When set to another value, *ignored* will always be zero.

It is possible to perform a combination of inserts and updates/replaces with a single
arangoimport run. When *--on-duplicate* is set to *update* or *replace*, all documents present
in the import file will be inserted into the target collection provided they are valid
and do not already exist with the specified *_key*. Documents that are already present
in the target collection (identified by *_key* attribute) will instead be updated/replaced.

Result output
-------------

An _arangoimport_ import run will print out the final results on the command line.
It will show the

- number of documents created (*created*)
- number of documents updated/replaced (*updated/replaced*, only non-zero if
  *--on-duplicate* was set to *update* or *replace*, see below)
- number of warnings or errors that occurred on the server side (*warnings/errors*)
- number of ignored documents (only non-zero if *--on-duplicate* was set to *ignore*).

*Example*

```js
created:          2
warnings/errors:  0
updated/replaced: 0
ignored:          0
```

For CSV and TSV imports, the total number of input file lines read will also be printed
(*lines read*).

_arangoimport_ will also print out details about warnings and errors that happened on the
server-side (if any).

### Automatic pacing with busy or low throughput disk subsystems

Arangoimport has an automatic pacing algorithm that limits how fast
data is sent to the ArangoDB servers.  This pacing algorithm exists to
prevent the import operation from failing due to slow responses.

Google Compute and other VM providers limit the throughput of disk
devices. Google's limit is more strict for smaller disk rentals, than
for larger. Specifically, a user could choose the smallest disk space
and be limited to 3 Mbytes per second.  Similarly, other users'
processes on the shared VM can limit available throughput of the disk
devices.

The automatic pacing algorithm adjusts the transmit block size
dynamically based upon the actual throughput of the server over the
last 20 seconds. Further, each thread delivers its portion of the data
in mostly non-overlapping chunks. The thread timing creates
intentional windows of non-import activity to allow the server extra
time for meta operations.

Automatic pacing intentionally does not use the full throughput of a
disk device.  An unlimited (really fast) disk device might not need
pacing. Raising the number of threads via the `--threads X` command
line to any value of `X` greater than 2 will increase the total
throughput used.

Automatic pacing frees the user from adjusting the throughput used to
match available resources.  It is disabled by manually specifying any
`--batch-size`. 16777216 was the previous default for *--batch-size*.
Having *--batch-size* too large can lead to transmitted data piling-up
on the server, resulting in a TimeoutError.

The pacing algorithm works successfully with MMFiles with disks
limited to read and write throughput as small as 1 Mbyte per
second. The algorithm works successfully with RocksDB with disks
limited to read and write throughput as small as 3 Mbyte per second.
