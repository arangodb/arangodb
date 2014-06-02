<a name="fulltext_indexes"></a>
# Fulltext indexes

<a name="introduction_to_fulltext_indexes"></a>
### Introduction to Fulltext Indexes

This is an introduction to ArangoDB's fulltext indexes.

It is possible to define a fulltext index on one textual attribute of a
collection of documents. The fulltext index can then be used to efficiently find
exact words or prefixes of words contained in these documents.

<a name="accessing_fulltext_indexes_from_the_shell"></a>
## Accessing Fulltext Indexes from the Shell

@anchor IndexFulltextShellEnsureFulltextIndex
@copydetails JSF_ArangoCollection_prototype_ensureFulltextIndex

