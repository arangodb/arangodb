# Importing data

## Problem

I want to import data from a file into ArangoDB.

## Solution

ArangoDB comes with a command-line tool utility named `arangoimp`. This utility can be
used for importing JSON-encoded, CSV, and tab-separated files into ArangoDB.

`arangoimp` needs to be invoked from the command-line once for each import file.
The target collection can already exist or can be created by the import run.

## Importing JSON-encoded data

### Input formats

There are two supported input formats for importing JSON-encoded data into ArangoDB:

* **line-by-line format**: This format expects each line in the input file to be a valid
  JSON objects. No line breaks must occur within each single JSON object

* **array format**: Expects a file containing a single array of JSON objects. Whitespace is
  allowed for formatting inside the JSON array and the JSON objects

Here's an example for the **line-by-line format** looks like this:

```js
{"author":"Frank Celler","time":"2011-10-26 08:42:49 +0200","sha":"c413859392a45873936cbe40797970f8eed93ff9","message":"first commit","user":"f.celler"}
{"author":"Frank Celler","time":"2011-10-26 21:32:36 +0200","sha":"10bb77b8cc839201ff59a778f0c740994083c96e","message":"initial release","user":"f.celler"}
...
```

Here's an example for the same data in **array format**:

```js
[
  {
    "author": "Frank Celler",
    "time": "2011-10-26 08:42:49 +0200",
    "sha": "c413859392a45873936cbe40797970f8eed93ff9",
    "message": "first commit",
    "user": "f.celler"
  },
  {
    "author": "Frank Celler",
    "time": "2011-10-26 21:32:36 +0200",
    "sha": "10bb77b8cc839201ff59a778f0c740994083c96e",
    "message": "initial release",
    "user": "f.celler"
  },
  ...
]
```

### Importing JSON data in line-by-line format

An example data file in **line-by-line format** can be downloaded 
[here](http://jsteemann.github.io/downloads/code/git-commits-single-line.json). The example
file contains all the commits to the ArangoDB repository as shown by `git log --reverse`.

The following commands will import the data from the file into a collection named `commits`:

```bash
# download file
wget http://jsteemann.github.io/downloads/code/git-commits-single-line.json

# actually import data
arangoimp --file git-commits-single-line.json --collection commits --create-collection true
```

Note that no file type has been specified when `arangoimp` was invoked. This is because `json`
is its default input format.

The other parameters used have the following meanings:

- `file`: input filename
- `collection`: name of the target collection
- `create-collection`: whether or not the collection should be created if it does not exist

The result of the import printed by `arangoimp` should be:

```
created:          20039
warnings/errors:  0
total:            20039
```

The collection `commits` should now contain the example commit data as present in the input file.

### Importing JSON data in array format

An example input file for the **array format** can be found [here](http://jsteemann.github.io/downloads/code/git-commits-array.json).

The command for importing JSON data in **array format** is similar to what we've done before:

```bash
# download file
wget http://jsteemann.github.io/downloads/code/git-commits-array.json

# actually import data
arangoimp --file git-commits-array.json --collection commits --create-collection true
```

Though the import command is the same (except the filename), there is a notable difference between the
two JSON formats: for the **array format**, `arangoimp` will read and parse the JSON in its entirety
before it sends any data to the ArangoDB server. That means the whole input file must fit into 
`arangoimp`'s buffer. By default, `arangoimp` will allocate a 16 MiB internal buffer, and input files bigger 
than that will be rejected with the following message:

```
import file is too big. please increase the value of --batch-size (currently 16777216).
```

So for JSON input files in **array format** it might be necessary to increase the value of `--batch-size`
in order to have the file imported. Alternatively, the input file can be converted to **line-by-line format**
manually.


### Importing CSV data

Data can also be imported from a CSV file. An example file can be found [here](http://jsteemann.github.io/downloads/code/git-commits.csv).

The `--type` parameter for the import command must now be set to `csv`:

```bash
# download file
wget http://jsteemann.github.io/downloads/code/git-commits.csv

# actually import data
arangoimp --file git-commits.csv --type csv --collection commits --create-collection true
```

For the CSV import, the first line in the input file has a special meaning: every value listed in the
first line will be treated as an attribute name for the values in all following lines. All following
lines should also have the same number of "columns".

"columns" inside the CSV input file can be left empty though. If a "column" is left empty in a line,
then this value will be omitted for the import so the respective attribute will not be set in the imported
document. Note that values from the input file that are enclosed in double quotes will always be imported as 
strings. To import numeric values, boolean values or the `null` value, don't enclose these values in quotes in
the input file. Note that leading zeros in numeric values will be removed. Importing numbers with leading 
zeros will only work when putting the numbers into strings.

Here is an example CSV file:

```plain
"author","time","sha","message"
"Frank Celler","2011-10-26 08:42:49 +0200","c413859392a45873936cbe40797970f8eed93ff9","first commit"
"Frank Celler","2011-10-26 21:32:36 +0200","10bb77b8cc839201ff59a778f0c740994083c96e","initial release"
...
```

`arangoimp` supports Windows (CRLF) and Unix (LF) line breaks. Line breaks might also occur inside values 
that are enclosed with the quote character.

The default separator for CSV files is the comma. It can be changed using the `--separator` parameter
when invoking `arangoimp`. The quote character defaults to the double quote (**"**). To use a literal double 
quote inside a "column" in the import data, use two double quotes. To change the quote character, use the
`--quote` parameter. To use a backslash for escaping quote characters, please set the option `--backslash-escape`
to `true`.


### Changing the database and server endpoint

By default, `arangoimp` will connect to the default database on `127.0.0.1:8529` with a user named
`root`. To change this, use the following parameters:

- `server.database`: name of the database to use when importing (default: `_system`)
- `server.endpoint`: address of the ArangoDB server (default: `tcp://127.0.0.1:8529`)


### Using authentication

`arangoimp` will by default send an username `root` and an empty password to the ArangoDB
server. This is ArangoDB's default configuration, and it should be changed. To make `arangoimp`
use a different username or password, the following command-line arguments can be used:

- `server.username`: username, used if authentication is enabled on server
- `server.password`: password for user, used if authentication is enabled on server

The password argument can also be omitted in order to avoid having it saved in the shell's
command-line history. When specifying a username but omitting the password parameter,
`arangoimp` will prompt for a password.


### Additional parameters

By default, `arangoimp` will import data into the specified collection but will not touch
existing data. Often it is convenient to first remove all data from a collection and then run
the import. `arangoimp` supports this with the optional `--overwrite` flag. When setting it to
`true`, all documents in the collection will be removed prior to the import.

**Author:** [Jan Steemann](https://github.com/jsteemann)

**Tags**: #arangoimp #import
