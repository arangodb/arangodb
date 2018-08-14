Arangoimport Examples: JSON
===========================

Using JSON as data format, records are represented as JSON objects and called
documents in ArangoDB. They are self-contained. Therefore, there is no need
for all records in a collection to have the same attribute names or types.
Documents can be inhomogeneous while data types can be fully preserved.

Input file formats
------------------

*arangoimport* supports two formats when importing JSON data:

- [JSON](http://json.org/) – JavaScript Object Notation
- [JSON Lines](http://jsonlines.org/) –
  also known as _JSONL_ or new-line delimited JSON

Multiple documents can be stored in standard JSON format in a top-level array
with objects as members:

```js
[
  { "_key": "one", "value": 1 },
  { "_key": "two", "value": 2 },
  { "_key": "foo", "value": "bar" },
  ...
]
```

This format allows line breaks for formatting (i.e. pretty printing):

```js
[
  {
    "_key": "one",
    "value": 1
  },
  {
    "_key": "two",
    "value": 2
  },
  {
    "_key": "foo",
    "value": "bar"
  },
  ...
]
```

It requires parsers to read the entire input in order to verify that the
array is properly closed at the very end. _arangoimport_ will need to read
the whole input before it can send the first batch to the server.
By default, it will allow importing such files up to a size of about 16 MB.
If you want to allow your _arangoimport_ instance to use more memory, increase
the maximum file size by specifying the command-line option `--batch-size`.
For example, to set the batch size to 32 MB, use the following command:

    arangoimport --file "data.json" --type json --collection "users" --batch-size 33554432

_JSON Lines_ formatted data allows processing each line individually:

```js
{ "_key": "one", "value": 1 }
{ "_key": "two", "value": 2 }
{ "_key": "foo", "value": "bar" }
...
```

The above format can be imported sequentially by _arangoimport_. It will read
data from the input in chunks and send it in batches to the server. Each batch
will be about as big as specified in the command-line parameter `--batch-size`.

Please note that you may still need to increase the value of `--batch-size` if a
single document inside the input file is bigger than the value of `--batch-size`.

_JSON Lines_ does not allow line breaks for pretty printing. There has to be one
complete JSON object on each line. A JSON array or primitive value per line is
not supported by _arangoimport_ in contrast to the JSON Lines specification,
which allows any valid JSON value on a line.

Converting JSON to JSON Lines
-----------------------------

An input with JSON objects in an array, optionally pretty printed, can be
easily converted into JSONL with one JSON object per line using the
[**jq** command line tool](http://stedolan.github.io/jq/):

```
jq -c ".[]" inputFile.json > outputFile.jsonl
```

The `-c` option enables compact JSON (as opposed to pretty printed JSON).
`".[]"` is a filter that unpacks the top-level array and effectively puts each
object in that array on a separate line in combination with the compact option.

An example `inputFile.json` can look like this:

```json
[
  {
    "isActive": true,
    "name": "Evans Wheeler",
    "latitude": -0.119406,
    "longitude": 146.271888,
    "tags": [
      "amet",
      "qui",
      "velit"
    ]
  },
  {
    "isActive": true,
    "name": "Coffey Barron",
    "latitude": -37.78772,
    "longitude": 131.218935,
    "tags": [
      "dolore",
      "exercitation",
      "irure",
      "velit"
    ]
  }
]
```

The conversion produces the following `outputFile.jsonl`:

```json
{"isActive":true,"name":"Evans Wheeler","latitude":-0.119406,"longitude":146.271888,"tags":["amet","qui","velit"]}
{"isActive":true,"name":"Coffey Barron","latitude":-37.78772,"longitude":131.218935,"tags":["dolore","exercitation","irure","velit"]}
```

Import Example and Common Options
---------------------------------

We will be using these example user records to import:

```js
{ "name" : { "first" : "John", "last" : "Connor" }, "active" : true, "age" : 25, "likes" : [ "swimming"] }
{ "name" : { "first" : "Jim", "last" : "O'Brady" }, "age" : 19, "likes" : [ "hiking", "singing" ] }
{ "name" : { "first" : "Lisa", "last" : "Jones" }, "dob" : "1981-04-09", "likes" : [ "running" ] }
```

To import these records, all you need to do is to put them into a file
(with one line for each record to import), save it as `data.jsonl` and run
the following command:

    arangoimport --file "data.jsonl" --type jsonl --collection users

This will transfer the data to the server, import the records, and print a
status summary.

To show the intermediate progress during the import process, the
option `--progress` can be added. This option will show the percentage of the
input file that has been sent to the server. This will only be useful for big
import files.

    arangoimport --file "data.jsonl" --type jsonl --collection users --progress true

It is also possible to use the output of another command as an input for
_arangoimport_. For example, the following shell command can be used to pipe
data from the `cat` process to arangoimport (Linux/Cygwin only):

    cat data.json | arangoimport --file - --type jsonl --collection users

In a command line or PowerShell on Windows, there is the `type` command:

    type data.json | arangoimport --file - --type jsonl --collection users

The option `--file -` with a hyphen as file name is special and makes it
read from standard input. No progress can be reported for such imports as the
size of the input will be unknown to arangoimport.

By default, the endpoint `tcp://127.0.0.1:8529` will be used. If you want to
specify a different endpoint, you can use the `--server.endpoint` option. You
probably want to specify a database user and password as well. You can do so by
using the options `--server.username` and `--server.password`. If you do not
specify a password, you will be prompted for one.

    arangoimport --server.endpoint tcp://127.0.0.1:8529 --server.username root ...

Note that the collection (*users* in this case) must already exist or the import
will fail. If you want to create a new collection with the import data, you need
to specify the `--create-collection` option. It will create a document collection
by default and not an edge collection.

    arangoimport --file "data.jsonl" --type jsonl --collection users --create-collection true

To create an edge collection instead, use the `--create-collection-type` option
and set it to *edge*:

    arangoimport --collection myedges --create-collection true --create-collection-type edge ...

When importing data into an existing collection it is often convenient to first
remove all data from the collection and then start the import. This can be achieved
by passing the `--overwrite` parameter to _arangoimport_. If it is set to *true*,
any existing data in the collection will be removed prior to the import. Note
that any existing index definitions for the collection will be preserved even if
`--overwrite` is set to true.

    arangoimport --file "data.jsonl" --type jsonl --collection users --overwrite true

Data gets imported into the specified collection in the default database
(*_system*). To specify a different database, use the `--server.database`
option when invoking _arangoimport_. If you want to import into a nonexistent
database you need to pass `--create-database true` to create it on-the-fly.

The tool also supports parallel imports, with multiple threads. Using multiple
threads may provide a speedup, especially when using the RocksDB storage engine.
To specify the number of parallel threads use the `--threads` option:

    arangoimport --threads 4 --file "data.jsonl" --type jsonl --collection users

Using multiple threads may lead to a non-sequential import of the input
data. Data that appears later in the input file may be imported earlier than data
that appears earlier in the input file. This is normally not a problem but may cause
issues when when there are data dependencies or duplicates in the import data. In
this case, the number of threads should be set to 1.
