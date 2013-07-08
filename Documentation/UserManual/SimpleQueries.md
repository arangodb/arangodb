Simple Queries {#SimpleQueries}
===============================

@NAVIGATE_SimpleQueries
@EMBEDTOC{SimpleQueriesTOC}

Queries {#SimpleQueriesQueries}
===============================

Simple queries can be used if the query condition is straight forward, i.e., a
document reference, all documents, a query-by-example, or a simple geo query. In
a simple query you can specify exactly one collection and one query criteria. In
the following sections we describe the JavaScript shell interface for simple
queries, which you can use within the ArangoDB shell and within actions and
transactions. For other languages see the corresponding language API
documentation.

If a query returns a cursor, then you can use @FN{hasNext} and @FN{next} to
iterate over the result set or @FN{toArray} to convert it to an array.

If the number of query results is expected to be big, it is possible to 
limit the amount of documents transferred between the server and the client
to a specific value. This value is called `batchSize`. The `batchSize`
can optionally be set before or when a simple query is executed.
If the server has more documents than should be returned in a single batch,
the server will set the `hasMore` attribute in the result. It will also
return the id of the server-side cursor in the `id` attribute in the result.
This id can be used with the cursor API to fetch any outstanding results from
the server and dispose the server-side cursor afterwards.

The initial `batchSize` value can be set using the @FN{setBatchSize} 
method that is available for each type of simple query, or when the simple
query is executed using its @FN{execute} method. If no `batchSize` value
is specified, the server will pick a reasonable default value.

@anchor SimpleQueryAll
@copydetails JSF_ArangoCollection_prototype_all

@CLEARPAGE
@anchor SimpleQueryByExample
@copydetails JSF_ArangoCollection_prototype_byExample

@CLEARPAGE
@anchor SimpleQueryFirstExample
@copydetails JSF_ArangoCollection_prototype_firstExample

@CLEARPAGE
@anchor SimpleQueryAny
@copydetails JSF_ArangoCollection_prototype_range

@CLEARPAGE
@anchor SimpleQueryRange
@copydetails JS_AnyQuery

@CLEARPAGE
@anchor SimpleQueryCollectionCount
@copydetails JS_CountVocbaseCol

@CLEARPAGE
@anchor SimpleQueryToArray
@copydetails JSF_ArangoCollection_prototype_toArray

@CLEARPAGE
Geo Queries {#SimpleQueriesGeoQueries}
======================================

The ArangoDB allows to select documents based on geographic coordinates. In
order for this to work, a geo-spatial index must be defined.  This index will
use a very elaborate algorithm to lookup neighbors that is a magnitude faster
than a simple R* index.

In general a geo coordinate is a pair of latitude and longitude.  This can
either be a list with two elements like `[-10, +30]` (latitude first, followed
by longitude) or an object like `{lon: -10, lat: +30`}.  In order to find all
documents within a given radius around a coordinate use the @FN{within}
operator. In order to find all documents near a given document use the @FN{near}
operator.

It is possible to define more than one geo-spatial index per collection.  In
this case you must give a hint using the @FN{geo} operator which of indexes
should be used in a query.

@anchor SimpleQueryNear
@copydetails JSF_ArangoCollection_prototype_near

@CLEARPAGE
@anchor SimpleQueryWithin
@copydetails JSF_ArangoCollection_prototype_within

@CLEARPAGE
@anchor SimpleQueryGeo
@copydetails JSF_ArangoCollection_prototype_geo

@CLEARPAGE
Fulltext queries {#SimpleQueriesFulltext}
=========================================

ArangoDB allows to run queries on text contained in document attributes.  To use
this, a fulltext index must be defined for the attribute of the collection that
contains the text. Creating the index will parse the text in the specified
attribute for all documents of the collection. Only documents will be indexed
that contain a textual value in the indexed attribute.  For such documents, the
text value will be parsed, and the individual words will be inserted into the
fulltext index.

When a fulltext index exists, it can be queried using a fulltext query.

@anchor SimpleQueryFulltext
@copydetails JSF_ArangoCollection_prototype_fulltext

@CLEARPAGE
Fulltext query syntax:

In the simplest form, a fulltext query contains just the sought word. If
multiple search words are given in a query, they should be separated by commas.
All search words will be combined with a logical AND by default, and only such
documents will be returned that contain all search words. This default behavior
can be changed by providing the extra control characters in the fulltext query,
which are:

- `+`: logical AND (intersection)
- `|`: logical OR (union)
- `-`: negation (exclusion)

Examples:

- `"banana"`: searches for documents containing "banana"
- `"banana,apple"`: searches for documents containing both "banana" AND "apple"
- `"banana,|orange"`: searches for documents containing eihter "banana" OR "orange" 
  (or both)
- `"banana,-apple"`: searches for documents that contain "banana" but NOT "apple".

Logical operators are evaluated from left to right.

Each search word can optionally be prefixed with `complete:` or `prefix:`, with
`complete:` being the default. This allows searching for complete words or for
word prefixes. Suffix searches or any other forms are partial-word matching are
currently not supported.

Examples:

- `"complete:banana"`: searches for documents containing the exact word "banana"
- `"prefix:head"`: searches for documents with words that start with prefix "head"
- `"prefix:head,banana"`: searches for documents contain words starting with prefix 
  "head" and that also contain the exact word "banana".

Complete match and prefix search options can be combined with the logical
operators.

Please note that only words with a minimum length will get indexed. This minimum
length can be defined when creating the fulltext index. For words tokenisation,
the libicu text boundary analysis is used, which takes into account the default
as defined at server startup (`--server.default-language` startup
option). Generally, the word boundary analysis will filter out punctuation but
will not do much more.

Especially no Word normalisation, stemming, or similarity analysises will be
performed when indexing or searching. If any of these features is required, it
is suggested that the user does the text normalisation on the client side, and
provides for each document an extra attribute containing just a comma-separated
list of normalised words. This attribute can then be indexed with a fulltext
index, and the user can send fulltext queries for this index, with the fulltext
queries also containing the stemmed or normalised versions of words as required
by the user.

@CLEARPAGE
Pagination {#SimpleQueriesPagination}
=====================================

If, for example, you display the result of a user search, then you are in
general not interested in the completed result set, but only the first 10 or so
documents. Or maybe the next 10 documents for the second page. In this case, you
can the @FN{skip} and @FN{limit} operators. These operators work like LIMIT in
MySQL.

@FN{skip} used together with @FN{limit} can be used to implement pagination.
The @FN{skip} operator skips over the first n documents. So, in order to create
result pages with 10 result documents per page, you can use `skip(n *
10).limit(10)` to access the 10 documents on the n.th page.  This result should
be sorted, so that the pagination works in a predicable way.

@anchor SimpleQueryLimit
@copydetails JSF_SimpleQuery_prototype_limit

@CLEARPAGE
@anchor SimpleQuerySkip
@copydetails JSF_SimpleQuery_prototype_skip

@CLEARPAGE
Sequential Access and Cursors {#SimpleQueriesCursor}
====================================================

@anchor SimpleQueryHasNext
@copydetails JSF_SimpleQuery_prototype_hasNext

@CLEARPAGE
@anchor SimpleQueryNext
@copydetails JSF_SimpleQuery_prototype_next

@CLEARPAGE
@anchor SimpleQuerySetBatchSize
@copydetails JSF_SimpleQuery_prototype_setBatchSize

@CLEARPAGE
@anchor SimpleQueryGetBatchSize
@copydetails JSF_SimpleQuery_prototype_getBatchSize

@CLEARPAGE
@anchor SimpleQueryExecute
@copydetails JSF_SimpleQuery_prototype_execute

@CLEARPAGE
@anchor SimpleQueryDispose
@copydetails JSF_SimpleQuery_prototype_dispose

@CLEARPAGE
@anchor SimpleQueryCount
@copydetails JSF_SimpleQuery_prototype_count

@CLEARPAGE
Modification Queries {#SimpleQueriesModify}
===========================================

ArangoDB also allows removing, replacing, and updating documents based 
on an example document.  Every document in the collection will be 
compared against the specified example document and be deleted/replaced/
updated if all attributes match.

These method should be used with caution as they are intended to remove or
modify lots of documents in a collection.

All methods can optionally be restricted to a specific number of operations.
However, if a limit is specific but is less than the number of matches, it
will be undefined which of the matching documents will get removed/modified.

@anchor SimpleQueryRemoveByExample
@copydetails JSF_ArangoCollection_prototype_removeByExample

@anchor SimpleQueryReplaceByExample
@copydetails JSF_ArangoCollection_prototype_replaceByExample

@anchor SimpleQueryUpdateByExample
@copydetails JSF_ArangoCollection_prototype_updateByExample
