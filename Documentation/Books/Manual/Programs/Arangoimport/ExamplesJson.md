Arangoimport Examples: JSON
===========================

Importing JSON-encoded Data
---------------------------

We will be using these example user records to import:

```js
{ "name" : { "first" : "John", "last" : "Connor" }, "active" : true, "age" : 25, "likes" : [ "swimming"] }
{ "name" : { "first" : "Jim", "last" : "O'Brady" }, "age" : 19, "likes" : [ "hiking", "singing" ] }
{ "name" : { "first" : "Lisa", "last" : "Jones" }, "dob" : "1981-04-09", "likes" : [ "running" ] }
```

To import these records, all you need to do is to put them into a file (with one
line for each record to import) and run the following command:

    arangoimport --file "data.json" --type jsonl --collection "users"

This will transfer the data to the server, import the records, and print a
status summary. To show the intermediate progress during the import process, the
option *--progress* can be added. This option will show the percentage of the
input file that has been sent to the server. This will only be useful for big
import files.

    arangoimport --file "data.json" --type json --collection users --progress true

It is also possible to use the output of another command as an input for arangoimport.
For example, the following shell command can be used to pipe data from the `cat`
process to arangoimport (Linux/Cygwin only):

    cat data.json | arangoimport --file - --type json --collection users

In a command line or PowerShell on Windows, there is the `type` command:

    type data.json | arangoimport --file - --type json --collection users

Note that you have to use `--file -` if you want to use another command as input
for arangoimport. No progress can be reported for such imports as the size of the input
will be unknown to arangoimport.

By default, the endpoint *tcp://127.0.0.1:8529* will be used.  If you want to
specify a different endpoint, you can use the *--server.endpoint* option. You
probably want to specify a database user and password as well.  You can do so by
using the options *--server.username* and *--server.password*.  If you do not
specify a password, you will be prompted for one.

    arangoimport --server.endpoint tcp://127.0.0.1:8529 --server.username root --file "data.json" --type json --collection "users"

Note that the collection (*users* in this case) must already exist or the import
will fail. If you want to create a new collection with the import data, you need
to specify the *--create-collection* option. Note that by default it will create
a document collection and no edge collection.

    arangoimport --file "data.json" --type json --collection "users" --create-collection true

To create an edge collection instead, use the *--create-collection-type* option
and set it to *edge*:

    arangoimport --file "data.json" --collection "myedges" --create-collection true --create-collection-type edge

When importing data into an existing collection it is often convenient to first
remove all data from the collection and then start the import. This can be achieved
by passing the *--overwrite* parameter to _arangoimport_. If it is set to *true*,
any existing data in the collection will be removed prior to the import. Note
that any existing index definitions for the collection will be preserved even if
*--overwrite* is set to true.

    arangoimport --file "data.json" --type json --collection "users" --overwrite true

As the import file already contains the data in JSON format, attribute names and
data types are fully preserved. As can be seen in the example data, there is no
need for all data records to have the same attribute names or types. Records can
be inhomogeneous.

Please note that by default, _arangoimport_ will import data into the specified
collection in the default database (*_system*). To specify a different database,
use the *--server.database* option when invoking _arangoimport_. If you want to 
import into a nonexistent database you need to pass *--create-database true*.
Note *--create-database* defaults to *false*

The tool also supports parallel imports, with multiple threads. Using multiple
threads may provide a speedup, especially when using the RocksDB storage engine.
To specify the number of parallel threads use the `--threads` option:

    arangoimport --threads 4 --file "data.json" --type json --collection "users"

Note that using multiple threads may lead to a non-sequential import of the input
data. Data that appears later in the input file may be imported earlier than data
that appears earlier in the input file. This is normally not a problem but may cause
issues when when there are data dependencies or duplicates in the import data. In
this case, the number of threads should be set to 1.

JSON input file formats
-----------------------

*arangoimport* supports two formats when importing JSON data from a file:

- JSON
- JSONL

The first format that we already used above is commonly known as
[JSONL](http://jsonlines.org), also called new-line delimited JSON.
However, in contrast to the JSONL specification it requires the input file to contain
one complete JSON object in each line, e.g.

```js
{ "_key": "one", "value": 1 }
{ "_key": "two", "value": 2 }
{ "_key": "foo", "value": "bar" }
...
```

So one could argue that this is only a subset of JSONL, which permits top-level arrays.

The above format can be imported sequentially by _arangoimport_. It will read data
from the input file in chunks and send it in batches to the server. Each batch
will be about as big as specified in the command-line parameter *--batch-size*.

An alternative is to put one big JSON array into the input file like this:

```js
[
  { "_key": "one", "value": 1 },
  { "_key": "two", "value": 2 },
  { "_key": "foo", "value": "bar" },
  ...
]
```

This format allows line breaks in the input file within documents. The downside
is that the whole input file will need to be read by _arangoimport_ before it can
send the first batch. This might be a problem if the input file is big. By
default, _arangoimport_ will allow importing such files up to a size of about 16 MB.

If you want to allow your _arangoimport_ instance to use more memory, you may want
to increase the maximum file size by specifying the command-line option
*--batch-size*. For example, to set the batch size to 32 MB, use the following
command:

    arangoimport --file "data.json" --type json --collection "users" --batch-size 33554432

Please also note that you may need to increase the value of *--batch-size* if
a single document inside the input file is bigger than the value of *--batch-size*.
