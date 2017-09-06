IResearch Views in AQL
======================

Views of type **iresearch** are an integration layer meant to seamlessly
integrate with and natively expose the full power of the
[IResearch library](https://github.com/iresearch-toolkit/iresearch)
to the ArangoDB user.

It provides the capability to:
* evaluate together documents located in different collections
* filter documents based on AQL boolean expressions and functions
* sort the resultset based on how closely each document matched the filter

IResearch value analysis
------------------------

A concept of value 'analysis' that is meant to break up a given value into
a set of sub-values internally tied together by metadata which influences both
the filter and sort stages to provide the most appropriate match for the
specified conditions, similar to queries to web search engines.

In plain terms this means a user can for example:
* request documents where the 'body' attribute best matches 'a quick brown fox'
* request documents where the 'dna' attribute best matches a DNA sub sequence
* request documents where the 'name' attribute best matches gender
* etc... (via custom analyzers described in the next section)

To a limited degree the concept of 'analysis' is even available in non-IResearch
AQL, e.g. the TOKENS(...) function will utilize the power of IResearch to break
up a value into an AQL array that can be used anywhere in the AQL query.

In plain terms this means a user can match a document attribute when its
value matches at least one entry from a set,
e.g. to match docs with 'word == quick' OR 'word == brown' OR 'word == fox'

    FOR doc IN someCollection
      FILTER doc.word IN TOKENS('a quick brown fox', 'text_en')
      RETRUN doc

IResearch filters
-----------------

The basic IReserch functionality can be accessed via common AQL filters and
operators, e.g.:
<br>AND, OR, NOT, ==, <=, >=, <, >, !=, IN \<ARRAY\>, IN \<RANGE\>

However, the full power of IResearch is harnessed and exposed via functions,
during both the filter and sort stages.

The supported filter functions are:
* EXISTS(\<attribute-name\>)
<br>  to match documents where the **attribute-name** exists in the document
* EXISTS(\<attribute-name\>, "analyzer", "\<analyzer\>")
<br>  to match documents where the **attribute-name** exists in the document and
      was indexed by the specified **analyzer**
* EXISTS(\<attribute-name\>, "type", "\<"bool"|"boolean"|"numeric"|"null"|"string"\>")
<br>  to match documents where the **attribute-name** exists in the document and
      is of the specified type, one of:
      **bool**, **boolean**, **numeric**, **null**, **string**
* PHRASE(\<attribute-name\>, "\<input\>" [, "\<offset\>", "\<input\>"], "\<analyzer\>")
<br>  to optionally break up **input**, with the help of the specified
      **analyzer**, into a vector of sub-tokens optionally separated from each
      other by **offset** words, then to match this vector against the value of
      the **attribute-name**
* STARTS_WITH(\<attribute-name\>, "\<prefix\>")
<br>  to match the value of the **attribute-name** that starts with **prefix**
* TOKENS("\<input\>", "\<analyzer\>")
<br>  to break up **input**, with the help of the specified **analyzer**, into
      an AQL array of sub-tokens which can then be used in filters with the
      **IN** operator

#### filtering examples:

to match documents which have a 'name' attribute

    FOR doc IN VIEW someView
      FILTER EXISTS(doc.name)
      RETURN doc

or

    FOR doc IN VIEW someView
      FILTER EXISTS(doc['name'])
      RETURN doc

to match documents where 'body' was analyzed via the 'text_en' analyzer

    FOR doc IN VIEW someView
      FILTER EXISTS(doc.body, 'analyzer', 'text_en')
      RETURN doc

or

    FOR doc IN VIEW someView
      FILTER EXISTS(doc['body'], 'analyzer', 'text_en')
      RETURN doc

to match documents which have an 'age' attribute of type number

    FOR doc IN VIEW someView
      FILTER EXISTS(doc.age, 'type' 'numeric')
      RETURN doc

or

    FOR doc IN VIEW someView
      FILTER EXISTS(doc['age'], 'type' 'numeric')
      RETURN doc

to match documents where 'description' contains a phrase 'quick brown'

    FOR doc IN VIEW someView
      FILTER PHRASE(doc.description, 'quick brown', 'text_en')
      RETURN doc

or

    FOR doc IN VIEW someView
      FILTER PHRASE(doc['description'], 'quick brown', 'text_en')
      RETURN doc

to match documents where 'body' contains 'quick' \<any 1 word\> 'fox jumps'

    FOR doc IN VIEW someView
      FILTER PHRASE(doc.body, 'quick', 1, 'fox jumps', 'text_en')
      RETURN doc

or

    FOR doc IN VIEW someView
      FILTER PHRASE(doc['body'], 'quick', 1, 'fox jumps', 'text_en')
      RETURN doc

to match documents where 'story' starts with 'In the beginning'

    FOR doc IN VIEW someView
      FILTER STARTS_WITH(doc.story, 'In the beginning')
      RETURN DOC

or

    FOR doc IN VIEW someView
      FILTER STARTS_WITH(doc['story'], 'In the beginning')
      RETURN DOC

to match documents where 'description' best matches 'a quick brown fox'

    FOR doc IN VIEW someView
      FILTER doc.description IN TOKENS('a quick brown fox', 'text_en')
      RETURN doc

IResearch sort
--------------

A major feature of IResearch views is their capability of sorting results based
on the supplied filter conditions and zero or more sorting functions. The
sorting functions are meant to be user-defined.

However out-of-the-box the following functions are provided:
* \<attribute-name\>
<br> to sort documents by the value of the **attribute-name**
* BM25(\<attribute-name\>, [k, [b]])
<br> to sort documents via the
     [BM25 algorithm](https://en.wikipedia.org/wiki/Okapi_BM25)
     optionally specifying the **k** and **b** values of the algorithm
* TFIDF(\<attribute-name\>, [\<with-norms\>])
<br> to sort documents via the
     [TFIDF algorithm](https://en.wikipedia.org/wiki/TF-IDF)
     optionally specifying that norms should be used via **with-norms**

Note: As a technical AQL requirement the first argument to any sorting function
      is _always_ either the output document variable itself (from the **FOR**
      statement) or some sub-attribute of the said output document variable.

#### sorting examples:

to sort documents by the value of the 'name' attribute

    FOR doc IN VIEW someView
      SORT doc.name
      RETURN doc

or

    FOR doc IN VIEW someView
      SORT doc['name']
      RETURN doc

to sort documents via the
[BM25 algorithm](https://en.wikipedia.org/wiki/Okapi_BM25)

    FOR doc IN VIEW someView
      SORT BM25(doc)
      RETURN doc

to sort documents via the
[BM25 algorithm](https://en.wikipedia.org/wiki/Okapi_BM25)
with 'k' = 1.2 and 'b' = 0.75

    FOR doc IN VIEW someView
      SORT BM25(doc, 1.2, 0.75)
      RETURN doc

to sort documents via the
[TFIDF algorithm](https://en.wikipedia.org/wiki/TF-IDF)

    FOR doc IN VIEW someView
      SORT TFIDF(doc)
      RETURN doc

to sort documents via the
[TFIDF algorithm](https://en.wikipedia.org/wiki/TF-IDF) with norms

    FOR doc IN VIEW someView
      SORT TFIDF(doc, true)
      RETURN doc

to sort documents by value of 'name' and then by the
[TFIDF algorithm](https://en.wikipedia.org/wiki/TF-IDF) where 'name' values are
equivalent

    FOR doc IN VIEW someView
      SORT doc.name, TFIDF(doc)
      RETURN doc


Use cases
---------

### Prefix search

Input data...
<br>{ "id": 1, "body": "ThisIsAVeryLongWord" }
<br>{ "id": 2, "body": "ThisIsNotSoLong" }
<br>{ "id": 3, "body": "ThisIsShorter" }
<br>{ "id": 4, "body": "ThisIs" }
<br>{ "id": 5, "body": "ButNotThis" }

What we want to find...
<br> all documents where body starts with "ThisIs", i.e. ids: 1, 2, 3, 4

How to express this in AQL...

    FOR doc IN VIEW someView
      FILTER STARTS_WITH(doc.body, 'ThisIs')
      RETURN doc
