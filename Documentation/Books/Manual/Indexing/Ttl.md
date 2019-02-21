TTL Indexes
===========

Introduction to TTL (time-to-live) Indexes
------------------------------------------

The TTL index provided by ArangoDB is used for removing expired documents
from a collection. 

The TTL index is set up by setting an `expireAfter` value and by picking a single 
document attribute which contains the documents' creation date and time. Documents 
are expired after `expireAfter` seconds after their creation time. The creation time
is specified as a numeric timestamp (Unix timestamp) or a date string in format
`YYYY-MM-DDTHH:MM:SS` with optional milliseconds. All date strings will be interpreted
as UTC dates.

For example, if `expireAfter` is set to 600 seconds (10 minutes) and the index
attribute is "creationDate" and there is the following document:

    { "creationDate" : 1550165973 }

This document will be indexed with a creation date time value of `1550165973`,
which translates to the human-readable date `2019-02-14T17:39:33.000Z`. The document
will expire 600 seconds afterwards, which is at timestamp `1550166573` (or
`2019-02-14T17:49:33.000Z` in the human-readable version).

The actual removal of expired documents will not necessarily happen immediately. 
Expired documents will eventually removed by a background thread that is periodically
going through all TTL indexes and removing the expired documents.

There is no guarantee when exactly the removal of expired documents will be carried
out, so queries may still find and return documents that have already expired. These
will eventually be removed when the background thread kicks in and has capacity to
remove the expired documents. It is guaranteed however that only documents which are 
past their expiration time will actually be removed.
  
Please note that the numeric date time values for the index attribute should be 
specified in milliseconds since January 1st 1970 (Unix timestamp). To calculate the current 
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

There can at most be one TTL index per collection. It is not recommended to use
TTL indexes for user-land AQL queries, as TTL indexes may store a transformed,
always numerical version of the index attribute value.

The frequency for invoking the background removal thread can be configured 
using the `--ttl.frequency` startup option. 
In order to avoid "random" load spikes by the background thread suddenly kicking 
in and removing a lot of documents at once, the number of to-be-removed documents
per thread invocation can be capped.
The total maximum number of documents to be removed per thread invocation is
controlled by the startup option `--ttl.max-total-removes`. The maximum number of
documents in a single collection at once can be controlled by the startup option
`--ttl.max-collection-removes`.


Accessing TTL Indexes from the Shell
-------------------------------------

Ensures that a TTL index exists:
`collection.ensureIndex({ type: "ttl", fields: [ "field" ], expireAfter: 600 })`

Creates a TTL index on all documents using *field* as attribute path. Exactly 
one attribute path has to be given. The index will be sparse in all cases.

In case that the index was successfully created, an object with the index
details, including the index-identifier, is returned.

    @startDocuBlockInline ensureTtlIndex
    @EXAMPLE_ARANGOSH_OUTPUT{ensureTtlIndex}
    ~db._create("test");
    db.test.ensureIndex({ type: "ttl", fields: [ "creationDate" ], expireAfter: 600 });
    for (let i = 0; i < 100; ++i) { db.test.insert({ creationDate: Date.now() / 1000 }); }
    ~db._drop("test");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock ensureTtlIndex
