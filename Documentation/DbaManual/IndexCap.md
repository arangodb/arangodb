Cap Constraint{#IndexCap}
=========================

@NAVIGATE_IndexCap
@EMBEDTOC{IndexCapTOC}

Introduction to Cap Constraints{#IndexCapIntro}
===============================================

This is an introduction to ArangoDB's size restrictions aka cap constraints for
collections.

It is possible to restrict the size of collections. If you add a document and
the size exceeds the limit, then the least recently created or updated document(s)
will be dropped. The size of a collection is measured in the number of
active documents a collection contains, and optionally in the total size of
the active documents' data in bytes.

It is possible to only restrict the number of documents in a collection, or to
only restrict the total active data size, or both at the same time. If there are
restrictions on both document count and total size, then the first violated 
constraint will trigger the auto-deletion of "too" old documents until all
constraints are satisfied.

Using a cap constraint, a collection can be used as a FIFO container, with just 
the newest documents remaining in the collection. 

For example, a cap constraint can be used to keep a list of just the most recent 
log entries, and at the same time ensure that the collection does not grow
indefinitely. Cap constraints can be used to automate the process of getting rid
of "old" documents, and so save the user from implementing own jobs to purge
"old" collection data.

Accessing Cap Constraints from the Shell{#IndexCapShell}
========================================================

@anchor IndexCapShellEnsureCapConstraint
@copydetails JS_EnsureCapConstraintVocbaseCol
