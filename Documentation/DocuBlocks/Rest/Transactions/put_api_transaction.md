
@startDocuBlock put_api_transaction
@brief commit a server-side transaction

@RESTHEADER{PUT /_api/transaction/{transaction-id}, Commit transaction, executeCommit:Transaction}

@RESTURLPARAMETERS

@RESTURLPARAM{transaction-id,string,required}
The transaction identifier,

@RESTDESCRIPTION
Commit a running server-side transaction. Committing is an idempotent operation.
It is not an error to commit a transaction more than once.

If the transaction can be committed, *HTTP 200* will be returned.
The returned JSON object has the following properties:

- *error*: boolean flag to indicate if an error occurred (*false*
  in this case)

- *code*: the HTTP status code

- *result*: result containing
    - *id*: the identifier of the transaction
    - *status*: containing the string 'committed'

If the transaction cannot be found, committing is not allowed or the
transaction was aborted, the server
will respond with *HTTP 400*, *HTTP 404* or *HTTP 409*.

The body of the response will then contain a JSON object with additional error
details. The object has the following attributes:

- *error*: boolean flag to indicate that an error occurred (*true* in this case)

- *code*: the HTTP status code

- *errorNum*: the server error number

- *errorMessage*: a descriptive error message

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the transaction was committed,
*HTTP 200* will be returned.

@RESTRETURNCODE{400}
If the transaction cannot be committed, the server
will respond with *HTTP 400*.

@RESTRETURNCODE{404}
If the transaction was not found, the server
will respond with *HTTP 404*.

@RESTRETURNCODE{409}
If the transaction was already aborted, the server
will respond with *HTTP 409*.

@EXAMPLES

Committing a transaction:

@EXAMPLE_ARANGOSH_RUN{RestTransactionBeginAbort}
    const cn = "products";
    db._drop(cn);
    db._create(cn);
    let body = {
      collections: {
        read : cn
      }
    };
    let trx = db._createTransaction(body);
    let url = "/_api/transaction/" + trx.id();

    var response = logCurlRequest('PUT', url, "");
    assert(response.code === 200);

    logJsonResponse(response);

  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
