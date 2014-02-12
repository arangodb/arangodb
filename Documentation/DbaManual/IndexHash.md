Hash Indexes{#IndexHash}
========================

@NAVIGATE_IndexHash
@EMBEDTOC{IndexHashTOC}

Introduction to Hash Indexes{#IndexHashIntro}
=============================================

This is an introduction to ArangoDB's hash indexes.

It is possible to define a hash index on one or more attributes (or paths) of a
document. This hash index is then used in queries to locate documents in O(1)
operations. If the hash is unique, then no two documents are allowed to have the
same set of attribute values.

Accessing Hash Indexes from the Shell{#IndexHashShell}
======================================================

@anchor IndexHashShellEnsureUniqueConstraint
@copydetails JSF_ArangoCollection_prototype_ensureUniqueConstraint

@CLEARPAGE
@anchor IndexHashShellEnsureHashIndex
@copydetails JSF_ArangoCollection_prototype_ensureHashIndex

@BNAVIGATE_IndexHash
