<a name="importing_self-contained_json_documents"></a>
# Importing Self-Contained JSON Documents

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

Setting the `type` URL parameter to `auto` will make the server auto-detect whether
the data are line-wise JSON documents (type = documents) or a JSON list (type = array).

__Examples__


@verbinclude api-import-documents

The server will respond with an HTTP 201 if everything went well. The number of
documents imported will be returned in the `created` attribute of the
response. If any documents were skipped or incorrectly formatted, this will be
returned in the `errors` attribute. There will also be an attribute `empty` in 
the response, which will contain a value of `0`.

If the `details` parameter was set to `true` in the request, the response will 
also contain an attribute `details` which is a list of details about errors that
occurred on the server side during the import. This list might be empty if no
errors occurred.


