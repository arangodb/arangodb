Accessing Skip-List Indexes via Http {#IndexSkiplistHttp}
=========================================================

@EMBEDTOC{IndexSkiplistHttpTOC}

If a suitable skip-list index exists, then @ref HttpSimpleRange
"/_api/simple/range" will use this index to execute a range query.

@anchor IndexSkiplistHttpEnsureSkiplist
@copydetails JSF_POST_api_index_skiplist

@CLEARPAGE
@anchor IndexSkiplistHttpRange
@copydetails JSA_PUT_api_simple_range
