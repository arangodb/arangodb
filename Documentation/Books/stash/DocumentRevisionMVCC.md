As ArangoDB uses MVCC (Multiple Version Concurrency Control)
internally, documents can exist in more than one revision.
The document revision is the MVCC token used to specify
a particular revision of a document (identified by its `_id`).



It is a string value that contained (up to ArangoDB 3.0)
an integer number and is unique within the list of document
revisions for a single document. 
In ArangoDB >= 3.1 the _rev strings
are in fact time stamps. They use the local clock of the DBserver that
actually writes the document and have millisecond accuracy. 
A [_Hybrid Logical Clock_](http://www.cse.buffalo.edu/tech-reports/2014-04.pdf)
is used.

In a single server setup, `_rev` values are unique across all documents
and all collections. In a cluster setup,
within one shard it is guaranteed that two different document revisions
have a different `_rev` string, even if they are written in the same
millisecond, and that these stamps are ascending.

Note however that different servers in your cluster might have a clock
skew, and therefore between different shards or even between different
collections the time stamps are not guaranteed to be comparable.

The Hybrid Logical Clock feature does one thing to address this
issue: Whenever a message is sent from some server A in your cluster to
another one B, it is ensured that any timestamp taken on B after the
message has arrived is greater than any timestamp taken on A before the
message was sent. This ensures that if there is some "causality" between
events on different servers, time stamps increase from cause to effect.
A direct consequence of this is that sometimes a server has to take
timestamps that seem to come from the future of its own clock. It will
however still produce ever increasing timestamps. If the clock skew is
small, then your timestamps will relatively accurately describe the time
when the document revision was actually written.

ArangoDB uses 64bit unsigned integer values to maintain
document revisions internally. At this stage we intentionally do not
document the exact format of the revision values. When returning 
document revisions to
clients, ArangoDB will put them into a string to ensure the revision
is not clipped by clients that do not support big integers. Clients
should treat the revision returned by ArangoDB as an opaque string
when they store or use it locally. This will allow ArangoDB to change
the format of revisions later if this should be required (as has happened
with 3.1 with the Hybrid Logical Clock). Clients can
use revisions to perform simple equality/non-equality comparisons
(e.g. to check whether a document has changed or not), but they should
not use revision ids to perform greater/less than comparisons with them
to check if a document revision is older than one another, even if this
might work for some cases.

Document revisions can be used to conditionally query, update, replace
or delete documents in the database.

In order to find a particular revision of a document, you need the document
handle or key, and the document revision.
