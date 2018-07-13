Arangodump Limitations
======================

_Arangodump_ has the following limitations:

- In a Cluster, _arangodump_ does not guarantee to dump a consistent snapshot if write
operations happen while the dump is in progress. It is therefore recommended not to 
perform any data-modification operations on the cluster whilst _arangodump_ 
is running. This is in contract to what happens on a single instance, a master/slave,
or active failover setup, where even if write operations are ongoing, the created dump
is consistent, as a snapshot is taken when the dump starts.
- If the MMFile engine is in use, on a single instance, a master/slave, or active failover
setup, even if the write operations are suspended, it is not guaranteed that the dump includes
all the data that has been previously written as _arangodump_ will only dump the data
included in the _datafiles_ but not the data that has not been transfered from the _WAL_
to the _datafiles_. Flush can be forced as documented [here](../../Appendix/JavaScriptModules/WAL.md#flushing).

