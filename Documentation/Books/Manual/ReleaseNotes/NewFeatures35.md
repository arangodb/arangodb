Features and Improvements in ArangoDB 3.5
=========================================

The following list shows in detail which features have been added or improved in
ArangoDB 3.5. ArangoDB 3.5 also contains several bug fixes that are not listed
here.

AQL
---

### SORT-LIMIT optimization

A new SORT-LIMIT optimization has been added. This optimization will be pulled off
by the query optimizer if there is a SORT statement followed by a LIMIT node, and the
overall number of documents to return is relatively small in relation to the total
number of documents to be sorted. In this case, the optimizer will use a size-constrained
heap for keeping only the required number of results in memory, which can drastically
reduce memory usage and, for some queries, also execution time for the sorting.

If the optimization is applied, it will show as "sort-limit" rule in the query execution
plan.

### Sorted primary index (RocksDB engine)

The query optimizer can now make use of the sortedness of primary indexes if the
RocksDB engine is used. This means the primary index can be utilized for queries
that sort by either the `_key` or `_id` attributes of a collection and also for
range queries on these attributes.

In the list of documents for a collection in the web interface, the documents will 
now always be sorted in lexicographical order of their `_key` values. An exception for 
keys representing quasi-numerical values has been removed when doing the sorting in 
the web interface. Removing this exception can also speed up the display of the list
of documents.

This change potentially affects the order in which documents are displayed in the
list of documents overview in the web interface. A document with a key value "10" will 
now be displayed before a document with a key value of "9". In previous versions of
ArangoDB this was exactly opposite.

### Edge index query optimization (RocksDB engine)

An AQL query that uses the edge index only and returns the opposite side of
the edge can now be executed in a more optimized way, e.g.

    FOR edge IN edgeCollection FILTER edge._from == "v/1" RETURN edge._to

is fully covered by the RocksDB edge index. 

For MMFiles this rule does not apply.

### AQL syntax improvements

AQL now allows the usage of floating point values without leading zeros, e.g.
`.1234`. Previous versions of ArangoDB required a leading zero in front of
the decimal separator, i.e `0.1234`.

TTL (time-to-live) Indexes
--------------------------

The new TTL indexes provided by ArangoDB can be used for removing expired documents
from a collection. 

A TTL index can be set up by setting an `expireAfter` value and by picking a single 
document attribute which contains the documents' creation date and time. Documents 
are expired after `expireAfter` seconds after their creation time. The creation time
is specified as either a numeric timestamp or a UTC datestring.

For example, if `expireAfter` is set to 600 seconds (10 minutes) and the index
attribute is "creationDate" and there is the following document:

    { "creationDate" : 1550165973 }

This document will be indexed with a creation timestamp value of `1550165973`,
which translates to the human-readable date string `2019-02-14T17:39:33.000Z`. The 
document will expire 600 seconds afterwards, which is at timestamp `1550166573000` (or
`2019-02-14T17:49:33.000Z` in the human-readable version).

The actual removal of expired documents will not necessarily happen immediately. 
Expired documents will eventually removed by a background thread that is periodically
going through all TTL indexes and removing the expired documents.

There is no guarantee when exactly the removal of expired documents will be carried
out, so queries may still find and return documents that have already expired. These
will eventually be removed when the background thread kicks in and has capacity to
remove the expired documents. It is guaranteed however that only documents which are 
past their expiration time will actually be removed.

Please note that the numeric timestamp values for the index attribute should be 
specified in seconds since January 1st 1970 (Unix timestamp). To calculate the current 
timestamp from JavaScript in this format, there is `Date.now() / 1000`, to calculate it 
from an arbitrary date string, there is `Date.getTime() / 1000`.

Alternatively, the index attribute values can be specified as a date string in format
`YYYY-MM-DDTHH:MM:SS` with optional milliseconds. All date strings will be interpreted 
as UTC dates.
    
The above example document using a datestring attribute value would be

    { "creationDate" : "2019-02-14T17:39:33.000Z" }

In case the index attribute does not contain a numeric value nor a proper date string, 
the document will not be stored in the TTL index and thus will not become a candidate 
for expiration and removal. Providing either a non-numeric value or even no value for 
the index attribute is a supported way of keeping documents from being expired and removed.

There can at most be one TTL index per collection.

The frequency for invoking the background removal thread can be configured 
using the `--ttl.frequency` startup option. 
In order to avoid "random" load spikes by the background thread suddenly kicking 
in and removing a lot of documents at once, the number of to-be-removed documents
per thread invocation can be capped.
The total maximum number of documents to be removed per thread invocation is
controlled by the startup option `--ttl.max-total-removes`. The maximum number of
documents in a single collection at once can be controlled by the startup option
`--ttl.max-collection-removes`.

Web interface
-------------

For the RocksDB engine, the selection of index types "persistent" and "skiplist" 
has been removed from the web interface when creating new indexes. 

The index types "hash", "skiplist" and "persistent" are just aliases of each other 
when using the RocksDB engine, so there is no need to offer all of them in parallel.


Miscellaneous
-------------

### Improved overview of available program options

The `--help-all` command-line option for all ArangoDB executables will now also 
show all hidden program options.

Previously hidden program options were only returned when invoking arangod or
a client tool with the cryptic `--help-.` option. Now `--help-all` simply retuns 
them as well.

### Fewer system collections

The system collections `_routing` and `_modules` are not created anymore for new
new databases, as both are only needed for legacy functionality.

Existing `_routing` collections will not be touched as they may contain user-defined
entries, and will continue to work.

Existing `_modules` collections will also remain functional.


Internal
--------

We have moved from C++11 to C++14, which allows us to use some of the simplifications,
features and guarantees that this standard has in stock.
To compile ArangoDB from source, a compiler that supports C++14 is now required.

The bundled JEMalloc memory allocator used in ArangoDB release packages has been
upgraded from version 5.0.1 to version 5.1.0.

The bundled version of the RocksDB library has been upgraded from 5.16 to 5.18.

The bundled version of the V8 JavaScript engine has been upgraded from 5.7.492.77 to 
7.1.302.28.
