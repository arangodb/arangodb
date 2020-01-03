
@startDocuBlock delete_api_transaction
@brief abort a server-side transaction

@RESTHEADER{DELETE /_api/transaction/{transaction-id}, Abort transaction, executeAbort:transaction}

@RESTURLPARAMETERS

@RESTURLPARAM{transaction-id,string,required}
The transaction identifier,

@RESTDESCRIPTION
Abort a running server-side transaction. Aborting is an idempotent operation.
It is not an error to abort a transaction more than once.

If the transaction can be aborted, *HTTP 200* will be returned.
The returned JSON object has the following properties:

- *error*: boolean flag to indicate if an error occurred (*false*
  in this case)

- *code*: the HTTP status code

- *result*: result containing
    - *id*: the identifier of the transaction
    - *status*: containing the string 'aborted'

If the transaction cannot be found, aborting is not allowed or the
transaction was already committed, the server
will respond with *HTTP 400*, *HTTP 404* or *HTTP 409*.

The body of the response will then contain a JSON object with additional error
details. The object has the following attributes:

- *error*: boolean flag to indicate that an error occurred (*true* in this case)

- *code*: the HTTP status code

- *errorNum*: the server error number

- *errorMessage*: a descriptive error message

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the transaction was aborted,
*HTTP 200* will be returned.

@RESTRETURNCODE{400}
If the transaction cannot be aborted, the server
will respond with *HTTP 400*.

@RESTRETURNCODE{404}
If the transaction was not found, the server
will respond with *HTTP 404*.

@RESTRETURNCODE{409}
If the transaction was already committed, the server
will respond with *HTTP 409*.

@EXAMPLES

Aborting a transaction:

@EXAMPLE_ARANGOSH_RUN{RestTransactionBeginCommit}
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

    var response = logCurlRequest('DELETE', url);
    assert(response.code === 200);

    logJsonResponse(response);

  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
