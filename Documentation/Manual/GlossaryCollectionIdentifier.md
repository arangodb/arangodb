CollectionIdentifier {#GlossaryCollectionIdentifier}
====================================================

@GE{Collection Identifier}: A collection identifier lets you refer to a
collection in a database. It is a string value and is unique within 
the database.
Up to including ArangoDB 1.1, the collection identifier has been a
client's primary means to access collections. Starting with ArangoDB 
1.2, clients should instead use a collection's unique name to access
a collection instead of its identifier.

ArangoDB currently uses 64bit unsigned integer values to maintain
collection ids internally. When returning collection ids to clients,
ArangoDB will put them into a string to ensure the collection id
is not clipped by clients that do not support big integers.
Clients should treat the collection ids returned by ArangoDB as  
opaque strings when they store or use it locally. 

Note: collection ids have been returned as integers up to including 
ArangoDB 1.1
