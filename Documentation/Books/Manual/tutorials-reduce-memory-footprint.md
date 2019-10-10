---
layout: default
description: Reducing the Memory Footprint of ArangoDB servers
---
Reducing the Memory Footprint of ArangoDB servers
=================================================

ArangoDB's memory usage can be restricted and the CPU utilization be reduced
by different configuration options:

- storage engine (this tutorial focuses on the RocksDB engine)
- edge cache
- server statistics
- background threads
- V8 (JavaScript features)
- operating system / memory allocator (Linux)

There are settings to make it run on systems with very limited resources, but
they may also be interesting for your development machine if you want to make it
less taxing for the hardware and do not work with much data. For production
environments, we recommend to use less restrictive settings, to
[benchmark](https://www.arangodb.com/performance/) your setup and fine-tune
the settings for maximal performance.

Let us assume our test system is a big server with many cores and a lot of
memory. However, we intend to run other services on this machine as well.
Therefore we want to restrict the memory usage of ArangoDB. By default, ArangoDB
in version 3.4 tries to use as much memory as possible. Using memory accesses
instead of disk accesses is faster and in the database business performance
rules. ArangoDB comes with a default configuration with that in mind. But
sometimes being a little less grabby on system resources may still be fast
enough, for example if your working data set is not huge. The goal is to reduce
the overall memory footprint.

There are the following big areas, which might eat up memory:

- RocksDB
- WAL (Write Ahead Log) 
- Write Buffers

WAL & Write Buffers
-------------------

RocksDB writes into
[memory buffers mapped to on-disk blocks](https://github.com/facebook/rocksdb/wiki/Write-Buffer-Manager)
first. At some point, the memory buffers will be full and have to be written
to disk. In order to support high write loads, RocksDB might open a lot of these
memory buffers.

Under normal write load, the write buffers will use less than 1 GByte of memory.
If you are tight on memory, or your usage pattern does not require this, you can
reduce these [RocksDB settings](programs-arangod-rocksdb.html):

``` 
--rocksdb.max-total-wal-size 1024000
--rocksdb.write-buffer-size 2048000
--rocksdb.max-write-buffer-number 2
--rocksdb.total-write-buffer-size 81920000
--rocksdb.dynamic-level-bytes false
```

Above settings will

- restrict the number of outstanding memory buffers
- limit the memory usage to around 100 MByte

During import or updates, the memory consumption may still grow bigger. On the
other hand, these restrictions will have an impact on the maximal write
performance. You should not go below above numbers.

Read-Buffers
------------

```
--rocksdb.block-cache-size 2560000
--rocksdb.enforce-block-cache-size-limit true
```

These settings are the counterpart of the settings from the previous section.
As soon as the memory buffers have been persisted to disk, answering read
queries implies to read them back into memory. The above option will limit the
number of cached buffers to a few megabytes. If possible, this setting should be
configured as large as the hot-set size of your dataset.

These restrictions may have an impact on query performance.

Edge-Cache
----------

```
--cache.size 10485760
```

This option limits the ArangoDB edge [cache](programs-arangod-cache.html) to 10
MB. If you do not have a graph use-case and do not use edge collections, it is
possible to use the minimum without a performance impact. In general, this
should correspond to the size of the hot-set.

In addition to all buffers, a query will use additional memory during its
execution, to process your data and build up your result set. This memory is
used during the query execution only and will be released afterwards, in
contrast to the held memory for buffers.

Query Memory Usage
------------------

By default, queries will build up their full results in memory. While you can
fetch the results batch by batch by using a cursor, every query needs to compute
the entire result first before you can retrieve the first batch. The server
also needs to hold the results in memory until the corresponding cursor is fully
consumed or times out. Building up the full results reduces the time the server
has to work with collections at the cost of main memory.

In ArangoDB version 3.4 we introduced
[streaming cursors](release-notes-new-features34.html#streaming-aql-cursors) with
somewhat inverted properties: less peak memory usage, longer access to the
collections. Streaming is possible on document level, which means that it can not
be applied to all query parts. For example, a *MERGE()* of all results of a
subquery can not be streamed (the result of the operation has to be built up fully).
Nonetheless, the surrounding query may be eligible for streaming.

Aside from streaming cursors, ArangoDB offers the possibility to specify a
memory limit which a query should not exceed. If it does, the query will be
aborted. Memory statistics are checked between execution blocks, which
correspond to lines in the *explain* output. That means queries which require
functions may require more memory for intermediate processing, but this will not
kill the query because the memory.

You can use *LIMIT* operations in AQL queries to reduce the number of documents
that need to be inspected and processed. This is not always what happens under
the hood however. Other operations may lead to an intermediate result being
computed before any limit is applied. Recently, we added a new ability to the
optimizer: the
[Sort-Limit Optimization in AQL](https://www.arangodb.com/2019/03/sort-limit-optimization-aql/).
In short, a *SORT* combined with a *LIMIT* operation only keeps as many documents
in memory during sorting as the subsequent *LIMIT* requires. This optimization
is applied automatically beginning with ArangoDB v3.5.0.

Statistics
----------

The server collects
[statistics](programs-arangod-server.html#toggling-server-statistics) regularly,
which it shows you in the web interface. You will have a light query load even
if your application is idle because of the statistics. You can disable them if
desired:

```
--server.statistics false
```

JavaScript & Foxx
-----------------

[JavaScript](programs-arangod-javascript.html) is executed in the ArangoDB
process using the embedded V8 engine:

- Backend parts of the web interface
- Foxx Apps
- Foxx Queues
- GraphQL
- JavaScript-based transactions
- User-defined AQL functions

There are several *V8 contexts* for parallel execution. You can think of them as
a thread pool. They are also called *isolates*. Each isolate has a heap of a few
gigabytes by default. You can restrict V8 if you use no or very little
JavaScript:

``` 
--javascript.v8-contexts 2
--javascript.v8-max-heap 512
```

This will limit the number of V8 isolates to two. All JavaScript related
requests will be queued up until one of the isolates becomes available for the
new task. It also restricts the heap size to 512 MByte, so that both V8 contexts
combined can not use more than 1 GByte of memory in the worst case.

### V8 for the Desperate

You should not use the following settings unless there are very good reasons,
like a local development system on which performance is not critical or an
embedded system with very limited hardware resources!

``` 
--javascript.v8-contexts 1
--javascript.v8-max-heap 256
```

You can reduce the memory usage of V8 to 256 MB and just one thread. There is a
chance that some operations will be aborted because they run out of memory, in
the web interface for instance. Also, JavaScript requests will be executed one
by one.

If you are very tight on memory, and you are sure that you do not need V8, you
can disable it completely:

``` 
--javascript.enabled false
--foxx.queues false
```

In consequence, the following features will not be available:

- Backend parts of the web interface
- Foxx Apps
- Foxx Queues
- GraphQL
- JavaScript-based transactions
- User-defined AQL functions

Note that JavaScript / V8 can be disabled for DB-Server and Agency nodes in a
cluster without these limitations. They apply to single server instances. They
also apply to Coordinator nodes, but you should not disable V8 on Coordinators
because certain cluster operations depend on it.

CPU usage
---------

We can not really reduce CPU usage, but the number of threads running in parallel.
Again, you should not do this unless there are very good reasons, like an
embedded system. Note that this will limit the performance for concurrent
requests, which may be okay for a local development system with you as only 
user.

The number of background threads can be limited in the following way:

``` 
--arangosearch.threads-limit 1
--rocksdb.max-background-jobs 4
--server.maintenance-threads 2 
--server.maximal-threads 4
--server.minimal-threads 1
```

In general, the number of threads is selected to fit the machine. However, each
thread requires at least 8 MB of stack memory. By sacrificing some performance
for parallel execution it is possible to reduce this.

This option will make logging synchronous:

```
--log.force-direct true
```

This is not recommended unless you only log errors and warnings.

Examples
--------

In general, you should adjust the read buffers and edge cache on a standard
server. If you have a graph use-case, you should go for a larger edge cache. For
example, split the memory 50:50 between read buffers and edge cache. If you
have no edges then go for a minimal edge cache and use most of the memory for
the read buffers.

For example, if you have a machine with 40 GByte of memory and you want to
restrict ArangoDB to 20 GB of that, use 10 GB for the edge cache and 10 GB for
the read buffers if you use graph features.

Please keep in mind that during query execution additional memory will be used
for query results temporarily. If you are tight on memory, you may want to go
for 7 GB each instead.

If you have an embedded system or your development laptop, you can use all of
the above settings to lower the memory footprint further. For normal operation,
especially production, these settings are not recommended.

Linux System Configuration
--------------------------

The main deployment target for ArangoDB is Linux. As you have learned above
ArangoDB and its innards work a lot with memory. Thus its vital to know how
ArangoDB and the Linux kernel interact on that matter. The linux kernel offers
several modes of how it will manage memory. You can influence this via the proc
filesystem, the file `/etc/sysctl.conf` or a file in `/etc/sysctl.conf.d/` which
your system will apply to the kernel settings at boot time. The settings as
named below are intended for the sysctl infrastructure, meaning that they map
to the `proc` filesystem as `/proc/sys/vm/overcommit_memory`.

A `vm.overcommit_memory` setting of **2** can cause issues in some environments
in combination with the bundled memory allocator ArangoDB ships with (jemalloc).

The allocator demands consecutive blocks of memory from the kernel, which are
also mapped to on-disk blocks. This is done on behalf of the server process
(*arangod*). The process may use some chunks of a block for a long time span, but
others only for a short while and therefore release the memory. It is then up to
the allocator to return the freed parts back to the kernel. Because it can only
give back consecutive blocks of memory, it has to split the large block into
multiple small blocks and can then return the unused ones.

With an `vm.overcommit_memory` kernel settings value of **2**, the allocator may
have trouble with splitting existing memory mappings, which makes the *number*
of memory mappings of an arangod server process grow over time. This can lead to
the kernel refusing to hand out more memory to the arangod process, even if more
physical memory is available. The kernel will only grant up to `vm.max_map_count`
memory mappings to each process, which defaults to 65530 on many Linux
environments.

Another issue when running jemalloc with `vm.overcommit_memory` set to **2** is
that for some workloads the amount of memory that the Linux kernel tracks as
*committed memory* also grows over time and never decreases. Eventually,
*arangod* may not get any more memory simply because it reaches the configured
overcommit limit (physical RAM * `overcommit_ratio` + swap space).

The solution is to
[modify the value of `vm.overcommit_memory`](installation-linux-osconfiguration.html#over-commit-memory)
from **2** to either **0** or **1**. This will fix both of these problems.
We still observe ever-increasing *virtual memory* consumption when using
jemalloc regardless of the overcommit setting, but in practice this should not
cause any issues. **0** is the Linux kernel default and also the setting we recommend.

For the sake of completeness, let us also mention another way to address the
problem: use a different memory allocator. This requires to compile ArangoDB
from the source code without jemalloc (`-DUSE_JEMALLOC=Off` in the call to cmake).
With the system's libc allocator you should see quite stable memory usage. We
also tried another allocator, precisely the one from `libmusl`, and this also
shows quite stable memory usage over time. What holds us back to change the
bundled allocator are that it is a non-trivial change and because jemalloc has
very nice performance characteristics for massively multi-threaded processes
otherwise.

Testing the Effects of Reduced I/O Buffers
------------------------------------------

![Performance Graph](images/performance_graph.png)

- 15:50 – Start bigger import
- 16:00 – Start writing documents of ~60 KB size one at a time
- 16:45 – Add similar second writer
- 16:55 – Restart ArangoDB with the RocksDB write buffer configuration suggested above
- 17:20 – Buffers are full, write performance drops
- 17:38 – WAL rotation

What you see in above performance graph are the consequences of restricting the
write buffers. Until we reach a 90% fill rate of the write buffers the server
can almost follow the load pattern for a while at the cost of constantly
increasing buffers. Once RocksDB reaches 90% buffer fill rate, it will
significantly throttle the load to ~50%. This is expected according to the
[upstream documentation](https://github.com/facebook/rocksdb/wiki/Write-Buffer-Manager):

> […] a flush will be triggered […] if total mutable memtable size exceeds 90%
> of the limit. If the actual memory is over the limit, more aggressive flush
> may also be triggered even if total mutable memtable size is below 90%.

Since we only measured the disk I/O bytes, we do not see that the document save
operations also doubled in request time.
