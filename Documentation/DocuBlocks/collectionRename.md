

@brief renames a collection
`collection.rename(new-name)`

Renames a collection using the *new-name*. The *new-name* must not
already be used for a different collection. *new-name* must also be a
valid collection name. For more information on valid collection names please
refer
to the [naming conventions](../NamingConventions/README.md).

If renaming fails for any reason, an error is thrown.
If renaming the collection succeeds, then the collection is also renamed in
all graph definitions inside the `_graphs` collection in the current
database.

**Note**: this method is not available in a cluster.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{collectionRename}
~ db._create("example");
  c = db.example;
  c.rename("better-example");
  c;
~ db._drop("better-example");
@END_EXAMPLE_ARANGOSH_OUTPUT


