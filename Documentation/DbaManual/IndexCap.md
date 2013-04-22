Cap Constraint{#IndexCap}
=========================

@NAVIGATE_IndexCap
@EMBEDTOC{IndexCapTOC}

Introduction to Cap Constraints{#IndexCapIntro}
===============================================

This is an introduction to ArangoDB's size restrictions aka cap constraints for
collections.

It is possible to restrict the size of collections. If you add a document and
the size exceeds the limit, then the least recently created or updated document
will be dropped. The size of a collection is measured in the number of
(non-deleted) documents a collection contains.
Using a cap constraint, a collection can be used as a FIFO container, with just 
the newest documents remaining in the collection. 

For example, this might be to keep a list of just the most recent log entries.

Accessing Cap Constraints from the Shell{#IndexCapShell}
========================================================

@anchor IndexCapShellEnsureCapConstraint
@copydetails JS_EnsureCapConstraintVocbaseCol
