

@brief number of background threads for parallel index creation
`--database.index-threads`

Specifies the *number* of background threads for index creation. When a
collection contains extra indexes other than the primary index, these
other
indexes can be built by multiple threads in parallel. The index threads
are shared among multiple collections and databases. Specifying a value of
*0* will turn off parallel building, meaning that indexes for each
collection
are built sequentially by the thread that opened the collection.
If the number of index threads is greater than 1, it will also be used to
built the edge index of a collection in parallel (this also requires the
edge index in the collection to be split into multiple buckets).

