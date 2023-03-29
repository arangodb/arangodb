
@startDocuBlock post_api_import
@brief imports documents from JSON

@RESTHEADER{POST /_api/import,Import JSON data as documents, importData}

@RESTDESCRIPTION
Load JSON data and store it as documents into the specified collection.

The request body can have different JSON formats:
- One JSON object per line (JSONL)
- A JSON array of objects
- One JSON array per line (CSV-like)

If you import documents into edge collections, all documents require a `_from`
and a `_to` attribute.

@RESTALLBODYPARAM{documents,string,required}
The body must either be a JSON-encoded array of objects or a string with
multiple JSON objects separated by newlines.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{collection,string,required}
The name of the target collection. The collection needs to exist already.

@RESTQUERYPARAM{type,string,optional}
Determines how the body of the request is interpreted.

- `documents`: JSON Lines (JSONL) format. Each line is expected to be one
  JSON object.

  Example:

  ```json
  {"_key":"john","name":"John Smith","age":35}
  {"_key":"katie","name":"Katie Foster","age":28}
  ```

- `array` (or `list`): JSON format. The request body is expected to be a
  JSON array of objects. This format requires ArangoDB to parse the complete
  array and keep it in memory for the duration of the import. This is more
  resource-intensive than the line-wise JSONL processing.
  
  Any whitespace outside of strings is ignored, which means the JSON data can be
  a single line or be formatted as multiple lines.

  Example:

  ```json
  [
    {"_key":"john","name":"John Smith","age":35},
    {"_key":"katie","name":"Katie Foster","age":28}
  ]
  ```

- `auto`: automatically determines the type (either `documents` or `array`).

- Omit the `type` parameter entirely to import JSON arrays of tabular data,
  similar to CSV.
  
  The first line is an array of strings that defines the attribute keys. The
  subsequent lines are arrays with the attribute values. The keys and values
  are matched by the order of the array elements.
  
  Example:

  ```json
  ["_key","name","age"]
  ["john","John Smith",35]
  ["katie","Katie Foster",28]
  ```

@RESTQUERYPARAM{fromPrefix,string,optional}
An optional prefix for the values in `_from` attributes. If specified, the
value is automatically prepended to each `_from` input value. This allows
specifying just the keys for `_from`.

@RESTQUERYPARAM{toPrefix,string,optional}
An optional prefix for the values in `_to` attributes. If specified, the
value is automatically prepended to each `_to` input value. This allows
specifying just the keys for `_to`.

@RESTQUERYPARAM{overwrite,boolean,optional}
If this parameter has a value of `true` or `yes`, then all data in the
collection will be removed prior to the import. Note that any existing
index definitions will be preserved.

@RESTQUERYPARAM{waitForSync,boolean,optional}
Wait until documents have been synced to disk before returning.

@RESTQUERYPARAM{onDuplicate,string,optional}
Controls what action is carried out in case of a unique key constraint
violation. Possible values are:

- `error`: this will not import the current document because of the unique
  key constraint violation. This is the default setting.
- `update`: this will update an existing document in the database with the
  data specified in the request. Attributes of the existing document that
  are not present in the request will be preserved.
- `replace`: this will replace an existing document in the database with the
  data specified in the request.
- `ignore`: this will not update an existing document and simply ignore the
  error caused by a unique key constraint violation.

Note that `update`, `replace` and `ignore` will only work when the
import document in the request contains the `_key` attribute. `update` and
`replace` may also fail because of secondary unique key constraint violations.

@RESTQUERYPARAM{complete,boolean,optional}
If set to `true`, the whole import fails if any error occurs. Otherwise, the
import continues even if some documents are invalid and cannot be imported,
skipping the problematic documents.

@RESTQUERYPARAM{details,boolean,optional}
If set to `true`, the result includes a `details` attribute with information
about documents that could not be imported.

@RESTRETURNCODES

@RESTRETURNCODE{201}
is returned if all documents could be imported successfully.

The response is a JSON object with the following attributes:

@RESTREPLYBODY{created,integer,required,}
The number of imported documents.

@RESTREPLYBODY{errors,integer,required,}
The number of documents that were not imported due to errors.

@RESTREPLYBODY{empty,integer,required,}
The number of empty lines found in the input. Only greater than zero for the
types `documents` and `auto`.

