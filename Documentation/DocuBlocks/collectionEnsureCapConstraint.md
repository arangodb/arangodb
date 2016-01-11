

@brief ensures that a cap constraint exists
`collection.ensureIndex({ type: "cap", size: size, byteSize: byteSize })`

Creates a size restriction aka cap for the collection of `size`
documents and/or `byteSize` data size. If the restriction is in place
and the (`size` plus one) document is added to the collection, or the
total active data size in the collection exceeds `byteSize`, then the
least recently created or updated documents are removed until all
constraints are satisfied.

It is allowed to specify either `size` or `byteSize`, or both at
the same time. If both are specified, then the automatic document removal
will be triggered by the first non-met constraint.

Note that at most one cap constraint is allowed per collection. Trying
to create additional cap constraints will result in an error. Creating
cap constraints is also not supported in sharded collections with more
than one shard.

Note that this does not imply any restriction of the number of revisions
of documents.

@EXAMPLES

Restrict the number of document to at most 10 documents:

@EXAMPLE_ARANGOSH_OUTPUT{collectionEnsureCapConstraint}
~db._create('examples');
 db.examples.ensureIndex({ type: "cap", size: 10 });
 for (var i = 0;  i < 20;  ++i) { var d = db.examples.save( { n : i } ); }
 db.examples.count();
~db._drop('examples');
@END_EXAMPLE_ARANGOSH_OUTPUT


