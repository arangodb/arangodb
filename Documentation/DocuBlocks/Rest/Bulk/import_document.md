
@startDocuBlock import_document
@brief imports documents from JSON-encoded lists

@RESTHEADER{POST /_api/import#document,imports document values, RestImportHandler#document}

@RESTALLBODYPARAM{documents,string,required}
The body must consist of JSON-encoded arrays of attribute values, with one
line per document. The first row of the request must be a JSON-encoded
array of attribute names. These attribute names are used for the data in the
subsequent lines.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{collection,string,required}
The collection name.

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
violation. Possible values are:<br>
- *error*: this will not import the current document because of the unique
  key constraint violation. This is the default setting.
- *update*: this will update an existing document in the database with the
  data specified in the request. Attributes of the existing document that
  are not present in the request will be preserved.
- *replace*: this will replace an existing document in the database with the
  data specified in the request.
- *ignore*: this will not update an existing document and simply ignore the
  error caused by the unique key constraint violation.<br>
Note that *update*, *replace* and *ignore* will only work when the
import document in the request contains the *_key* attribute. *update* and
*replace* may also fail because of secondary unique key constraint
violations.

@RESTQUERYPARAM{complete,boolean,optional}
If set to `true` or `yes`, it will make the whole import fail if any error
occurs. Otherwise the import will continue even if some documents cannot
be imported.

@RESTQUERYPARAM{details,boolean,optional}
If set to `true` or `yes`, the result will include an attribute `details`
with details about documents that could not be imported.

@RESTDESCRIPTION
Creates documents in the collection identified by `collection-name`.
The first line of the request body must contain a JSON-encoded array of
attribute names. All following lines in the request body must contain
JSON-encoded arrays of attribute values. Each line is interpreted as a
separate document, and the values specified will be mapped to the array
of attribute names specified in the first header line.

The response is a JSON object with the following attributes:

- `created`: number of documents imported.

- `errors`: number of documents that were not imported due to an error.

- `empty`: number of empty lines found in the input (will only contain a
  value greater zero for types `documents` or `auto`).

- `updated`: number of updated/replaced documents (in case `onDuplicate`
  was set to either `update` or `replace`).

- `ignored`: number of failed but ignored insert operations (in case
  `onDuplicate` was set to `ignore`).

- `details`: if query parameter `details` is set to true, the result will
  contain a `details` attribute which is an array with more detailed
  information about which documents could not be inserted.

@RESTRETURNCODES

@RESTRETURNCODE{201}
is returned if all documents could be imported successfully.

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

Importing two documents, with attributes `_key`, `value1` and `value2` each. One
line in the import data is empty

@EXAMPLE_ARANGOSH_RUN{RestImportCsvExample}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var body = '[ "_key", "value1", "value2" ]\n' +
               '[ "abc", 25, "test" ]\n\n' +
               '[ "foo", "bar", "baz" ]';

    var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn, body);

    assert(response.code === 201);
    var r = JSON.parse(response.body)
    assert(r.created === 2);
    assert(r.errors === 0);
    assert(r.empty === 1);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Importing into an edge collection, with attributes `_from`, `_to` and `name`

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
    var r = JSON.parse(response.body)
    assert(r.created === 2);
    assert(r.errors === 0);
    assert(r.empty === 0);

    logJsonResponse(response);
    db._drop(cn);
    db._drop("products");
@END_EXAMPLE_ARANGOSH_RUN

Importing into an edge collection, omitting `_from` or `_to`

@EXAMPLE_ARANGOSH_RUN{RestImportCsvEdgeInvalid}
    var cn = "links";
    db._drop(cn);
    db._createEdgeCollection(cn);

    var body = '[ "name" ]\n[ "some name" ]\n[ "other name" ]';

    var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&details=true", body);

    assert(response.code === 201);
    var r = JSON.parse(response.body)
    assert(r.created === 0);
    assert(r.errors === 2);
    assert(r.empty === 0);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Violating a unique constraint, but allow partial imports

@EXAMPLE_ARANGOSH_RUN{RestImportCsvUniqueContinue}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var body = '[ "_key", "value1", "value2" ]\n' +
               '[ "abc", 25, "test" ]\n' +
               '["abc", "bar", "baz" ]';

    var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&details=true", body);

    assert(response.code === 201);
    var r = JSON.parse(response.body)
    assert(r.created === 1);
    assert(r.errors === 1);
    assert(r.empty === 0);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Violating a unique constraint, not allowing partial imports

@EXAMPLE_ARANGOSH_RUN{RestImportCsvUniqueFail}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var body = '[ "_key", "value1", "value2" ]\n' +
               '[ "abc", 25, "test" ]\n' +
               '["abc", "bar", "baz" ]';

    var response = logCurlRequest('POST', "/_api/import?collection=" + cn + "&complete=true", body);

    assert(response.code === 409);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Using a non-existing collection

@EXAMPLE_ARANGOSH_RUN{RestImportCsvInvalidCollection}
    var cn = "products";
    db._drop(cn);

    var body = '[ "_key", "value1", "value2" ]\n' +
               '[ "abc", 25, "test" ]\n' +
               '["foo", "bar", "baz" ]';

    var response = logCurlRequest('POST', "/_api/import?collection=" + cn, body);

    assert(response.code === 404);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Using a malformed body

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
