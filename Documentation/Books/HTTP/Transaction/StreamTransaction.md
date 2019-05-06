HTTP Interface for Stream Transactions
======================================

*Stream Transactions* allow you to perform a multi-document transaction 
with individual begin and commit / abort commands. This is similar to
the way traditional RDBMS do it with *BEGIN*, *COMMIT* and *ROLLBACK* operations.

To use a stream transaction a client first sends the (configuration)[#begin-a-transaction]
of the transaction to the ArangoDB server.

{% hint 'info' %}
Contrary to the [JS-Transaction](JsTransaction.md) the definition of this 
transaction must only contain the collections which are going to be used
and (optionally) the various transaction options supported by ArangoDB.
No *action* attribute is supported.
{% endhint %}

The stream transaction API works in *conjunction* with other APIs in ArangoDB.
To use the transaction for a supported operation a client needs to specify
the transaction identifier in the *x-arango-trx-id* header on each request.
This will automatically cause these operations to use the specified transaction.

Supported transactional API operations include:

1. All operations in the [Document API](../Document/WorkingWithDocuments.md)
2. Number of documents via the [Collection API](../Collection/Getting.md#return-number-of-documents-in-a-collection)
3. Truncate a collection via the [Collection API](../Collection/Getting.md#return-number-of-documents-in-a-collection)
4. Create an AQL cursor via the [Cursor API](../AqlQueryCursor/AccessingCursors.md)

Note that a client *always needs to start the transaction first* and it is required to
explicitly specify the collections used for write accesses. The client is responsible
for making sure that the transaction is committed or aborted when it is no longer needed.
This avoids taking up resources on the ArangoDB server.

For a more detailed description of how transactions work in ArangoDB please
refer to [Transactions](../../Manual/Transactions/index.html). 

Begin a Transaction
-------------------

<!-- RestTransactionHandler.cpp -->

@startDocuBlock post_api_transaction_begin

Check Status of a Transaction
-----------------------------

@startDocuBlock get_api_transaction

Commit or Abort a Transaction
-----------------------------

Committing or aborting a running transaction must be done by the client.
It is *bad practice* to not commit or abort a transaction once you are done
using it. It will force the server to keep resources and collection locks 
until the entire transaction times out.

<!-- RestTransactionHandler.cpp -->

@startDocuBlock put_api_transaction

<!-- RestTransactionHandler.cpp -->

@startDocuBlock delete_api_transaction
