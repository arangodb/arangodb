
@startDocuBlock JSF_import_json
@brief imports documents from JSON

@RESTHEADER{POST /_api/import#json,imports documents from JSON}

@RESTALLBODYPARAM{documents,string,required}
The body must either be a JSON-encoded array of objects or a string with
multiple JSON objects separated by newlines.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{type,string,required}
Determines how the body of the request will be interpreted. `type` can have
the following values:
- `documents`: when this type is used, each line in the request body is
  expected to be an individual JSON-encoded document. Multiple JSON objects
  in the request body need to be separated by newlines.
- `list`: when this type is used, the request body must contain a single
  JSON-encoded array of individual objects to import.
- `auto`: if set, this will automatically determine the body type (either
  `documents` or `list`).

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
index definitions will be preseved.

@RESTQUERYPARAM{waitForSync,boolean,optional}
Wait until documents have been synced to disk before returning.

@RESTQUERYPARAM{onDuplicate,string,optional}
Controls what action is carried out in case of a unique key constraint
violation. Possible values are:

- *error*: this will not import the current document because of the unique
  key constraint violation. This is the default setting.

- *update*: this will update an existing document in the database with the 
  data specified in the request. Attributes of the existing document that
  are not present in the request will be preseved.

- *replace*: this will replace an existing document in the database with the
  data specified in the request. 

- *ignore*: this will not update an existing document and simply ignore the
  error caused by a unique key constraint violation.

Note that that *update*, *replace* and *ignore* will only work when the
import document in the request contains the *_key* attribute. *update* and
*replace* may also fail because of secondary unique key constraint violations.

@RESTQUERYPARAM{complete,boolean,optional}
If set to `true` or `yes`, it will make the whole import fail if any error
occurs. Otherwise the import will continue even if some documents cannot
be imported.

@RESTQUERYPARAM{details,boolean,optional}
If set to `true` or `yes`, the result will include an attribute `details`
with details about documents that could not be imported.

@RESTDESCRIPTION
**NOTE** Swagger examples won't work due to the anchor.


Creates documents in the collection identified by `collection-name`.
The JSON representations of the documents must be passed as the body of the
POST request. The request body can either consist of multiple lines, with
each line being a single stand-alone JSON object, or a singe JSON array with
sub-objects.

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

Importing documents with heterogenous attributes from a JSON array

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
    var r = JSON.parse(response.body);
    assert(r.created === 3);
    assert(r.errors === 0);
    assert(r.empty === 0);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Importing documents from individual JSON lines

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
    var r = JSON.parse(response.body);
    assert(r.created === 3);
    assert(r.errors === 0);
    assert(r.empty === 1);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Using the auto type detection

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
    var r = JSON.parse(response.body);
    assert(r.created === 3);
    assert(r.errors === 0);
    assert(r.empty === 0);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Importing into an edge collection, with attributes `_from`, `_to` and `name`

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
    var r = JSON.parse(response.body);
    assert(r.created === 2);
    assert(r.errors === 0);
    assert(r.empty === 0);

    logJsonResponse(response);
    db._drop(cn);
    db._drop("products");
@END_EXAMPLE_ARANGOSH_RUN

Importing into an edge collection, omitting `_from` or `_to`

@EXAMPLE_ARANGOSH_RUN{RestImportJsonEdgeInvalid}
    db._flushCache();
    var cn = "links";
    db._drop(cn);
    db._createEdgeCollection(cn);
    db._flushCache();

    var body = [ { name: "some name" } ];

    var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&type=list&details=true", body);

    assert(response.code === 201);
    var r = JSON.parse(response.body);
    assert(r.created === 0);
    assert(r.errors === 1);
    assert(r.empty === 0);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Violating a unique constraint, but allow partial imports

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
    var r = JSON.parse(response.body);
    assert(r.created === 1);
    assert(r.errors === 1);
    assert(r.empty === 0);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Violating a unique constraint, not allowing partial imports

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

Using a non-existing collection

@EXAMPLE_ARANGOSH_RUN{RestImportJsonInvalidCollection}
    var cn = "products";
    db._drop(cn);

    var body = '{ "name": "test" }';

    var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&type=documents", body);

    assert(response.code === 404);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Using a malformed body

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
@endDocuBlock

