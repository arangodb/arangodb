Skip-Lists{#IndexSkiplist}
==========================

@NAVIGATE_IndexSkiplist
@EMBEDTOC{IndexSkiplistTOC}

Introduction to Skiplist Indexes{#IndexSkiplistIntro}
=====================================================

This is an introduction to ArangoDB's skip-lists.

It is possible to define a skip-list index on one or more attributes (or paths)
of a documents. This skip-list is then used in queries to locate documents
within a given range. If the skip-list is unique, then no two documents are
allowed to have the same set of attribute values.

Accessing Skip-List Indexes from the Shell{#IndexSkiplistShell}
===============================================================

@anchor IndexSkiplistShellEnsureUniqueSkiplist
@copydetails JS_EnsureUniqueSkiplistVocbaseCol

@CLEARPAGE
@anchor IndexSkiplistShellEnsureSkiplist
@copydetails JS_EnsureSkiplistVocbaseCol