@RESTREPLYBODY{updated,integer,required,}
The number of updated/replaced documents. Only greater than zero if `onDuplicate`
is set to either `update` or `replace`.

@RESTREPLYBODY{ignored,integer,required,}
The number of failed but ignored insert operations. Only greater than zero if
`onDuplicate` is set to `ignore`.

@RESTREPLYBODY{details,array,optional,string}
An array with the error messages caused by documents that could not be imported.
Only present if `details` is set to `true`.

@RESTRETURNCODE{400}
is returned if `type` contains an invalid value, no `collection` is
specified, the documents are incorrectly encoded, or the request
is malformed.

@RESTRETURNCODE{404}
is returned if `collection` or the `_from` or `_to` attributes of an
imported edge refer to an unknown collection.

@RESTRETURNCODE{409}
is returned if the import would trigger a unique key violation and
`complete` is set to `true`.

@RESTRETURNCODE{500}
is returned if the server cannot auto-generate a document key (out of keys
error) for a document with no user-defined key.

@EXAMPLES

Importing documents with heterogenous attributes from an array of JSON objects:

@EXAMPLE_ARANGOSH_RUN{RestImportJsonList}
    db._flushCache();
    var cn = "products";
    db._drop(cn);
    db._create(cn);
    db._flushCache();

    var body = [
      { _key: "abc", value1: 25, value2: "test", allowed: true },
      { _key: "foo", name: "baz" },
      { name: { detailed: "detailed name", short: "short name" } }
    ];

    var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&type=list", body);

    assert(response.code === 201);
    assert(response.parsedBody.created === 3);
    assert(response.parsedBody.errors === 0);
    assert(response.parsedBody.empty === 0);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Importing documents using JSON objects separated by new lines (JSONL):

@EXAMPLE_ARANGOSH_RUN{RestImportJsonLines}
    db._flushCache();
    var cn = "products";
    db._drop(cn);
    db._create(cn);
    db._flushCache();

    var body = '{ "_key": "abc", "value1": 25, "value2": "test",' +
               '"allowed": true }\n' +
               '{ "_key": "foo", "name": "baz" }\n\n' +
               '{ "name": {' +
               ' "detailed": "detailed name", "short": "short name" } }\n';
    var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn
    + "&type=documents", body);

    assert(response.code === 201);
    assert(response.parsedBody.created === 3);
    assert(response.parsedBody.errors === 0);
    assert(response.parsedBody.empty === 1);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Using the `auto` type detection:

@EXAMPLE_ARANGOSH_RUN{RestImportJsonType}
    db._flushCache();
    var cn = "products";
    db._drop(cn);
    db._create(cn);
    db._flushCache();

    var body = [
      { _key: "abc", value1: 25, value2: "test", allowed: true },
      { _key: "foo", name: "baz" },
      { name: { detailed: "detailed name", short: "short name" } }
    ];

    var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&type=auto", body);

    assert(response.code === 201);
    assert(response.parsedBody.created === 3);
    assert(response.parsedBody.errors === 0);
    assert(response.parsedBody.empty === 0);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Importing JSONL into an edge collection, with `_from`, `_to` and `name`
attributes:

@EXAMPLE_ARANGOSH_RUN{RestImportJsonEdge}
    db._flushCache();
    var cn = "links";
    db._drop(cn);
    db._createEdgeCollection(cn);
    db._drop("products");
    db._create("products");
    db._flushCache();

    var body = '{ "_from": "products/123", "_to": "products/234" }\n' +
               '{"_from": "products/332", "_to": "products/abc", ' +
               '  "name": "other name" }';

    var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&type=documents", body);

    assert(response.code === 201);
    assert(response.parsedBody.created === 2);
    assert(response.parsedBody.errors === 0);
    assert(response.parsedBody.empty === 0);

    logJsonResponse(response);
    db._drop(cn);
    db._drop("products");
@END_EXAMPLE_ARANGOSH_RUN

Importing an array of JSON objects into an edge collection,
omitting `_from` or `_to`:

