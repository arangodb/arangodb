////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_put_api_simple_last
/// @brief returns the last document(s) of a collection
///
/// @RESTHEADER{PUT /_api/simple/last, Last document of a collection}
///
/// @RESTBODYPARAM{collection,string,required,string}
///  the name of the collection
///
/// @RESTBODYPARAM{count,integer,required,int64}
/// the number of documents to return at most. Specifying count is
/// optional. If it is not specified, it defaults to 1.
///
/// @RESTDESCRIPTION
///
/// This will return the last documents from the collection, in the order of
/// insertion/update time. When the *count* argument is supplied, the result
/// will be an array of documents, with the "latest" document being first in the
/// result array.
///
/// If the *count* argument is not supplied, the result is the "latest" document
/// of the collection, or *null* if the collection is empty.
///
/// Note: this method is not supported for sharded collections with more than
/// one shard.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned when the query was successfully executed.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// query. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by *collection* is unknown.  The
/// response body contains an error document in this case.
///
/// @EXAMPLES
///
/// Retrieving the last n documents
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleLast}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn);
///     products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
///     products.save({ "a": { "j": 1 }, "i": 1});
///     products.save({ "i": 1});
///     products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
///     var url = "/_api/simple/last";
///     var body = { "collection": "products", "count" : 2 };
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Retrieving the first document
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleLastSingle}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn);
///     products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
///     products.save({ "a": { "j": 1 }, "i": 1});
///     products.save({ "i": 1});
///     products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
///     var url = "/_api/simple/last";
///     var body = { "collection": "products" };
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////