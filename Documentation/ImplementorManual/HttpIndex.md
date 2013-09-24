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

Specialised Index Type Methods {#HttpIndexSpecial}
==================================================

Working with Cap Constraints {#HttpIndexCap}
--------------------------------------------

@anchor IndexCapHttpEnsureCapConstraint
@copydetails JSF_post_api_index_cap

Working with Hash Indexes {#HttpIndexHash}
------------------------------------------

If a suitable hash index exists, then @ref HttpSimpleByExample
"/_api/simple/by-example" will use this index to execute a query-by-example.

@anchor IndexHashHttpEnsureHash
@copydetails JSF_post_api_index_hash

@CLEARPAGE
@anchor IndexHashHttpByExample
@copydetails JSA_put_api_simple_by_example

@CLEARPAGE
@anchor IndexHashHttpFirstExample
@copydetails JSA_put_api_simple_first_example

Working with Skiplist Indexes {#HttpIndexSkiplist}
--------------------------------------------------

If a suitable skip-list index exists, then @ref HttpSimpleRange
"/_api/simple/range" will use this index to execute a range query.

@anchor IndexSkiplistHttpEnsureSkiplist
@copydetails JSF_post_api_index_skiplist

@CLEARPAGE
@anchor IndexSkiplistHttpRange
@copydetails JSA_put_api_simple_range

Working with Geo Indexes {#HttpIndexGeo}
----------------------------------------

@anchor IndexGeoHttpEnsureGeo
@copydetails JSF_post_api_index_geo

@CLEARPAGE
@anchor IndexGeoHttpNear
@copydetails JSA_put_api_simple_near

@CLEARPAGE
@anchor IndexGeoHttpWithin
@copydetails JSA_put_api_simple_within

Working with Fulltext Indexes {#HttpIndexFulltext}
--------------------------------------------------

If a fulltext index exists, then @ref HttpSimpleFulltext
"/_api/simple/fulltext" will use this index to execute the specified
fulltext query.

@anchor IndexFulltextHttpEnsureFulltextIndex
@copydetails JSF_post_api_index_fulltext

@CLEARPAGE
@anchor IndexFulltextHttpFulltext
@copydetails JSA_put_api_simple_fulltext