@EXAMPLE_ARANGOSH_RUN{RestImportJsonEdgeInvalid}
    db._flushCache();
    var cn = "links";
    db._drop(cn);
    db._createEdgeCollection(cn);
    db._flushCache();

    var body = [ { name: "some name" } ];

    var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&type=list&details=true", body);

    assert(response.code === 201);
    assert(response.parsedBody.created === 0);
    assert(response.parsedBody.errors === 1);
    assert(response.parsedBody.empty === 0);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Violating a unique constraint, but allowing partial imports:

@EXAMPLE_ARANGOSH_RUN{RestImportJsonUniqueContinue}
    var cn = "products";
    db._drop(cn);
    db._create(cn);
    db._flushCache();

    var body = '{ "_key": "abc", "value1": 25, "value2": "test" }\n' +
               '{ "_key": "abc", "value1": "bar", "value2": "baz" }';

    var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn
    + "&type=documents&details=true", body);

    assert(response.code === 201);
    assert(response.parsedBody.created === 1);
    assert(response.parsedBody.errors === 1);
    assert(response.parsedBody.empty === 0);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Violating a unique constraint, not allowing partial imports:

@EXAMPLE_ARANGOSH_RUN{RestImportJsonUniqueFail}
    var cn = "products";
    db._drop(cn);
    db._create(cn);
    db._flushCache();

    var body = '{ "_key": "abc", "value1": 25, "value2": "test" }\n' +
               '{ "_key": "abc", "value1": "bar", "value2": "baz" }';

    var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&type=documents&complete=true", body);

    assert(response.code === 409);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Using a non-existing collection:

@EXAMPLE_ARANGOSH_RUN{RestImportJsonInvalidCollection}
    var cn = "products";
    db._drop(cn);

    var body = '{ "name": "test" }';

    var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&type=documents", body);

    assert(response.code === 404);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Using a malformed body with an array of JSON objects being expected:

@EXAMPLE_ARANGOSH_RUN{RestImportJsonInvalidBody}
    var cn = "products";
    db._drop(cn);
    db._create(cn);
    db._flushCache();

    var body = '{ }';

    var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&type=list", body);

    assert(response.code === 400);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Importing two documents using the JSON arrays format. The documents have a
`_key`, `value1`, and `value2` attribute each. One line in the import data is
empty and skipped:

@EXAMPLE_ARANGOSH_RUN{RestImportCsvExample}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var body = '[ "_key", "value1", "value2" ]\n' +
               '[ "abc", 25, "test" ]\n\n' +
               '[ "foo", "bar", "baz" ]';

    var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn, body);

    assert(response.code === 201);
    assert(response.parsedBody.created === 2);
    assert(response.parsedBody.errors === 0);
    assert(response.parsedBody.empty === 1);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Importing JSON arrays into an edge collection, with `_from`, `_to`, and `name`
attributes:

@EXAMPLE_ARANGOSH_RUN{RestImportCsvEdge}
    var cn = "links";
    db._drop(cn);
    db._createEdgeCollection(cn);
    db._drop("products");
    db._create("products");

    var body = '[ "_from", "_to", "name" ]\n' +
               '[ "products/123","products/234", "some name" ]\n' +
               '[ "products/332", "products/abc", "other name" ]';

    var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn, body);

    assert(response.code === 201);
    assert(response.parsedBody.created === 2);
    assert(response.parsedBody.errors === 0);
    assert(response.parsedBody.empty === 0);

    logJsonResponse(response);
    db._drop(cn);
    db._drop("products");
@END_EXAMPLE_ARANGOSH_RUN

Importing JSON arrays into an edge collection, omitting `_from` or `_to`:

@EXAMPLE_ARANGOSH_RUN{RestImportCsvEdgeInvalid}
    var cn = "links";
    db._drop(cn);
    db._createEdgeCollection(cn);

    var body = '[ "name" ]\n[ "some name" ]\n[ "other name" ]';

    var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&details=true", body);

    assert(response.code === 201);
    assert(response.parsedBody.created === 0);
    assert(response.parsedBody.errors === 2);
    assert(response.parsedBody.empty === 0);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Using a malformed body with JSON arrays being expected:

@EXAMPLE_ARANGOSH_RUN{RestImportCsvInvalidBody}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var body = '{ "_key": "foo", "value1": "bar" }';

    var response = logCurlRequest('POST', "/_api/import?collection=" + cn, body);

    assert(response.code === 400);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
