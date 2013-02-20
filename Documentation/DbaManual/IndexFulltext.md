Fulltext indexes{#IndexFulltext}
================================

@NAVIGATE_IndexFulltext
@EMBEDTOC{IndexFulltextTOC}

Introduction to Fulltext Indexes{#IndexFulltextIntro}
=====================================================

This is an introduction to ArangoDB's fulltext indexes.

It is possible to define a fulltext index on one textual attribute of a
collection of documents. The fulltext index can then be used to efficiently find
exact words or prefixes of words contained in these documents.

Accessing Fulltext Indexes from the Shell{#IndexFulltextShell}
==============================================================

@anchor IndexFulltextShellEnsureFulltextIndex
@copydetails JS_EnsureFulltextIndexVocbaseCol
