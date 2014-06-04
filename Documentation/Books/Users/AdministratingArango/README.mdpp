!CHAPTER Administrating ArangoDB 

!SUBSECTION Mostly Memory/Durability

Database documents are stored in memory-mapped files. Per default, these
memory-mapped files are synced regularly but not instantly. This is often a good
tradeoff between storage performance and durability. If this level of durability
is too low for an application, the server can also sync all modifications to
disk instantly. This will give full durability but will come with a performance
penalty as each data modification will trigger a sync I/O operation.

!SECTION AppendOnly/MVCC 

Instead of overwriting existing documents, a completely new version of the
document is generated. The two benefits are:

- Objects can be stored coherently and compactly in the main memory.
- Objects are preserved-isolated writing and reading transactions allow
  accessing these objects for parallel operations.

The system collects obsolete versions as garbage, recognizing them as
forsaken. Garbage collection is asynchronous and runs parallel to other
processes.

!SECTION Configuration

!SUBSECTION Global Configuration 

There are certain default values, which you can store in the configuration file
or supply on the command line.

`--database.maximal-journal-size size`

Maximal size of journal in bytes. Can be overwritten when creating a new collection. Note that this also limits the maximal size of a single document.

The default is 32MB.
<!-- @copydetails triagens::arango::ArangoServer::_defaultMaximalSize -->

!SUBSECTION Per Collection Configuration

You can configure the durability behavior on a per collection basis.
Use the ArangoDB shell to change these properties.

`collection.properties()`

Returns an object containing all collection properties.

* waitForSync: If true creating a document will only return after the data was synced to disk.
* journalSize : The size of the journal in bytes.
* isVolatile: If true then the collection data will be kept in memory only and ArangoDB will not write or sync the data to disk.
* keyOptions (optional) additional options for key generation. This is a JSON array containing the following attributes (note: some of the attributes are optional):
* type: the type of the key generator used for the collection.
* allowUserKeys: if set to true, then it is allowed to supply own key values in the _key attribute of a document. If set to false, then the key generator will solely be responsible for generating keys and supplying own key values in the _key attribute of documents is considered an error.
* increment: increment value for autoincrement key generator. Not used for other key generator types.
* offset: initial offset value for autoincrement key generator. Not used for other key generator types.

In a cluster setup, the result will also contain the following attributes:

* numberOfShards: the number of shards of the collection.
* shardKeys: contains the names of document attributes that are used to determine the target shard for documents.
* collection.properties( properties)

Changes the collection properties. properties must be a object with one or more of the following attribute(s):

* waitForSync: If true creating a document will only return after the data was synced to disk.
* journalSize : The size of the journal in bytes.

Note that it is not possible to change the journal size after the journal or datafile has been created. Changing this parameter will only effect newly created journals. Also note that you cannot lower the journal size to less then size of the largest document already stored in the collection.

Note: some other collection properties, such as type, isVolatile, or keyOptions cannot be changed once the collection is created.

*Examples*

Read all properties

  arango> db.examples.properties()
  { "waitForSync" : false, "journalSize" : 33554432, "isVolatile" : false }

Change a property

  arango> db.examples.properties({ waitForSync : false })
  { "waitForSync" : false, "journalSize" : 33554432, "isVolatile" : false }


<!--@copydetails JS_PropertiesVocbaseCol-->

