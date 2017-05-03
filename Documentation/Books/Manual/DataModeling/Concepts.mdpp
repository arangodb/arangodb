Concepts
========

Database Interaction
--------------------

ArangoDB is a database that serves documents to clients. These documents are
transported using [JSON](https://en.wikipedia.org/wiki/JSON) via a TCP connection,
using the HTTP protocol. A [REST API](https://en.wikipedia.org/wiki/Representational_state_transfer)
is provided to interact with the database system.

The [web interface](../Administration/WebInterface/README.md) that comes with
ArangoDB, called *Aardvark*, provides graphical user interface that is easy to use.
An [interactive shell](../GettingStarted/Arangosh.md), called *Arangosh*, is also
shipped. In addition, there are so called [drivers](https://arangodb.com/downloads/arangodb-drivers/)
that make it easy to use the database system in various environments and
programming languages. All these tools use the HTTP interface of the server and
remove the necessity to roll own low-level code for basic communication in most
cases.

Data model
----------

The documents you can store in ArangoDB closely follow the JSON format,
although they are stored in a binary format called [VelocyPack](https://github.com/arangodb/velocypack#readme).
A **document** contains zero or more attributes, each of these attributes having
a value. A value can either be an atomic type, i. e. number, string, boolean
or null, or a compound type, i.e. an array or embedded document / object.
Arrays and sub-objects can contain all of these types, which means that
arbitrarily nested data structures can be represented in a single document.

Documents are grouped into **collections**. A collection contains zero or more
documents. If you are familiar with relational database management systems (RDBMS)
then it is safe to compare collections to tables and documents to rows. The
difference is that in a traditional RDBMS, you have to define columns before
you can store records in a table. Such definitions are also known as schemas.
ArangoDB is schema-less, which means that there is no need to define what
attributes a document can have. Every single document can have a completely
different structure and still be stored together with other documents in a
single collection. In practice, there will be common denominators among the
documents in a collection, but the database system itself doesn't force you to
limit yourself to a certain data structure.

There are two types of collections: **document collection** (also refered to as
*vertex collections* in the context of graphs) as well as **edge collections**.
Edge collections store documents as well, but they include two special attributes,
*_from* and *_to*, which are used to create relations between documents.
Usually, two documents (**vertices**) stored in document collections are linked
by a document (**edge**) stored in an edge collection. This is ArangoDB's graph
data model. It follows the mathematical concept of a directed, labeled graph,
except that edges don't just have labels, but are full-blown documents.

Collections exist inside of **databases**. There can be one or many databases.
Different databases are usually used for multi tenant setups, as the data inside
them (collections, documents etc.) is isolated from one another. The default
database *_system* is special, because it cannot be removed. Database users
are managed in this database, and their credentials are valid for all databases
of a server instance.

Data Retrieval
--------------

**Queries** are used to filter documents based on certain criteria, to compute
new data, as well as to manipulate or delete existing documents. Queries can be
as simple as a "query by example" or as complex as ["joins"](../../AQL/Examples/Join.html)
using many collections or traversing graph structures. They are written in
the [ArangoDB Query Language](../../AQL/index.html) (AQL).

**Cursors** are used to iterate over the result of queries, so that you get
easily processable batches instead of one big hunk.

**Indexes** are used to speed up searches. There are various types of indexes,
such as [hash indexes](../Indexing/Hash.md) and [geo indexes](../Indexing/Geo.md).
