HTTP Interface for JavaScript Transactions
==========================================

ArangoDB's JS-transactions are executed on the server. Transactions can be 
initiated by clients by sending the transaction description for execution to
the server.

JS-Transactions in ArangoDB do not offer separate *BEGIN*, *COMMIT* and *ROLLBACK*
operations. Instead, ArangoDB JS-transactions are described by a JavaScript function, 
and the code inside the JavaScript function will then be executed transactionally.

At the end of the function, the transaction is automatically committed, and all
changes done by the transaction will be persisted. If an exception is thrown
during transaction execution, all operations performed in the transaction are
rolled back.

For a more detailed description of how transactions work in ArangoDB please
refer to [Transactions](../../Manual/Transactions/index.html). 

<!-- RestTransactionHandler.cpp -->

@startDocuBlock post_api_transaction


