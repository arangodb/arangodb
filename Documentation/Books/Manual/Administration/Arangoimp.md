Arangoimp
=========

This manual describes the ArangoDB importer _arangoimp_, which can be used for
bulk imports.

The most convenient method to import a lot of data into ArangoDB is to use the
*arangoimp* command-line tool. It allows you to import data records from a file
into an existing database collection.

It is possible to import [document keys](../Appendix/Glossary.md#document-key) with the documents using the *_key*
attribute. When importing into an [edge collection](../Appendix/Glossary.md#edge-collection), it is mandatory that all
imported documents have the *_from* and *_to* attributes, and that they contain
valid references.

Let's assume for the following examples you want to import user data into an 
existing collection named "users" on the server.


Importing Data into an ArangoDB Database
----------------------------------------

### Importing JSON-encoded Data

Let's further assume the import at hand is encoded in JSON. We'll be using these
example user records to import:

```js
{ "name" : { "first" : "John", "last" : "Connor" }, "active" : true, "age" : 25, "likes" : [ "swimming"] }
{ "name" : { "first" : "Jim", "last" : "O'Brady" }, "age" : 19, "likes" : [ "hiking", "singing" ] }
{ "name" : { "first" : "Lisa", "last" : "Jones" }, "dob" : "1981-04-09", "likes" : [ "running" ] }
```

To import these records, all you need to do is to put them into a file (with one
line for each record to import) and run the following command:

    > arangoimp --file "data.json" --type jsonl --collection "users"

This will transfer the data to the server, import the records, and print a
status summary. To show the intermediate progress during the import process, the
option *--progress* can be added. This option will show the percentage of the
input file that has been sent to the server. This will only be useful for big
import files. 

    > arangoimp --file "data.json" --type json --collection users --progress true

It is also possible to use the output of another command as an input for arangoimp.
For example, the following shell command can be used to pipe data from the `cat`
process to arangoimp:

    > cat data.json | arangoimp --file - --type json --collection users

Note that you have to use `--file -` if you want to use another command as input 
for arangoimp. No progress can be reported for such imports as the size of the input
will be unknown to arangoimp.

By default, the endpoint *tcp://127.0.0.1:8529* will be used.  If you want to
specify a different endpoint, you can use the *--server.endpoint* option. You
probably want to specify a database user and password as well.  You can do so by
using the options *--server.username* and *--server.password*.  If you do not
specify a password, you will be prompted for one.

    > arangoimp --server.endpoint tcp://127.0.0.1:8529 --server.username root --file "data.json" --type json --collection "users"

Note that the collection (*users* in this case) must already exist or the import
will fail. If you want to create a new collection with the import data, you need
to specify the *--create-collection* option. Note that by default it will create
a document collection and no ede collection.

    > arangoimp --file "data.json" --type json --collection "users" --create-collection true

To create an edge collection instead, use the *--create-collection-type* option
and set it to *edge*:

    > arangoimp --file "data.json" --collection "myedges" --create-collection true --create-collection-type edge

When importing data into an existing collection it is often convenient to first
remove all data from the collection and then start the import. This can be achieved
by passing the *--overwrite* parameter to _arangoimp_. If it is set to *true*,
any existing data in the collection will be removed prior to the import. Note
that any existing index definitions for the collection will be preserved even if 
*--overwrite* is set to true.
    
    > arangoimp --file "data.json" --type json --collection "users" --overwrite true

As the import file already contains the data in JSON format, attribute names and
data types are fully preserved. As can be seen in the example data, there is no
need for all data records to have the same attribute names or types. Records can
be inhomogeneous.

Please note that by default, _arangoimp_ will import data into the specified 
collection in the default database (*_system*). To specify a different database, 
use the *--server.database* option when invoking _arangoimp_. 

The tool also supports parallel imports, with multiple threads. Using multiple
threads may provide a speedup, especially when using the RocksDB storage engine.
To specify the number of parallel threads use the `--threads` option:

    > arangoimp --threads 4 --file "data.json" --type json --collection "users"

Note that using multiple threads may lead to a non-sequential import of the input
data. Data that appears later in the input file may be imported earlier than data
that appears earlier in the input file. This is normally not a problem but may cause
issues when when there are data dependencies or duplicates in the import data. In
this case, the number of threads should be set to 1.

### JSON input file formats

**Note**: *arangoimp* supports two formats when importing JSON data from 
a file. The first format that we also used above is commonly known as [jsonl](http://jsonlines.org)).
However, in contrast to the JSONL specification it requires the input file to contain
one complete JSON document in each line, e.g.

```js
{ "_key": "one", "value": 1 }
{ "_key": "two", "value": 2 }
{ "_key": "foo", "value": "bar" }
...
```

So one could argue that this is only a subset of JSONL.

The above format can be imported sequentially by _arangoimp_. It will read data
from the input file in chunks and send it in batches to the server. Each batch
will be about as big as specified in the command-line parameter *--batch-size*.

An alternative is to put one big JSON document into the input file like this:

```js
[
  { "_key": "one", "value": 1 },
  { "_key": "two", "value": 2 },
  { "_key": "foo", "value": "bar" },
  ...
]
```

This format allows line breaks within the input file as required. The downside 
is that the whole input file will need to be read by _arangoimp_ before it can
send the first batch. This might be a problem if the input file is big. By
default, _arangoimp_ will allow importing such files up to a size of about 16 MB.

If you want to allow your _arangoimp_ instance to use more memory, you may want
to increase the maximum file size by specifying the command-line option
*--batch-size*. For example, to set the batch size to 32 MB, use the following
command:

    > arangoimp --file "data.json" --type json --collection "users" --batch-size 33554432

Please also note that you may need to increase the value of *--batch-size* if
a single document inside the input file is bigger than the value of *--batch-size*.


### Importing CSV Data

_arangoimp_ also offers the possibility to import data from CSV files. This
comes handy when the data at hand is in CSV format already and you don't want to
spend time converting them to JSON for the import.

To import data from a CSV file, make sure your file contains the attribute names
in the first row. All the following lines in the file will be interpreted as
data records and will be imported.

The CSV import requires the data to have a homogeneous structure. All records
must have exactly the same amount of columns as there are headers.

The cell values can have different data types though. If a cell does not have
any value, it can be left empty in the file. These values will not be imported
so the attributes will not "be there" in document created. Values enclosed in
quotes will be imported as strings, so to import numeric values, boolean values
or the null value, don't enclose the value in quotes in your file.

We'll be using the following import for the CSV import:

```
"first","last","age","active","dob"
"John","Connor",25,true,
"Jim","O'Brady",19,,
"Lisa","Jones",,,"1981-04-09"
Hans,dos Santos,0123,,
Wayne,Brewer,,false,
```

The command line to execute the import is:

    > arangoimp --file "data.csv" --type csv --collection "users"

The above data will be imported into 5 documents which will look as follows:

```js
{ "first" : "John", "last" : "Connor", "active" : true, "age" : 25 } 
{ "first" : "Jim", "last" : "O'Brady", "age" : 19 }
{ "first" : "Lisa", "last" : "Jones", "dob" : "1981-04-09" } 
{ "first" : "Hans", "last" : "dos Santos", "age" : 123 } 
{ "first" : "Wayne", "last" : "Brewer", "active" : false } 
```

As can be seen, values left completely empty in the input file will be treated
as absent. Numeric values not enclosed in quotes will be treated as numbers.
Note that leading zeros in numeric values will be removed. To import numbers
with leading zeros, please use strings.
The literals *true* and *false* will be treated as booleans if they are not
enclosed in quotes. Other values not enclosed in quotes will be treated as
strings.
Any values enclosed in quotes will be treated as strings, too.

String values containing the quote character or the separator must be enclosed
with quote characters. Within a string, the quote character itself must be
escaped with another quote character (or with a backslash if the *--backslash-escape*
option is used).

Note that the quote and separator characters can be adjusted via the
*--quote* and *--separator* arguments when invoking _arangoimp_. The quote
character defaults to the double quote (*"*). To use a literal quote in a 
string, you can use two quote characters. 
To use backslash for escaping quote characters, please set the option 
*--backslash-escape* to *true*.

The importer supports Windows (CRLF) and Unix (LF) line breaks. Line breaks might
also occur inside values that are enclosed with the quote character.

Here's an example for using literal quotes and newlines inside values:

```
"name","password"
"Foo","r4ndom""123!"
"Bar","wow!
this is a
multine password!"
"Bartholomew ""Bart"" Simpson","Milhouse"
```

Extra whitespace at the end of each line will be ignored. Whitespace at the 
start of lines or between field values will not be ignored, so please make sure 
that there is no extra whitespace in front of values or between them.


### Importing TSV Data

You may also import tab-separated values (TSV) from a file. This format is very
simple: every line in the file represents a data record. There is no quoting or
escaping. That also means that the separator character (which defaults to the
tabstop symbol) must not be used anywhere in the actual data.

As with CSV, the first line in the TSV file must contain the attribute names,
and all lines must have an identical number of values.

If a different separator character or string should be used, it can be specified
with the *--separator* argument. 

An example command line to execute the TSV import is:

    > arangoimp --file "data.tsv" --type tsv --collection "users" 


### Attribute Name Translation

For the CSV and TSV input formats, attribute names can be translated automatically.
This is useful in case the import file has different attribute names than those
that should be used in ArangoDB.

A common use case is to rename an "id" column from the input file into "_key" as
it is expected by ArangoDB. To do this, specify the following translation when
invoking arangoimp:

    > arangoimp --file "data.csv" --type csv --translate "id=_key"

Other common cases are to rename columns in the input file to *_from* and *_to*:
    
    > arangoimp --file "data.csv" --type csv --translate "from=_from" --translate "to=_to"

The *translate* option can be specified multiple types. The source attribute name
and the target attribute must be separated with a *=*.


### Ignoring Attributes 


For the CSV and TSV input formats, certain attribute names can be ignored on imports.
In an ArangoDB cluster there are cases where this can come in handy,
when your documents already contain a `_key` attribute
and your collection has a sharding attribute other than `_key`: In the cluster this
configuration is not supported, because ArangoDB needs to guarantee the uniqueness of the `_key` 
attribute in *all* shards of the collection.

    > arangoimp --file "data.csv" --type csv --remove-attribute "_key"

The same thing would apply if your data contains an *_id* attribute:

    > arangoimp --file "data.csv" --type csv --remove-attribute "_id"


### Importing into an Edge Collection

arangoimp can also be used to import data into an existing edge collection.
The import data must, for each edge to import, contain at least the *_from* and
*_to* attributes. These indicate which other two documents the edge should connect.
It is necessary that these attributes are set for all records, and point to 
valid document ids in existing collections.

*Examples*

```js
{ "_from" : "users/1234", "_to" : "users/4321", "desc" : "1234 is connected to 4321" }
```

**Note**: The edge collection must already exist when the import is started. Using 
the *--create-collection* flag will not work because arangoimp will always try to 
create a regular document collection if the target collection does not exist.


### Updating existing documents

By default, arangoimp will try to insert all documents from the import file into the
specified collection. In case the import file contains documents that are already present
in the target collection (matching is done via the *_key* attributes), then a default
arangoimp run will not import these documents and complain about unique key constraint
violations.

However, arangoimp can be used to update or replace existing documents in case they
already exist in the target collection. It provides the command-line option *--on-duplicate*
to control the behavior in case a document is already present in the database.

The default value of *--on-duplicate* is *error*. This means that when the import file
contains a document that is present in the target collection already, then trying to
re-insert a document with the same *_key* value is considered an error, and the document in
the database will not be modified.

Other possible values for *--on-duplicate* are:

- *update*: each document present in the import file that is also present in the target
  collection already will be updated by arangoimp. *update* will perform a partial update
  of the existing document, modifying only the attributes that are present in the import 
  file and leaving all other attributes untouched.
  
  The values of system attributes *_id*, *_key*, *_rev*, *_from* and *_to* cannot be
  updated or replaced in existing documents. 

- *replace*: each document present in the import file that is also present in the target
  collection already will be replace by arangoimp. *replace* will replace the existing
  document entirely, resulting in a document with only the attributes specified in the import
  file. 
  
  The values of system attributes *_id*, *_key*, *_rev*, *_from* and *_to* cannot be
  updated or replaced in existing documents. 

- *ignore*: each document present in the import file that is also present in the target
  collection already will be ignored and not modified in the target collection. 

When *--on-duplicate* is set to either *update* or *replace*, arangoimp will return the
number of documents updated/replaced in the *updated* return value. When set to another
value, the value of *updated* will always be zero. When *--on-duplicate* is set to *ignore*,
arangoimp will return the number of ignored documents in the *ignored* return value.
When set to another value, *ignored* will always be zero.

It is possible to perform a combination of inserts and updates/replaces with a single 
arangoimp run. When *--on-duplicate* is set to *update* or *replace*, all documents present
in the import file will be inserted into the target collection provided they are valid
and do not already exist with the specified *_key*. Documents that are already present
in the target collection (identified by *_key* attribute) will instead be updated/replaced.


### Arangoimp result output

An _arangoimp_ import run will print out the final results on the command line.
It will show the

* number of documents created (*created*)
* number of documents updated/replaced (*updated/replaced*, only non-zero if 
  *--on-duplicate* was set to *update* or *replace*, see below) 
* number of warnings or errors that occurred on the server side (*warnings/errors*)
* number of ignored documents (only non-zero if *--on-duplicate* was set to *ignore*). 

*Example*

```js
created:          2
warnings/errors:  0
updated/replaced: 0
ignored:          0
```

For CSV and TSV imports, the total number of input file lines read will also be printed
(*lines read*).

_arangoimp_ will also print out details about warnings and errors that happened on the 
server-side (if any).


### Attribute Naming and Special Attributes

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

    > arangoimp --from-collection-prefix users --to-collection-prefix products ...

Importing the following document will then create an edge between *users/1234* and
*products/4321*:

```js
{ "_from" : "1234", "_to" : "4321", "desc" : "users/1234 is connected to products/4321" }
```
