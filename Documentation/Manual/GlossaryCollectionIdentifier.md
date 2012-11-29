CollectionIdentifier {#GlossaryCollectionIdentifier}
====================================================

@GE{Collection Identifier}: A collection identifier identifies a
collection in a database. It is an integer and is unique within the
database.
Up to including ArangoDB 1.1, the collection identifier has been a
client's primay means to access collections. Starting with ArangoDB 
1.2, clients should instead use a collection's unique name instead of 
its identifier.
