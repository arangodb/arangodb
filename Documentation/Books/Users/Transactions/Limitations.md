!CHAPTER Limitations

Transactions in ArangoDB have been designed with particular use cases 
in mind. They will be mainly useful for short and small data retrieval 
and/or modification operations.

The implementation is not optimized for very long-running or very voluminous
operations, and may not be usable for these cases. 

A major limitation is that a transaction must entirely fit into main
memory. This includes all data that is created, updated, or deleted during a
transaction, plus management overhead.

Transactions should thus be kept as small as possible, and big operations
should be split into multiple smaller transactions if they are too big to fit
into one transaction.

Additionally, transactions in ArangoDB cannot be nested, i.e. a transaction 
must not call any other transaction. If an attempt is made to call a transaction 
from inside a running transaction, the server will throw error `1651 (nested 
transactions detected)`.

It is also disallowed to execute user transaction on some of ArangoDB's own system
collections. This shouldn't be a problem for regular usage as system collections will
not contain user data and there is no need to access them from within a user
transaction.

Finally, all collections that may be modified during a transaction must be 
declared beforehand, i.e. using the `collections` attribute of the object passed
to the `_executeTransaction` function. If any attempt is made to carry out a data
modification operation on a collection that was not declared in the `collections`
attribute, the transaction will be aborted and ArangoDB will throw error `1652
unregistered collection used in transaction`. 
It is legal to not declare read-only collections, but this should be avoided if
possible to reduce the probability of deadlocks and non-repeatable reads.

Please refer to [Locking and Isolation](../Transactions/LockingAndIsolation.md) for more details.
