
@startDocuBlock get_api_transactions
@brief Return the currently running server-side transactions

@RESTHEADER{GET /_api/transaction, Get currently running transactions, executeGetState:transactions}

@RESTDESCRIPTION
The result is an object describing with the attribute *transactions*, which contains
the list of transaction ids.
In a cluster the list will contain all transactions from all coordinators.

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the list of transactions can be retrieved successfully, *HTTP 200* will be returned.

@EXAMPLES

Get currently running transactions

@EXAMPLE_ARANGOSH_RUN{RestTransactionsGet}
    db._drop("products");
    db._create("products");
    let body = {
      collections: {
        read : "products"
      }
    };
    let trx = db._createTransaction(body);
    let url = "/_api/transaction";

    let response = logCurlRequest('GET', url);
    assert(response.code === 200);

    logJsonResponse(response);

  ~ trx.abort();
  ~ db._drop("products");
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock

