
@startDocuBlock get_api_transactions
@brief Return the currently running server-side transactions

@RESTHEADER{GET /_api/transaction, Get currently running transactions, executeGetState:transactions}

@RESTDESCRIPTION
The result is an object with the attribute *transactions*, which contains
an array of transactions.
In a cluster the array will contain the transactions from all Coordinators.

Each array entry contains an object with the following attributes:

- *id*: the transaction's id
- *state*: the transaction's status

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
