!CHAPTER Replication Overhead

In ArangoDB before version 2.2, there was a so-called *replication logger* which needed to
be activated on the master in order to allow replicating from it. If the logger was not
activated, slaves could not replicate from the master. Running the replication logger 
caused extra write operations on the master, which made all data modification operations more 
expensive. Each data modification operation was first executed normally, and then was
additionally written to the master's replication log. Turning on the replication logger 
may have reduce throughput on an ArangoDB master by some extent. If the replication feature 
was not required, the replication logger should have been turned off to avoid this reduction.

Since ArangoDB 2.2, a master will automatically write all data modification operations into its 
write-ahead log, which can also be used for replication. No separate replication log is
written on the master anymore. This reduces the overhead of replication on the master when
compared to previous versions of ArangoDB. In fact, a master server that is used in a replication
setup will have the same throughput than a standalone server that is not used in replication.

Slaves that connect to an ArangoDB master will cause some work on the master as the master 
needs to process the incoming HTTP requests, return the requested data from its write-ahead and 
send the response. 

In ArangoDB versions prior to 2.2, transactions were logged on the master as an uninterrupted 
sequence, restricting their maximal size considerably. While a transaction was written to the
master's replication log, any other replication logging activity was blocked.

This is not the case since ArangoDB 2.2. Transactions are now written to the write-ahead log
as the operations of the transactions occur. They may be interleaved with other operations.

