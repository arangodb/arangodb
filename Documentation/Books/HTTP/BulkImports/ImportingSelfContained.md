Importing Self-Contained JSON Documents
=======================================

This import method allows uploading self-contained JSON documents. The documents
must be uploaded in the body of the HTTP POST request. Each line of the body
will be interpreted as one stand-alone document. Empty lines in the body are
allowed but will be skipped. Using this format, the documents are imported
line-wise.

Example input data:
    { "_key": "key1", ... }
    { "_key": "key2", ... }
    ...

To use this method, the *type* query parameter should be set to *documents*.

It is also possible to upload self-contained JSON documents that are embedded
into a JSON array. Each element from the array will be treated as a document and
be imported.

Example input data for this case:

```js
[
  { "_key": "key1", ... },
  { "_key": "key2", ... },
  ...
]
```

This format does not require each document to be on a separate line, and any
whitespace in the JSON data is allowed. It can be used to import a
JSON-formatted result array (e.g. from arangosh) back into ArangoDB.  Using this
format requires ArangoDB to parse the complete array and keep it in memory for
the duration of the import. This might be more resource-intensive than the
line-wise processing.

To use this method, the *type* query parameter should be set to *array*.

Setting the *type* query parameter to *auto* will make the server auto-detect whether
the data are line-wise JSON documents (type = documents) or a JSON array (type = array).

*Examples*

```js
curl --data-binary @- -X POST --dump - "http://localhost:8529/_api/import?type=documents&collection=test"
{ "name" : "test", "gender" : "male", "age" : 39 }
{ "type" : "bird", "name" : "robin" }

HTTP/1.1 201 Created
Server: ArangoDB
Connection: Keep-Alive
Content-type: application/json; charset=utf-8

{"error":false,"created":2,"empty":0,"errors":0}
```

The server will respond with an HTTP 201 if everything went well. The number of
documents imported will be returned in the *created* attribute of the
response. If any documents were skipped or incorrectly formatted, this will be
returned in the *errors* attribute. There will also be an attribute *empty* in 
the response, which will contain a value of *0*.

If the *details* parameter was set to *true* in the request, the response will 
also contain an attribute *details* which is an array of details about errors that
occurred on the server side during the import. This array might be empty if no
errors occurred.


