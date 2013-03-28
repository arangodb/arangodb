HTTP Interface for Collections {#HttpCollection}
================================================

@NAVIGATE_HttpCollection
@EMBEDTOC{HttpCollectionTOC}

Collections {#HttpCollectionIntro}
==================================

This is an introduction to ArangoDB's Http interface for collections.

@copydoc GlossaryCollection

@copydoc GlossaryCollectionIdentifier

@copydoc GlossaryCollectionName

@copydoc GlossaryKeyGenerator

The basic operations (create, read, update, delete) for documents are mapped
to the standard HTTP methods (`POST`, `GET`, `PUT`, `DELETE`). 

Address of a Collection {#HttpCollectionResource}
=================================================

All collections in ArangoDB have an unique identifier and a unique
name. ArangoDB internally uses the collection's unique identifier to
look up collections. This identifier however is managed by ArangoDB
and the user has no control over it. In order to allow users use their
own names, each collection also has a unique name, which is specified
by the user.  To access a collection from the user perspective, the
collection name should be used, i.e.:

    http://server:port/_api/collection/collection-name

For example: Assume that the collection identifier is `7254820` and
the collection name is `demo`, then the URL of that collection is:

    http://localhost:8529/_api/collection/demo

Working with Collections using HTTP {#HttpCollectionHttp}
=========================================================

Creating and Deleting Collections {#HttpCollectionConstructor}
--------------------------------------------------------------

@anchor HttpCollectionCreate
@copydetails JSF_post_api_collection

@CLEARPAGE
@anchor HttpCollectionDelete
@copydetails JSF_delete_api_collection

@CLEARPAGE
@anchor HttpCollectionTruncate
@copydetails JSF_put_api_collection_truncate

@CLEARPAGE
Getting Information about a Collection {#HttpCollectionReading}
---------------------------------------------------------------

@anchor HttpCollectionRead
@copydetails JSF_get_api_collection

@CLEARPAGE
@anchor HttpCollectionReadAll
@copydetails JSF_get_api_collections

@CLEARPAGE
Modifying a Collection {#HttpCollectionChanging}
------------------------------------------------

@anchor HttpCollectionLoad
@copydetails JSF_put_api_collection_load

@CLEARPAGE
@anchor HttpCollectionUnload
@copydetails JSF_put_api_collection_unload

@CLEARPAGE
@anchor HttpCollectionProperties
@copydetails JSF_put_api_collection_properties

@CLEARPAGE
@anchor HttpCollectionRename
@copydetails JSF_put_api_collection_rename
