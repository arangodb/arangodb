<a name="database_methods"></a>
# Database Methods

`db._collection(collection-name)`

Returns the collection with the given name or null if no such collection exists.

`db._collection(collection-identifier)`
Returns the collection with the given identifier or null if no such collection exists. Accessing collections by identifier is discouraged for end users. End users should access collections using the collection name.

*Examples*

Get a collection by name:

	arango> db._collection("demo");
	[ArangoCollection 145387, "demo" (status loaded)]

Get a collection by id:

	arango> db._collection(145387);
	[ArangoCollection 145387, "demo" (status loaded)]

Unknown collection:

	arango> db._collection("unknown")
	null

`db._create(collection-name)`

Creates a new document collection named collection-name. If the collection name already exists or if the name format is invalid, an error is thrown. For more information on valid collection names please refer to Naming Conventions in ArangoDB.

`db._create(collection-name, properties)`

Properties must be an object with the following attributes:

* waitForSync (optional, default false): If true creating a document will only return after the data was synced to disk.
* journalSize (optional, default is a configuration parameter): The maximal size of a journal or datafile. Note that this also limits the maximal size of a single object. Must be at least 1MB.
* isSystem (optional, default is false): If true, create a system collection. In this case collection-name should start with an underscore. End users should normally create non-system collections only. API implementors may be required to create system collections in very special occasions, but normally a regular collection will do.
* isVolatile (optional, default is false): If true then the collection data is kept in-memory only and not made persistent. Unloading the collection will cause the collection data to be discarded. Stopping or re-starting the server will also cause full loss of data in the collection. Setting this option will make the resulting collection be slightly faster than regular collections because ArangoDB does not enforce any synchronisation to disk and does not calculate any CRC checksums for datafiles (as there are no datafiles).
* keyOptions (optional) additional options for key generation. If specified, then keyOptions should be a JSON array containing the following attributes (note: some of them are optional):
* type: specifies the type of the key generator. The currently available generators are traditional and autoincrement.
* allowUserKeys: if set to true, then it is allowed to supply own key values in the _key attribute of a document. If set to false, then the key generator will solely be responsible for generating keys and supplying own key values in the _key attribute of documents is considered an error.
* increment: increment value for autoincrement key generator. Not used for other key generator types.
* offset: initial offset value for autoincrement key generator. Not used for other key generator types.

*Examples*

With defaults:

	arango> c = db._create("cars");
	[ArangoCollection 111137, "cars" (status loaded)]
	arango> c.properties()
	{ "waitForSync" : false, "journalSize" : 33554432, "isVolatile" : false }

With properties:

	arango> c = db._create("cars", { waitForSync : true, journalSize : 1024 * 1204 });
	[ArangoCollection 96384, "cars" (status loaded)]
	arango> c.properties()
	{ "waitForSync" : true, "journalSize" : 1232896, "isVolatile" : false }

With a key generator:

	arango> db._create("users", { keyOptions: { type: "autoincrement", offset: 10, increment: 5 } });
	[ArangoCollection 1533103587969, "users" (type document, status loaded)]

	arango> db.users.save({ name: "user 1" });
	{ "_id" : "users/10", "_rev" : "1533104964225", "_key" : "10" }
	arango> db.users.save({ name: "user 2" });
	{ "_id" : "users/15", "_rev" : "1533105095297", "_key" : "15" }
	arango> db.users.save({ name: "user 3" });
	{ "_id" : "users/20", "_rev" : "1533105226369", "_key" : "20" }

With a special key option:

	arangod> db._create("users", { keyOptions: { allowUserKeys: false } });
	[ArangoCollection 1533105490832, "users" (type document, status loaded)]
	
	arangod> db.users.save({ name: "user 1" });
	{ "_id" : "users/1533106867088", "_rev" : "1533106867088", "_key" : "1533106867088" }

	arangod> db.users.save({ name: "user 2", _key: "myuser" });
	JavaScript exception in file '(arango)' at 1,10: [ArangoError 1222: cannot save document: unexpected document key]
	!db.users.save({ name: "user 2", _key: "myuser" });
	!         ^
	stacktrace: Error: cannot save document: unexpected document key
    at (arango):1:10

`db._collections()`

Returns all collections of the given database.

*Examples*

	arango> db.examples.load();
	arango> var d = db.demo;
	arango> db._collections();
	[[ArangoCollection 96393, "examples" (status loaded)], [ArangoCollection 1407113, "demo" (status new born)]]

`db.collection-name`

Returns the collection with the given collection-name. If no such collection exists, create a collection named collection-name with the default properties.

*Examples*

	arango> db.examples;
	[ArangoCollection 110371, "examples" (status new born)]

`db._drop(collection)`

Drops a collection and all its indexes.

`db._drop(collection-identifier)`

Drops a collection identified by collection-identifier and all its indexes. No error is thrown if there is no such collection.

`db._drop(collection-name)`

Drops a collection named collection-name and all its indexes. No error is thrown if there is no such collection.

*Examples*

Drops a collection:

	arango> col = db.examples;
	[ArangoCollection 109757, "examples" (status unloaded)]
	arango> db._drop(col)
	arango> col;
	[ArangoCollection 109757, "examples" (status deleted)]

Drops a collection identified by name:

	arango> col = db.examples;
	[ArangoCollection 85198, "examples" (status new born)]
	arango> db._drop("examples");
	arango> col;
	[ArangoCollection 85198, "examples" (status deleted)]

`db._truncate(collection)`

Truncates a collection, removing all documents but keeping all its indexes.

`db._truncate(collection-identifier)`

Truncates a collection identified by collection-identified. No error is thrown if there is no such collection.

`db._truncate(collection-name)`

Truncates a collection named collection-name. No error is thrown if there is no such collection.

*Examples*

Truncates a collection:

	arango> col = db.examples;
	[ArangoCollection 91022, "examples" (status unloaded)]
	arango> col.save({ "Hello" : "World" });
	{ "_id" : "examples/1532814", "_key" : "1532814", "_rev" : "1532814" }
	arango> col.count();
	1
	arango> db._truncate(col);
	arango> col.count();
	0

Truncates a collection identified by name:

	arango> col = db.examples;
	[ArangoCollection 91022, "examples" (status unloaded)]
	arango> col.save({ "Hello" : "World" });
	{ "_id" : "examples/1532814", "_key" : "1532814", "_rev" : "1532814" }
	arango> col.count();
	1
	arango> db._truncate("examples");
	arango> col.count();
	0

<!--
@anchor HandlingCollectionsRead
@copydetails JS_CollectionVocbase

@CLEARPAGE
@anchor HandlingCollectionsCreate
@copydetails JS_CreateVocbase

@CLEARPAGE
@anchor HandlingCollectionsReadAll
@copydetails JS_CollectionsVocbase

@CLEARPAGE
@anchor HandlingCollectionsReadShortCut
@copydetails MapGetVocBase

@CLEARPAGE
@anchor HandlingCollectionsDropDb
@copydetails JSF_ArangoDatabase_prototype__drop

@CLEARPAGE
@anchor HandlingCollectionsTruncateDb
@copydetails JSF_ArangoDatabase_prototype__truncate

-->