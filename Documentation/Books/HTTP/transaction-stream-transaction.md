---
layout: default
description: Stream Transactions allow you to perform a multi-document transaction with individual begin and commit / abort commands
---
HTTP Interface for Stream Transactions
======================================

_Introduced in: v3.5.0_

*Stream Transactions* allow you to perform a multi-document transaction 
with individual begin and commit / abort commands. This is similar to
the way traditional RDBMS do it with *BEGIN*, *COMMIT* and *ROLLBACK* operations.

To use a stream transaction a client first sends the [configuration](#begin-a-transaction)
of the transaction to the ArangoDB server.

{% hint 'info' %}
Contrary to the [**JS-Transaction**](transaction-js-transaction.html) the definition of this 
transaction must only contain the collections which are going to be used
and (optionally) the various transaction options supported by ArangoDB.
No *action* attribute is supported.
{% endhint %}

The stream transaction API works in *conjunction* with other APIs in ArangoDB.
To use the transaction for a supported operation a client needs to specify
the transaction identifier in the *x-arango-trx-id* header on each request.
This will automatically cause these operations to use the specified transaction.

Supported transactional API operations include:

1. All operations in the [Document API](document-working-with-documents.html)
2. Number of documents via the [Collection API](collection-getting.html#return-number-of-documents-in-a-collection)
3. Truncate a collection via the [Collection API](collection-creating.html#truncate-collection)
4. Create an AQL cursor via the [Cursor API](aql-query-cursor-accessing-cursors.html)

Note that a client *always needs to start the transaction first* and it is required to
explicitly specify the collections used for write accesses. The client is responsible
for making sure that the transaction is committed or aborted when it is no longer needed.
This avoids taking up resources on the ArangoDB server.

{% hint 'warning' %}
Transactions will acquire collection locks for read and write operations
in the MMFiles storage engine, and for write operations in RocksDB.
It is therefore advisable to keep the transactions as short as possible.
{% endhint %}

For a more detailed description of how transactions work in ArangoDB please
refer to [Transactions](../transactions.html).

Also see:
- [Limitations](#limitations)
- [Known Issues](../release-notes-known-issues35.html#stream-transactions)

Begin a Transaction
-------------------

<!-- RestTransactionHandler.cpp -->
{% docublock post_api_transaction_begin %}

Check Status of a Transaction
-----------------------------
{% docublock get_api_transaction %}

Commit or Abort a Transaction
-----------------------------

Committing or aborting a running transaction must be done by the client.
It is *bad practice* to not commit or abort a transaction once you are done
using it. It will force the server to keep resources and collection locks 
until the entire transaction times out.

<!-- RestTransactionHandler.cpp -->
{% docublock put_api_transaction %}

<!-- RestTransactionHandler.cpp -->
{% docublock delete_api_transaction %}

List currently ongoing Transactions
-----------------------------------
{% docublock get_api_transactions %}

Limitations
-----------

A maximum lifetime and transaction size for stream transactions is enforced
on the coordinator to ensure that transactions cannot block the cluster from
operating properly:

- Maximum idle timeout of **10 seconds** between operations
- Maximum transaction size of **128 MB** per DBServer

These limits are also enforced for stream transactions on single servers.

Enforcing the limits is useful to free up resources used by abandoned 
transactions, for example from transactions that are abandoned by client 
applications due to programming errors or that were left over because client 
connections were interrupted.
