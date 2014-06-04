<a name="specialized_index_type_methods"></a>
# Specialized Index Type Methods

<a name="working_with_cap_constraints"></a>
### Working with Cap Constraints

@anchor IndexCapHttpEnsureCapConstraint
@copydetails JSF_post_api_index_cap

<a name="working_with_hash_indexes"></a>
### Working with Hash Indexes

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

<a name="working_with_skiplist_indexes"></a>
### Working with Skiplist Indexes

If a suitable skip-list index exists, then @ref HttpSimpleRange
"/_api/simple/range" will use this index to execute a range query.

@anchor IndexSkiplistHttpEnsureSkiplist
@copydetails JSF_post_api_index_skiplist

@CLEARPAGE
@anchor IndexSkiplistHttpRange
@copydetails JSA_put_api_simple_range

<a name="working_with_geo_indexes"></a>
### Working with Geo Indexes

@anchor IndexGeoHttpEnsureGeo
@copydetails JSF_post_api_index_geo

@CLEARPAGE
@anchor IndexGeoHttpNear
@copydetails JSA_put_api_simple_near

@CLEARPAGE
@anchor IndexGeoHttpWithin
@copydetails JSA_put_api_simple_within

<a name="working_with_fulltext_indexes"></a>
### Working with Fulltext Indexes

If a fulltext index exists, then @ref HttpSimpleFulltext
"/_api/simple/fulltext" will use this index to execute the specified
fulltext query.

@anchor IndexFulltextHttpEnsureFulltextIndex
@copydetails JSF_post_api_index_fulltext

@CLEARPAGE
@anchor IndexFulltextHttpFulltext
@copydetails JSA_put_api_simple_fulltext

