
@startDocuBlock get_read_document_MULTI
@brief reads a single document

@RESTHEADER{PUT /_api/document/{collection}#get,Read multiple documents,readDocuments}

@RESTURLPARAMETERS

@RESTURLPARAM{collection,string,required}
Name of the *collection* from which the documents are to be read.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{onlyget,boolean,required}
This parameter is required to be **true**, otherwise a replace
operation is executed!

@RESTQUERYPARAM{ignoreRevs,string,optional}
Should the value be *true* (the default):
If a search document contains a value for the *_rev* field,
then the document is only returned if it has the same revision value.
Otherwise a precondition failed error is returned.

@RESTDESCRIPTION
Returns the documents identified by their *_key* in the body objects.
The body of the request _must_ contain a JSON array of either
strings (the *_key* values to lookup) or search documents.

A search document _must_ contain at least a value for the *_key* field.
A value for `_rev` _may_ be specified to verify whether the document
has the same revision value, unless _ignoreRevs_ is set to false.

Cluster only: The search document _may_ contain
values for the collection's pre-defined shard keys. Values for the shard keys
are treated as hints to improve performance. Should the shard keys
values be incorrect ArangoDB may answer with a *not found* error.

The returned array of documents contain three special attributes: *_id* containing the document
identifier, *_key* containing key which uniquely identifies a document
in a given collection and *_rev* containing the revision.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if no error happened

@RESTRETURNCODE{400}
is returned if the body does not contain a valid JSON representation
of an array of documents. The response body contains
an error document in this case.

@RESTRETURNCODE{404}
is returned if the collection was not found.

@EXAMPLES

Reading multiple documents identifier:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerReadMultiDocument}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    db.products.save({"_key":"doc1", "hello":"world"});
    db.products.save({"_key":"doc2", "say":"hi to mom"});
    var url = "/_api/document/products?onlyget=true";
    var body = '["doc1", {"_key":"doc2"}]';

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
