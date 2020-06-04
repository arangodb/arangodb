
@startDocuBlock get_api_transaction
@brief Fetch status of a server-side transaction

@RESTHEADER{GET /_api/transaction/{transaction-id}, Get transaction status, executeGetState:transaction}

@RESTURLPARAMETERS

@RESTURLPARAM{transaction-id,string,required}
The transaction identifier.

@RESTDESCRIPTION
The result is an object describing the status of the transaction.
It has at least the following attributes:

- *id*: the identifier of the transaction

- *status*: the status of the transaction. One of "running", "committed" or "aborted".

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the transaction is fully executed and committed on the server,
*HTTP 200* will be returned.

@RESTRETURNCODE{400}
If the transaction identifier specified is either missing or malformed, the server
will respond with *HTTP 400*.

@RESTRETURNCODE{404}
If the transaction was not found with the specified identifier, the server
will respond with *HTTP 404*.

@EXAMPLES

Get transaction status

@EXAMPLE_ARANGOSH_RUN{RestTransactionGet}
    db._drop("products");
    db._create("products");
    let body = {
      collections: {
        read : "products"
      }
    };
    let trx = db._createTransaction(body);
    let url = "/_api/transaction/" + trx.id();

    let response = logCurlRequest('GET', url);
    assert(response.code === 200);

    logJsonResponse(response);

  ~ trx.abort();
  ~ db._drop("products");
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
