!CHAPTER Transactions

Starting with version 1.3, ArangoDB provides support for user-definable 
transactions. 

Transactions in ArangoDB are atomic, consistent, isolated, and durable (*ACID*).

These *ACID* properties provide the following guarantees:

* The *atomicity* principle makes transactions either complete in their
  entirety or have no effect at all.
* The *consistency* principle ensures that no constraints or other invariants
  will be violated during or after any transaction.
* The *isolation* property will hide the modifications of a transaction from
  other transactions until the transaction commits. 
* Finally, the *durability* proposition makes sure that operations from 
  transactions that have committed will be made persistent. The amount of
  transaction durability is configurable in ArangoDB, as is the durability
  on collection level. 

