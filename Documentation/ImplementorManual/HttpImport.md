HTTP Interface for bulk imports {#HttpImport}
=============================================

@NAVIGATE_HttpImport
@EMBEDTOC{HttpImportTOC}

ArangoDB provides an HTTP interface to import multiple documents at once into a
collection. This is known as a bulk import.

The data uploaded must be provided in JSON format. There are two mechanisms to
import the data:
- self-contained documents: in this case, each document contains all 
  attribute names and values. Attribute names may be completely different
  among the documents uploaded
- attribute names plus document data: in this case, the first document must 
  be a JSON list containing the attribute names of the documents that follow.
  The following documents must be lists containing only the document data.
  Data will be mapped to the attribute names by attribute positions.

The endpoint address is `/_api/import` for both input mechanisms. Data must be
sent to this URL using an HTTP POST request. The data to import must be
contained in the body of the POST request.

The `collection` URL parameter must be used to specify the target collection for
the import. The optional URL parameter `createCollection` can be used to create
a non-existing collection during the import. If not used, importing data into a
non-existing collection will produce an error. Please note that the `createCollection`
flag can only be used to create document collections, not edge collections.

Importing self-contained documents {#HttpImportSelfContained}
=============================================================

This import method allows uploading self-contained JSON documents. The documents
must be uploaded in the body of the HTTP POST request. Each line of the body
will be interpreted as one stand-alone document. Empty lines in the body are
allowed but will be skipped. Using this format, the documents are imported
line-wise.

Example input data:
    { "_key": "key1", ... }
    { "_key": "key2", ... }
    ...

To use this method, the `type` URL parameter should be set to `documents`.

It is also possible to upload self-contained JSON documents that are embedded
into a JSON list. Each element from the list will be treated as a document and
be imported.

Example input data for this case:

    [
      { "_key": "key1", ... },
      { "_key": "key2", ... },
      ...
    ]

This format does not require each document to be on a separate line, and any
whitespace in the JSON data is allowed. It can be used to import a
JSON-formatted result list (e.g. from arangosh) back into ArangoDB.  Using this
format requires ArangoDB to parse the complete list and keep it in memory for
the duration of the import. This might be more resource-intensive than the
line-wise processing.

To use this method, the `type` URL parameter should be set to `array`.

@EXAMPLES

@verbinclude api-import-documents

The server will respond with an HTTP 201 if everything went well. The number of
documents imported will be returned in the `created` attribute of the
response. If any documents were skipped or incorrectly formatted, this will be
returned in the `errors` attribute.

Importing headers and values {#HttpImportHeaderData}
====================================================

When using this type of import, the attribute names of the documents to be
imported are specified separate from the actual document value data.  The first
line of the HTTP POST request body must be a JSON list containing the attribute
names for the documents that follow.  The following lines are interpreted as the
document data. Each document must be a JSON list of values. No attribute names
are needed or allowed in this data section.

@EXAMPLES

@verbinclude api-import-headers

The server will again respond with an HTTP 201 if everything went well. The
number of documents imported will be returned in the `created` attribute of the
response. If any documents were skipped or incorrectly formatted, this will be
returned in the `errors` attribute.

Importing in edge collections {#HttpImportEdges}
================================================

Please note that when importing documents into an edge collection, it is 
mandatory that all imported documents contain the `_from` and `_to` attributes,
and that these contain valid references.

Please also note that it is not possible to create a new edge collection on the
fly using the `createCollection` parameter.
