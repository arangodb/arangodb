!CHAPTER Architecture

!SECTION AppendOnly/MVCC 

Instead of overwriting existing documents, ArangoDB will create a new version of 
modified documents. This is even the case when a document gets deleted. The
two benefits are:

* Objects can be stored coherently and compactly in the main memory.
* Objects are preserved, isolated writing and reading transactions allow
  accessing these objects for parallel operations.

The system collects obsolete versions as garbage, recognizing them as
forsaken. Garbage collection is asynchronous and runs parallel to other
processes.

!SECTION Mostly Memory/Durability

Database documents are stored in memory-mapped files. Per default, these
memory-mapped files are synced regularly but not instantly. This is often a good
tradeoff between storage performance and durability. If this level of durability
is too low for an application, the server can also sync all modifications to
disk instantly. This will give full durability but will come with a performance
penalty as each data modification will trigger a sync I/O operation.
