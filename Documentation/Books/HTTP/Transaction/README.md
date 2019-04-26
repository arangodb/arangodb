HTTP Interface for Transactions
===============================

### Transactions

ArangoDB's transactions are executed on the server. Transactions can be 
executed by clients in two different ways:

1. Via the [Stream Transaction](StreamTransaction.md) API
2. Via the [JavaScript Transaction](JsTransaction.md) API

The difference between these two is not difficult to understand, a short primer 
is listed below. 
For a more detailed description of how transactions work in ArangoDB and
what guarantees ArangoDB can deliver please
refer to [Transactions](../../Manual/Transactions/index.html). 


### Stream Transactions

[Stream Transactions](StreamTransaction.md) allow you to perform a multi-document transaction 
with individual begin and commit / abort commands. This is similar to
the way traditional RDBMS do it with *BEGIN*, *COMMIT* and *ROLLBACK* operations.

This the recommended API for larger transactions. However the client is responsible
for making sure that the transaction is committed or aborted when it is no longer needed,
to avoid taking up resources.

###  JavaScript Transactions

[JS-Transactions](JsTransaction.md) allow you to send the server
a dedicated piece of JavaScript code (i.e. a function), which will be executed transactionally.

At the end of the function, the transaction is automatically committed, and all
changes done by the transaction will be persisted. No interaction is required by 
the client beyond the initial start request.
