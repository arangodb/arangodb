Exporting Data from an ArangoDB database
======================================

To export data from an ArangoDB server instance, you will need to invoke _arangoexport_.
_arangoexport_ can be invoked by executing
the following command:

    unix> arangoexport --collection test --output-directory "dump"

This exports the collections *test* into the directory *dump* as one big json array. Every entry
in this array is one document from the collection without a specific order. To export more than
one collection at a time specify multiple *--collection* options.

The default output directory is *export*.

_arangoexport_ will by default connect to the *_system* database using the default
endpoint. If you want to connect to a different database or a different endpoint, 
or use authentication, you can use the following command-line options:

* *--server.database <string>*: name of the database to connect to
* *--server.endpoint <string>*: endpoint to connect to
* *--server.username <string>*: username
* *--server.password <string>*: password to use (omit this and you'll be prompted for the
  password)
* *--server.authentication <bool>*: whether or not to use authentication

Here's an example of exporting data from a non-standard endpoint, using a dedicated
[database name](../Appendix/Glossary.md#database-name):

    unix> arangoexport --server.endpoint tcp://192.168.173.13:8531 --server.username backup --server.database mydb --collection test --output-directory "my-export"

When finished, _arangoexport_ will print out a summary line with some aggregate 
statistics about what it did, e.g.:

    Processed 2 collection(s), wrote 9031763 Byte(s), 78 HTTP request(s)


Export JSON
-----------

    unix> arangoexport --type json --collection test

This exports the collection *test* into the output directory *export* as one json array.
Every array entry is one document from the collection *test*

Export JSONL
------------

    unix> arangoexport --type jsonl --collection test

This exports the collection *test* into the output directory *export* as [jsonl](http://jsonlines.org). Every line in the export is one document from the collection *test* as json.

Export CSV
----------

    unix> arangoexport --type csv --collection test --fields _key,_id,_rev

This exports the collection *test* into the output directory *export* as CSV. The first
line contains the header with all field names. Each line is one document represented as
CSV and separated with a comma. Objects and Arrays are represented as a JSON string.


Export XML
----------

    unix> arangoexport --type xml --collection test

This exports the collection *test* into the output directory *export* as generic XML. The root element of the generated XML file is named *collection*. Each document in the collection is exported in a *doc* XML attribute. Each document attribute is export in a generic *att* element, which has a *type* attribute indicating the attribute value, and a *value* attribute containing the attribute's value.

Export XGMML
------------

[XGMML](https://en.wikipedia.org/wiki/XGMML) is an XML application based on [GML](https://en.wikipedia.org/wiki/Graph_Modelling_Language). To view the XGMML file you can use for example [Cytoscape](http://cytoscape.org).

## important note
If you export all attributes (*--xgmml-label-only false*) keep in mind that a atrribute names type have to be the same type for all documents. It wont work if you have a attribute named rank that is in one document a string and in another document a integer.

Bad

    // doc1
    {
        "rank": 1
    }
    // doc2
    {
        "rank": "2"
    }

Good

    // doc1
    {
        "rank": 1
    }
    // doc2
    {
        "rank": 2
    }


## XGMML specific options

*--xgmml-label-attribute* specify the name of the attribute that will become the label in the xgmml file.

*--xgmml-label-only* set to true will only export the label without any attributes in edges or nodes.


## export based on collections

    unix> arangoexport --type xgmml --graph-name mygraph --collection vertex --collection edge

This exports the a unnamed graph with vertex collection *vertex* and edge collection *edge* into the xgmml file *mygraph.xgmml*.


## export based on a named graph

    unix> arangoexport --type xgmml --graph-name mygraph

This exports the named graph mygraph into the xgmml file *mygraph.xgmml*.


## export XGMML without attributes

    unix> arangoexport --type xgmml --graph-name mygraph --xgmml-label-only true

This exports the named graph mygraph into the xgmml file *mygraph.xgmml* without the *&lt;att&gt;* tag in nodes and edges.


## export XGMML with a specific label

    unix> arangoexport --type xgmml --graph-name mygraph --xgmml-label-attribute name

This exports the named graph mygraph into the xgmml file *mygraph.xgmml* with a label from documents attribute *name* instead of the default attribute *label*.

Export via AQL query
--------------------

    unix> arangoexport --type jsonl --query "for book in books filter book.sells > 100 return book"

Export via an aql query allows you to export the returned data as the type specified with *--type*.
The example exports all books as jsonl that are sold more than 100 times.