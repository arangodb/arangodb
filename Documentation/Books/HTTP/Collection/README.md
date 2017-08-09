HTTP Interface for Collections
==============================

### Collections

This is an introduction to ArangoDB's HTTP interface for collections.

#### Collection

A collection consists of documents. It is uniquely identified by its 
[collection identifier](../../Manual/Appendix/Glossary.html#collection-identifier).
It also has a unique name that clients should 
use to identify and access it. Collections can be renamed. This will 
change the collection name, but not the collection identifier.
Collections have a type that is specified by the user when the collection 
is created. There are currently two types: document and edge. The default 
type is document.

#### Collection Identifier

A collection identifier lets you refer to a collection in a database. 
It is a string value and is unique within the database. Up to including 
ArangoDB 1.1, the collection identifier has been a client's primary 
means to access collections. Starting with ArangoDB 1.2, clients should 
instead use a collection's unique name to access a collection instead of 
its identifier.
ArangoDB currently uses 64bit unsigned integer values to maintain 
collection ids internally. When returning collection ids to clients, 
ArangoDB will put them into a string to ensure the collection id is not 
clipped by clients that do not support big integers. Clients should treat 
the collection ids returned by ArangoDB as opaque strings when they store 
or use it locally.

Note: collection ids have been returned as integers up to including ArangoDB 1.1

#### Collection Name

A collection name identifies a collection in a database. It is a string 
and is unique within the database. Unlike the collection identifier it is 
supplied by the creator of the collection. The collection name must consist 
of letters, digits, and the _ (underscore) and - (dash) characters only. 
Please refer to Naming Conventions in ArangoDB for more information on valid 
collection names.

#### Key Generator

ArangoDB allows using key generators for each collection. Key generators 
have the purpose of auto-generating values for the _key attribute of a document 
if none was specified by the user. By default, ArangoDB will use the traditional 
key generator. The traditional key generator will auto-generate key values that 
are strings with ever-increasing numbers. The increment values it uses are 
non-deterministic.

Contrary, the auto increment key generator will auto-generate deterministic key 
values. Both the start value and the increment value can be defined when the 
collection is created. The default start value is 0 and the default increment 
is 1, meaning the key values it will create by default are:

1, 2, 3, 4, 5, ...

When creating a collection with the auto increment key generator and an increment of 5, the generated keys would be:

1, 6, 11, 16, 21, ...

The auto-increment values are increased and handed out on each document insert 
attempt. Even if an insert fails, the auto-increment value is never rolled back.
That means there may exist gaps in the sequence of assigned auto-increment values
if inserts fails.

The basic operations (create, read, update, delete) for documents are mapped
to the standard HTTP methods (*POST*, *GET*, *PUT*, *DELETE*). 


Address of a Collection
-----------------------

All collections in ArangoDB have an unique identifier and a unique
name. ArangoDB internally uses the collection's unique identifier to
look up collections. This identifier however is managed by ArangoDB
and the user has no control over it. In order to allow users use their
own names, each collection also has a unique name, which is specified
by the user.  To access a collection from the user perspective, the
collection name should be used, i.e.:

    http://server:port/_api/collection/collection-name

For example: Assume that the collection identifier is *7254820* and
the collection name is *demo*, then the URL of that collection is:

    http://localhost:8529/_api/collection/demo


