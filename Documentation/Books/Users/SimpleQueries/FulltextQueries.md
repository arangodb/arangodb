<a name="fulltext_queries"></a>
# Fulltext queries

ArangoDB allows to run queries on text contained in document attributes.  To use
this, a fulltext index must be defined for the attribute of the collection that
contains the text. Creating the index will parse the text in the specified
attribute for all documents of the collection. Only documents will be indexed
that contain a textual value in the indexed attribute.  For such documents, the
text value will be parsed, and the individual words will be inserted into the
fulltext index.

When a fulltext index exists, it can be queried using a fulltext query.

<!--
@anchor SimpleQueryFulltext
@copydetails JSF_ArangoCollection_prototype_fulltext
-->

`collection.fulltext( attribute, query)`

This will find the documents from the collection's fulltext index that match the search query.

In order to use the fulltext operator, a fulltext index must be defined for the collection, for the specified attribute. If multiple fulltext indexes are defined for the collection and attribute, the most capable one will be selected.

*Examples*

To find all documents which contain the terms foo and bar:

	arango> db.emails.fulltext("text", "word").toArray();
	[ { "_id" : "emails/1721603", "_key" : "1721603", "_rev" : "1721603", "text" : "this document contains a word" },  
	  { "_id" : "emails/1783231", "_key" : "1783231", "_rev" : "1783231", "text" : "this document also contains a word" } ] 


<a name="fulltext_query_syntax:"></a>
#### Fulltext query syntax:

In the simplest form, a fulltext query contains just the sought word. If
multiple search words are given in a query, they should be separated by commas.
All search words will be combined with a logical AND by default, and only such
documents will be returned that contain all search words. This default behavior
can be changed by providing the extra control characters in the fulltext query,
which are:

- `+`: logical AND (intersection)
- `|`: logical OR (union)
- `-`: negation (exclusion)

*Examples:*

- `"banana"`: searches for documents containing "banana"
- `"banana,apple"`: searches for documents containing both "banana" AND "apple"
- `"banana,|orange"`: searches for documents containing eihter "banana" OR "orange" 
  (or both)
- `"banana,-apple"`: searches for documents that contain "banana" but NOT "apple".

Logical operators are evaluated from left to right.

Each search word can optionally be prefixed with `complete:` or `prefix:`, with
`complete:` being the default. This allows searching for complete words or for
word prefixes. Suffix searches or any other forms are partial-word matching are
currently not supported.

Examples:

- `"complete:banana"`: searches for documents containing the exact word "banana"
- `"prefix:head"`: searches for documents with words that start with prefix "head"
- `"prefix:head,banana"`: searches for documents contain words starting with prefix 
  "head" and that also contain the exact word "banana".

Complete match and prefix search options can be combined with the logical
operators.

Please note that only words with a minimum length will get indexed. This minimum
length can be defined when creating the fulltext index. For words tokenisation,
the libicu text boundary analysis is used, which takes into account the default
as defined at server startup (`--server.default-language` startup
option). Generally, the word boundary analysis will filter out punctuation but
will not do much more.

Especially no Word normalisation, stemming, or similarity analysises will be
performed when indexing or searching. If any of these features is required, it
is suggested that the user does the text normalisation on the client side, and
provides for each document an extra attribute containing just a comma-separated
list of normalised words. This attribute can then be indexed with a fulltext
index, and the user can send fulltext queries for this index, with the fulltext
queries also containing the stemmed or normalised versions of words as required
by the user.
