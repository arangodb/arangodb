Durability
==========

Transactions are executed until there is either a rollback
or a commit. On rollback the operations from the transaction will be reversed.

On commit, all modifications done in the transaction will be written to the 
collection datafiles. These writes will be synchronized to disk if any of the
modified collections has the *waitForSync* property set to *true*, or if any
individual operation in the transaction was executed with the *waitForSync* 
attribute. 
Additionally, transactions that modify data in more than one collection are
automatically synchronized to disk. This synchronization is done to not only
ensure durability, but to also ensure consistency in case of a server crash.

That means if you only modify data in a single collection, and that collection 
has its *waitForSync* property set to *false*, the whole transaction will not 
be synchronized to disk instantly, but with a small delay.

There is thus the potential risk of losing data between the commit of the 
transaction and the actual (delayed) disk synchronization. This is the same as 
writing into collections that have the *waitForSync* property set to *false*
outside of a transaction.
In case of a crash with *waitForSync* set to false, the operations performed in
the transaction will either be visible completely or not at all, depending on
whether the delayed synchronization had kicked in or not.

To ensure durability of transactions on a collection that have the *waitForSync*
property set to *false*, you can set the *waitForSync* attribute of the object
that is passed to *executeTransaction*. This will force a synchronization of the
transaction to disk even for collections that have *waitForSync* set to *false*:

    db._executeTransaction({
      collections: { 
        write: "users"
      },
      waitForSync: true,
      action: function () { ... }
    });


An alternative is to perform an operation with an explicit *sync* request in
a transaction, e.g.

    db.users.save({ _key: "1234" }, true); 

In this case, the *true* value will make the whole transaction be synchronized
to disk at the commit.

In any case, ArangoDB will give users the choice of whether or not they want 
full durability for single collection transactions. Using the delayed synchronization
(i.e. *waitForSync* with a value of *false*) will potentially increase throughput 
and performance of transactions, but will introduce the risk of losing the last
committed transactions in the case of a crash.

The call to the *_executeTransaction* function 
will only return after the data of all modified collections has been synchronized 
to disk and the transaction has been made fully durable. This not only reduces the
risk of losing data in case of a crash but also ensures consistency after a
restart.

MMFiles Storage Engine
----------------------

The MMFiles storage engine continuously writes the transaction operation into 
a journal file on the disk (Journal is sometimes also referred to as write-ahead-log).

This means that the commit operation can be very fast because the engine only needs
to write the *commit* marker into the journal (and perform a disk-sync if 
*waitForSync* was set to *true*). This also means that failed or aborted
transactions need to be rolled back by reversing every single operation.

In case of a server crash, any multi-collection transactions that were not yet 
committed or in preparation to be committed will be rolled back on server restart.

RocksDB Storage Engine
----------------------

The RocksDB Storage Engine applies operations in a transaction only in main memory
until they are committed. In case of an a rollback the entire transaction is just 
cleared, no extra rollback steps are required.

In the event of a server-crash the storage engine will scan the write-ahead log
to restore certain meta-data like the number of documents in collection 
or the selectivity estimates of secondary indexes.