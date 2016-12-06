Fulltext queries
================

ArangoDB allows to run queries on text contained in document attributes.  To use
this, a [fulltext index](../../Glossary.md#fulltext-index) must be defined for the attribute of the collection that
contains the text. Creating the index will parse the text in the specified
attribute for all documents of the collection. Only documents will be indexed
that contain a textual value in the indexed attribute.  For such documents, the
text value will be parsed, and the individual words will be inserted into the
fulltext index.

When a fulltext index exists, it can be queried using a fulltext query.

### Fulltext
<!-- js/common/modules/@arangodb/arango-collection-common.js-->


queries the fulltext index
`collection.fulltext(attribute, query)`

The *fulltext* simple query functions performs a fulltext search on the specified
*attribute* and the specified *query*.

Details about the fulltext query syntax can be found below.

Note: the *fulltext* simple query function is **deprecated** as of ArangoDB 2.6. 
The function may be removed in future versions of ArangoDB. The preferred
way for executing fulltext queries is to use an AQL query using the *FULLTEXT*
[AQL function](../../../../AQL/Functions/Fulltext.html) as follows:

    FOR doc IN FULLTEXT(@@collection, @attributeName, @queryString, @limit) 
      RETURN doc


**Examples**


    @startDocuBlockInline collectionFulltext
    @EXAMPLE_ARANGOSH_OUTPUT{collectionFulltext}
    ~ db._drop("emails");
    ~ db._create("emails");
      db.emails.ensureFulltextIndex("content");
    | db.emails.save({ content:
                       "Hello Alice, how are you doing? Regards, Bob"});
    | db.emails.save({ content:
                       "Hello Charlie, do Alice and Bob know about it?"});
      db.emails.save({ content: "I think they don't know. Regards, Eve" });
      db.emails.fulltext("content", "charlie,|eve").toArray();
    ~ db._drop("emails");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock collectionFulltext


### Fulltext Syntax:

In the simplest form, a fulltext query contains just the sought word. If
multiple search words are given in a query, they should be separated by commas.
All search words will be combined with a logical AND by default, and only such
documents will be returned that contain all search words. This default behavior
can be changed by providing the extra control characters in the fulltext query,
which are:

* *+*: logical AND (intersection)
* *|*: logical OR (union)
* *-*: negation (exclusion)

*Examples:*

* *"banana"*: searches for documents containing "banana"
* *"banana,apple"*: searches for documents containing both "banana" *AND* "apple"
* *"banana,|orange"*: searches for documents containing either "banana" *OR* "orange" *OR* both
* *"banana,-apple"*: searches for documents that contains "banana" but *NOT* "apple".

Logical operators are evaluated from left to right.

Each search word can optionally be prefixed with *complete*: or *prefix*:, with
*complete*: being the default. This allows searching for complete words or for
word prefixes. Suffix searches or any other forms are partial-word matching are
currently not supported.

Examples:

* *"complete:banana"*: searches for documents containing the exact word "banana"
* *"prefix:head"*: searches for documents with words that start with prefix "head"
* *"prefix:head,banana"*: searches for documents contain words starting with prefix 
  "head" and that also contain the exact word "banana".

Complete match and prefix search options can be combined with the logical
operators.

Please note that only words with a minimum length will get indexed. This minimum
length can be defined when creating the fulltext index. For words tokenization,
the libicu text boundary analysis is used, which takes into account the default
as defined at server startup (*--server.default-language* startup
option). Generally, the word boundary analysis will filter out punctuation but
will not do much more.

Especially no word normalization, stemming, or similarity analysis will be
performed when indexing or searching. If any of these features is required, it
is suggested that the user does the text normalization on the client side, and
provides for each document an extra attribute containing just a comma-separated
list of normalized words. This attribute can then be indexed with a fulltext
index, and the user can send fulltext queries for this index, with the fulltext
queries also containing the stemmed or normalized versions of words as required
by the user.
