The "db" Object
===============

The `db` object is available in [arangosh](../../Programs/Arangosh/README.md) by
default, and can also be imported and used in Foxx services.

*db.name* returns a [collection object](CollectionObject.md) for the collection *name*.

The following methods exists on the *_db* object:

*Database*

* [db._createDatabase(name, options, users)](../../DataModeling/Databases/WorkingWith.md#create-database)
* [db._databases()](../../DataModeling/Databases/WorkingWith.md#list-databases)
* [db._dropDatabase(name, options, users)](../../DataModeling/Databases/WorkingWith.md#drop-database)
* [db._useDatabase(name)](../../DataModeling/Databases/WorkingWith.md#use-database)

*Indexes*

* [db._index(index)](../../Indexing/WorkingWithIndexes.md#fetching-an-index-by-handle)
* [db._dropIndex(index)](../../Indexing/WorkingWithIndexes.md#dropping-an-index-via-a-database-handle)

*Properties*

* [db._id()](../../DataModeling/Databases/WorkingWith.md#id)
* [db._isSystem()](../../DataModeling/Databases/WorkingWith.md#issystem)
* [db._name()](../../DataModeling/Databases/WorkingWith.md#name)
* [db._path()](../../DataModeling/Databases/WorkingWith.md#path)
* [db._version()](../../DataModeling/Documents/DocumentMethods.md#get-the-version-of-arangodb)

*Collection*

* [db._collection(name)](../../DataModeling/Collections/DatabaseMethods.md#collection)
* [db._collections()](../../DataModeling/Collections/DatabaseMethods.md#all-collections)
* [db._create(name)](../../DataModeling/Collections/DatabaseMethods.md#create)
* [db._drop(name)](../../DataModeling/Collections/DatabaseMethods.md#drop)
* [db._truncate(name)](../../DataModeling/Collections/DatabaseMethods.md#truncate)

*AQL*

* [db._createStatement(query)](../../../AQL/Invocation/WithArangosh.html#with-createstatement-arangostatement)
* [db._query(query)](../../../AQL/Invocation/WithArangosh.html#with-dbquery)
* [db._explain(query)](../../ReleaseNotes/NewFeatures28.md#miscellaneous-improvements)
* [db._parse(query)](../../../AQL/Invocation/WithArangosh.html#query-validation)

*Document*

* [db._document(object)](../../DataModeling/Documents/DatabaseMethods.md#document)
* [db._exists(object)](../../DataModeling/Documents/DatabaseMethods.md#exists)
* [db._remove(selector)](../../DataModeling/Documents/DatabaseMethods.md#remove)
* [db._replace(selector,data)](../../DataModeling/Documents/DatabaseMethods.md#replace)
* [db._update(selector,data)](../../DataModeling/Documents/DatabaseMethods.md#update)

*Views*

* [db._view(name)](../../DataModeling/Views/DatabaseMethods.md#view)
* [db._views()](../../DataModeling/Views/DatabaseMethods.md#all-views)
* [db._createView(name, type, properties)](../../DataModeling/Views/DatabaseMethods.md#create)
* [db._dropView(name)](../../DataModeling/Views/DatabaseMethods.md#drop)

*Global*

* [db._engine()](../../DataModeling/Databases/WorkingWith.md#engine)
* [db._engineStats()](../../DataModeling/Databases/WorkingWith.md#engine-statistics)
* [db._executeTransaction()](../../Transactions/TransactionInvocation.md)
