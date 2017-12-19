!CHAPTER Importing Headers and Values

When using this type of import, the attribute names of the documents to be
imported are specified separate from the actual document value data.  The first
line of the HTTP POST request body must be a JSON array containing the attribute
names for the documents that follow.  The following lines are interpreted as the
document data. Each document must be a JSON array of values. No attribute names
are needed or allowed in this data section.

*Examples*

```js
curl --data-binary @- -X POST --dump - "http://localhost:8529/_api/import?collection=test"
[ "firstName", "lastName", "age", "gender" ]
[ "Joe", "Public", 42, "male" ]
[ "Jane", "Doe", 31, "female" ]

HTTP/1.1 201 Created
Server: ArangoDB
Connection: Keep-Alive
Content-type: application/json; charset=utf-8

{"error":false,"created":2,"empty":0,"errors":0}
```

The server will again respond with an HTTP 201 if everything went well. The
number of documents imported will be returned in the *created* attribute of the
response. If any documents were skipped or incorrectly formatted, this will be
returned in the *errors* attribute. The number of empty lines in the input file
will be returned in the *empty* attribute.

If the *details* parameter was set to *true* in the request, the response will 
also contain an attribute *details* which is an array of details about errors that
occurred on the server side during the import. This array might be empty if no
errors occurred.

!SECTION Importing into Edge Collections

Please note that when importing documents into an
[edge collection](../../Manual/Appendix/Glossary.html#edge-collection), 
it is mandatory that all imported documents contain the *_from* and *_to* attributes,
and that these contain references to existing collections.
