////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_post_api_index_cap
/// @brief creates a cap constraint
///
/// @RESTHEADER{POST /_api/index#CapConstraints, Create cap constraint}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection,string,required}
/// The collection name.
///
/// @RESTBODYPARAM{type,string,required,string}
/// must be equal to *"cap"*.
///
/// @RESTBODYPARAM{size,integer,optional,int64}
/// The maximal number of documents for the collection. If specified,
/// the value must be greater than zero.
///
/// @RESTBODYPARAM{byteSize,integer,optional,int64}
/// The maximal size of the active document data in the collection
/// (in bytes). If specified, the value must be at least 16384.
///
///
/// @RESTDESCRIPTION
/// **NOTE** Swagger examples won't work due to the anchor.
///
///
///
/// Creates a cap constraint for the collection *collection-name*,
/// if it does not already exist. Expects an object containing the index details.
///
/// **Note**: The cap constraint does not index particular attributes of the
/// documents in a collection, but limits the number of documents in the
/// collection to a maximum value. The cap constraint thus does not support
/// attribute names specified in the *fields* attribute nor uniqueness of
/// any kind via the *unique* attribute.
///
/// It is allowed to specify either *size* or *byteSize*, or both at
/// the same time. If both are specified, then the automatic document removal
/// will be triggered by the first non-met constraint.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the index already exists, then an *HTTP 200* is returned.
///
/// @RESTRETURNCODE{201}
/// If the index does not already exist and could be created, then an *HTTP 201*
/// is returned.
///
/// @RESTRETURNCODE{400}
/// If either *size* or *byteSize* contain invalid values, then an *HTTP 400*
/// is returned.
///
/// @RESTRETURNCODE{404}
/// If the *collection-name* is unknown, then a *HTTP 404* is returned.
///
/// @EXAMPLES
///
/// Creating a cap constraint
///
/// @EXAMPLE_ARANGOSH_RUN{RestIndexCreateNewCapConstraint}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var url = "/_api/index?collection=" + cn;
///     var body = {
///       type: "cap",
///       size : 10
///     };
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////