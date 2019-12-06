
@startDocuBlock post_api_transaction
@brief execute a server-side transaction

@RESTHEADER{POST /_api/transaction, Execute transaction, executeCommit}

@RESTBODYPARAM{collections,string,required,string}
*collections* must be a JSON object that can have one or all sub-attributes
*read*, *write* or *exclusive*, each being an array of collection names or a
single collection name as string. Collections that will be written to in the
transaction must be declared with the *write* or *exclusive* attribute or it
will fail, whereas non-declared collections from which is solely read will be
added lazily. The optional sub-attribute *allowImplicit* can be set to *false*
to let transactions fail in case of undeclared collections for reading.
Collections for reading should be fully declared if possible, to avoid
deadlocks.
See [locking and isolation](../../Manual/Transactions/LockingAndIsolation.html)
for more information.

@RESTBODYPARAM{action,string,required,string}
the actual transaction operations to be executed, in the
form of stringified JavaScript code. The code will be executed on server
side, with late binding. It is thus critical that the code specified in
*action* properly sets up all the variables it needs.
If the code specified in *action* ends with a return statement, the
value returned will also be returned by the REST API in the *result*
attribute if the transaction committed successfully.

@RESTBODYPARAM{waitForSync,boolean,optional,}
an optional boolean flag that, if set, will force the
transaction to write all data to disk before returning.

@RESTBODYPARAM{allowImplicit,boolean,optional,}
Allow reading from undeclared collections.

@RESTBODYPARAM{lockTimeout,integer,optional,int64}
an optional numeric value that can be used to set a
timeout for waiting on collection locks. If not specified, a default
value will be used. Setting *lockTimeout* to *0* will make ArangoDB
not time out waiting for a lock.

@RESTBODYPARAM{params,string,optional,string}
optional arguments passed to *action*.

@RESTBODYPARAM{maxTransactionSize,integer,optional,int64}
Transaction size limit in bytes. Honored by the RocksDB storage engine only.

@RESTDESCRIPTION
The transaction description must be passed in the body of the POST request.

If the transaction is fully executed and committed on the server,
*HTTP 200* will be returned. Additionally, the return value of the
code defined in *action* will be returned in the *result* attribute.

For successfully committed transactions, the returned JSON object has the
following properties:

- *error*: boolean flag to indicate if an error occurred (*false*
  in this case)

- *code*: the HTTP status code

- *result*: the return value of the transaction

If the transaction specification is either missing or malformed, the server
will respond with *HTTP 400*.

The body of the response will then contain a JSON object with additional error
details. The object has the following attributes:

- *error*: boolean flag to indicate that an error occurred (*true* in this case)

- *code*: the HTTP status code

- *errorNum*: the server error number

- *errorMessage*: a descriptive error message

If a transaction fails to commit, either by an exception thrown in the
*action* code, or by an internal error, the server will respond with
an error.
Any other errors will be returned with any of the return codes
*HTTP 400*, *HTTP 409*, or *HTTP 500*.

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the transaction is fully executed and committed on the server,
*HTTP 200* will be returned.

@RESTRETURNCODE{400}
If the transaction specification is either missing or malformed, the server
will respond with *HTTP 400*.

@RESTRETURNCODE{404}
If the transaction specification contains an unknown collection, the server
will respond with *HTTP 404*.

@RESTRETURNCODE{500}
Exceptions thrown by users will make the server respond with a return code of
*HTTP 500*

@EXAMPLES

Executing a transaction on a single collection

@EXAMPLE_ARANGOSH_RUN{RestTransactionSingle}
    var cn = "products";
    db._drop(cn);
    var products = db._create(cn);
    var url = "/_api/transaction";
    var body = {
      collections: {
        write : "products"
      },
      action: "function () { var db = require('@arangodb').db; db.products.save({});  return db.products.count(); }"
    };

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Executing a transaction using multiple collections

@EXAMPLE_ARANGOSH_RUN{RestTransactionMulti}
    var cn1 = "materials";
    db._drop(cn1);
    var materials = db._create(cn1);
    var cn2 = "products";
    db._drop(cn2);
    var products = db._create(cn2);
    products.save({ "a": 1});
    materials.save({ "b": 1});
    var url = "/_api/transaction";
    var body = {
      collections: {
        write : [ "products", "materials" ]
      },
      action: (
        "function () {" +
        "var db = require('@arangodb').db;" +
        "db.products.save({});" +
        "db.materials.save({});" +
        "return 'worked!';" +
        "}"
      )
    };

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn1);
    db._drop(cn2);
@END_EXAMPLE_ARANGOSH_RUN

Aborting a transaction due to an internal error

@EXAMPLE_ARANGOSH_RUN{RestTransactionAbortInternal}
    var cn = "products";
    db._drop(cn);
    var products = db._create(cn);
    var url = "/_api/transaction";
    var body = {
      collections: {
        write : "products"
      },
      action : (
        "function () {" +
        "var db = require('@arangodb').db;" +
        "db.products.save({ _key: 'abc'});" +
        "db.products.save({ _key: 'abc'});" +
        "}"
      )
    };

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 409);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Aborting a transaction by explicitly throwing an exception

@EXAMPLE_ARANGOSH_RUN{RestTransactionAbort}
    var cn = "products";
    db._drop(cn);
    var products = db._create(cn, { waitForSync: true });
    products.save({ "a": 1 });
    var url = "/_api/transaction";
    var body = {
      collections: {
        read : "products"
      },
      action : "function () { throw 'doh!'; }"
    };

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 500);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Referring to a non-existing collection

@EXAMPLE_ARANGOSH_RUN{RestTransactionNonExisting}
    var cn = "products";
    db._drop(cn);
    var url = "/_api/transaction";
    var body = {
      collections: {
        read : "products"
      },
      action : "function () { return true; }"
    };

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 404);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
