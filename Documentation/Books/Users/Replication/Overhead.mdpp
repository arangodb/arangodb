!CHAPTER Replication Overhead

Running the replication logger will make all data modification operations more 
expensive, as the ArangoDB server will write the operations into the replication log, too.

Additionally, replication appliers that connect to an ArangoDB master will cause some
extra work for the master as it needs to process incoming HTTP requests, and respond.

Overall, turning on the replication logger may reduce throughput on an ArangoDB master
by some extent. If the replication feature is not required, the replication logger should 
be turned off.

Transactions are logged to the event log on the master as an uninterrupted sequence.
While a transaction is written to the event log, the event log is blocked for other
writes. Transactions should thus be as small as possible.

