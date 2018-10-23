Arangoimport Examples: CSV / TSV
================================

Importing CSV Data
------------------

_arangoimport_ offers the possibility to import data from CSV files. This
comes handy when the data at hand is in CSV format already and you don't want to
spend time converting them to JSON for the import.

To import data from a CSV file, make sure your file contains the attribute names
in the first row. All the following lines in the file will be interpreted as
data records and will be imported.

The CSV import requires the data to have a homogeneous structure. All records
must have exactly the same amount of columns as there are headers. By default,
lines with a different number of values will not be imported and there will be 
warnings for them. To still import lines with less values than in the header,
there is the *--ignore-missing* option. If set to true, lines that have a
different amount of fields will be imported. In this case only those attributes
will be populated for which there are values. Attributes for which there are
no values present will silently be discarded.

Example:

```
"first","last","age","active","dob"
"John","Connor",25,true
"Jim","O'Brady"
```

With *--ignore-missing* this will produce the following documents:

```js
{ "first" : "John", "last" : "Connor", "active" : true, "age" : 25 }
{ "first" : "Jim", "last" : "O'Brady" }
```

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

    arangoimport --file "data.csv" --type csv --collection "users"

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
*--quote* and *--separator* arguments when invoking _arangoimport_. The quote
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

Importing TSV Data
------------------

You may also import tab-separated values (TSV) from a file. This format is very
simple: every line in the file represents a data record. There is no quoting or
escaping. That also means that the separator character (which defaults to the
tabstop symbol) must not be used anywhere in the actual data.

As with CSV, the first line in the TSV file must contain the attribute names,
and all lines must have an identical number of values.

If a different separator character or string should be used, it can be specified
with the *--separator* argument.

An example command line to execute the TSV import is:

    arangoimport --file "data.tsv" --type tsv --collection "users"

Attribute Name Translation
--------------------------

For the CSV and TSV input formats, attribute names can be translated automatically.
This is useful in case the import file has different attribute names than those
that should be used in ArangoDB.

A common use case is to rename an "id" column from the input file into "_key" as
it is expected by ArangoDB. To do this, specify the following translation when
invoking arangoimport:

    arangoimport --file "data.csv" --type csv --translate "id=_key"

Other common cases are to rename columns in the input file to *_from* and *_to*:

    arangoimport --file "data.csv" --type csv --translate "from=_from" --translate "to=_to"

The *translate* option can be specified multiple types. The source attribute name
and the target attribute must be separated with a *=*.

Ignoring Attributes
-------------------

For the CSV and TSV input formats, certain attribute names can be ignored on imports.
In an ArangoDB cluster there are cases where this can come in handy,
when your documents already contain a `_key` attribute
and your collection has a sharding attribute other than `_key`: In the cluster this
configuration is not supported, because ArangoDB needs to guarantee the uniqueness of the `_key`
attribute in *all* shards of the collection.

    arangoimport --file "data.csv" --type csv --remove-attribute "_key"

The same thing would apply if your data contains an *_id* attribute:

    arangoimport --file "data.csv" --type csv --remove-attribute "_id"
