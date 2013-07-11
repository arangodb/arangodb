Handling Collections {#HandlingCollections}
===========================================

@NAVIGATE_HandlingCollections
@EMBEDTOC{HandlingCollectionsTOC}

JavaScript Interface to Collections {#HandlingCollectionsIntro}
===============================================================

This is an introduction to ArangoDB's interface for collections and how handle
collections from the JavaScript shell _arangosh_. For other languages see the
corresponding language API.

The most import call is the call to create a new collection, see
@ref HandlingCollectionsCreate "_create".

@copydoc GlossaryCollection

@copydoc GlossaryCollectionIdentifier

@copydoc GlossaryCollectionName

Address of a Collection {#HandlingCollectionsResource}
======================================================

All collections in ArangoDB have an unique identifier and a unique
name. ArangoDB internally uses the collection's unique identifier to look up
collections. This identifier however is managed by ArangoDB and the user has no
control over it. In order to allow users use their own names, each collection
also has a unique name, which is specified by the user.  To access a collection
from the user perspective, the collection name should be used, i.e.:

    db._collection(@FA{collection-name})

A collection is created by a @FN{db._create} call, see @ref
HandlingCollectionsCreate "_create".

For example: Assume that the collection identifier is `7254820` and the name is
`demo`, then the collection can be accessed as:

    db._collection("demo")

If no collection with such a name exists, then @LIT{null} is returned.

There is a short-cut that can be used for non-system collections:

    db.@FA{collection-name}

This call will either return the collection named @FA{collection-name} or create
a new one with that name and a set of default properties.

Note: creating a collection on the fly using @LIT{db.@FA{collection-name}} does
not work in arangosh. To create a new collection from arangosh, please use

    db._create(@FA{collection-name})

@CLEARPAGE
Working with Collections {#HandlingCollectionsShell}
====================================================

Collection Methods {#HandlingCollectionsCollectionMethods}
----------------------------------------------------------

@anchor HandlingCollectionsDrop
@copydetails JS_DropVocbaseCol

@CLEARPAGE
@anchor HandlingCollectionsTruncate
@copydetails JSF_ArangoCollection_prototype_truncate

@CLEARPAGE
@anchor HandlingCollectionsProperties
@copydetails JS_PropertiesVocbaseCol

@CLEARPAGE
@anchor HandlingCollectionsFigures
@copydetails JS_FiguresVocbaseCol

@CLEARPAGE
@anchor HandlingCollectionsLoad
@copydetails JS_LoadVocbaseCol

@CLEARPAGE
@anchor HandlingCollectionsRevision
@copydetails JS_RevisionVocbaseCol

@CLEARPAGE
@anchor HandlingCollectionsChecksum
@copydetails JS_ChecksumCollection

@CLEARPAGE
@anchor HandlingCollectionsUnload
@copydetails JS_UnloadVocbaseCol

@CLEARPAGE
@anchor HandlingCollectionsRename
@copydetails JS_RenameVocbaseCol

@CLEARPAGE
Database Methods {#HandlingCollectionsDatabaseMethods}
------------------------------------------------------

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
