<a name="hash_indexes"></a>
# Hash Indexes

<a name="introduction_to_hash_indexes"></a>
### Introduction to Hash Indexes

This is an introduction to ArangoDB's hash indexes.

It is possible to define a hash index on one or more attributes (or paths) of a
document. This hash index is then used in queries to locate documents in O(1)
operations. If the hash is unique, then no two documents are allowed to have the
same set of attribute values.

<a name="accessing_hash_indexes_from_the_shell"></a>
## Accessing Hash Indexes from the Shell

@anchor IndexHashShellEnsureUniqueConstraint
@copydetails JSF_ArangoCollection_prototype_ensureUniqueConstraint

@CLEARPAGE
@anchor IndexHashShellEnsureHashIndex
@copydetails JSF_ArangoCollection_prototype_ensureHashIndex

