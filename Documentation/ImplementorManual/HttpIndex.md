HTTP Interface for Indexes {#HttpIndex}
=======================================

@NAVIGATE_HttpIndex
@EMBEDTOC{HttpIndexTOC}

Indexes{#HttpIndexIntro}
========================

This is an introduction to ArangoDB's Http interface for indexes in
general. There are special sections for various index types.

@copydoc GlossaryIndex

@copydoc GlossaryIndexHandle

@copydoc GlossaryIndexGeo

@copydoc GlossaryIndexHash

@copydoc GlossaryIndexEdge

@copydoc GlossaryIndexSkiplist

@copydoc GlossaryIndexFulltext

The basic operations (create, read, update, delete) for documents are mapped to
the standard HTTP methods (`POST`, `GET`, `PUT`, `DELETE`).

Address of an Index {#HttpIndexResource}
========================================

All indexes in ArangoDB have an unique handle. This index handle identifies an
index and is managed by ArangoDB. All indexes are found under the URI

    http://server:port/_api/index/index-handle

For example: Assume that the index handle is `demo/63563528` then the URL of
that index is:

    http://localhost:8529/_api/index/demo/63563528

Working with Indexes using HTTP {#HttpIndexHttp}
================================================

@anchor HttpIndexRead
@copydetails JSF_get_api_index

@CLEARPAGE
@anchor HttpIndexCreate
@copydetails JSF_post_api_index

@CLEARPAGE
@anchor HttpIndexDelete
@copydetails JSF_delete_api_index

@CLEARPAGE
@anchor HttpIndexReadAll
@copydetails JSF_get_api_indexes
