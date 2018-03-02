Pagination
==========

If, for example, you display the result of a user search, then you are in
general not interested in the completed result set, but only the first 10 or so
documents. Or maybe the next 10 documents for the second page. In this case, you
can the *skip* and *limit* operators. These operators work like LIMIT in
MySQL.

*skip* used together with *limit* can be used to implement pagination.
The *skip* operator skips over the first n documents. So, in order to create
result pages with 10 result documents per page, you can use <i>skip(n *
10).limit(10)</i> to access the 10 documents on the *n*th page. This result
should be sorted, so that the pagination works in a predicable way.

### Limit
<!-- js/common/modules/org/arangodb/simple-query-common.js -->
@startDocuBlock queryLimit

### Skip
<!-- js/common/modules/org/arangodb/simple-query-common.js -->
@startDocuBlock querySkip