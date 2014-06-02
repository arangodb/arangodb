<a name="skip-lists"></a>
# Skip-Lists


<a name="introduction_to_skiplist_indexes"></a>
## Introduction to Skiplist Indexes

This is an introduction to ArangoDB's skip-lists.

It is possible to define a skip-list index on one or more attributes (or paths)
of a documents. This skip-list is then used in queries to locate documents
within a given range. If the skip-list is unique, then no two documents are
allowed to have the same set of attribute values.

<a name="accessing_skip-list_indexes_from_the_shell"></a>
## Accessing Skip-List Indexes from the Shell

@anchor IndexSkiplistShellEnsureUniqueSkiplist
@copydetails JSF_ArangoCollection_prototype_ensureUniqueSkiplist

@CLEARPAGE
@anchor IndexSkiplistShellEnsureSkiplist
@copydetails JSF_ArangoCollection_prototype_ensureSkiplist
