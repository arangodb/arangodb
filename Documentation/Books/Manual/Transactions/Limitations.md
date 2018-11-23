Limitations
===========

In General
----------

Transactions in ArangoDB have been designed with particular use cases 
in mind. They will be mainly useful for short and small data retrieval 
and/or modification operations.

The implementation is not optimized for very long-running or very voluminous
operations, and may not be usable for these cases. 

One limitation is that a transaction operation information must fit into main
memory. The transaction information consists of record pointers, revision numbers
and rollback information. The actual data modification operations of a transaction
are written to the write-ahead log and do not need to fit entirely into main
memory.

Ongoing transactions will also prevent the write-ahead logs from being fully
garbage-collected. Information in the write-ahead log files cannot be written
to collection data files or be discarded while transactions are ongoing.

To ensure progress of the write-ahead log garbage collection, transactions should 
be kept as small as possible, and big transactions should be split into multiple
smaller transactions.

Transactions in ArangoDB cannot be nested, i.e. a transaction must not start another 
transaction. If an attempt is made to call a transaction from inside a running 
transaction, the server will throw error *1651 (nested transactions detected)*.

It is also disallowed to execute user transaction on some of ArangoDB's own system
collections. This shouldn't be a problem for regular usage as system collections will
not contain user data and there is no need to access them from within a user
transaction.

Some operations are not allowed inside transactions in general:

- creation and deletion of databases (`db._createDatabase()`, `db._dropDatabase()`)
- creation and deletion of collections (`db._create()`, `db._drop()`, `db.<collection>.rename()`)
- creation and deletion of indexes (`db.<collection>.ensureIndex()`, `db.<collection>.dropIndex()`)

If an attempt is made to carry out any of these operations during a transaction,
ArangoDB will abort the transaction with error code *1653 (disallowed operation inside
transaction)*.

Finally, all collections that may be modified during a transaction must be 
declared beforehand, i.e. using the *collections* attribute of the object passed
to the *_executeTransaction* function. If any attempt is made to carry out a data
modification operation on a collection that was not declared in the *collections*
attribute, the transaction will be aborted and ArangoDB will throw error *1652
unregistered collection used in transaction*. 
It is legal to not declare read-only collections, but this should be avoided if
possible to reduce the probability of deadlocks and non-repeatable reads.

Please refer to [Locking and Isolation](LockingAndIsolation.md) for more details.

In Clusters
-----------

Using a single instance of ArangoDB, multi-document / multi-collection queries
are guaranteed to be fully ACID. This is more than many other NoSQL database
systems support. In cluster mode, single-document operations are also fully ACID.
Multi-document / multi-collection queries in a cluster are not ACID, which is
equally the case with competing database systems. Transactions in a cluster
will be supported in a future version of ArangoDB and make these operations
fully ACID as well.


With RocksDB storage engine
---------------------------

Data of ongoing transactions is stored in RAM. Transactions that get too big 
(in terms of number of operations involved or the total size of data created or
modified by the transaction) will be committed automatically. Effectively this 
means that big user transactions are split into multiple smaller RocksDB 
transactions that are committed individually. The entire user transaction will 
not necessarily have ACID properties in this case.
 
The following global options can be used to control the RAM usage and automatic 
intermediate commits for the RocksDB engine: 

`--rocksdb.max-transaction-size`

Transaction size limit (in bytes). Transactions store all keys and values in
RAM, so large transactions run the risk of causing out-of-memory sitations.
This setting allows you to ensure that does not happen by limiting the size of
any individual transaction. Transactions whose operations would consume more
RAM than this threshold value will abort automatically with error 32 ("resource
limit exceeded").

`--rocksdb.intermediate-commit-size`

If the size of all operations in a transaction reaches this threshold, the transaction 
is committed automatically and a new transaction is started. The value is specified in bytes.
  
`--rocksdb.intermediate-commit-count`

If the number of operations in a transaction reaches this value, the transaction is 
committed automatically and a new transaction is started.

The above values can also be adjusted per query, by setting the following
attributes in the call to *db._query()*:

- *maxTransactionSize*: transaction size limit in bytes
- *intermediateCommitSize*: maximum total size of operations after which an intermediate
  commit is performed automatically
- *intermediateCommitCount*: maximum number of operations after which an intermediate
  commit is performed automatically
