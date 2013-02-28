ArangoDB Importer {#ImpManualBasics}
====================================

@NAVIGATE_ImpManual

@EMBEDTOC{ImpManualBasicsTOC}

This manual describes the ArangoDB importer _arangoimp_, which can be used for
bulk imports.

The most convenient method to import a lot of data into ArangoDB is to use the
`arangoimp` command-line tool. It allows you to import data records from a file
into an existing database collection.

It is possible to import document keys with the documents using the `_key`
attribute. When importing into an edge collection, it is mandatory that all
imported documents have the `_from` and `_to` attributes, and that they contain
valid references.


Let's assume you want to import user records into an existing collection named
"users" on the server.

Importing JSON-encoded Data {#ImpManualBasicsJson}
==================================================

Let's further assume the import at hand is encoded in JSON. We'll be using these
example user records to import:

@verbinclude arangoimp-data-json

To import these records, all you need to do is to put them into a file (with one
line for each record to import) and run the following command:

    ./arangoimp --file "data.json" --type json --collection "users"

This will transfer the data to the server, import the records, and print a
status summary. To show the intermediate progress during the import process, the
option `--progress` can be added. This option will show the percentage of the
input file that has been sent to the server. This will only be useful for big
import files.

    ./arangoimp --file "data.json" --type json --collection "users" --progress true

By default, the endpoint `tcp://127.0.0.1:8529` will be used.  If you want to
specify a different endpoint, you can use the --server.endpoint option. You
probably want to specify a database user and password as well.  You can do so by
using the options --server.username and --server.password.  If you do not
specify a password, you will be prompted for one.

    ./arangoimp --server.endpoint tcp://127.0.0.1:8529 --server.username root --file "data.json" --type json --collection "users"

Note that the collection (`users` in this case) must already exist or the import
will fail. If you want to create a new collection with the import data, you need
to specify the `--create-collection` option. Note that it is only possible to 
create a document collection using the `--create-collection` flag.

    ./arangoimp --file "data.json" --type json --collection "users" --create-collection true

As the import file already contains the data in JSON format, attribute names and
data types are fully preserved. As can be seen in the example data, there is no
need for all data records to have the same attribute names or types. Records can
be inhomogenous.

Importing CSV Data {#ImpManualBasicsCsv}
========================================

_arangoimp_ also offers the possibility to import data from CSV files. This
comes handy when the data at hand is in CSV format already and you don't want to
spend time converting them to JSON for the import.

To import data from a CSV file, make sure your file contains the attribute names
in the first row. All the following lines in the file will be interpreted as
data records and will be imported.

The CSV import requires the data to have a homogenuous structure. All records
must have exactly the same amount of columns as there are headers.

The cell values can have different data types though. If a cell does not have
any value, it can be left empty in the file. These values will not be imported
so the attributes will not "be there" in document created. Values enclosed in
quotes will be imported as strings, so to import numeric values, boolean values
or the null value, don't enclose the value into the quotes in your file.

We'll be using the following import for the CSV import:

@verbinclude arangoimp-data-csv

The command line to execute the import then is:

    arangoimp --file "data.csv" --type csv --collection "users"

Note that the quote and separator characters can be adjusted via the
`--quote` and `--separator` arguments when invoking _arangoimp_.  The importer
supports Windows (CRLF) and Unix (LF) line breaks.

Importing TSV Data {#ImpManualBasicsTsv}
========================================

You may also import tab-separated values (TSV) from a file. This format is very
simple: every line in the file represents a data record. There is no quoting or
escaping. That also means that the separator character (which defaults to the
tabstop symbol) must not be used anywhere in the actual data.

As with CSV, the first line in the TSV file must contain the attribute names,
and all lines must have an identical number of values.

If a different separator character or string should be used, it can be specified
with the `--separator` argument. 

An example command line to execute the TSV import is:

    arangoimp --file "data.tsv" --type tsv --collection "users" 
