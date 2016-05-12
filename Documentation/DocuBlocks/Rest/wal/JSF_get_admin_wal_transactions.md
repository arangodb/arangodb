
@startDocuBlock JSF_get_admin_wal_transactions
@brief returns information about the currently running transactions

@RESTHEADER{GET /_admin/wal/transactions, Returns information about the currently running transactions}

@RESTDESCRIPTION

Returns information about the currently running transactions. The result
is a JSON object with the following attributes:
- *runningTransactions*: number of currently running transactions
- *minLastCollected*: minimum id of the last collected logfile (at the
  start of each running transaction). This is *null* if no transaction is
  running.
- *minLastSealed*: minimum id of the last sealed logfile (at the
  start of each running transaction). This is *null* if no transaction is
  running.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the operation succeeds.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.
@endDocuBlock

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestWalTransactionsGet}
    var url = "/_admin/wal/transactions";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

